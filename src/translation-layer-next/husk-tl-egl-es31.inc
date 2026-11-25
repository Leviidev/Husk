/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * GLES 3.1 on a 3.0 driver (included by husk-tl-egl.c, which supplies a_glGetIntegerv / a_eglGetProcAddress).
 *
 * ANGLE over Metal stops at OpenGL ES 3.0, and Minecraft's Android materials hold only "#version 310 es" shaders, chosen by
 * reporting 3.1. For a game that asks for it (tl_egl_es31_shim) the driver is described as 3.1 and each such shader is written
 * down to 300 es: the version line changes, and `layout(binding = N)` -- which 3.0 does not have -- is dropped and applied after
 * the link as glUniform1i / glUniformBlockBinding. What 3.1 really adds (compute, images, buffers) is not there.
 */
static bool g_es31_shim;
static int g_es3_minor = 1;                                 /* the minor version told: 1, or 2 for an app whose manifest needs 3.2 */
void tl_egl_es31_shim(bool on) { g_es31_shim = on; }
void tl_egl_es3_claim(int minor)
{
    if (minor <= 0) { g_es31_shim = false; g_es3_minor = 1; return; }
    g_es31_shim = true;
    g_es3_minor = minor > 2 ? 2 : minor;
}

typedef struct { char name[72]; int binding; bool block; } es31_bind;
#define ES31_SHADERS 1024
#define ES31_BINDS 32
#define ES31_ATTACHED 2048
static struct { unsigned shader; int n; es31_bind b[ES31_BINDS]; } g_es31_sh[ES31_SHADERS];
static struct { unsigned prog, shader; } g_es31_att[ES31_ATTACHED];
static int g_es31_natt;
static pthread_mutex_t g_es31_mu = PTHREAD_MUTEX_INITIALIZER;

/* "OpenGL ES 3.0 ..." -> "OpenGL ES 3.1 ...", "OpenGL ES GLSL ES 3.00 ..." -> "... 3.10 ..." (a copy: the driver's string is not ours to edit) */
static const char *es31_fix(const char *s, bool glsl)
{
    static char ring[8][300]; static atomic_uint k;
    char *out = ring[atomic_fetch_add(&k, 1) % 8];
    snprintf(out, 300, "%s", s);
    if (glsl) { char *p = strstr(out, "GLSL ES 3.00"); if (p) { p[10] = (char)('0' + g_es3_minor); p[11] = '0'; } }
    else { char *p = strstr(out, "OpenGL ES 3.0"); if (p) p[12] = (char)('0' + g_es3_minor); }
    return out;
}

static bool es31_ident(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'; }

/* The 300 es text of a 310 es shader (malloc'd), with the bindings it dropped in rec[0..*nrec). Anything else is returned as it came. */
static char *es31_downgrade(const char *src, size_t n, es31_bind *rec, int *nrec)
{
    char *out = malloc(n + 4096); size_t o = 0;
    *nrec = 0;
    const char *ver = strstr(src, "#version 310 es");
    if (!ver) ver = strstr(src, "#version 320 es");
    if (!ver) { memcpy(out, src, n); out[n] = 0; return out; }
    for (size_t i = 0; i < n;) {
        if (src + i == ver) { memcpy(out + o, "#version 300 es", 15); o += 15; i += 15; continue; }
        if (i + 7 <= n && !strncmp(src + i, "layout(", 7) && (i == 0 || !es31_ident(src[i - 1]))) {
            size_t j = i + 7; int depth = 1;
            while (j < n && depth) { if (src[j] == '(') depth++; else if (src[j] == ')') depth--; j++; }
            char q[160]; size_t ql = j - 1 - (i + 7); if (ql >= sizeof(q)) ql = sizeof(q) - 1;
            memcpy(q, src + i + 7, ql); q[ql] = 0;
            char *b = strstr(q, "binding");
            size_t d = j; while (d < n && src[d] != ';' && src[d] != '{') d++;
            char decl[200]; size_t dl = d - j; if (dl >= sizeof(decl)) dl = sizeof(decl) - 1;
            memcpy(decl, src + j, dl); decl[dl] = 0;
            bool opaque = strstr(decl, "image") || strstr(decl, "atomic_uint");
            bool ssbo = !opaque && strstr(decl, "buffer") && d < n && src[d] == '{' && !strstr(decl, "writeonly") && !strstr(decl, "coherent");
            if (ssbo && b) {
                /* A read-only storage block of structs becomes a uniform block of 1024 of them (std140 and std430 lay these structs out alike
                 * when their members are 4-byte scalars); the game's glBindBufferBase on the storage target is redirected to the uniform one. */
                size_t close = d; int depth2 = 0;
                while (close < n) { if (src[close] == '{') depth2++; else if (src[close] == '}' && --depth2 == 0) break; close++; }
                char *eq = strchr(b, '='); int val = eq ? atoi(eq + 1) : -1;
                size_t e = dl;
                while (e && (decl[e - 1] == ' ' || decl[e - 1] == '\n' || decl[e - 1] == '\t' || decl[e - 1] == '\r')) e--;
                size_t st = e; while (st && es31_ident(decl[st - 1])) st--;
                if (close < n && val >= 0 && e > st && e - st < sizeof(rec[0].name) && *nrec < ES31_BINDS) {
                    memcpy(rec[*nrec].name, decl + st, e - st); rec[*nrec].name[e - st] = 0; rec[*nrec].binding = val; rec[*nrec].block = true; (*nrec)++;
                    o += (size_t)sprintf(out + o, "layout(std140) uniform %.*s\n{", (int)(e - st), decl + st);
                    for (size_t k = d + 1; k < close; k++) {
                        if (src[k] == '[' && k + 1 < close && src[k + 1] == ']') { o += (size_t)sprintf(out + o, "[1024]"); k++; }
                        else out[o++] = src[k];
                    }
                    out[o++] = '}';
                    i = close + 1; continue;
                }
            }
            if (b && !opaque && !ssbo) {
                char *eq = strchr(b, '='); int val = eq ? atoi(eq + 1) : -1;
                bool block = d < n && src[d] == '{';
                if (*nrec < ES31_BINDS && val >= 0) {
                    size_t e = dl;
                    while (e && (decl[e - 1] == ' ' || decl[e - 1] == '\n' || decl[e - 1] == '\t' || decl[e - 1] == '\r')) e--;
                    while (e && decl[e - 1] == ']') { while (e && decl[e - 1] != '[') e--; if (e) e--; }
                    size_t st = e; while (st && es31_ident(decl[st - 1])) st--;
                    if (e > st && e - st < sizeof(rec[0].name)) {
                        memcpy(rec[*nrec].name, decl + st, e - st); rec[*nrec].name[e - st] = 0;
                        rec[*nrec].binding = val; rec[*nrec].block = block; (*nrec)++;
                    }
                }
                /* the layout again, without the binding qualifier */
                char keep[160]; size_t kl = 0; const char *tok = q;
                while (tok) {
                    const char *comma = strchr(tok, ',');
                    size_t tl = comma ? (size_t)(comma - tok) : strlen(tok);
                    const char *t = tok; size_t tn = tl; while (tn && *t == ' ') { t++; tn--; }
                    if (!(tn >= 7 && !strncmp(t, "binding", 7)) && tn) { if (kl) keep[kl++] = ','; memcpy(keep + kl, t, tn); kl += tn; }
                    tok = comma ? comma + 1 : NULL;
                }
                if (kl) { memcpy(out + o, "layout(", 7); o += 7; memcpy(out + o, keep, kl); o += kl; out[o++] = ')'; }
                i = j; continue;
            }
            memcpy(out + o, src + i, j - i); o += j - i; i = j; continue;
        }
        out[o++] = src[i++];
    }
    out[o] = 0;
    return out;
}

/* External (camera/video) textures: ANGLE over Metal has no OES_EGL_image_external, so a shader declaring one does not compile,
 * and Flutter's Impeller then draws nothing at all (it builds every pipeline at start). The extension lines go and samplerExternalOES
 * becomes sampler2D, which texture()/texture2D() take as well; games rarely sample a real external image. NULL: nothing to change. */
static char *oes_external_rewrite(const char *src)
{
    if (!strstr(src, "samplerExternalOES") && !strstr(src, "GL_OES_EGL_image_external")) return NULL;
    size_t n = strlen(src);
    char *out = malloc(n + 1); size_t o = 0;
    for (size_t i = 0; i < n;) {
        if ((i == 0 || src[i - 1] == '\n') && src[i] == '#') {
            size_t j = i + 1; while (j < n && (src[j] == ' ' || src[j] == '\t')) j++;
            if (!strncmp(src + j, "extension", 9)) {
                const char *eol = memchr(src + j, '\n', n - j);
                size_t e = eol ? (size_t)(eol - src) : n;
                if (memmem(src + j, e - j, "GL_OES_EGL_image_external", 25)) { i = e; continue; }
            }
        }
        if (!strncmp(src + i, "samplerExternalOES", 18)) { memcpy(out + o, "sampler2D", 9); o += 9; i += 18; continue; }
        out[o++] = src[i++];
    }
    out[o] = 0;
    return out;
}

static void (*r_glShaderSource)(unsigned, int, const char *const *, const int *);
static void w_glShaderSource(unsigned sh, int count, const char *const *str, const int *len)
{
    if (count <= 0 || !str) { r_glShaderSource(sh, count, str, len); return; }
    size_t total = 0;
    for (int i = 0; i < count; i++) total += str[i] ? (len && len[i] >= 0 ? (size_t)len[i] : strlen(str[i])) : 0;
    char *cat = malloc(total + 1); size_t k = 0;
    for (int i = 0; i < count; i++) if (str[i]) { size_t l = len && len[i] >= 0 ? (size_t)len[i] : strlen(str[i]); memcpy(cat + k, str[i], l); k += l; }
    cat[k] = 0;
    char *ext = oes_external_rewrite(cat);
    if (ext) { free(cat); cat = ext; k = strlen(cat); }
    if (!g_es31_shim) {
        const char *one = cat; int one_len = (int)k;
        r_glShaderSource(sh, 1, &one, &one_len);
        free(cat);
        return;
    }
    es31_bind rec[ES31_BINDS]; int nrec = 0;
    char *down = es31_downgrade(cat, k, rec, &nrec);
    /* Unity's 3.1 shaders give uniforms explicit locations (UNITY_LOCATION(x) -> layout(location = x)), which ES 3.0 refuses
     * on anything but inputs and outputs; its own switch turns them off. */
    /* Unity gives explicit locations only to samplers, and the location is the texture unit: each becomes a binding, set after link. */
    for (char *u = down; (u = strstr(u, "#define UNITY_SUPPORTS_UNIFORM_LOCATION 1")); ) {
        u[sizeof("#define UNITY_SUPPORTS_UNIFORM_LOCATION ") - 1] = '0';
        for (const char *l = down; (l = strstr(l, "UNITY_LOCATION(")) && nrec < ES31_BINDS; l++) {
            const char *d = l + sizeof("UNITY_LOCATION(") - 1;
            if (*d < '0' || *d > '9') continue;
            int unit = atoi(d);
            const char *eol = strchr(d, '\n'), *semi = strchr(d, ';');
            if (!semi || (eol && semi > eol) || !strstr(d, "uniform") || strstr(d, "uniform") > semi) continue;
            const char *e = semi;                          /* the name: the identifier before ';', past an array size */
            while (e > d && e[-1] == ' ') e--;
            if (e > d && e[-1] == ']') { while (e > d && e[-1] != '[') e--; if (e > d) e--; while (e > d && e[-1] == ' ') e--; }
            const char *b = e; while (b > d && es31_ident((unsigned char)b[-1])) b--;
            if (e - b <= 0 || e - b >= (long)sizeof(rec[0].name)) continue;
            es31_bind *r = &rec[nrec++];
            memcpy(r->name, b, (size_t)(e - b)); r->name[e - b] = 0; r->binding = unit; r->block = false;
        }
        break;
    }
    pthread_mutex_lock(&g_es31_mu);
    int slot = (int)(sh % ES31_SHADERS);
    g_es31_sh[slot].shader = sh; g_es31_sh[slot].n = nrec; memcpy(g_es31_sh[slot].b, rec, sizeof(es31_bind) * (size_t)nrec);
    pthread_mutex_unlock(&g_es31_mu);
    const char *one = down; int one_len = (int)strlen(down);
    r_glShaderSource(sh, 1, &one, &one_len);
    free(cat); free(down);
}

static void (*r_glAttachShader)(unsigned, unsigned);
static void w_glAttachShader(unsigned prog, unsigned sh)
{
    r_glAttachShader(prog, sh);
    if (!g_es31_shim) return;
    pthread_mutex_lock(&g_es31_mu);
    g_es31_att[g_es31_natt % ES31_ATTACHED].prog = prog; g_es31_att[g_es31_natt % ES31_ATTACHED].shader = sh; g_es31_natt++;
    pthread_mutex_unlock(&g_es31_mu);
}

static void (*r_glLinkProgram)(unsigned);
static void w_glLinkProgram(unsigned prog)
{
    r_glLinkProgram(prog);
    if (!g_es31_shim || !a_eglGetProcAddress) return;
    static int (*getloc)(unsigned, const char *); static unsigned (*getblk)(unsigned, const char *);
    static void (*blkbind)(unsigned, unsigned, unsigned), (*uni1i)(int, int), (*useprog)(unsigned), (*getprogiv)(unsigned, unsigned, int *);
    if (!getloc) {
        getloc = a_eglGetProcAddress("glGetUniformLocation"); getblk = a_eglGetProcAddress("glGetUniformBlockIndex");
        blkbind = a_eglGetProcAddress("glUniformBlockBinding"); uni1i = a_eglGetProcAddress("glUniform1i");
        useprog = a_eglGetProcAddress("glUseProgram"); getprogiv = a_eglGetProcAddress("glGetProgramiv");
    }
    if (!getloc || !getblk || !blkbind || !uni1i || !useprog || !getprogiv) return;
    int ok = 0; getprogiv(prog, 0x8B82 /* GL_LINK_STATUS */, &ok);
    if (!ok) return;
    int cur = 0; a_glGetIntegerv(0x8B8D /* GL_CURRENT_PROGRAM */, &cur);
    bool used = false;
    pthread_mutex_lock(&g_es31_mu);
    int total = g_es31_natt < ES31_ATTACHED ? g_es31_natt : ES31_ATTACHED;
    for (int a = 0; a < total; a++) {
        if (g_es31_att[a].prog != prog) continue;
        unsigned sh = g_es31_att[a].shader;
        int slot = (int)(sh % ES31_SHADERS);
        if (g_es31_sh[slot].shader != sh) continue;
        for (int b = 0; b < g_es31_sh[slot].n; b++) {
            const es31_bind *e = &g_es31_sh[slot].b[b];
            if (e->block) { unsigned idx = getblk(prog, e->name); if (idx != 0xFFFFFFFFu) blkbind(prog, idx, (unsigned)e->binding); }
            else { int loc = getloc(prog, e->name); if (loc >= 0) { if (!used) { useprog(prog); used = true; } uni1i(loc, e->binding); } }
        }
    }
    pthread_mutex_unlock(&g_es31_mu);
    if (used) useprog((unsigned)cur);
}

static void (*r_glGetIntegerv)(unsigned, int *);
static void w_glGetIntegerv(unsigned pname, int *data)
{
    if (g_es31_shim && data) *data = 0;   /* a 3.1-only limit is an error on the 3.0 context, which writes nothing */
    r_glGetIntegerv(pname, data);
    if (g_es31_shim && pname == 0x821C /* GL_MINOR_VERSION */ && data && *data == 0) *data = g_es3_minor;
}

/* The storage-buffer target (a block of the game's data) is a uniform buffer here; a buffer the game made for it is grown to what the
 * 1024-element block above needs, so the binding is never smaller than the block. */
static void es31_note_storage(const char *call, unsigned target)
{
    static atomic_int said;
    if (g_es31_shim && target == 0x90D2 && atomic_fetch_add(&said, 1) < 12) tl_log_line("es31: %s on the storage-buffer target", call);
}
#define ES31_UBO_MIN (1024 * 32)
static void (*r_glBindBufferBase)(unsigned, unsigned, unsigned);
static void w_glBindBufferBase(unsigned target, unsigned index, unsigned buffer)
{
    es31_note_storage("glBindBufferBase", target);
    if (g_es31_shim && target == 0x90D2 /* GL_SHADER_STORAGE_BUFFER */) {
        static void (*getparam)(unsigned, unsigned, int *), (*bindbuf)(unsigned, unsigned), (*bufdata)(unsigned, long, const void *, unsigned),
                    (*copysub)(unsigned, unsigned, long, long, long), (*getsub)(unsigned, long, long, void *), (*subdata)(unsigned, long, long, const void *);
        if (!getparam && a_eglGetProcAddress) {
            getparam = a_eglGetProcAddress("glGetBufferParameteriv"); bindbuf = a_eglGetProcAddress("glBindBuffer");
            bufdata = a_eglGetProcAddress("glBufferData"); getsub = a_eglGetProcAddress("glGetBufferSubData"); subdata = a_eglGetProcAddress("glBufferSubData");
            copysub = a_eglGetProcAddress("glCopyBufferSubData");
        }
        if (getparam && bindbuf && bufdata && subdata && copysub && buffer) {
            int size = 0, usage = 0x88E4; /* GL_STATIC_DRAW */
            bindbuf(0x8F36 /* GL_COPY_WRITE_BUFFER */, buffer);
            getparam(0x8F36, 0x8764 /* GL_BUFFER_SIZE */, &size);
            getparam(0x8F36, 0x8765 /* GL_BUFFER_USAGE */, &usage);
            if (size > 0 && size < ES31_UBO_MIN) {
                unsigned tmp = 0; void (*gen)(int, unsigned *) = a_eglGetProcAddress("glGenBuffers"); void (*del)(int, const unsigned *) = a_eglGetProcAddress("glDeleteBuffers");
                if (gen && del) {
                    gen(1, &tmp);
                    bindbuf(0x8F37 /* GL_COPY_READ_BUFFER */, tmp);
                    bufdata(0x8F37, size, NULL, 0x88E4);
                    copysub(0x8F36, 0x8F37, 0, 0, size);
                    bufdata(0x8F36, ES31_UBO_MIN, NULL, (unsigned)usage);
                    copysub(0x8F37, 0x8F36, 0, 0, size);
                    del(1, &tmp);
                }
            }
            bindbuf(0x8F36, 0);
        }
        target = 0x8A11; /* GL_UNIFORM_BUFFER */
    }
    r_glBindBufferBase(target, index, buffer);
}

/* The other calls that name the storage target: it is the uniform one here. */
#define ES31_STORAGE(t) ((t) == 0x90D2 ? 0x8A11u : (t))
static void (*r_glBindBuffer)(unsigned, unsigned);
static void w_glBindBuffer(unsigned t, unsigned b) { es31_note_storage("glBindBuffer", t); r_glBindBuffer(g_es31_shim ? ES31_STORAGE(t) : t, b); }
static void (*r_glBufferData)(unsigned, long, const void *, unsigned);
static void w_glBufferData(unsigned t, long n, const void *d, unsigned u)
{ es31_note_storage("glBufferData", t); RHI_PROF_CALL(TL_GL_PHASE_BUF, r_glBufferData(g_es31_shim ? ES31_STORAGE(t) : t, n, d, u)); }
static void (*r_glBufferSubData)(unsigned, long, long, const void *);
static void w_glBufferSubData(unsigned t, long o, long n, const void *d)
{ es31_note_storage("glBufferSubData", t); RHI_PROF_CALL(TL_GL_PHASE_BUF, r_glBufferSubData(g_es31_shim ? ES31_STORAGE(t) : t, o, n, d)); }
static void (*r_glBindBufferRange)(unsigned, unsigned, unsigned, long, long);
static void w_glBindBufferRange(unsigned t, unsigned i, unsigned b, long o, long n)
{
    es31_note_storage("glBindBufferRange", t);
    if (g_es31_shim && t == 0x90D2) { w_glBindBufferBase(t, i, b); return; }   /* grows the buffer if need be; the whole buffer is bound */
    r_glBindBufferRange(t, i, b, o, n);
}

/*
 * The ES 3.1 program-interface queries, for the shim's 3.0 context. ANGLE exports them, but on a 3.0 context they only raise
 * GL_INVALID_OPERATION and write nothing, and an engine that was told 3.1 (Unity 6 reflects its shaders this way) then reads
 * uninitialised counts and name lengths and corrupts its heap. They are answered from the 3.0 queries instead: uniforms, uniform
 * blocks (the shim's storage blocks are uniform blocks too, so they are reported as such) and vertex inputs. Any other interface
 * has no resources.
 */
#define ES31_UNIFORM 0x92E1u
#define ES31_UNIFORM_BLOCK 0x92E2u
#define ES31_PROGRAM_INPUT 0x92E3u
static struct {
    bool ready;
    void (*progiv)(unsigned, unsigned, int *);
    void (*active_uniform)(unsigned, unsigned, int, int *, int *, unsigned *, char *);
    void (*active_attrib)(unsigned, unsigned, int, int *, int *, unsigned *, char *);
    void (*uniforms_iv)(unsigned, int, const unsigned *, unsigned, int *);
    void (*block_iv)(unsigned, unsigned, unsigned, int *);
    void (*block_name)(unsigned, unsigned, int, int *, char *);
    unsigned (*block_index)(unsigned, const char *);
    void (*uniform_indices)(unsigned, int, const char *const *, unsigned *);
    int (*uniform_loc)(unsigned, const char *);
    int (*attrib_loc)(unsigned, const char *);
} Q;
static bool es31_q(void)
{
    if (!Q.ready && E.ready && a_eglGetProcAddress) {
        Q.progiv = a_eglGetProcAddress("glGetProgramiv"); Q.active_uniform = a_eglGetProcAddress("glGetActiveUniform");
        Q.active_attrib = a_eglGetProcAddress("glGetActiveAttrib"); Q.uniforms_iv = a_eglGetProcAddress("glGetActiveUniformsiv");
        Q.block_iv = a_eglGetProcAddress("glGetActiveUniformBlockiv"); Q.block_name = a_eglGetProcAddress("glGetActiveUniformBlockName");
        Q.block_index = a_eglGetProcAddress("glGetUniformBlockIndex"); Q.uniform_indices = a_eglGetProcAddress("glGetUniformIndices");
        Q.uniform_loc = a_eglGetProcAddress("glGetUniformLocation"); Q.attrib_loc = a_eglGetProcAddress("glGetAttribLocation");
        Q.ready = Q.progiv && Q.active_uniform && Q.active_attrib && Q.uniforms_iv && Q.block_iv && Q.block_name && Q.block_index
               && Q.uniform_indices && Q.uniform_loc && Q.attrib_loc;
    }
    return Q.ready;
}

static void (*r_glGetProgramInterfaceiv)(unsigned, unsigned, unsigned, int *);
static void w_glGetProgramInterfaceiv(unsigned prog, unsigned iface, unsigned pname, int *params)
{
    if (!g_es31_shim) { r_glGetProgramInterfaceiv(prog, iface, pname, params); return; }
    if (!params) return;
    *params = 0;
    if (!es31_q()) return;
    bool count = pname == 0x92F5 /* GL_ACTIVE_RESOURCES */, maxname = pname == 0x92F6 /* GL_MAX_NAME_LENGTH */;
    if (iface == ES31_UNIFORM) {
        if (count) Q.progiv(prog, 0x8B86 /* GL_ACTIVE_UNIFORMS */, params);
        else if (maxname) Q.progiv(prog, 0x8B87 /* GL_ACTIVE_UNIFORM_MAX_LENGTH */, params);
    } else if (iface == ES31_UNIFORM_BLOCK) {
        int n = 0; Q.progiv(prog, 0x8A36 /* GL_ACTIVE_UNIFORM_BLOCKS */, &n);
        if (count) *params = n;
        else if (maxname) Q.progiv(prog, 0x8A35 /* GL_ACTIVE_UNIFORM_BLOCK_MAX_NAME_LENGTH */, params);
        else if (pname == 0x92F7 /* GL_MAX_NUM_ACTIVE_VARIABLES */)
            for (int i = 0; i < n; i++) { int v = 0; Q.block_iv(prog, (unsigned)i, 0x8A42 /* GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS */, &v); if (v > *params) *params = v; }
    } else if (iface == ES31_PROGRAM_INPUT) {
        if (count) Q.progiv(prog, 0x8B89 /* GL_ACTIVE_ATTRIBUTES */, params);
        else if (maxname) Q.progiv(prog, 0x8B8A /* GL_ACTIVE_ATTRIBUTE_MAX_LENGTH */, params);
    }
}

static unsigned (*r_glGetProgramResourceIndex)(unsigned, unsigned, const char *);
static unsigned w_glGetProgramResourceIndex(unsigned prog, unsigned iface, const char *name)
{
    if (!g_es31_shim) return r_glGetProgramResourceIndex(prog, iface, name);
    if (!name || !es31_q()) return 0xFFFFFFFFu;
    if (iface == ES31_UNIFORM) { unsigned i = 0xFFFFFFFFu; Q.uniform_indices(prog, 1, &name, &i); return i; }
    if (iface == ES31_UNIFORM_BLOCK) return Q.block_index(prog, name);
    if (iface == ES31_PROGRAM_INPUT) {
        int n = 0; Q.progiv(prog, 0x8B89, &n);
        for (int i = 0; i < n; i++) {
            char nm[256]; int len = 0, size = 0; unsigned type = 0; nm[0] = 0;
            Q.active_attrib(prog, (unsigned)i, sizeof(nm), &len, &size, &type, nm);
            if (!strcmp(nm, name)) return (unsigned)i;
        }
    }
    return 0xFFFFFFFFu;
}

static void (*r_glGetProgramResourceName)(unsigned, unsigned, unsigned, int, int *, char *);
static void w_glGetProgramResourceName(unsigned prog, unsigned iface, unsigned index, int bufsize, int *length, char *name)
{
    if (!g_es31_shim) { r_glGetProgramResourceName(prog, iface, index, bufsize, length, name); return; }
    if (length) *length = 0;
    if (name && bufsize > 0) name[0] = 0;
    if (!es31_q()) return;
    int size = 0; unsigned type = 0;
    if (iface == ES31_UNIFORM) Q.active_uniform(prog, index, bufsize, length, &size, &type, name);
    else if (iface == ES31_UNIFORM_BLOCK) Q.block_name(prog, index, bufsize, length, name);
    else if (iface == ES31_PROGRAM_INPUT) Q.active_attrib(prog, index, bufsize, length, &size, &type, name);
}

static void (*r_glGetProgramResourceiv)(unsigned, unsigned, unsigned, int, const unsigned *, int, int *, int *);
static void w_glGetProgramResourceiv(unsigned prog, unsigned iface, unsigned index, int nprops, const unsigned *props, int bufsize,
                                     int *length, int *params)
{
    if (!g_es31_shim) { r_glGetProgramResourceiv(prog, iface, index, nprops, props, bufsize, length, params); return; }
    int k = 0;
    bool q = es31_q();
    char nm[256]; nm[0] = 0;
    int asize = 0; unsigned atype = 0;
    if (q && iface == ES31_PROGRAM_INPUT) { int l = 0; Q.active_attrib(prog, index, sizeof(nm), &l, &asize, &atype, nm); }
    for (int p = 0; props && params && p < nprops && k < bufsize; p++) {
        unsigned prop = props[p];
        int v = 0;
        if (q && iface == ES31_UNIFORM) {
            unsigned up = prop == 0x92FA ? 0x8A37 : prop == 0x92FB ? 0x8A38 : prop == 0x92F9 ? 0x8A39 : prop == 0x92FD ? 0x8A3A
                        : prop == 0x92FC ? 0x8A3B : prop == 0x92FE ? 0x8A3C : prop == 0x92FF ? 0x8A3D : prop == 0x9300 ? 0x8A3E : 0;
            if (up) Q.uniforms_iv(prog, 1, &index, up, &v);
            else if (prop == 0x930E /* GL_LOCATION */) {
                int l = 0, s = 0; unsigned t = 0; Q.active_uniform(prog, index, sizeof(nm), &l, &s, &t, nm);
                v = Q.uniform_loc(prog, nm);
            } else if (prop >= 0x9306 && prop <= 0x930B) v = prop == 0x9306 || prop == 0x930A;   /* referenced by vertex, fragment */
            else if (prop == 0x9301 /* GL_ATOMIC_COUNTER_BUFFER_INDEX */) v = -1;
        } else if (q && iface == ES31_UNIFORM_BLOCK) {
            unsigned bp = prop == 0x9302 ? 0x8A3F : prop == 0x9303 ? 0x8A40 : prop == 0x92F9 ? 0x8A41 : prop == 0x9304 ? 0x8A42
                        : prop == 0x9306 ? 0x8A44 : prop == 0x930A ? 0x8A46 : 0;
            if (prop == 0x9305 /* GL_ACTIVE_VARIABLES: one value per member */) {
                int n = 0; Q.block_iv(prog, index, 0x8A42, &n);
                if (n > 0 && n <= 4096) {
                    int *ix = calloc((size_t)n, sizeof(int));
                    if (ix) { Q.block_iv(prog, index, 0x8A43, ix); for (int i = 0; i < n && k < bufsize; i++) params[k++] = ix[i]; free(ix); }
                }
                continue;
            }
            if (bp) Q.block_iv(prog, index, bp, &v);
        } else if (q && iface == ES31_PROGRAM_INPUT) {
            if (prop == 0x92FA) v = (int)atype;
            else if (prop == 0x92FB) v = asize;
            else if (prop == 0x92F9) v = nm[0] ? (int)strlen(nm) + 1 : 0;
            else if (prop == 0x930E) v = nm[0] ? Q.attrib_loc(prog, nm) : -1;
            else if (prop == 0x9306) v = 1;
        }
        params[k++] = v;
    }
    if (length) *length = k;
}
/* 3.1's indexed limits (compute work-group counts and sizes) are errors on the 3.0 context, which writes nothing: they read as zero. */
static void (*r_glGetIntegeri_v)(unsigned, unsigned, int *);
static void w_glGetIntegeri_v(unsigned p, unsigned i, int *d) { if (g_es31_shim && d) *d = 0; r_glGetIntegeri_v(p, i, d); }
