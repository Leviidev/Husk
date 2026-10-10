/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Dalvik interpreter. See husk-tl-dvm.h. */
#define _DARWIN_C_SOURCE
#include "husk-tl-dvm-internal.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"

int g_dvm_trace;
void tl_dvm_set_trace(int level) { g_dvm_trace = level; }

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

/* ================================================================== DEX files */

static dvm_dex *g_dex[256];
static int g_add_ns, g_next_ns;    /* the namespace dex_add gives the dexes it adds now (DexFile at run time) */
static int g_nboot_dex = -1;     /* the boot class path's dexes are g_dex[0 .. g_nboot_dex) */
static int g_ndex;

static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t uleb(const uint8_t **pp)
{
    const uint8_t *p = *pp; uint32_t r = 0; int s = 0;
    for (;;) { uint8_t b = *p++; r |= (uint32_t)(b & 0x7f) << s; if (!(b & 0x80)) break; s += 7; if (s > 28) break; }
    *pp = p; return r;
}
static int32_t sleb(const uint8_t **pp)
{
    const uint8_t *p = *pp; int32_t r = 0; int s = 0; uint8_t b;
    do { b = *p++; r |= (int32_t)(b & 0x7f) << s; s += 7; } while ((b & 0x80) && s < 35);
    if (s < 32 && (b & 0x40)) r |= -(1 << s);
    *pp = p; return r;
}

static const char *dex_str(const dvm_dex *d, uint32_t idx)
{
    if (idx >= d->nstr) return "";
    const uint8_t *p = d->b + rd32(d->b + d->str_off + 4 * idx);
    uleb(&p);
    return (const char *)p;
}
static const char *dex_type(const dvm_dex *d, uint32_t idx) { return idx < d->ntype ? dex_str(d, rd32(d->b + d->type_off + 4 * idx)) : ""; }

/* A descriptor as a JNI class name: "Ljava/lang/String;" -> "java/lang/String"; arrays and primitives as they are. */
static void desc_to_name(const char *desc, char *out, size_t n)
{
    if (desc[0] == 'L') {
        size_t l = strlen(desc);
        snprintf(out, n, "%.*s", (int)(l >= 2 ? l - 2 : 0), desc + 1);
    } else snprintf(out, n, "%s", desc);
}

/* "(II)V" from a proto. */
static void proto_sig(const dvm_dex *d, uint32_t pi, char *out, size_t n, char *shorty, size_t sn, int *nparams)
{
    const uint8_t *p = d->b + d->proto_off + 12u * pi;
    uint32_t ret = rd32(p + 4), params = rd32(p + 8);
    size_t k = 0, sk = 0;
    const char *rt = dex_type(d, ret);
    if (shorty && sn > 1) shorty[sk++] = (rt[0] == '[') ? 'L' : rt[0];
    out[k++] = '(';
    int np = 0;
    if (params) {
        uint32_t cnt = rd32(d->b + params);
        for (uint32_t i = 0; i < cnt; i++) {
            const char *t = dex_type(d, rd16(d->b + params + 4 + 2 * i));
            size_t l = strlen(t);
            if (k + l + 2 < n) { memcpy(out + k, t, l); k += l; }
            if (shorty && sk + 1 < sn) shorty[sk++] = (t[0] == '[') ? 'L' : t[0];
            np++;
        }
    }
    out[k++] = ')';
    size_t l = strlen(rt);
    if (k + l + 1 < n) { memcpy(out + k, rt, l); k += l; }
    out[k] = 0;
    if (shorty) shorty[sk] = 0;
    if (nparams) *nparams = np;
}

/* ---- the class index: every class any DEX defines, first definition wins (boot class path first) */

typedef struct cdef { const char *name; dvm_dex *dex; uint32_t def; struct cdef *next; } cdef;
#define CBUCKETS 16384
static cdef *g_cdefs[CBUCKETS];
static uint32_t hstr(const char *s) { uint32_t h = 2166136261u; while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; } return h; }

static cdef *cdef_find(const char *name)
{
    for (cdef *c = g_cdefs[hstr(name) % CBUCKETS]; c; c = c->next) if (!strcmp(c->name, name)) return c;
    return NULL;
}

static bool dex_add(const uint8_t *b, size_t size, const char *label)
{
    if (size < 0x70 || memcmp(b, "dex\n", 4)) { tl_log_line("dvm: %s is not a DEX file", label); return false; }
    if (g_ndex >= 256) return false;
    dvm_dex *d = calloc(1, sizeof(*d));
    d->b = b; d->size = size;
    d->nstr = rd32(b + 0x38); d->str_off = rd32(b + 0x3c);
    d->ntype = rd32(b + 0x40); d->type_off = rd32(b + 0x44);
    d->nproto = rd32(b + 0x48); d->proto_off = rd32(b + 0x4c);
    d->nfield = rd32(b + 0x50); d->field_off = rd32(b + 0x54);
    d->nmeth = rd32(b + 0x58); d->meth_off = rd32(b + 0x5c);
    d->ncls = rd32(b + 0x60); d->cls_off = rd32(b + 0x64);
    d->mcache = calloc(d->nmeth ? d->nmeth : 1, sizeof(*d->mcache));
    d->fcache = calloc(d->nfield ? d->nfield : 1, sizeof(*d->fcache));
    d->tcache = calloc(d->ntype ? d->ntype : 1, sizeof(*d->tcache));
    d->scache = calloc(d->nstr ? d->nstr : 1, sizeof(*d->scache));
    snprintf(d->name, sizeof(d->name), "%s", label);
    d->ns = g_add_ns;
    int added = 0;
    for (uint32_t i = 0; i < d->ncls; i++) {
        const char *desc = dex_type(d, rd32(b + d->cls_off + 32 * i));
        char name[512]; desc_to_name(desc, name, sizeof(name));
        if (cdef_find(name)) {
            /* a run-time loader's class named like one already defined: its own, in its namespace (name@ns) */
            if (!d->ns) continue;
            char key[540]; snprintf(key, sizeof(key), "%s@%d", name, d->ns);
            if (cdef_find(key)) continue;
            snprintf(name, sizeof(name), "%s", key);
        }
        cdef *c = malloc(sizeof(*c));
        c->name = strdup(name); c->dex = d; c->def = i;
        uint32_t h = hstr(name) % CBUCKETS;
        c->next = g_cdefs[h]; g_cdefs[h] = c;
        added++;
    }
    g_dex[g_ndex++] = d;
    tl_log_line("dvm: %s: %u classes (%d new)", label, d->ncls, added);
    return true;
}

/* Every classes*.dex in a jar/APK, or a plain .dex file. The data is kept for the life of the process. */
/* Every jar and APK on the class path, for VMClassLoader.getBootClassPathEntries: libcore serves classpath resources
   (Class.getResourceAsStream) from these, as Android's boot class loader does from its jars. */
static char *g_cp[64]; static int g_ncp;
int tl_dvm_class_path(const char **out, int max) { int n = g_ncp < max ? g_ncp : max; for (int i = 0; i < n; i++) out[i] = g_cp[i]; return n; }
static bool open_path(const char *path)
{
    { size_t pl = strlen(path); if (pl > 4 && strcmp(path + pl - 4, ".dex") && g_ncp < 64) { bool dup = false; for (int i = 0; i < g_ncp && !dup; i++) dup = !strcmp(g_cp[i], path); if (!dup) g_cp[g_ncp++] = strdup(path); } }
    size_t l = strlen(path);
    if (l > 4 && !strcmp(path + l - 4, ".dex")) {
        FILE *f = fopen(path, "rb");
        if (!f) return false;
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *b = malloc((size_t)n);
        bool ok = fread(b, 1, (size_t)n, f) == (size_t)n;
        fclose(f);
        return ok && dex_add(b, (size_t)n, path);
    }
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, path, err, sizeof(err))) { tl_log_line("dvm: %s: %s", path, err); return false; }
    bool any = false;
    for (int i = 1; i < 100; i++) {
        char name[32];
        if (i == 1) snprintf(name, sizeof(name), "classes.dex"); else snprintf(name, sizeof(name), "classes%d.dex", i);
        const tl_zip_entry *e = tl_zip_find(&z, name);
        if (!e) break;
        const uint8_t *data; size_t len; bool owned;
        if (!tl_zip_data(&z, e, (size_t)1 << 30, &data, &len, &owned, err, sizeof(err))) continue;
        uint8_t *copy = malloc(len);
        memcpy(copy, data, len);
        if (owned) free((void *)data);
        char label[96];
        const char *slash = strrchr(path, '/');
        snprintf(label, sizeof(label), "%s!%s", slash ? slash + 1 : path, name);
        any |= dex_add(copy, len, label);
    }
    tl_zip_close(&z);
    return any;
}

/* ================================================================== strings */

/* MUTF-8 (a DEX's strings) to UTF-16. */
static uint16_t *mutf8_to_u16(const char *s, int32_t *n)
{
    size_t cap = strlen(s) + 1;
    uint16_t *o = malloc(cap * 2);
    int32_t k = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned c = *p++;
        if (c < 0x80) o[k++] = (uint16_t)c;
        else if ((c & 0xE0) == 0xC0) { o[k++] = (uint16_t)(((c & 0x1F) << 6) | (*p & 0x3F)); if (*p) p++; }
        else if ((c & 0xF0) == 0xE0) { unsigned c2 = *p ? *p++ : 0, c3 = *p ? *p++ : 0; o[k++] = (uint16_t)(((c & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F)); }
        else o[k++] = 0xFFFD;
    }
    *n = k;
    return o;
}

/* UTF-16 to standard UTF-8 (surrogate pairs joined). */
static char *u16_to_utf8(const uint16_t *s, int32_t n)
{
    char *o = malloc((size_t)n * 3 + 1);
    size_t k = 0;
    for (int32_t i = 0; i < n; i++) {
        uint32_t c = s[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && s[i + 1] >= 0xDC00 && s[i + 1] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (s[i + 1] - 0xDC00); i++;
            o[k++] = (char)(0xF0 | c >> 18); o[k++] = (char)(0x80 | ((c >> 12) & 0x3F)); o[k++] = (char)(0x80 | ((c >> 6) & 0x3F)); o[k++] = (char)(0x80 | (c & 0x3F));
            continue;
        }
        if (c < 0x80) o[k++] = (char)c;
        else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 0x3F)); }
        else { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | ((c >> 6) & 0x3F)); o[k++] = (char)(0x80 | (c & 0x3F)); }
    }
    o[k] = 0;
    return o;
}

/* Standard UTF-8 to UTF-16 (four-byte sequences become surrogate pairs). */
static uint16_t *utf8_to_u16(const char *s, int32_t *n)
{
    size_t cap = strlen(s) + 1;
    uint16_t *o = malloc(cap * 2);
    int32_t k = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        uint32_t c = *p++;
        int extra = c < 0x80 ? 0 : (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : -1;
        if (extra < 0) { o[k++] = 0xFFFD; continue; }
        if (extra) c &= (0x3F >> extra);
        for (int i = 0; i < extra && *p; i++) c = (c << 6) | (*p++ & 0x3F);
        if (c >= 0x10000) { c -= 0x10000; o[k++] = (uint16_t)(0xD800 | (c >> 10)); o[k++] = (uint16_t)(0xDC00 | (c & 0x3FF)); }
        else o[k++] = (uint16_t)c;
    }
    *n = k;
    return o;
}

const uint16_t *tl_dvm_string_chars(jobj *s, int32_t *n)
{
    if (!s || s->kind != TL_K_STRING) { *n = 0; return NULL; }
    if (!s->str.u16) {
        int32_t len; uint16_t *u = utf8_to_u16(s->str.utf8 ? s->str.utf8 : "", &len);
        pthread_mutex_lock(&g_lock);
        if (!s->str.u16) { s->str.u16 = u; s->str.len16 = len; u = NULL; }
        pthread_mutex_unlock(&g_lock);
        free(u);
    }
    *n = s->str.len16;
    return s->str.u16;
}

static tl_jclass *g_string_class;

jobj *tl_dvm_string_u16(const uint16_t *s, int32_t n)
{
    jobj *o = tl_jni_new_string("");
    free(o->str.utf8);
    o->str.utf8 = u16_to_utf8(s, n);
    o->str.u16 = malloc((size_t)(n ? n : 1) * 2);
    if (n) memcpy(o->str.u16, s, (size_t)n * 2);
    o->str.len16 = n;
    o->refs = 1u << 30;
    if (g_string_class) { o->cls = g_string_class; dvm_slots(o); }
    return o;
}

jobj *dvm_new_string_utf8(const char *s)
{
    jobj *o = tl_jni_new_string(s);
    o->refs = 1u << 30;
    if (g_string_class) { o->cls = g_string_class; dvm_slots(o); }
    return o;
}

static jobj *const_string(dvm_dex *d, uint32_t idx)
{
    jobj *s = __atomic_load_n(&d->scache[idx], __ATOMIC_ACQUIRE);
    if (s) return s;
    int32_t n; uint16_t *u = mutf8_to_u16(dex_str(d, idx), &n);
    s = tl_dvm_string_u16(u, n);
    free(u);
    __atomic_store_n(&d->scache[idx], s, __ATOMIC_RELEASE);
    return s;
}

/* For the annotations (husk-tl-dvm-annotations.c): the dex's strings, types, prototypes and constant strings. */
const char *dvm_dex_str(const dvm_dex *d, uint32_t idx) { return dex_str(d, idx); }
const char *dvm_dex_type(const dvm_dex *d, uint32_t idx) { return dex_type(d, idx); }
void dvm_dex_proto(const dvm_dex *d, uint32_t proto, char *sig, size_t n)
{
    char shorty[300]; int np;
    proto_sig(d, proto, sig, n, shorty, sizeof(shorty), &np);
}
jobj *dvm_dex_string(dvm_dex *d, uint32_t idx) { return const_string(d, idx); }

/* ================================================================== classes */

dvm_class *dvm_class_of(tl_jclass *jc) { if (!jc) return NULL; if (jc->linking) tl_jni_wait_linked(jc); return (dvm_class *)jc->dvm; }

tl_jclass *dvm_class_named(const char *name)
{
    tl_jclass *jc = tl_jni_find_declared(name);
    if (jc) return jc;
    if (name[0] == '[' || cdef_find(name)) return tl_jni_class(name);
    static const char *const prims[] = { "int", "long", "float", "double", "boolean", "byte", "char", "short", "void", NULL };
    for (int i = 0; prims[i]; i++) if (!strcmp(prims[i], name)) return tl_jni_class(name);
    return NULL;
}

static tl_jclass *class_for_desc(const char *desc)
{
    char name[512];
    desc_to_name(desc, name, sizeof(name));
    switch (desc[0]) {
    case 'I': return tl_jni_class("int"); case 'J': return tl_jni_class("long"); case 'F': return tl_jni_class("float");
    case 'D': return tl_jni_class("double"); case 'Z': return tl_jni_class("boolean"); case 'B': return tl_jni_class("byte");
    case 'C': return tl_jni_class("char"); case 'S': return tl_jni_class("short"); case 'V': return tl_jni_class("void");
    }
    tl_jclass *jc = dvm_class_named(name);
    return jc ? jc : tl_jni_class(name);        /* not defined anywhere: a framework class Husk may implement */
}

static tl_jclass *resolve_type(dvm_dex *d, uint32_t idx)
{
    tl_jclass *c = __atomic_load_n(&d->tcache[idx], __ATOMIC_ACQUIRE);
    if (c) return c;
    const char *desc = dex_type(d, idx);
    if (d->ns && desc[0] == 'L') {
        /* a run-time loader's code sees its own classes first */
        char name[512], key[540]; desc_to_name(desc, name, sizeof(name)); snprintf(key, sizeof(key), "%s@%d", name, d->ns);
        if (cdef_find(key)) c = tl_jni_class(key);
    }
    if (!c) c = class_for_desc(desc);
    __atomic_store_n(&d->tcache[idx], c, __ATOMIC_RELEASE);
    return c;
}

/* Parse encoded static values into a class's statics (before <clinit>, as ART does). */
static void read_value(dvm_dex *d, const uint8_t **pp, jvalue *out)
{
    const uint8_t *p = *pp;
    uint8_t h = *p++;
    int type = h & 0x1f, arg = h >> 5, size = arg + 1;
    uint64_t v = 0;
    memset(out, 0, sizeof(*out));
    switch (type) {
    case 0x00: case 0x02: case 0x04: case 0x06:                  /* byte short int long: sign-extended */
        for (int i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * i);
        if (size < 8 && (p[size - 1] & 0x80)) v |= ~0ull << (8 * size);
        if (type == 0x06) out->j = (int64_t)v; else out->i = (int32_t)v;
        p += size; break;
    case 0x03:                                                  /* char */
        for (int i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * i);
        out->i = (int32_t)(uint16_t)v; p += size; break;
    case 0x10:                                                  /* float: the high bytes */
        for (int i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * (4 - size + i));
        out->i = (int32_t)(uint32_t)v; p += size; break;
    case 0x11:                                                  /* double */
        for (int i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * (8 - size + i));
        out->j = (int64_t)v; p += size; break;
    case 0x17:                                                  /* string */
        for (int i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * i);
        out->l = const_string(d, (uint32_t)v); p += size; break;
    case 0x18:                                                  /* type */
        for (int i = 0; i < size; i++) v |= (uint64_t)p[i] << (8 * i);
        out->l = resolve_type(d, (uint32_t)v)->mirror; p += size; break;
    case 0x1e: break;                                           /* null */
    case 0x1f: out->i = arg; break;                             /* boolean */
    case 0x1c: {                                                /* array: skipped */
        uint32_t n = uleb(&p);
        for (uint32_t i = 0; i < n; i++) { jvalue tmp; read_value(d, &p, &tmp); }
        break;
    }
    case 0x1d: {                                                /* annotation: skipped */
        uleb(&p); uint32_t n = uleb(&p);
        for (uint32_t i = 0; i < n; i++) { uleb(&p); jvalue tmp; read_value(d, &p, &tmp); }
        break;
    }
    default: p += size; break;                                  /* method types/handles, field, method, enum refs */
    }
    *pp = p;
}

static bool same_package(const char *a, const char *b)
{
    const char *sa = strrchr(a, '/'), *sb = strrchr(b, '/');
    size_t la = sa ? (size_t)(sa - a) : 0, lb = sb ? (size_t)(sb - b) : 0;
    return la == lb && !strncmp(a, b, la);
}

/* Link a class the class index has: fields laid out after the superclass's, methods, the vtable, static values. */
static bool attach(tl_jclass *jc)
{
    const char *name = jc->name;
    dvm_class *c = calloc(1, sizeof(*c));
    c->jc = jc; c->name = jc->name;
    pthread_mutex_init(&c->init_lock, NULL);
    static const struct { const char *n; char k; } prims[] = { { "int", 'I' }, { "long", 'J' }, { "float", 'F' }, { "double", 'D' },
        { "boolean", 'Z' }, { "byte", 'B' }, { "char", 'C' }, { "short", 'S' }, { "void", 'V' } };
    for (size_t i = 0; i < sizeof(prims) / sizeof(prims[0]); i++)
        if (!strcmp(name, prims[i].n)) { c->prim = prims[i].k; c->flags = 0x411; c->state = CS_INITIALIZED; jc->dvm = c; return true; }
    if (name[0] == '[') {
        c->elem = name[1] == '[' ? 'L' : name[1];
        c->flags = 0x411;                                       /* public final abstract */
        c->state = CS_INITIALIZED;
        tl_jclass *obj = tl_jni_class("java/lang/Object");
        jc->super = obj;
        c->super = dvm_class_of(obj);
        jc->dvm = c;
        c->nslots = c->super ? c->super->nslots : 0;
        return true;
    }
    cdef *cd = cdef_find(name);
    if (!cd) { free(c); return false; }
    dvm_dex *d = cd->dex;
    const uint8_t *def = d->b + d->cls_off + 32 * cd->def;
    c->dex = d; c->def = cd->def;
    c->flags = rd32(def + 4);
    jc->dvm = c;                                                /* before the superclass, which may refer back */

    uint32_t super_idx = rd32(def + 8);
    if (super_idx != 0xFFFFFFFFu) {
        tl_jclass *sj = resolve_type(d, super_idx);
        jc->super = sj;
        c->super = dvm_class_of(sj);
    }
    uint32_t ifaces_off = rd32(def + 12);
    if (ifaces_off) {
        uint32_t n = rd32(d->b + ifaces_off);
        c->ifaces = calloc(n, sizeof(*c->ifaces));
        for (uint32_t i = 0; i < n; i++) c->ifaces[c->nifaces++] = resolve_type(d, rd16(d->b + ifaces_off + 4 + 2 * i));
    }
    c->nslots = c->super ? c->super->nslots : 0;

    uint32_t data_off = rd32(def + 24);
    if (data_off) {
        const uint8_t *p = d->b + data_off;
        uint32_t nsf = uleb(&p), nif = uleb(&p), ndm = uleb(&p), nvm = uleb(&p);
        /* As ART lays them out (LengthPrefixedArray: a count, then the members back to back): fields instance then static,
         * methods direct then virtual. Class.fields/methods point at these, and code that walks ART's tables through Unsafe
         * (HiddenApiBypass) finds dvm_field/dvm_method entries a fixed stride apart. */
        uint8_t *fb = calloc(1, DVM_MEMBERS_HDR + (nif + nsf + 1) * sizeof(dvm_field));
        *(uint32_t *)fb = nif + nsf;
        c->inf = (dvm_field *)(fb + DVM_MEMBERS_HDR); c->sf = c->inf + nif;
        uint8_t *mb = calloc(1, DVM_MEMBERS_HDR + (ndm + nvm + 1) * sizeof(dvm_method));
        *(uint32_t *)mb = ndm + nvm;
        c->dm = (dvm_method *)(mb + DVM_MEMBERS_HDR); c->vm = c->dm + ndm;
        c->art_tables = true;
        uint32_t idx = 0;
        for (uint32_t i = 0; i < nsf; i++) {
            idx += uleb(&p);
            dvm_field *f = &c->sf[c->nsf++];
            f->flags = uleb(&p); f->cls = c; f->idx = idx; f->slot = i;
            const uint8_t *fi = d->b + d->field_off + 8 * idx;
            f->type = dex_type(d, rd16(fi + 2)); f->name = dex_str(d, rd32(fi + 4));
        }
        idx = 0;
        for (uint32_t i = 0; i < nif; i++) {
            idx += uleb(&p);
            dvm_field *f = &c->inf[c->ninf++];
            f->flags = uleb(&p); f->cls = c; f->idx = idx; f->slot = c->nslots++;
            const uint8_t *fi = d->b + d->field_off + 8 * idx;
            f->type = dex_type(d, rd16(fi + 2)); f->name = dex_str(d, rd32(fi + 4));
        }
        for (int group = 0; group < 2; group++) {
            uint32_t n = group ? nvm : ndm;
            idx = 0;
            for (uint32_t i = 0; i < n; i++) {
                idx += uleb(&p);
                dvm_method *m = group ? &c->vm[c->nvm++] : &c->dm[c->ndm++];
                m->flags = uleb(&p);
                uint32_t code_off = uleb(&p);
                m->cls = c; m->idx = idx; m->vidx = -1;
                const uint8_t *mi = d->b + d->meth_off + 8 * idx;
                m->name = dex_str(d, rd32(mi + 4));
                { char sg[8192], sh[300]; proto_sig(d, rd16(mi + 2), sg, sizeof(sg), sh, sizeof(sh), &m->nparams); m->sig = strdup(sg); m->shorty = strdup(sh); }
                if (code_off) {
                    const uint8_t *ci = d->b + code_off;
                    m->code = ci;
                    m->regs = rd16(ci); m->ins = rd16(ci + 2); m->outs = rd16(ci + 4); m->ntries = rd16(ci + 6);
                    m->ninsns = rd32(ci + 12);
                    m->insns = (const uint16_t *)(ci + 16);
                }
                if (m->flags & 0x100 /* native */) m->intrinsic = dvm_intrinsic(name, m->name, m->sig);
                else { dvm_native_fn dvm_fast_path(const char *, const char *, const char *); m->intrinsic = dvm_fast_path(name, m->name, m->sig); }
            }
        }
    }
    /* Statics: storage on the class itself (tl_jclass->statics), so JNI's GetStatic*Field find them. */
    if (c->nsf) {
        jc->statics = calloc((size_t)c->nsf, sizeof(jvalue));
        jc->nstatics = c->nsf;
        uint32_t sv = rd32(def + 28);
        if (sv) {
            const uint8_t *p = d->b + sv;
            uint32_t n = uleb(&p);
            for (uint32_t i = 0; i < n && (int)i < c->nsf; i++) read_value(d, &p, &jc->statics[i]);
        }
    }
    /* The vtable: the superclass's, overrides replacing entries (package-private methods only within their package), new
     * methods appended. A framework superclass (not interpreted) contributes nothing: its methods are found by name. */
    int sv_n = c->super ? c->super->nvtab : 0;
    c->vtab = calloc((size_t)(sv_n + c->nvm + 1), sizeof(*c->vtab));
    if (sv_n) memcpy(c->vtab, c->super->vtab, (size_t)sv_n * sizeof(*c->vtab));
    c->nvtab = sv_n;
    for (int i = 0; i < c->nvm; i++) {
        dvm_method *m = &c->vm[i];
        int slot = -1;
        for (int k = 0; k < sv_n; k++) {
            dvm_method *o = c->vtab[k];
            if (strcmp(o->name, m->name) || strcmp(o->sig, m->sig)) continue;
            if (!(o->flags & 0x7) && !same_package(o->cls->name, c->name)) continue;        /* package-private elsewhere */
            slot = k; break;
        }
        if (slot < 0) slot = c->nvtab++;
        c->vtab[slot] = m;
        m->vidx = slot;
    }
    if (g_dvm_trace >= 1) tl_log_line("dvm: linked %s (%d slots, %d vtable)", name, c->nslots, c->nvtab);
    return true;
}

/* ---- lookups */

dvm_method *dvm_find_method(dvm_class *c, const char *name, const char *sig, bool want_static)
{
    for (dvm_class *k = c; k; k = k->super) {
        for (int i = 0; i < k->ndm; i++) {
            dvm_method *m = &k->dm[i];
            if (!strcmp(m->name, name) && !strcmp(m->sig, sig) && (!!(m->flags & 8)) == want_static) return m;
        }
        if (!want_static)
            for (int i = 0; i < k->nvm; i++) if (!strcmp(k->vm[i].name, name) && !strcmp(k->vm[i].sig, sig)) return &k->vm[i];
        if (want_static && k != c) continue;
    }
    if (want_static) return NULL;
    /* default (and abstract) interface methods */
    for (dvm_class *k = c; k; k = k->super)
        for (int i = 0; i < k->nifaces; i++) {
            dvm_class *ic = dvm_class_of(k->ifaces[i]);
            if (!ic) continue;
            dvm_method *m = dvm_find_method(ic, name, sig, false);
            if (m) return m;
        }
    return NULL;
}

/* The method a virtual call on an object of class `receiver` runs: the most derived non-abstract one. */
dvm_method *dvm_find_virtual(tl_jclass *receiver, const char *name, const char *sig)
{
    dvm_class *c = dvm_class_of(receiver);
    dvm_method *abstract_hit = NULL;
    for (dvm_class *k = c; k; k = k->super)
        for (int i = 0; i < k->nvm; i++) {
            dvm_method *m = &k->vm[i];
            if (strcmp(m->name, name) || strcmp(m->sig, sig)) continue;
            if (!(m->flags & 0x400)) return m;
            if (!abstract_hit) abstract_hit = m;
        }
    /* a default method from an interface */
    for (dvm_class *k = c; k; k = k->super)
        for (int i = 0; i < k->nifaces; i++) {
            dvm_class *ic = dvm_class_of(k->ifaces[i]);
            if (!ic) continue;
            dvm_method *m = dvm_find_method(ic, name, sig, false);
            if (m && !(m->flags & 0x400)) return m;
            if (m && !abstract_hit) abstract_hit = m;
        }
    return abstract_hit;
}

dvm_field *dvm_find_field(dvm_class *c, const char *name, bool want_static)
{
    for (dvm_class *k = c; k; k = k->super) {
        if (want_static) { for (int i = 0; i < k->nsf; i++) if (!strcmp(k->sf[i].name, name)) return &k->sf[i]; }
        else for (int i = 0; i < k->ninf; i++) if (!strcmp(k->inf[i].name, name)) return &k->inf[i];
        if (want_static)
            for (int i = 0; i < k->nifaces; i++) {
                dvm_class *ic = dvm_class_of(k->ifaces[i]);
                dvm_field *f = ic ? dvm_find_field(ic, name, true) : NULL;
                if (f) return f;
            }
    }
    return NULL;
}

/* ================================================================== objects */

static tl_jclass *g_class_class;

jvalue *dvm_slots(jobj *o)
{
    if (o->fields) return o->fields;
    tl_jclass *jc = o->kind == TL_K_CLASS ? g_class_class : o->cls;
    dvm_class *c = dvm_class_of(jc);
    uint32_t n = c ? c->nslots : 0;
    if (!n) n = 1;
    jvalue *f = calloc(n, sizeof(jvalue));
    pthread_mutex_lock(&g_lock);
    bool mine = !o->fields;
    if (mine) { o->fields = f; o->nfields = n; f = NULL; }
    pthread_mutex_unlock(&g_lock);
    free(f);
    if (mine && o->kind == TL_K_CLASS) { void dvm_fill_mirror(jobj *mirror); dvm_fill_mirror(o); }
    return o->fields;
}

/* An object with fewer slots than its class has (made before the class was linked, or through JNI): grown under the lock, to the
 * class's full size, and the old array is kept, not freed -- another thread may be reading it this moment, and a freed array read
 * that way is how a register ends up holding garbage. */
jvalue *dvm_grow_slots(jobj *o, uint32_t need)
{
    pthread_mutex_lock(&g_lock);
    if (o->nfields < need) {
        dvm_class *c = dvm_class_of(o->kind == TL_K_CLASS ? g_class_class : o->cls);
        uint32_t nn = need;
        if (c && c->nslots > nn) nn = c->nslots;
        jvalue *f = calloc(nn, sizeof(jvalue));
        if (o->fields && o->nfields) memcpy(f, o->fields, (size_t)o->nfields * sizeof(jvalue));
        __atomic_store_n(&o->fields, f, __ATOMIC_RELEASE);
        o->nfields = nn;
    }
    jvalue *r = o->fields;
    pthread_mutex_unlock(&g_lock);
    return r;
}

jobj *dvm_new_object(tl_jclass *jc)
{
    jobj *o = tl_jni_new_object(jc);
    o->refs = 1u << 30;
    return o;
}

tl_jclass *dvm_object_class(jobj *o)
{
    if (!o) return NULL;
    if (o->kind == TL_K_CLASS) return g_class_class;
    return o->cls;
}

bool dvm_assignable(tl_jclass *sub, tl_jclass *sup)
{
    if (!sub || !sup) return false;
    if (sub == sup) return true;
    const char *sn = sub->name, *pn = sup->name;
    if (!strcmp(pn, "java/lang/Object")) { dvm_class *dc = dvm_class_of(sub); return !dc || !dc->prim; }
    if (sn[0] == '[') {
        if (!strcmp(pn, "java/lang/Cloneable") || !strcmp(pn, "java/io/Serializable")) return true;
        if (pn[0] != '[') return false;
        if ((sn[1] == 'L' || sn[1] == '[') && (pn[1] == 'L' || pn[1] == '[')) {
            tl_jclass *se = class_for_desc(sn + 1), *pe = class_for_desc(pn + 1);
            return dvm_assignable(se, pe);
        }
        return !strcmp(sn, pn);
    }
    for (tl_jclass *k = sub; k; k = k->super) {
        if (k == sup) return true;
        dvm_class *dc = dvm_class_of(k);
        if (dc) for (int i = 0; i < dc->nifaces; i++) if (dvm_assignable(dc->ifaces[i], sup)) return true;
    }
    return false;
}

/* Whether a class itself declares a method (not one it inherits): JNI's RegisterNatives binds a native to the class that does. */
bool tl_dvm_declares(tl_jclass *jc, const char *name, const char *sig)
{
    dvm_class *c = dvm_class_of(jc);
    if (!c) return false;
    for (int i = 0; i < c->ndm; i++) if (!strcmp(c->dm[i].name, name) && !strcmp(c->dm[i].sig, sig)) return true;
    for (int i = 0; i < c->nvm; i++) if (!strcmp(c->vm[i].name, name) && !strcmp(c->vm[i].sig, sig)) return true;
    return false;
}

bool dvm_instance_of(jobj *o, tl_jclass *c) { return o && dvm_assignable(dvm_object_class(o), c); }

/* ================================================================== exceptions */

bool dvm_throw(const char *cls, const char *fmt, ...)
{
    char msg[600] = "";
    if (fmt) { va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof(msg), fmt, ap); va_end(ap); }
    tl_jclass *jc = dvm_class_named(cls);
    if (!jc) jc = tl_jni_class(cls);
    dvm_class *c = dvm_class_of(jc);
    jobj *e = dvm_new_object(jc);
    if (c) {
        dvm_ensure_init(c);
        dvm_method *init = dvm_find_method(c, "<init>", "(Ljava/lang/String;)V", false);
        jvalue a; a.l = fmt ? dvm_new_string_utf8(msg) : NULL;
        if (init) { jobj *saved = tl_jni_pending_object(); tl_jni_set_pending(NULL); jvalue r; dvm_call(init, e, &a, &r); if (!tl_jni_pending_object()) tl_jni_set_pending(saved); }
    } else {
        jvalue v; v.l = dvm_new_string_utf8(msg);
        tl_jni_set_field(e, "detailMessage", "Ljava/lang/String;", v);
    }
    if (g_dvm_trace >= 1) tl_log_line("dvm: throw %s: %s", cls, msg);
    tl_jni_set_pending(e);
    return false;
}

const char *tl_dvm_describe_pending(char *buf, size_t n)
{
    jobj *e = tl_jni_pending_object();
    if (!e) return NULL;
    tl_jclass *c = dvm_object_class(e);
    const char *msg = NULL;
    dvm_class *dc = dvm_class_of(c);
    dvm_field *f = dc ? dvm_find_field(dc, "detailMessage", false) : NULL;
    if (f && e->fields && f->slot < e->nfields) msg = tl_jni_string(e->fields[f->slot].l);
    else { jvalue v = tl_jni_get_field(e, "detailMessage", "Ljava/lang/String;"); msg = tl_jni_string(v.l); }
    char name[600]; snprintf(name, sizeof(name), "%s", c ? c->name : "?");
    for (char *p = name; *p; p++) if (*p == '/') *p = '.';
    size_t k = (size_t)snprintf(buf, n, "%s%s%s", name, msg ? ": " : "", msg ? msg : "");
    /* and what caused it, a few levels down */
    jobj *e2 = e;
    for (int depth = 0; depth < 4 && k + 40 < n; depth++) {
        dvm_class *ec = dvm_class_of(dvm_object_class(e2));
        dvm_field *cf = ec ? dvm_find_field(ec, "cause", false) : NULL;
        jobj *cause = cf && e2->fields && cf->slot < e2->nfields ? e2->fields[cf->slot].l : NULL;
        if (!cause || cause == e2) break;
        tl_jclass *cc = dvm_object_class(cause);
        dvm_class *dcc = dvm_class_of(cc);
        dvm_field *mf = dcc ? dvm_find_field(dcc, "detailMessage", false) : NULL;
        const char *cm = mf && cause->fields && mf->slot < cause->nfields ? tl_jni_string(cause->fields[mf->slot].l) : NULL;
        k += (size_t)snprintf(buf + k, n - k, " | caused by %s%s%s", cc ? cc->name : "?", cm ? ": " : "", cm ? cm : "");
        e2 = cause;
    }
    return buf;
}

/* ================================================================== monitors */

/* A Java monitor: a plain mutex with its owner and how many times the owner holds it. wait() lets go of every level (as Java's
 * does: a thread that waits inside two synchronized blocks on the same object must not keep it locked) and takes them back. */
typedef struct mon { pthread_mutex_t mu; pthread_cond_t cv; _Atomic(pthread_t) owner; int count; } mon;

static mon *monitor_of(jobj *o)
{
    mon *m = o->monitor;
    if (m) return m;
    mon *n = calloc(1, sizeof(*n));
    pthread_mutex_init(&n->mu, NULL);
    pthread_cond_init(&n->cv, NULL);
    pthread_mutex_lock(&g_lock);
    if (!o->monitor) o->monitor = n; else { pthread_mutex_destroy(&n->mu); free(n); }
    pthread_mutex_unlock(&g_lock);
    return o->monitor;
}
static bool mon_mine(mon *m) { pthread_t w = atomic_load(&m->owner); return w && pthread_equal(w, pthread_self()); }
void dvm_monitor_enter(jobj *o)
{
    mon *m = monitor_of(o);
    if (mon_mine(m)) { m->count++; return; }
    pthread_mutex_lock(&m->mu);
    atomic_store(&m->owner, pthread_self());
    m->count = 1;
}
bool dvm_monitor_exit(jobj *o)
{
    mon *m = monitor_of(o);
    if (!mon_mine(m)) return false;
    if (--m->count == 0) { atomic_store(&m->owner, (pthread_t)0); pthread_mutex_unlock(&m->mu); }
    return true;
}
void dvm_monitor_wait(jobj *o, int64_t ms, int32_t ns);
void dvm_monitor_wait(jobj *o, int64_t ms, int32_t ns)
{
    mon *m = monitor_of(o);
    if (!mon_mine(m)) { dvm_throw("java/lang/IllegalMonitorStateException", "object not locked by thread before wait()"); return; }
    int held = m->count;
    m->count = 0;
    atomic_store(&m->owner, (pthread_t)0);
    if (ms <= 0 && ns <= 0) pthread_cond_wait(&m->cv, &m->mu);
    else {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += ms / 1000; ts.tv_nsec += (ms % 1000) * 1000000 + ns;
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
        pthread_cond_timedwait(&m->cv, &m->mu, &ts);
    }
    atomic_store(&m->owner, pthread_self());
    m->count = held;
}
void dvm_monitor_notify(jobj *o, bool all);
void dvm_monitor_notify(jobj *o, bool all) { mon *m = monitor_of(o); if (all) pthread_cond_broadcast(&m->cv); else pthread_cond_signal(&m->cv); }

/* ================================================================== class initialisation */

bool dvm_ensure_init(dvm_class *c)
{
    if (!c || c->state == CS_INITIALIZED) return true;
    pthread_mutex_lock(&c->init_lock);
    if (c->state == CS_INITIALIZED) { pthread_mutex_unlock(&c->init_lock); return true; }
    if (c->state == CS_FAILED) { pthread_mutex_unlock(&c->init_lock); return dvm_throw("java/lang/NoClassDefFoundError", "%s failed to initialise", c->name); }
    if (c->state == CS_INITIALIZING) {
        /* this thread is already initialising it (a <clinit> that reaches back): carry on, as the JVM does */
        bool mine = pthread_equal(c->init_thread, pthread_self());
        pthread_mutex_unlock(&c->init_lock);
        if (mine) return true;
        while (c->state == CS_INITIALIZING) usleep(200);
        return c->state == CS_INITIALIZED;
    }
    c->state = CS_INITIALIZING;
    c->init_thread = pthread_self();
    pthread_mutex_unlock(&c->init_lock);
    if (c->super && !dvm_ensure_init(c->super)) { c->state = CS_FAILED; return false; }
    for (int i = 0; i < c->ndm; i++) {
        dvm_method *m = &c->dm[i];
        if (!strcmp(m->name, "<clinit>")) {
            if (g_dvm_trace >= 1) tl_log_line("dvm: <clinit> %s", c->name);
            jvalue r;
            /* native code that left an exception pending from an earlier call, then touches a new class: the initialiser runs
               regardless, and the old exception stays pending after it, as on ART */
            jobj *stale = tl_jni_pending_object();
            if (stale) tl_jni_clear();
            bool inited = dvm_call(m, NULL, NULL, &r);
            if (inited && stale && !tl_jni_pending_object()) tl_jni_set_pending(stale);
            if (!inited) {
                char buf[400];
                tl_log_line("dvm: %s.<clinit> threw %s", c->name, tl_dvm_describe_pending(buf, sizeof(buf)) ? buf : "?");
                /* where (the first few): a class that fails to initialise fails every later use of it */
                static atomic_int shown;
                jobj *e = tl_jni_pending_object();
                if (e && atomic_fetch_add(&shown, 1) < 30) {
                    tl_jni_clear();
                    jvalue a[1], st; a[0].j = 0; a[0].l = e;
                    if (tl_dvm_call_static("android/util/Log", "getStackTraceString", "(Ljava/lang/Throwable;)Ljava/lang/String;", a, &st)) {
                        const char *t = tl_jni_string(st.l);
                        for (int ln = 0; t && *t && ln < 14; ln++) {
                            const char *nl = strchr(t, '\n');
                            tl_log_line("dvm:   %.*s", nl ? (int)(nl - t) : (int)strlen(t), t);
                            t = nl ? nl + 1 : NULL;
                        }
                    }
                    tl_jni_set_pending(e);
                }
                c->state = CS_FAILED;
                return false;
            }
            break;
        }
    }
    c->state = CS_INITIALIZED;
    return true;
}

/* ================================================================== calling */

static bool interpret(dvm_method *m, uint64_t *regs, jvalue *ret);
static bool call_native(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret);

static int arg_words(const dvm_method *m)
{
    int w = (m->flags & 8) ? 0 : 1;
    for (const char *s = m->shorty + 1; *s; s++) w += (*s == 'J' || *s == 'D') ? 2 : 1;
    return w;
}

/* The interpreter's call stack, per thread: what Reflection.getCallerClass and stack traces read. */
_Thread_local dvm_method *t_frames[8192];
_Thread_local uint32_t t_pcs[8192];             /* each frame's pc at its last call out: the line a stack trace shows */
_Thread_local int t_depth;

/* The source line of a pc in a method, from its debug info; -1 without one. */
int dvm_line_of(dvm_method *m, uint32_t pc)
{
    if (!m || !m->code || !m->cls->dex) return -1;
    const dvm_dex *d = m->cls->dex;
    uint32_t dbg = rd32(m->code + 8);
    if (!dbg || dbg >= d->size) return -1;
    const uint8_t *p = d->b + dbg;
    int32_t line = (int32_t)uleb(&p);
    uint32_t np = uleb(&p);
    for (uint32_t i = 0; i < np; i++) uleb(&p);
    uint32_t addr = 0; int best = line;
    for (;;) {
        uint8_t op = *p++;
        if (op == 0x00) break;                                   /* DBG_END_SEQUENCE */
        switch (op) {
        case 0x01: addr += uleb(&p); break;                      /* ADVANCE_PC */
        case 0x02: line += sleb(&p); break;                      /* ADVANCE_LINE */
        case 0x03: uleb(&p); uleb(&p); uleb(&p); break;          /* START_LOCAL */
        case 0x04: uleb(&p); uleb(&p); uleb(&p); uleb(&p); break;/* START_LOCAL_EXTENDED */
        case 0x05: case 0x06: uleb(&p); break;                   /* END_LOCAL, RESTART_LOCAL */
        case 0x07: case 0x08: break;                             /* SET_PROLOGUE_END, SET_EPILOGUE_BEGIN */
        case 0x09: uleb(&p); break;                              /* SET_FILE */
        default: {
            int adj = op - 0x0a;
            addr += (uint32_t)(adj / 15); line += -4 + adj % 15;
            if (addr > pc) return best;
            best = line;
        }
        }
    }
    return best;
}
/* The source file of a class (its class_def's source_file_idx), or NULL. */
const char *dvm_source_file(dvm_class *c)
{
    if (!c || !c->dex) return NULL;
    uint32_t idx = rd32(c->dex->b + c->dex->cls_off + 32 * c->def + 16);
    return idx == 0xFFFFFFFFu ? NULL : dex_str(c->dex, idx);
}
/* This thread's frames, innermost first: method and pc pairs into out (2 per frame); returns how many frames. */
int dvm_capture_frames(void **out, uint32_t *pcs, int max)
{
    int n = 0;
    for (int i = (t_depth < 8192 ? t_depth : 8192) - 1; i >= 0 && n < max; i--, n++) { out[n] = t_frames[i]; pcs[n] = t_pcs[i]; }
    return n;
}

static bool dvm_call_inner(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret);
bool dvm_call(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret)
{
    if (t_depth < 8192) { t_frames[t_depth] = m; t_pcs[t_depth] = 0; }
    t_depth++;
    /* past 8000 frames: StackOverflowError, which is made with calls of its own, so those get 200 frames more (a deeper one
       throwing again would recurse without end) */
    static _Thread_local bool t_overflowing;
    /* ...or when the thread's own stack is nearly used up (a native thread made with a small one calling into Java): each
       interpreted call takes several KB of it, where ART's take a few hundred bytes */
    static _Thread_local uintptr_t t_stack_low;
    if (!t_stack_low) {
        pthread_t me = pthread_self();
        uintptr_t top = (uintptr_t)pthread_get_stackaddr_np(me); size_t sz = pthread_get_stacksize_np(me);
        t_stack_low = top - sz + (sz > (2u << 20) ? (512u << 10) : sz / 4);   /* what is kept back for C below the interpreter */
    }
    uintptr_t sp = (uintptr_t)__builtin_frame_address(0);
    /* the error is thrown with 128 KB still above that, which its own calls may use */
    if (t_depth > (t_overflowing ? 8200 : 8000) || sp < t_stack_low + (t_overflowing ? 0 : (128u << 10))) {
        t_depth--;
        if (t_overflowing) return false;
        t_overflowing = true;
        bool r = dvm_throw("java/lang/StackOverflowError", "stack size 8000 frames");
        t_overflowing = false;
        return r;
    }
    static const char *watch;
    if (!watch) watch = getenv("TL_DVM_CALLS") ? getenv("TL_DVM_CALLS") : "";
    if (watch[0] && strchr(watch, '.') && !strcmp(m->name, strchr(watch, '.') + 1) && !strncmp(m->cls->name, watch, (size_t)(strchr(watch, '.') - watch))) {
        { char pb[300]; int po = 0, pn = 0;
          for (const char *q = (m->shorty ? m->shorty : "") + 1; *q && po < 280; q++, pn++) po += snprintf(pb + po, sizeof(pb) - po, " %c:%lld", *q, params ? (long long)params[pn].j : 0);
          char cb[600]; int co = 0;
          for (int up = 2; up <= 5 && t_depth - up >= 0 && t_depth - up < 8192 && co < 560; up++) {
              dvm_method *cm = t_frames[t_depth - up];
              co += snprintf(cb + co, sizeof(cb) - co, " <- %s.%s@%u", cm ? cm->cls->name : "?", cm ? cm->name : "?", t_pcs[t_depth - up]);
          }
          cb[co] = 0;
          tl_log_line("dvm: call %s.%s%s self %p args%s%s", m->cls->name, m->name, m->sig, (void *)self, pb, cb); }
        int np = 0; const char *sh = m->shorty ? m->shorty : "";
        for (const char *q = sh + 1; *q; q++, np++) {
            if (*q != 'L' || !params || !params[np].l) continue;
            jobj *saved = tl_jni_pending_object(); tl_jni_clear();
            dvm_method *ts = dvm_find_virtual(dvm_object_class(params[np].l), "toString", "()Ljava/lang/String;");
            jvalue sv; sv.l = NULL;
            if (ts && dvm_call_inner(ts, params[np].l, NULL, &sv)) tl_log_line("dvm:   arg %d = %.600s", np, tl_jni_string(sv.l) ? tl_jni_string(sv.l) : "null");
            tl_jni_clear(); if (saved) tl_jni_set_pending(saved);
        }
    }
    bool ok = dvm_call_inner(m, self, params, ret);
    t_depth--;
    return ok;
}

static bool dvm_call_inner(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret)
{
    ret->j = 0;
    if (m->proxy_method) { bool dvm_proxy_invoke(dvm_method *, jobj *, const jvalue *, jvalue *); return dvm_proxy_invoke(m, self, params, ret); }
    if (m->intrinsic) return m->intrinsic(self, params, ret);
    if (!m->insns) {
        if (m->flags & 0x100) return call_native(m, self, params, ret);
        return dvm_throw("java/lang/AbstractMethodError", "%s.%s%s", m->cls->name, m->name, m->sig);
    }
    uint64_t small[64];
    uint64_t *regs = m->regs <= 64 ? small : calloc(m->regs, sizeof(uint64_t));
    if (regs == small) memset(small, 0, sizeof(uint64_t) * m->regs);
    int r = m->regs - m->ins;
    if (!(m->flags & 8)) regs[r++] = (uint64_t)(uintptr_t)self;
    for (int i = 0; i < m->nparams; i++) {
        char k = m->shorty[1 + i];
        switch (k) {
        case 'J': case 'D': regs[r] = (uint64_t)params[i].j; r += 2; break;
        case 'L': regs[r++] = (uint64_t)(uintptr_t)params[i].l; break;
        case 'F': regs[r++] = (uint32_t)params[i].i; break;
        case 'Z': regs[r++] = params[i].z; break;
        case 'B': regs[r++] = (uint32_t)(int32_t)params[i].b; break;
        case 'C': regs[r++] = params[i].c; break;
        case 'S': regs[r++] = (uint32_t)(int32_t)params[i].s; break;
        default: regs[r++] = (uint32_t)params[i].i; break;
        }
    }
    bool ok = interpret(m, regs, ret);
    if (regs != small) free(regs);
    return ok;
}

/* ---- JNI natives: an engine's or libcore's library, called with the arm64 calling convention */

typedef int64_t (*gcall_i)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                           double, double, double, double, double, double, double, double,
                           int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                           int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
typedef double (*gcall_d)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                          double, double, double, double, double, double, double, double,
                          int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t,
                          int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);

static void mangle(const char *s, char *out, size_t n, bool sig)
{
    size_t k = 0;
    for (; *s && k + 7 < n; s++) {
        unsigned char c = (unsigned char)*s;
        if (sig && c == ')') break;
        if (c == '/') out[k++] = '_';
        else if (c == '_') { out[k++] = '_'; out[k++] = '1'; }
        else if (c == ';') { out[k++] = '_'; out[k++] = '2'; }
        else if (c == '[') { out[k++] = '_'; out[k++] = '3'; }
        else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out[k++] = (char)c;
        else k += (size_t)snprintf(out + k, n - k, "_0%04x", c);
    }
    out[k] = 0;
}

static void *find_jni(dvm_method *m)
{
    if (m->jni_looked) return m->jni;
    void *fn = tl_jni_native(m->cls->name, m->name, m->sig);
    if (!fn) {
        char cls[400], nm[200], sg[400], sym[1100];
        mangle(m->cls->name, cls, sizeof(cls), false);
        mangle(m->name, nm, sizeof(nm), false);
        snprintf(sym, sizeof(sym), "Java_%s_%s", cls, nm);
        fn = tl_ld_sym(NULL, sym);
        if (!fn) {
            mangle(m->sig + 1, sg, sizeof(sg), true);
            snprintf(sym, sizeof(sym), "Java_%s_%s__%s", cls, nm, sg);
            fn = tl_ld_sym(NULL, sym);
        }
    }
    m->jni = fn;
    m->jni_looked = true;
    return fn;
}

static bool call_native(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret)
{
    if (m->intrinsic) return m->intrinsic(self, params, ret);
    void *fn = find_jni(m);
    if (!fn) return dvm_throw("java/lang/UnsatisfiedLinkError", "No implementation found for %s.%s%s", m->cls->name, m->name, m->sig);
    if (g_dvm_trace >= 2) tl_log_line("dvm: jni %s.%s%s", m->cls->name, m->name, m->sig);
    int64_t gp[8]; double fp[8]; int64_t stk[24];
    int ng = 0, nf = 0, ns = 0;
    memset(gp, 0, sizeof(gp)); memset(fp, 0, sizeof(fp)); memset(stk, 0, sizeof(stk));
    /* @CriticalNative: a static method called with its arguments alone, no JNIEnv and no class (protobuf's upb, Google's libraries) */
    if (m->critical == 0) { bool dvm_method_annotated_build(dvm_method *, const char *); m->critical = (m->flags & 8) && dvm_method_annotated_build(m, "Ldalvik/annotation/optimization/CriticalNative;") ? 1 : -1; }
    if (m->critical < 0) {
        gp[ng++] = (int64_t)(uintptr_t)tl_jni_env();
        gp[ng++] = (int64_t)(uintptr_t)((m->flags & 8) ? m->cls->jc->mirror : self);
    }
    for (int i = 0; i < m->nparams; i++) {
        char k = m->shorty[1 + i];
        if (k == 'F' || k == 'D') {
            union { double d; uint64_t u; } v;
            if (k == 'F') v.u = (uint32_t)params[i].i; else v.u = (uint64_t)params[i].j;
            if (nf < 8) fp[nf++] = v.d; else if (ns < 24) stk[ns++] = (int64_t)v.u;
        } else {
            int64_t v;
            switch (k) {
            case 'Z': v = params[i].z; break; case 'B': v = params[i].b; break; case 'C': v = params[i].c; break;
            case 'S': v = params[i].s; break; case 'I': v = params[i].i; break; case 'J': v = params[i].j; break;
            default: v = (int64_t)(uintptr_t)params[i].l; break;
            }
            if (ng < 8) gp[ng++] = v; else if (ns < 24) stk[ns++] = v;
        }
    }
    char rk = m->shorty[0];
    if (rk == 'F' || rk == 'D') {
        union { double d; uint64_t u; } r;
        r.d = ((gcall_d)fn)(gp[0], gp[1], gp[2], gp[3], gp[4], gp[5], gp[6], gp[7], fp[0], fp[1], fp[2], fp[3], fp[4], fp[5], fp[6], fp[7],
                            stk[0], stk[1], stk[2], stk[3], stk[4], stk[5], stk[6], stk[7], stk[8], stk[9], stk[10], stk[11],
                            stk[12], stk[13], stk[14], stk[15], stk[16], stk[17], stk[18], stk[19], stk[20], stk[21], stk[22], stk[23]);
        if (rk == 'F') ret->i = (int32_t)(uint32_t)r.u; else ret->j = (int64_t)r.u;
    } else {
        int64_t r = ((gcall_i)fn)(gp[0], gp[1], gp[2], gp[3], gp[4], gp[5], gp[6], gp[7], fp[0], fp[1], fp[2], fp[3], fp[4], fp[5], fp[6], fp[7],
                                  stk[0], stk[1], stk[2], stk[3], stk[4], stk[5], stk[6], stk[7], stk[8], stk[9], stk[10], stk[11],
                                  stk[12], stk[13], stk[14], stk[15], stk[16], stk[17], stk[18], stk[19], stk[20], stk[21], stk[22], stk[23]);
        switch (rk) {
        case 'Z': ret->j = 0; ret->z = (uint8_t)r; break;
        case 'B': ret->i = (int8_t)r; break;
        case 'C': ret->i = (uint16_t)r; break;
        case 'S': ret->i = (int16_t)r; break;
        case 'I': ret->i = (int32_t)r; break;
        case 'J': ret->j = r; break;
        case 'V': ret->j = 0; break;
        default: ret->l = (void *)(uintptr_t)r; if (ret->l) ((jobj *)ret->l)->refs = 1u << 30; break;
        }
    }
    return !tl_jni_pending();
}

/* ---- what the interpreter calls: an interpreted class's method, or one Husk implements in C (the android.* framework) */

typedef struct mref {
    tl_jclass *cls;                 /* the class the reference names */
    const char *name;
    const char *sig, *shorty;
    int nparams;
    dvm_method *dm;                 /* resolved in an interpreted class */
    tl_jmeth *hm;                   /* or Husk's own */
} mref;

static mref *resolve_mref(dvm_dex *d, uint32_t idx, bool is_static)
{
    static _Thread_local mref *last;
    (void)last;
    mref *r = (mref *)__atomic_load_n(&d->mcache[idx], __ATOMIC_ACQUIRE);
    if (r) return r;
    r = calloc(1, sizeof(*r));
    const uint8_t *mi = d->b + d->meth_off + 8 * idx;
    r->cls = resolve_type(d, rd16(mi));
    r->name = dex_str(d, rd32(mi + 4));
    { char sg[8192], sh[300]; proto_sig(d, rd16(mi + 2), sg, sizeof(sg), sh, sizeof(sh), &r->nparams); r->sig = strdup(sg); r->shorty = strdup(sh); }
    dvm_class *c = dvm_class_of(r->cls);
    if (c) r->dm = dvm_find_method(c, r->name, r->sig, is_static);
    if (!r->dm && !c) r->hm = tl_jni_method(r->cls, r->name, r->sig, is_static);
    if (!r->dm && c && !r->hm) {
        /* an interpreted class whose framework superclass declares it */
        for (tl_jclass *k = r->cls; k; k = k->super) if (!dvm_class_of(k)) { r->hm = tl_jni_method(k, r->name, r->sig, is_static); break; }
    }
    pthread_mutex_lock(&g_lock);
    if (!d->mcache[idx]) __atomic_store_n(&d->mcache[idx], (dvm_method *)r, __ATOMIC_RELEASE); else { free(r); r = (mref *)d->mcache[idx]; }
    pthread_mutex_unlock(&g_lock);
    return r;
}

/* ---- fields */

typedef struct fref { dvm_field *df; tl_jfield *hf; tl_jclass *cls; char type; } fref;

static fref *resolve_fref(dvm_dex *d, uint32_t idx, bool is_static)
{
    fref *r = (fref *)__atomic_load_n(&d->fcache[idx], __ATOMIC_ACQUIRE);
    if (r) return r;
    r = calloc(1, sizeof(*r));
    const uint8_t *fi = d->b + d->field_off + 8 * idx;
    tl_jclass *cls = resolve_type(d, rd16(fi));
    const char *type = dex_type(d, rd16(fi + 2)), *name = dex_str(d, rd32(fi + 4));
    r->type = type[0] == '[' ? 'L' : type[0];
    dvm_class *c = dvm_class_of(cls);
    if (c) r->df = dvm_find_field(c, name, is_static);
    if (!r->df) r->hf = tl_jni_field(cls, name, type, is_static);
    r->cls = r->df ? r->df->cls->jc : cls;
    pthread_mutex_lock(&g_lock);
    if (!d->fcache[idx]) __atomic_store_n(&d->fcache[idx], (dvm_field *)r, __ATOMIC_RELEASE); else { free(r); r = (fref *)d->fcache[idx]; }
    pthread_mutex_unlock(&g_lock);
    return r;
}

/* ================================================================== the interpreter */

static bool find_handler(dvm_method *m, uint32_t pc, jobj *exc, uint32_t *target)
{
    if (!m->ntries) return false;
    const uint8_t *tries = (const uint8_t *)(m->insns + m->ninsns);
    if (m->ninsns & 1) tries += 2;                                 /* padding to 4 bytes */
    const uint8_t *handlers = tries + 8 * m->ntries;
    tl_jclass *ec = dvm_object_class(exc);
    for (uint32_t t = 0; t < m->ntries; t++) {
        const uint8_t *ti = tries + 8 * t;
        uint32_t start = rd32(ti); uint16_t count = rd16(ti + 4), hoff = rd16(ti + 6);
        if (pc < start || pc >= start + count) continue;
        const uint8_t *h = handlers + hoff;
        int32_t size = sleb(&h);
        int n = size < 0 ? -size : size;
        for (int i = 0; i < n; i++) {
            uint32_t type = uleb(&h), addr = uleb(&h);
            tl_jclass *tc = resolve_type(m->cls->dex, type);
            if (dvm_assignable(ec, tc)) { *target = addr; return true; }
        }
        if (size <= 0) { *target = uleb(&h); return true; }
        return false;                                              /* the innermost try that covers pc decides */
    }
    return false;
}

static inline int32_t RI(uint64_t *r, int n) { return (int32_t)(uint32_t)r[n]; }
static inline float RF(uint64_t *r, int n) { union { uint32_t u; float f; } v; v.u = (uint32_t)r[n]; return v.f; }
static inline double RD(uint64_t *r, int n) { union { uint64_t u; double d; } v; v.u = r[n]; return v.d; }
static inline int64_t RJ(uint64_t *r, int n) { return (int64_t)r[n]; }
static inline jobj *RL(uint64_t *r, int n) { return (jobj *)(uintptr_t)r[n]; }
static inline void WI(uint64_t *r, int n, int32_t v) { r[n] = (uint32_t)v; }
static inline void WF(uint64_t *r, int n, float f) { union { uint32_t u; float f; } v; v.f = f; r[n] = v.u; }
static inline void WD(uint64_t *r, int n, double d) { union { uint64_t u; double d; } v; v.d = d; r[n] = v.u; }
static inline void WJ(uint64_t *r, int n, int64_t v) { r[n] = (uint64_t)v; }
/* TL_DVM_CHECK=1: every reference put in a register is checked for being an object, and the first that is not is logged with the
 * method and instruction that loaded it (a debugging aid for memory bugs; off by default). */
int g_dvm_check = -1;
static void check_ref(jobj *o, int n);
static inline void WL(uint64_t *r, int n, jobj *o)
{
    if (__builtin_expect(g_dvm_check != 0, 0) && o) check_ref(o, n);
    r[n] = (uint64_t)(uintptr_t)o;
}
extern _Thread_local uint32_t t_pcs[8192];
static void check_ref(jobj *o, int n)
{
    if (g_dvm_check < 0) { g_dvm_check = getenv("TL_DVM_CHECK") ? 1 : 0; if (!g_dvm_check) return; }
    uintptr_t p = (uintptr_t)o;
    bool ok = (p & 7) == 0 && p > 0x100000 && o->kind <= TL_K_OBJ_ARRAY && (o->kind == TL_K_CLASS || ((uintptr_t)o->cls & 7) == 0);
    if (ok && o->kind != TL_K_CLASS && o->cls) { unsigned char ch = (unsigned char)o->cls->name[0]; ok = ch > 0x20 && ch < 0x7f; }
    if (ok) return;
    dvm_method *m = t_depth > 0 && t_depth <= 8192 ? t_frames[t_depth - 1] : NULL;
    uint32_t pc = t_depth > 0 && t_depth <= 8192 ? t_pcs[t_depth - 1] : 0;
    char callee[300] = "";
    if (m && m->insns && m->cls->dex) {
        unsigned op = m->insns[pc] & 0xFF;
        if ((op >= 0x6e && op <= 0x72) || (op >= 0x74 && op <= 0x78)) {
            dvm_dex *d = m->cls->dex; uint32_t midx = m->insns[pc + 1];
            const uint8_t *mi = d->b + d->meth_off + 8 * midx;
            snprintf(callee, sizeof(callee), " calling %s.%s", dex_type(d, rd16(mi)), dex_str(d, rd32(mi + 4)));
        }
    }
    tl_log_line("dvm-check: v%d gets %p (\"%.16s\") in %s.%s near pc %u (insn %04x)%s", n, (void *)o, (p > 0x100000 && !(p & 7)) ? (const char *)o : "?",
                m ? m->cls->name : "?", m ? m->name : "?", pc, m && m->insns ? m->insns[pc] : 0, callee);
}

static int32_t f2i(float f) { if (isnan(f)) return 0; if (f >= 2147483647.0f) return INT32_MAX; if (f <= -2147483648.0f) return INT32_MIN; return (int32_t)f; }
static int64_t f2l(double f) { if (isnan(f)) return 0; if (f >= 9223372036854775807.0) return INT64_MAX; if (f <= -9223372036854775808.0) return INT64_MIN; return (int64_t)f; }
static int32_t d2i(double f) { if (isnan(f)) return 0; if (f >= 2147483647.0) return INT32_MAX; if (f <= -2147483648.0) return INT32_MIN; return (int32_t)f; }

/* Array element access: one helper per kind, bounds checked. */
static bool array_check(jobj *a, int32_t i)
{
    if (!a) return dvm_throw("java/lang/NullPointerException", "Attempt to get length of null array");
    uint32_t len = a->kind == TL_K_OBJ_ARRAY ? a->oarr.len : a->arr.len;
    if (i < 0 || (uint32_t)i >= len) return dvm_throw("java/lang/ArrayIndexOutOfBoundsException", "length=%u; index=%d", len, i);
    return true;
}

static jobj *new_array(tl_jclass *arrcls, int32_t n)
{
    const char *name = arrcls->name;
    jobj *a;
    if (name[1] == 'L' || name[1] == '[') {
        char elem[400]; desc_to_name(name + 1, elem, sizeof(elem));
        a = tl_jni_new_obj_array(class_for_desc(name + 1), (uint32_t)n);
        a->cls = arrcls;
    } else {
        a = tl_jni_new_prim_array(name[1], (uint32_t)n);
    }
    a->refs = 1u << 30;
    return a;
}

/* new String(...) in app bytecode: ART turns String's constructors into StringFactory calls. The new-instance made a placeholder;
 * the constructor's result replaces it in every register that held it. */
bool dvm_string_init(const char *sig, const jvalue *params, jvalue *ret);

static bool invoke_regs(dvm_method *caller, int kind, uint32_t midx, int count, const uint16_t *argregs, uint16_t first, bool range,
                        uint64_t *regs, jvalue *result)
{
    dvm_dex *d = caller->cls->dex;
    bool is_static = kind == 3;
    mref *r = resolve_mref(d, midx, is_static);
    uint16_t areg[256];
    for (int i = 0; i < count && i < 256; i++) areg[i] = range ? (uint16_t)(first + i) : argregs[i];
    int ai = 0;
    jobj *self = NULL;
    if (!is_static) {
        self = RL(regs, areg[ai++]);
        if (!self) return dvm_throw("java/lang/NullPointerException", "Attempt to invoke %s method '%s.%s%s' on a null object reference",
                                     kind == 4 ? "interface" : "virtual", r->cls->name, r->name, r->sig);
        /* not an object at all (a bug elsewhere put something else in the register): say where, rather than crash on it */
        if (self->kind > TL_K_OBJ_ARRAY || (self->kind != TL_K_CLASS && self->cls && (((uintptr_t)self->cls & 7) || (unsigned char)self->cls->name[0] <= 0x20 || (unsigned char)self->cls->name[0] >= 0x7f))) {
            tl_log_line("dvm: register v%u of %s.%s holds %p, not an object (kind %u, class %p \"%.40s\"), for %s.%s%s", areg[0], caller->cls->name, caller->name,
                        (void *)self, self->kind, (void *)self->cls, self->cls && !((uintptr_t)self->cls & 7) ? self->cls->name : "?", r->cls->name, r->name, r->sig);
            return dvm_throw("java/lang/InternalError", "not an object in v%u for %s.%s", areg[0], r->cls->name, r->name);
        }
    }
    jvalue params[256];                     /* a call can pass up to 255 registers' worth (Kotlin's default-argument constructors do) */
    for (int i = 0; i < r->nparams && i < 256; i++) {
        char k = r->shorty[1 + i];
        params[i].j = 0;
        switch (k) {
        case 'J': case 'D': params[i].j = RJ(regs, areg[ai]); ai += 2; break;
        case 'L': params[i].l = RL(regs, areg[ai++]); break;
        case 'Z': params[i].z = (uint8_t)regs[areg[ai++]]; break;
        case 'B': params[i].b = (int8_t)regs[areg[ai++]]; break;
        case 'C': params[i].c = (uint16_t)regs[areg[ai++]]; break;
        case 'S': params[i].s = (int16_t)regs[areg[ai++]]; break;
        default: params[i].i = RI(regs, areg[ai++]); break;
        }
    }
    /* String's constructors */
    if (kind == 2 && self && self->kind == TL_K_OBJECT && self->cls == g_string_class && !strcmp(r->name, "<init>")) {
        jvalue s;
        if (!dvm_string_init(r->sig, params, &s)) return false;
        for (int i = 0; i < caller->regs; i++) if (regs[i] == (uint64_t)(uintptr_t)self) regs[i] = (uint64_t)(uintptr_t)s.l;
        result->l = s.l;
        return true;
    }
    dvm_method *target = r->dm;
    { static const char *w2; if (!w2) w2 = getenv("TL_DVM_RES") ? getenv("TL_DVM_RES") : "";
      const char *dot = strchr(w2, '.');
      bool hit = w2[0] && (dot ? (r->cls && !strcmp(r->name, dot + 1) && !strncmp(r->cls->name, w2, (size_t)(dot - w2)) && !r->cls->name[dot - w2]) : strstr(r->name, w2) != NULL);
      if (hit) tl_log_line("dvm: resolve %s.%s%s kind %d -> dm %p (%s) hm %p, receiver %s", r->cls ? r->cls->name : "?", r->name, r->sig, kind, (void *)r->dm,
                           r->dm ? r->dm->cls->name : "", (void *)r->hm, self ? dvm_object_class(self)->name : "-"); }
    switch (kind) {
    case 3:                                                        /* static */
        if (target && !dvm_ensure_init(target->cls)) return false;
        break;
    case 1: {                                                      /* super: from the caller's superclass */
        dvm_class *sc = caller->cls->super;
        dvm_class *rc = dvm_class_of(r->cls);
        if (rc && (rc->flags & 0x200)) {
            /* Interface.super.m(): that interface's default method (or one it inherits) */
            target = dvm_find_method(rc, r->name, r->sig, false);
            if (target && !(target->flags & 0x400)) break;
        }
        target = sc ? dvm_find_method(sc, r->name, r->sig, false) : NULL;
        if (target && (target->flags & 0x400)) target = dvm_find_virtual(sc->jc, r->name, r->sig);
        if (!target && !r->hm) {
            for (tl_jclass *k = caller->cls->jc->super; k; k = k->super) if (!dvm_class_of(k)) { r->hm = tl_jni_method(k, r->name, r->sig, false); break; }
        }
        if (!target && r->hm) { *result = tl_jni_invoke(self, r->hm, true, params); return !tl_jni_pending(); }
        break;
    }
    case 0: case 4: {                                              /* virtual, interface */
        tl_jclass *rc = dvm_object_class(self);
        dvm_class *dc = dvm_class_of(rc);
        if (dc && target && target->vidx >= 0 && !(target->cls->flags & 0x200) && target->vidx < dc->nvtab
            && dc->vtab[target->vidx] && !strcmp(dc->vtab[target->vidx]->name, r->name) && !strcmp(dc->vtab[target->vidx]->sig, r->sig)) {
            target = dc->vtab[target->vidx];
        } else if (dc) {
            dvm_method *v = dvm_find_virtual(rc, r->name, r->sig);
            if (v) target = v;
            else if (!r->hm) {
                for (tl_jclass *k = rc; k; k = k->super) if (!dvm_class_of(k)) { r->hm = tl_jni_method(k, r->name, r->sig, false); break; }
            }
            if (!v && r->hm) { *result = tl_jni_invoke(self, r->hm, false, params); return !tl_jni_pending(); }
        } else {
            /* an object Husk made in C (an android.* class): its implementation */
            tl_jmeth *hm = tl_jni_method(rc ? rc : r->cls, r->name, r->sig, false);
            *result = tl_jni_invoke(self, hm, false, params);
            return !tl_jni_pending();
        }
        break;
    }
    default: break;                                                /* direct */
    }
    if (!target) {
        if (r->hm) { *result = tl_jni_invoke(self, r->hm, kind == 2 || kind == 1, params); return !tl_jni_pending(); }
        return dvm_throw("java/lang/NoSuchMethodError", "%s.%s%s", r->cls->name, r->name, r->sig);
    }
    if (g_dvm_trace >= 2) tl_log_line("dvm: -> %s.%s%s", target->cls->name, target->name, target->sig);
    { static const char *watch; if (!watch) watch = getenv("TL_DVM_ARGS") ? getenv("TL_DVM_ARGS") : "";
      if (watch[0] && !strcmp(target->name, strchr(watch, '.') + 1) && !strncmp(target->cls->name, watch, (size_t)(strchr(watch, '.') - watch))) {
          char b[600]; int o = 0;
          for (int i = 0; i < r->nparams && i < 10; i++) o += snprintf(b + o, sizeof(b) - o, " %c:%llx", r->shorty[1 + i], (unsigned long long)params[i].j);
          tl_log_line("dvm: args %s.%s from %s.%s regs[%d..]:%s", target->cls->name, target->name, caller->cls->name, caller->name, areg[0], b);
          /* TL_DVM_ARGS_STR: and what each object argument says of itself */
          if (getenv("TL_DVM_ARGS_STR")) for (int i = 0; i < r->nparams && i < 10; i++) {
              if (r->shorty[1 + i] != 'L' || !params[i].l) continue;
              jobj *saved = tl_jni_pending_object(); tl_jni_clear();
              dvm_method *ts = dvm_find_virtual(dvm_object_class(params[i].l), "toString", "()Ljava/lang/String;");
              jvalue sv; sv.l = NULL;
              if (ts && dvm_call(ts, params[i].l, NULL, &sv)) tl_log_line("dvm:   arg %d = %.600s", i, tl_jni_string(sv.l) ? tl_jni_string(sv.l) : "null");
              tl_jni_clear(); if (saved) tl_jni_set_pending(saved);
          }
      } }
    return dvm_call(target, self, params, result);
}

/* A value returned from C is only defined in its own width; the registers want ints zero-extended, sub-ints widened. */
static void normalize(char k, jvalue *v)
{
    switch (k) {
    case 'Z': v->j = v->z ? 1 : 0; break;
    case 'B': v->j = (uint32_t)(int32_t)v->b; break;
    case 'C': v->j = v->c; break;
    case 'S': v->j = (uint32_t)(int32_t)v->s; break;
    case 'I': case 'F': v->j = (uint32_t)v->i; break;
    default: break;
    }
}

/* ---- invoke-polymorphic on a VarHandle: field, static field and array element handles, every access mode */

static int esize(char t) { return t == 'Z' || t == 'B' ? 1 : t == 'C' || t == 'S' ? 2 : t == 'I' || t == 'F' ? 4 : 8; }

static uint64_t ld(void *p, int sz) { switch (sz) { case 1: return __atomic_load_n((uint8_t *)p, __ATOMIC_SEQ_CST); case 2: return __atomic_load_n((uint16_t *)p, __ATOMIC_SEQ_CST); case 4: return __atomic_load_n((uint32_t *)p, __ATOMIC_SEQ_CST); default: return __atomic_load_n((uint64_t *)p, __ATOMIC_SEQ_CST); } }
static void st(void *p, int sz, uint64_t v) { switch (sz) { case 1: __atomic_store_n((uint8_t *)p, (uint8_t)v, __ATOMIC_SEQ_CST); break; case 2: __atomic_store_n((uint16_t *)p, (uint16_t)v, __ATOMIC_SEQ_CST); break; case 4: __atomic_store_n((uint32_t *)p, (uint32_t)v, __ATOMIC_SEQ_CST); break; default: __atomic_store_n((uint64_t *)p, v, __ATOMIC_SEQ_CST); } }
static bool cas(void *p, int sz, uint64_t *expect, uint64_t v)
{
    switch (sz) {
    case 1: { uint8_t e = (uint8_t)*expect; bool r = __atomic_compare_exchange_n((uint8_t *)p, &e, (uint8_t)v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *expect = e; return r; }
    case 2: { uint16_t e = (uint16_t)*expect; bool r = __atomic_compare_exchange_n((uint16_t *)p, &e, (uint16_t)v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *expect = e; return r; }
    case 4: { uint32_t e = (uint32_t)*expect; bool r = __atomic_compare_exchange_n((uint32_t *)p, &e, (uint32_t)v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST); *expect = e; return r; }
    default: return __atomic_compare_exchange_n((uint64_t *)p, expect, v, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }
}
/* A register's bits as a value of the variable's type, and back. */
static uint64_t to_bits(char t, uint64_t reg) { int sz = esize(t); return sz == 8 ? reg : reg & ((1ull << (sz * 8)) - 1); }
static uint64_t from_bits(char t, uint64_t v)
{
    switch (t) {
    case 'B': return (uint32_t)(int32_t)(int8_t)v; case 'S': return (uint32_t)(int32_t)(int16_t)v;
    case 'Z': return v & 0xFF; case 'C': return v & 0xFFFF; case 'I': case 'F': return v & 0xFFFFFFFFu; default: return v;
    }
}

static bool varhandle(const char *mode, const char *shorty, jobj *vh, const uint64_t *args, jvalue *result)
{
    tl_jclass *vc = dvm_object_class(vh);
    const char *cn = vc->name;
    void *place = NULL; char type = 0; int ai = 0;
    dvm_class *c = dvm_class_of(vc);
    if (strstr(cn, "FieldVarHandle")) {
        dvm_field *af = NULL;
        for (dvm_class *k = c; k && !af; k = k->super) { dvm_field *x = dvm_find_field(k, "artField", false); if (x) af = x; }
        dvm_field *f = af ? (dvm_field *)(uintptr_t)dvm_slots(vh)[af->slot].j : NULL;
        if (!f) return dvm_throw("java/lang/IllegalStateException", "VarHandle without a field");
        type = f->type[0] == '[' ? 'L' : f->type[0];
        if (f->flags & 8) {
            if (!dvm_ensure_init(f->cls)) return false;
            place = &f->cls->jc->statics[f->slot];
        } else {
            jobj *o = (jobj *)(uintptr_t)args[ai++];
            if (!o) return dvm_throw("java/lang/NullPointerException", "VarHandle %s on null", mode);
            place = &dvm_slots(o)[f->slot];
        }
    } else if (strstr(cn, "ArrayElementVarHandle")) {
        jobj *a = (jobj *)(uintptr_t)args[ai++];
        int32_t i = (int32_t)(uint32_t)args[ai++];
        if (!array_check(a, i)) return false;
        if (a->kind == TL_K_OBJ_ARRAY) { place = &a->oarr.v[i]; type = 'L'; }
        else { place = (uint8_t *)a->arr.data + (size_t)i * a->arr.esz; type = a->arr.etype; }
    } else {
        return dvm_throw("java/lang/UnsupportedOperationException", "%s.%s", cn, mode);
    }
    int sz = esize(type);
    uint64_t v0 = shorty[1 + ai] ? to_bits(type, args[ai]) : 0, v1 = shorty[2 + ai] ? to_bits(type, args[ai + 1]) : 0;
    uint64_t out = 0;
    if (!strncmp(mode, "get", 3) && (!mode[3] || !strcmp(mode, "getVolatile") || !strcmp(mode, "getAcquire") || !strcmp(mode, "getOpaque"))) {
        out = ld(place, sz);
    } else if (!strncmp(mode, "set", 3)) {
        st(place, sz, v0);
    } else if (!strncmp(mode, "compareAndSet", 13) || !strncmp(mode, "weakCompareAndSet", 17)) {
        uint64_t e = v0; out = cas(place, sz, &e, v1);
        result->j = (int64_t)out; return true;
    } else if (!strncmp(mode, "compareAndExchange", 18)) {
        uint64_t e = v0; cas(place, sz, &e, v1); out = e;
    } else if (!strncmp(mode, "getAndSet", 9)) {
        uint64_t e = ld(place, sz);
        while (!cas(place, sz, &e, v0)) {}
        out = e;
    } else if (!strncmp(mode, "getAndAdd", 9) || !strncmp(mode, "getAndBitwise", 13)) {
        uint64_t e = ld(place, sz), n;
        for (;;) {
            if (!strncmp(mode, "getAndAdd", 9)) {
                if (type == 'F') { union { uint32_t u; float f; } a, b; a.u = (uint32_t)e; b.u = (uint32_t)v0; a.f += b.f; n = a.u; }
                else if (type == 'D') { union { uint64_t u; double d; } a, b; a.u = e; b.u = v0; a.d += b.d; n = a.u; }
                else n = e + v0;
            } else if (strstr(mode, "Or")) n = e | v0;
            else if (strstr(mode, "And")) n = e & v0;
            else n = e ^ v0;
            if (cas(place, sz, &e, n)) break;
        }
        out = e;
    } else {
        return dvm_throw("java/lang/UnsupportedOperationException", "VarHandle.%s", mode);
    }
    result->j = (int64_t)from_bits(type, out);
    return true;
}

#define FETCH(n) (insns[pc + (n)])
#define A4 ((w >> 8) & 0xF)
#define B4 (w >> 12)
#define AA (w >> 8)
#define THROW_IF(c) do { if (c) goto exception; } while (0)
#define NEXT(n) do { pc += (n); goto dispatch; } while (0)

static bool interpret(dvm_method *m, uint64_t *regs, jvalue *ret)
{
    const uint16_t *insns = m->insns;
    dvm_dex *dex = m->cls->dex;
    uint32_t pc = 0;
    jvalue result; result.j = 0;
    jobj *caught = NULL;
    if ((m->flags & 0x20) /* synchronized */) dvm_monitor_enter((m->flags & 8) ? m->cls->jc->mirror : RL(regs, m->regs - m->ins));

dispatch:;
    uint16_t w = insns[pc];
    switch (w & 0xFF) {
    case 0x00: {                                                   /* nop, and the data payloads (skipped by size) */
        uint16_t ident = w;
        if (ident == 0x0100) NEXT(4 + FETCH(1) * 2);
        if (ident == 0x0200) NEXT(2 + FETCH(1) * 4);
        if (ident == 0x0300) { uint32_t n = FETCH(2) | (uint32_t)FETCH(3) << 16; NEXT(4 + (FETCH(1) * n + 1) / 2); }
        NEXT(1);
    }
    case 0x01: regs[A4] = (uint32_t)regs[B4]; NEXT(1);
    case 0x02: regs[AA] = (uint32_t)regs[FETCH(1)]; NEXT(2);
    case 0x03: regs[FETCH(1)] = (uint32_t)regs[FETCH(2)]; NEXT(3);
    case 0x04: regs[A4] = regs[B4]; regs[A4 + 1] = regs[B4 + 1]; NEXT(1);
    case 0x05: { uint16_t b = FETCH(1); uint64_t lo = regs[b], hi = regs[b + 1]; regs[AA] = lo; regs[AA + 1] = hi; NEXT(2); }
    case 0x06: { uint16_t a = FETCH(1), b = FETCH(2); uint64_t lo = regs[b], hi = regs[b + 1]; regs[a] = lo; regs[a + 1] = hi; NEXT(3); }
    case 0x07: regs[A4] = regs[B4]; NEXT(1);
    case 0x08: regs[AA] = regs[FETCH(1)]; NEXT(2);
    case 0x09: regs[FETCH(1)] = regs[FETCH(2)]; NEXT(3);
    case 0x0a: regs[AA] = (uint32_t)result.i; NEXT(1);
    case 0x0b: regs[AA] = (uint64_t)result.j; NEXT(1);
    case 0x0c: WL(regs, AA, (jobj *)result.l); NEXT(1);
    case 0x0d: regs[AA] = (uint64_t)(uintptr_t)caught; caught = NULL; NEXT(1);
    case 0x0e: ret->j = 0; goto done;
    case 0x0f: ret->j = 0; ret->i = RI(regs, AA); goto done;
    case 0x10: ret->j = RJ(regs, AA); goto done;
    case 0x11: ret->l = RL(regs, AA); goto done;
    case 0x12: WI(regs, A4, ((int32_t)(w << 16)) >> 28); NEXT(1);
    case 0x13: WI(regs, AA, (int16_t)FETCH(1)); NEXT(2);
    case 0x14: WI(regs, AA, (int32_t)(FETCH(1) | (uint32_t)FETCH(2) << 16)); NEXT(3);
    case 0x15: WI(regs, AA, (int32_t)((uint32_t)FETCH(1) << 16)); NEXT(2);
    case 0x16: WJ(regs, AA, (int16_t)FETCH(1)); NEXT(2);
    case 0x17: WJ(regs, AA, (int32_t)(FETCH(1) | (uint32_t)FETCH(2) << 16)); NEXT(3);
    case 0x18: WJ(regs, AA, (int64_t)(FETCH(1) | (uint64_t)FETCH(2) << 16 | (uint64_t)FETCH(3) << 32 | (uint64_t)FETCH(4) << 48)); NEXT(5);
    case 0x19: WJ(regs, AA, (int64_t)((uint64_t)FETCH(1) << 48)); NEXT(2);
    case 0x1a: WL(regs, AA, const_string(dex, FETCH(1))); NEXT(2);
    case 0x1b: WL(regs, AA, const_string(dex, FETCH(1) | (uint32_t)FETCH(2) << 16)); NEXT(3);
    case 0x1c: WL(regs, AA, resolve_type(dex, FETCH(1))->mirror); NEXT(2);
    case 0x1d: { jobj *o = RL(regs, AA); THROW_IF(!o && !dvm_throw("java/lang/NullPointerException", "monitor-enter on null")); dvm_monitor_enter(o); NEXT(1); }
    case 0x1e: { jobj *o = RL(regs, AA); THROW_IF(!o && !dvm_throw("java/lang/NullPointerException", "monitor-exit on null")); dvm_monitor_exit(o); NEXT(1); }
    case 0x1f: {                                                   /* check-cast */
        jobj *o = RL(regs, AA);
        if (o) {
            tl_jclass *c = resolve_type(dex, FETCH(1));
            if (!dvm_instance_of(o, c)) {
                tl_jclass *oc = dvm_object_class(o);
                dvm_throw("java/lang/ClassCastException", "%s cannot be cast to %s", oc ? oc->name : "?", c->name);
                goto exception;
            }
        }
        NEXT(2);
    }
    case 0x20: WI(regs, A4, dvm_instance_of(RL(regs, B4), resolve_type(dex, FETCH(1)))); NEXT(2);
    case 0x21: {
        jobj *a = RL(regs, B4);
        THROW_IF(!a && !dvm_throw("java/lang/NullPointerException", "Attempt to get length of null array"));
        WI(regs, A4, (int32_t)(a->kind == TL_K_OBJ_ARRAY ? a->oarr.len : a->arr.len));
        NEXT(1);
    }
    case 0x22: {                                                   /* new-instance */
        tl_jclass *c = resolve_type(dex, FETCH(1));
        dvm_class *dc = dvm_class_of(c);
        if (dc && !dvm_ensure_init(dc)) goto exception;
        jobj *o = dvm_new_object(c);
        if (dc) dvm_slots(o);
        WL(regs, AA, o);
        NEXT(2);
    }
    case 0x23: {                                                   /* new-array */
        int32_t n = RI(regs, B4);
        THROW_IF(n < 0 && !dvm_throw("java/lang/NegativeArraySizeException", "%d", n));
        WL(regs, A4, new_array(resolve_type(dex, FETCH(1)), n));
        NEXT(2);
    }
    case 0x24: case 0x25: {                                        /* filled-new-array(/range) */
        bool range = (w & 0xFF) == 0x25;
        int count = range ? AA : B4;
        tl_jclass *ac = resolve_type(dex, FETCH(1));
        jobj *a = new_array(ac, count);
        uint16_t list = FETCH(2);
        for (int i = 0; i < count; i++) {
            int reg = range ? list + i : (i < 4 ? (list >> (4 * i)) & 0xF : A4);
            if (a->kind == TL_K_OBJ_ARRAY) a->oarr.v[i] = RL(regs, reg);
            else ((int32_t *)a->arr.data)[i] = RI(regs, reg);
        }
        result.l = a;
        NEXT(3);
    }
    case 0x26: {                                                   /* fill-array-data */
        jobj *a = RL(regs, AA);
        THROW_IF(!a && !dvm_throw("java/lang/NullPointerException", "fill-array-data on null"));
        int32_t off = (int32_t)(FETCH(1) | (uint32_t)FETCH(2) << 16);
        const uint16_t *p = insns + pc + off;
        uint16_t width = p[1]; uint32_t n = p[2] | (uint32_t)p[3] << 16;
        THROW_IF(n > a->arr.len && !dvm_throw("java/lang/ArrayIndexOutOfBoundsException", "fill-array-data"));
        memcpy(a->arr.data, p + 4, (size_t)width * n);
        NEXT(3);
    }
    case 0x27: {
        jobj *e = RL(regs, AA);
        if (!e) dvm_throw("java/lang/NullPointerException", "throw with null exception"); else tl_jni_set_pending(e);
        goto exception;
    }
    case 0x28: pc += (int8_t)AA; goto dispatch;
    case 0x29: pc += (int16_t)FETCH(1); goto dispatch;
    case 0x2a: pc += (int32_t)(FETCH(1) | (uint32_t)FETCH(2) << 16); goto dispatch;
    case 0x2b: {                                                   /* packed-switch */
        const uint16_t *p = insns + pc + (int32_t)(FETCH(1) | (uint32_t)FETCH(2) << 16);
        uint16_t size = p[1]; int32_t first = (int32_t)(p[2] | (uint32_t)p[3] << 16);
        int32_t v = RI(regs, AA);
        int64_t i = (int64_t)v - first;
        if (i >= 0 && i < size) { const uint16_t *t = p + 4 + 2 * i; pc += (int32_t)(t[0] | (uint32_t)t[1] << 16); goto dispatch; }
        NEXT(3);
    }
    case 0x2c: {                                                   /* sparse-switch */
        const uint16_t *p = insns + pc + (int32_t)(FETCH(1) | (uint32_t)FETCH(2) << 16);
        uint16_t size = p[1];
        int32_t v = RI(regs, AA);
        const uint16_t *keys = p + 2, *tgts = p + 2 + 2 * size;
        for (int i = 0; i < size; i++) {
            int32_t k = (int32_t)(keys[2 * i] | (uint32_t)keys[2 * i + 1] << 16);
            if (k == v) { pc += (int32_t)(tgts[2 * i] | (uint32_t)tgts[2 * i + 1] << 16); goto dispatch; }
            if (k > v) break;
        }
        NEXT(3);
    }
    case 0x2d: case 0x2e: {                                        /* cmpl-float, cmpg-float */
        float a = RF(regs, FETCH(1) & 0xFF), b = RF(regs, FETCH(1) >> 8);
        WI(regs, AA, a < b ? -1 : a > b ? 1 : a == b ? 0 : ((w & 0xFF) == 0x2d ? -1 : 1)); NEXT(2);
    }
    case 0x2f: case 0x30: {
        double a = RD(regs, FETCH(1) & 0xFF), b = RD(regs, FETCH(1) >> 8);
        WI(regs, AA, a < b ? -1 : a > b ? 1 : a == b ? 0 : ((w & 0xFF) == 0x2f ? -1 : 1)); NEXT(2);
    }
    case 0x31: { int64_t a = RJ(regs, FETCH(1) & 0xFF), b = RJ(regs, FETCH(1) >> 8); WI(regs, AA, a < b ? -1 : a > b); NEXT(2); }
#define IFCMP(op, cond) case op: if (cond) { pc += (int16_t)FETCH(1); goto dispatch; } NEXT(2);
    /* ints are kept zero-extended and references whole, so equality is the whole register either way */
    IFCMP(0x32, regs[A4] == regs[B4])
    IFCMP(0x33, regs[A4] != regs[B4])
    IFCMP(0x34, RI(regs, A4) < RI(regs, B4))
    IFCMP(0x35, RI(regs, A4) >= RI(regs, B4))
    IFCMP(0x36, RI(regs, A4) > RI(regs, B4))
    IFCMP(0x37, RI(regs, A4) <= RI(regs, B4))
    IFCMP(0x38, regs[AA] == 0)
    IFCMP(0x39, regs[AA] != 0)
    IFCMP(0x3a, RI(regs, AA) < 0)
    IFCMP(0x3b, RI(regs, AA) >= 0)
    IFCMP(0x3c, RI(regs, AA) > 0)
    IFCMP(0x3d, RI(regs, AA) <= 0)
#undef IFCMP
    case 0x44: case 0x45: case 0x46: case 0x47: case 0x48: case 0x49: case 0x4a:     /* aget* */
    case 0x4b: case 0x4c: case 0x4d: case 0x4e: case 0x4f: case 0x50: case 0x51: {   /* aput* */
        uint16_t bc = FETCH(1);
        jobj *a = RL(regs, bc & 0xFF);
        int32_t i = RI(regs, bc >> 8);
        if (!array_check(a, i)) goto exception;
        int op = w & 0xFF;
        bool put = op >= 0x4b;
        switch (put ? op - 7 : op) {
        case 0x44:                                                 /* int, float */
            if (put) ((int32_t *)a->arr.data)[i] = RI(regs, AA); else WI(regs, AA, ((int32_t *)a->arr.data)[i]);
            break;
        case 0x45: if (put) ((int64_t *)a->arr.data)[i] = RJ(regs, AA); else WJ(regs, AA, ((int64_t *)a->arr.data)[i]); break;
        case 0x46:
            if (put) {
                jobj *v = RL(regs, AA);
                if (v && a->cls && a->cls->name[0] == '[' && a->cls->name[1] == 'L') {
                    tl_jclass *ec = class_for_desc(a->cls->name + 1);
                    if (!dvm_instance_of(v, ec)) { dvm_throw("java/lang/ArrayStoreException", "%s cannot be stored in an array of type %s", dvm_object_class(v)->name, a->cls->name); goto exception; }
                }
                a->oarr.v[i] = v;
            } else WL(regs, AA, a->oarr.v[i]);
            break;
        case 0x47: if (put) ((uint8_t *)a->arr.data)[i] = (uint8_t)regs[AA]; else WI(regs, AA, ((uint8_t *)a->arr.data)[i]); break;
        case 0x48: if (put) ((int8_t *)a->arr.data)[i] = (int8_t)regs[AA]; else WI(regs, AA, ((int8_t *)a->arr.data)[i]); break;
        case 0x49: if (put) ((uint16_t *)a->arr.data)[i] = (uint16_t)regs[AA]; else WI(regs, AA, ((uint16_t *)a->arr.data)[i]); break;
        case 0x4a: if (put) ((int16_t *)a->arr.data)[i] = (int16_t)regs[AA]; else WI(regs, AA, ((int16_t *)a->arr.data)[i]); break;
        }
        NEXT(2);
    }
    case 0x52: case 0x53: case 0x54: case 0x55: case 0x56: case 0x57: case 0x58:     /* iget* */
    case 0x59: case 0x5a: case 0x5b: case 0x5c: case 0x5d: case 0x5e: case 0x5f: {   /* iput* */
        jobj *o = RL(regs, B4);
        fref *f = resolve_fref(dex, FETCH(1), false);
        bool put = (w & 0xFF) >= 0x59;
        if (!o) { dvm_throw("java/lang/NullPointerException", "Attempt to %s field on a null object reference", put ? "write to" : "read from"); goto exception; }
        jvalue *slot;
        if (f->df) {
            /* Object.shadow$_klass_: the object's class, which ART keeps in the object's header */
            if (!put && f->df->name[0] == 's' && !strcmp(f->df->name, "shadow$_klass_")) { WL(regs, A4, dvm_object_class(o)->mirror); NEXT(2); }
            /* String.count: the length and the coder ART keeps there; Husk's strings keep their characters elsewhere */
            if (o->kind == TL_K_STRING && !put && !strcmp(f->df->name, "count")) {
                int32_t n; const uint16_t *s = tl_dvm_string_chars(o, &n);
                bool latin1 = true;
                for (int32_t k = 0; k < n && latin1; k++) if (s[k] > 0xFF) latin1 = false;
                WI(regs, A4, (n << 1) | (latin1 ? 0 : 1));
                NEXT(2);
            }
            jvalue *sl = dvm_slots(o);
            if (f->df->slot >= o->nfields) sl = dvm_grow_slots(o, f->df->slot + 1);
            slot = &sl[f->df->slot];
        } else {
            if (!f->hf) { dvm_throw("java/lang/NoSuchFieldError", "field %u", FETCH(1)); goto exception; }
            slot = tl_jni_field_slot(o, f->hf);
        }
        switch (f->type) {
        case 'J': case 'D': if (put) slot->j = RJ(regs, A4); else WJ(regs, A4, slot->j); break;
        case 'L': if (put) slot->l = RL(regs, A4); else WL(regs, A4, slot->l); break;
        case 'Z': if (put) { slot->j = 0; slot->z = (uint8_t)regs[A4]; } else WI(regs, A4, slot->z); break;
        case 'B': if (put) { slot->j = 0; slot->b = (int8_t)regs[A4]; } else WI(regs, A4, slot->b); break;
        case 'C': if (put) { slot->j = 0; slot->c = (uint16_t)regs[A4]; } else WI(regs, A4, slot->c); break;
        case 'S': if (put) { slot->j = 0; slot->s = (int16_t)regs[A4]; } else WI(regs, A4, slot->s); break;
        default: if (put) { slot->j = 0; slot->i = RI(regs, A4); } else WI(regs, A4, slot->i); break;
        }
        NEXT(2);
    }
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66:     /* sget* */
    case 0x67: case 0x68: case 0x69: case 0x6a: case 0x6b: case 0x6c: case 0x6d: {   /* sput* */
        fref *f = resolve_fref(dex, FETCH(1), true);
        bool put = (w & 0xFF) >= 0x67;
        jvalue *slot;
        if (f->df) {
            if (!dvm_ensure_init(f->df->cls)) goto exception;
            slot = &f->df->cls->jc->statics[f->df->slot];
        } else {
            if (!f->hf) { dvm_throw("java/lang/NoSuchFieldError", "static field %u", FETCH(1)); goto exception; }
            slot = tl_jni_field_slot(NULL, f->hf);
        }
        switch (f->type) {
        case 'J': case 'D': if (put) slot->j = RJ(regs, AA); else WJ(regs, AA, slot->j); break;
        case 'L': if (put) slot->l = RL(regs, AA); else WL(regs, AA, slot->l); break;
        case 'Z': if (put) { slot->j = 0; slot->z = (uint8_t)regs[AA]; } else WI(regs, AA, slot->z); break;
        case 'B': if (put) { slot->j = 0; slot->b = (int8_t)regs[AA]; } else WI(regs, AA, slot->b); break;
        case 'C': if (put) { slot->j = 0; slot->c = (uint16_t)regs[AA]; } else WI(regs, AA, slot->c); break;
        case 'S': if (put) { slot->j = 0; slot->s = (int16_t)regs[AA]; } else WI(regs, AA, slot->s); break;
        default: if (put) { slot->j = 0; slot->i = RI(regs, AA); } else WI(regs, AA, slot->i); break;
        }
        NEXT(2);
    }
    case 0x6e: case 0x6f: case 0x70: case 0x71: case 0x72: {      /* invoke-kind */
        if (t_depth > 0 && t_depth <= 8192) t_pcs[t_depth - 1] = pc;
        uint16_t list = FETCH(2);
        uint16_t argregs[5] = { (uint16_t)(list & 0xF), (uint16_t)((list >> 4) & 0xF), (uint16_t)((list >> 8) & 0xF), (uint16_t)(list >> 12), (uint16_t)A4 };
        result.j = 0;
        if (!invoke_regs(m, (w & 0xFF) - 0x6e, FETCH(1), B4, argregs, 0, false, regs, &result)) goto exception;
        normalize(((mref *)dex->mcache[FETCH(1)])->shorty[0], &result);
        NEXT(3);
    }
    case 0x74: case 0x75: case 0x76: case 0x77: case 0x78: {      /* invoke-kind/range */
        if (t_depth > 0 && t_depth <= 8192) t_pcs[t_depth - 1] = pc;
        result.j = 0;
        if (!invoke_regs(m, (w & 0xFF) - 0x74, FETCH(1), AA, NULL, FETCH(2), true, regs, &result)) goto exception;
        normalize(((mref *)dex->mcache[FETCH(1)])->shorty[0], &result);
        NEXT(3);
    }
    case 0x7b: WI(regs, A4, -RI(regs, B4)); NEXT(1);
    case 0x7c: WI(regs, A4, ~RI(regs, B4)); NEXT(1);
    case 0x7d: WJ(regs, A4, -RJ(regs, B4)); NEXT(1);
    case 0x7e: WJ(regs, A4, ~RJ(regs, B4)); NEXT(1);
    case 0x7f: WF(regs, A4, -RF(regs, B4)); NEXT(1);
    case 0x80: WD(regs, A4, -RD(regs, B4)); NEXT(1);
    case 0x81: WJ(regs, A4, RI(regs, B4)); NEXT(1);
    case 0x82: WF(regs, A4, (float)RI(regs, B4)); NEXT(1);
    case 0x83: WD(regs, A4, (double)RI(regs, B4)); NEXT(1);
    case 0x84: WI(regs, A4, (int32_t)RJ(regs, B4)); NEXT(1);
    case 0x85: WF(regs, A4, (float)RJ(regs, B4)); NEXT(1);
    case 0x86: WD(regs, A4, (double)RJ(regs, B4)); NEXT(1);
    case 0x87: WI(regs, A4, f2i(RF(regs, B4))); NEXT(1);
    case 0x88: WJ(regs, A4, f2l(RF(regs, B4))); NEXT(1);
    case 0x89: WD(regs, A4, (double)RF(regs, B4)); NEXT(1);
    case 0x8a: WI(regs, A4, d2i(RD(regs, B4))); NEXT(1);
    case 0x8b: WJ(regs, A4, f2l(RD(regs, B4))); NEXT(1);
    case 0x8c: WF(regs, A4, (float)RD(regs, B4)); NEXT(1);
    case 0x8d: WI(regs, A4, (int8_t)RI(regs, B4)); NEXT(1);
    case 0x8e: WI(regs, A4, (uint16_t)RI(regs, B4)); NEXT(1);
    case 0x8f: WI(regs, A4, (int16_t)RI(regs, B4)); NEXT(1);
    default: break;
    }
    /* binary operations: 23x (0x90-0xaf), 2addr (0xb0-0xcf), lit16 (0xd0-0xd7), lit8 (0xd8-0xe2) */
    {
        int op = w & 0xFF;
        if (op >= 0x90 && op <= 0xcf) {
            int dst, s1, s2, len;
            if (op < 0xb0) { dst = AA; s1 = FETCH(1) & 0xFF; s2 = FETCH(1) >> 8; len = 2; }
            else { dst = A4; s1 = A4; s2 = B4; len = 1; op -= 0x20; }
            switch (op) {
            case 0x90: WI(regs, dst, (int32_t)((uint32_t)RI(regs, s1) + (uint32_t)RI(regs, s2))); break;
            case 0x91: WI(regs, dst, (int32_t)((uint32_t)RI(regs, s1) - (uint32_t)RI(regs, s2))); break;
            case 0x92: WI(regs, dst, (int32_t)((uint32_t)RI(regs, s1) * (uint32_t)RI(regs, s2))); break;
            case 0x93: case 0x94: {
                int32_t a = RI(regs, s1), b = RI(regs, s2);
                if (!b) { dvm_throw("java/lang/ArithmeticException", "divide by zero"); goto exception; }
                if (a == INT32_MIN && b == -1) WI(regs, dst, op == 0x93 ? a : 0); else WI(regs, dst, op == 0x93 ? a / b : a % b);
                break;
            }
            case 0x95: WI(regs, dst, RI(regs, s1) & RI(regs, s2)); break;
            case 0x96: WI(regs, dst, RI(regs, s1) | RI(regs, s2)); break;
            case 0x97: WI(regs, dst, RI(regs, s1) ^ RI(regs, s2)); break;
            case 0x98: WI(regs, dst, (int32_t)((uint32_t)RI(regs, s1) << (RI(regs, s2) & 31))); break;
            case 0x99: WI(regs, dst, RI(regs, s1) >> (RI(regs, s2) & 31)); break;
            case 0x9a: WI(regs, dst, (int32_t)((uint32_t)RI(regs, s1) >> (RI(regs, s2) & 31))); break;
            case 0x9b: WJ(regs, dst, (int64_t)((uint64_t)RJ(regs, s1) + (uint64_t)RJ(regs, s2))); break;
            case 0x9c: WJ(regs, dst, (int64_t)((uint64_t)RJ(regs, s1) - (uint64_t)RJ(regs, s2))); break;
            case 0x9d: WJ(regs, dst, (int64_t)((uint64_t)RJ(regs, s1) * (uint64_t)RJ(regs, s2))); break;
            case 0x9e: case 0x9f: {
                int64_t a = RJ(regs, s1), b = RJ(regs, s2);
                if (!b) { dvm_throw("java/lang/ArithmeticException", "divide by zero"); goto exception; }
                if (a == INT64_MIN && b == -1) WJ(regs, dst, op == 0x9e ? a : 0); else WJ(regs, dst, op == 0x9e ? a / b : a % b);
                break;
            }
            case 0xa0: WJ(regs, dst, RJ(regs, s1) & RJ(regs, s2)); break;
            case 0xa1: WJ(regs, dst, RJ(regs, s1) | RJ(regs, s2)); break;
            case 0xa2: WJ(regs, dst, RJ(regs, s1) ^ RJ(regs, s2)); break;
            case 0xa3: WJ(regs, dst, (int64_t)((uint64_t)RJ(regs, s1) << (RI(regs, s2) & 63))); break;
            case 0xa4: WJ(regs, dst, RJ(regs, s1) >> (RI(regs, s2) & 63)); break;
            case 0xa5: WJ(regs, dst, (int64_t)((uint64_t)RJ(regs, s1) >> (RI(regs, s2) & 63))); break;
            case 0xa6: WF(regs, dst, RF(regs, s1) + RF(regs, s2)); break;
            case 0xa7: WF(regs, dst, RF(regs, s1) - RF(regs, s2)); break;
            case 0xa8: WF(regs, dst, RF(regs, s1) * RF(regs, s2)); break;
            case 0xa9: WF(regs, dst, RF(regs, s1) / RF(regs, s2)); break;
            case 0xaa: WF(regs, dst, fmodf(RF(regs, s1), RF(regs, s2))); break;
            case 0xab: WD(regs, dst, RD(regs, s1) + RD(regs, s2)); break;
            case 0xac: WD(regs, dst, RD(regs, s1) - RD(regs, s2)); break;
            case 0xad: WD(regs, dst, RD(regs, s1) * RD(regs, s2)); break;
            case 0xae: WD(regs, dst, RD(regs, s1) / RD(regs, s2)); break;
            case 0xaf: WD(regs, dst, fmod(RD(regs, s1), RD(regs, s2))); break;
            }
            NEXT(len);
        }
        if (op >= 0xd0 && op <= 0xe2) {
            int dst, src; int32_t lit; int len = 2;
            if (op <= 0xd7) { dst = A4; src = B4; lit = (int16_t)FETCH(1); op -= 0xd0; }
            else { dst = AA; src = FETCH(1) & 0xFF; lit = (int8_t)(FETCH(1) >> 8); op -= 0xd8; }
            int32_t a = RI(regs, src);
            switch (op) {
            case 0: WI(regs, dst, (int32_t)((uint32_t)a + (uint32_t)lit)); break;
            case 1: WI(regs, dst, (int32_t)((uint32_t)lit - (uint32_t)a)); break;
            case 2: WI(regs, dst, (int32_t)((uint32_t)a * (uint32_t)lit)); break;
            case 3: case 4:
                if (!lit) { dvm_throw("java/lang/ArithmeticException", "divide by zero"); goto exception; }
                if (a == INT32_MIN && lit == -1) WI(regs, dst, op == 3 ? a : 0); else WI(regs, dst, op == 3 ? a / lit : a % lit);
                break;
            case 5: WI(regs, dst, a & lit); break;
            case 6: WI(regs, dst, a | lit); break;
            case 7: WI(regs, dst, a ^ lit); break;
            case 8: WI(regs, dst, (int32_t)((uint32_t)a << (lit & 31))); break;
            case 9: WI(regs, dst, a >> (lit & 31)); break;
            case 10: WI(regs, dst, (int32_t)((uint32_t)a >> (lit & 31))); break;
            }
            NEXT(len);
        }
        if (op == 0xfe || op == 0xff) { dvm_throw("java/lang/UnsupportedOperationException", "const-method-handle/type"); goto exception; }
        if (op == 0xfa || op == 0xfb) {                            /* invoke-polymorphic(/range) */
            bool range = op == 0xfb;
            int count = range ? AA : B4;
            uint16_t list = FETCH(2);
            uint64_t vals[256];
            for (int i = 0; i < count && i < 256; i++) vals[i] = regs[range ? list + i : (i < 4 ? (list >> (4 * i)) & 0xF : A4)];
            const uint8_t *mi = dex->b + dex->meth_off + 8 * FETCH(1);
            const char *mname = dex_str(dex, rd32(mi + 4));
            tl_jclass *owner = resolve_type(dex, rd16(mi));
            char csig[8192], cshorty[300]; int np;
            proto_sig(dex, FETCH(3), csig, sizeof(csig), cshorty, sizeof(cshorty), &np);
            jobj *recv = (jobj *)(uintptr_t)vals[0];
            if (!recv) { dvm_throw("java/lang/NullPointerException", "invoke-polymorphic on null"); goto exception; }
            /* wide arguments take two registers; pack one value per parameter */
            uint64_t args[64]; int k = 1;
            for (int i = 0; i < np && i < 63; i++) { args[i] = vals[k]; k += (cshorty[1 + i] == 'J' || cshorty[1 + i] == 'D') ? 2 : 1; }
            result.j = 0;
            if (!strcmp(owner->name, "java/lang/invoke/VarHandle")) {
                if (!varhandle(mname, cshorty, recv, args, &result)) goto exception;
            } else {
                dvm_throw("java/lang/UnsupportedOperationException", "MethodHandle.%s is not supported yet (in %s.%s)", mname, m->cls->name, m->name);
                goto exception;
            }
            NEXT(4);
        }
        if (op >= 0xfc && op <= 0xfd) { dvm_throw("java/lang/UnsupportedOperationException", "invoke-custom in %s.%s is not supported yet", m->cls->name, m->name); goto exception; }
        dvm_throw("java/lang/VerifyError", "bad opcode %#x in %s.%s", op, m->cls->name, m->name);
        goto exception;
    }

exception: {
        jobj *e = tl_jni_pending_object();
        if (!e) { dvm_throw("java/lang/InternalError", "exception with nothing pending"); e = tl_jni_pending_object(); }
        uint32_t target;
        if (find_handler(m, pc, e, &target)) {
            tl_jni_set_pending(NULL);
            caught = e;
            pc = target;
            goto dispatch;
        }
        if ((m->flags & 0x20)) dvm_monitor_exit((m->flags & 8) ? m->cls->jc->mirror : RL(regs, m->regs - m->ins));
        return false;
    }
done:
    if ((m->flags & 0x20)) dvm_monitor_exit((m->flags & 8) ? m->cls->jc->mirror : RL(regs, m->regs - m->ins));
    return true;
}

/* ================================================================== the JNI world's view */

static bool hook_attach(tl_jclass *jc)
{
    if (!strcmp(jc->name, "java/lang/Class")) g_class_class = jc;
    bool ok = attach(jc);
    if (ok && !strcmp(jc->name, "java/lang/String")) g_string_class = jc;
    return ok;
}
static void *hook_find_method(tl_jclass *jc, const char *name, const char *sig, bool is_static)
{
    dvm_class *c = dvm_class_of(jc);
    return c ? dvm_find_method(c, name, sig, is_static) : NULL;
}
static jvalue hook_invoke(void *method, jobj *self, bool nonvirtual, const jvalue *args)
{
    dvm_method *m = method;
    jvalue r; r.j = 0;
    if ((m->flags & 8)) { if (!dvm_ensure_init(m->cls)) return r; }
    else if (!nonvirtual && self && m->name[0] != '<' && !(m->flags & 2)) {
        dvm_method *v = dvm_find_virtual(dvm_object_class(self), m->name, m->sig);
        if (v) m = v;
    }
    dvm_call(m, self, args, &r);
    return r;
}
static bool hook_find_field(tl_jclass *jc, const char *name, const char *sig, bool is_static, tl_jclass **decl, uint32_t *slot)
{
    (void)sig;
    dvm_class *c = dvm_class_of(jc);
    dvm_field *f = c ? dvm_find_field(c, name, is_static) : NULL;
    if (!f) return false;
    if (is_static) dvm_ensure_init(f->cls);
    *decl = f->cls->jc; *slot = f->slot;
    return true;
}
static uint32_t hook_slots(tl_jclass *jc) { dvm_class *c = dvm_class_of(jc); return c ? c->nslots : 0; }

static const tl_dvm_hooks k_hooks = { hook_attach, hook_find_method, hook_invoke, hook_find_field, hook_slots };

static bool g_running;

static void reattach(tl_jclass *jc)
{
    if (jc->dvm || !(cdef_find(jc->name) || jc->name[0] == '[')) return;
    jc->nmeths = 0;                 /* lookups made before belong to Husk's stand-ins; the bytecode answers now */
    jc->nfields = 0;
    hook_attach(jc);
}
bool tl_dvm_running(void) { return g_running; }

/* java.boot.class.path: the boot jars, colon-separated, core-oj first (HiddenApiBypass reloads libcore's classes from it). */
char g_dvm_boot_path[4096];

bool tl_dvm_start(const char *const *boot, int nboot)
{
    size_t k = 0;
    for (int i = 0; i < nboot && k < sizeof(g_dvm_boot_path); i++)
        k += (size_t)snprintf(g_dvm_boot_path + k, sizeof(g_dvm_boot_path) - k, "%s%s", i ? ":" : "", boot[i]);
    for (int i = 0; i < nboot; i++) if (!open_path(boot[i])) tl_log_line("dvm: could not open %s", boot[i]);
    if (!cdef_find("java/lang/Object")) { tl_log_line("dvm: no java/lang/Object on the boot class path"); return false; }
    dvm_natives_init();
    tl_dvm = &k_hooks;
    g_running = true;
    /* classes declared before the runtime started (Husk's own Java world): those it has bytecode for become its */
    tl_jni_each_declared(reattach);
    /* The core classes, linked in the order everything else leans on. */
    g_class_class = tl_jni_class("java/lang/Class");
    tl_jni_class("java/lang/Object");
    g_nboot_dex = g_ndex;
    g_string_class = tl_jni_class("java/lang/String");
    return true;
}

bool tl_dvm_add_apk(const char *apk) { return open_path(apk); }

/* ---- the app's class loader: what Class.getClassLoader says for the app's own classes (a PathClassLoader over its APKs, made by
 * husk.AppRunner), so code that reads resources through its class's loader, or checks a class came from its own loader, works */
static jobj *g_app_loader;
static bool from_app(dvm_class *c)
{
    if (!c || !c->dex || g_nboot_dex < 0) return false;
    for (int i = 0; i < g_nboot_dex; i++) if (g_dex[i] == c->dex) return false;
    return true;
}
static dvm_field *class_loader_field(void)
{
    static dvm_field *f;
    if (!f) { dvm_class *cc = dvm_class_of(g_class_class); f = cc ? dvm_find_field(cc, "classLoader", false) : NULL; }
    return f;
}
void dvm_mirror_loader(jobj *mirror);
void dvm_mirror_loader(jobj *mirror)
{
    dvm_field *f = g_app_loader ? class_loader_field() : NULL;
    if (!f || !mirror->fields || f->slot >= mirror->nfields || mirror->fields[f->slot].l) return;
    if (from_app((dvm_class *)mirror->klass.jc->dvm)) mirror->fields[f->slot].l = g_app_loader;
}
static void loader_for(tl_jclass *jc) { if (jc->mirror && jc->mirror->fields) dvm_mirror_loader(jc->mirror); }
void dvm_set_app_loader(jobj *loader);
void dvm_set_app_loader(jobj *loader)
{
    if (!loader) return;
    loader->refs = 1u << 30;
    g_app_loader = loader;
    tl_jni_each_declared(loader_for);
}

/* ---- dalvik.system.DexFile: DexClassLoader, InMemoryDexClassLoader and PathClassLoaders made at run time.
 * Classes live in one namespace here (first definition wins), so opening a file adds its classes to it and defineClass finds a class
 * by name. The cookie is a long[] { first dex, count } into g_dex; a file already open is not read again. */
static struct { char *path; int first, count; } g_opened[256];
static int g_nopened;
static pthread_mutex_t g_open_lock = PTHREAD_MUTEX_INITIALIZER;

static jobj *dex_cookie(int first, int count)
{
    jobj *c = tl_jni_new_prim_array('J', 2);
    c->refs = 1u << 30;
    ((int64_t *)c->arr.data)[0] = first; ((int64_t *)c->arr.data)[1] = count;
    return c;
}

static bool DexFile_open(jobj *self, const jvalue *a, jvalue *ret)
{
    (void)self;
    const char *path = tl_jni_string(a[0].l);
    if (!path) return dvm_throw("java/lang/NullPointerException", "path");
    pthread_mutex_lock(&g_open_lock);
    int first = -1, count = 0;
    for (int i = 0; i < g_nopened; i++) if (!strcmp(g_opened[i].path, path)) { first = g_opened[i].first; count = g_opened[i].count; break; }
    if (first < 0) {
        /* the boot class path is open already: its dexes by label */
        const char *base = strrchr(path, '/'); base = base ? base + 1 : path;
        size_t bl = strlen(base);
        for (int i = 0; i < g_ndex; i++) if (!strncmp(g_dex[i]->name, base, bl) && g_dex[i]->name[bl] == '!') { if (first < 0) first = i; count++; }
    }
    if (first < 0) {
        int before = g_ndex;
        g_add_ns = ++g_next_ns;
        bool ok = open_path(path);
        g_add_ns = 0;
        if (ok || g_ndex > before) { first = before; count = g_ndex - before; }
        if (first >= 0 && g_nopened < 256) { g_opened[g_nopened].path = strdup(path); g_opened[g_nopened].first = first; g_opened[g_nopened++].count = count; }
    }
    pthread_mutex_unlock(&g_open_lock);
    if (first < 0) return dvm_throw("java/io/IOException", "No original dex files found for dex location %s", path);
    if (g_dvm_trace >= 1) tl_log_line("dvm: DexFile %s: dexes %d..%d", path, first, first + count - 1);
    ret->l = dex_cookie(first, count);
    return true;
}

/* openInMemoryDexFilesNative(ByteBuffer[] bufs, byte[][] arrays, int[] starts, int[] ends, loader, elements) */
static bool DexFile_openInMemory(jobj *self, const jvalue *a, jvalue *ret)
{
    (void)self;
    jobj *bufs = a[0].l, *arrays = a[1].l, *starts = a[2].l, *ends = a[3].l;
    uint32_t n = bufs ? bufs->oarr.len : arrays ? arrays->oarr.len : 0;
    pthread_mutex_lock(&g_open_lock);
    int first = g_ndex;
    g_add_ns = ++g_next_ns;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *src = NULL; size_t len = 0;
        jobj *arr = arrays ? arrays->oarr.v[i] : NULL;
        int32_t st = starts ? ((int32_t *)starts->arr.data)[i] : 0, en = ends ? ((int32_t *)ends->arr.data)[i] : 0;
        if (arr) { src = (const uint8_t *)arr->arr.data + st; len = (size_t)(en - st); }
        else if (bufs && bufs->oarr.v[i]) {
            /* a direct ByteBuffer: its address */
            jobj *bb = bufs->oarr.v[i];
            dvm_class *bc = dvm_class_of(dvm_object_class(bb));
            dvm_field *addr = bc ? dvm_find_field(bc, "address", false) : NULL;
            if (addr) { src = (const uint8_t *)(uintptr_t)dvm_slots(bb)[addr->slot].j + st; len = (size_t)(en - st); }
        }
        if (!src || !len) continue;
        uint8_t *copy = malloc(len);
        memcpy(copy, src, len);
        char label[64]; snprintf(label, sizeof(label), "memory-%d.dex", g_ndex);
        /* TL_DEX_DUMP=<dir>: keep a copy of each dex an app loads from memory, to look at */
        if (getenv("TL_DEX_DUMP")) { char fp[600]; snprintf(fp, sizeof(fp), "%s/%s", getenv("TL_DEX_DUMP"), label); FILE *df = fopen(fp, "wb"); if (df) { fwrite(copy, 1, len, df); fclose(df); } }
        if (!dex_add(copy, len, label)) free(copy);
    }
    int count = g_ndex - first;
    g_add_ns = 0;
    pthread_mutex_unlock(&g_open_lock);
    if (!count) return dvm_throw("java/io/IOException", "no dex in memory");
    ret->l = dex_cookie(first, count);
    return true;
}

static bool cookie_range(jobj *c, int *first, int *count)
{
    if (!c || c->kind != TL_K_PRIM_ARRAY || c->arr.len < 2) return false;
    *first = (int)((int64_t *)c->arr.data)[0]; *count = (int)((int64_t *)c->arr.data)[1];
    return *first >= 0 && *first + *count <= g_ndex;
}

/* defineClassNative(String name "a/b/C", ClassLoader, Object cookie, DexFile) */
static bool DexFile_define(jobj *self, const jvalue *a, jvalue *ret)
{
    (void)self;
    const char *n = tl_jni_string(a[0].l);
    ret->l = NULL;
    if (!n) return true;
    char name[512]; snprintf(name, sizeof(name), "%s", n);
    for (char *q = name; *q; q++) if (*q == '.') *q = '/';
    /* only what this file defines (or a class of the same name already loaded from elsewhere, first definition winning) */
    int first, count;
    cdef *cd = cdef_find(name);
    if (!cd) return true;
    if (cookie_range(a[2].l, &first, &count)) {
        bool here = false;
        dvm_dex *hd = NULL; uint32_t hdef = 0;
        for (int i = first; i < first + count && !here; i++) {
            dvm_dex *d = g_dex[i];
            for (uint32_t k = 0; k < d->ncls && !here; k++) {
                char nm[512]; desc_to_name(dex_type(d, rd32(d->b + d->cls_off + 32 * k)), nm, sizeof(nm));
                if (!strcmp(nm, name)) { here = true; hd = d; hdef = k; }
            }
        }
        if (!here) return true;
    }
    tl_jclass *jc = NULL;
    if (cookie_range(a[2].l, &first, &count) && count > 0 && g_dex[first]->ns) {
        char key[540]; snprintf(key, sizeof(key), "%s@%d", name, g_dex[first]->ns);
        if (cdef_find(key)) jc = tl_jni_class(key);
    }
    if (!jc) jc = dvm_class_named(name);
    ret->l = jc ? jc->mirror : NULL;
    /* the loader that defined it, which Class.getClassLoader answers (loaders that check their classes are their own rely on it) */
    if (jc && a[1].l) {
        static dvm_field *loader_field;
        if (!loader_field) { dvm_class *cc = dvm_class_of(g_class_class); loader_field = cc ? dvm_find_field(cc, "classLoader", false) : NULL; }
        if (loader_field) { jvalue *sl = dvm_slots(jc->mirror); if (!sl[loader_field->slot].l) sl[loader_field->slot].l = a[1].l; }
    }
    return true;
}

static bool DexFile_names(jobj *self, const jvalue *a, jvalue *ret)
{
    (void)self;
    int first = 0, count = 0;
    cookie_range(a[0].l, &first, &count);
    uint32_t total = 0;
    for (int i = first; i < first + count; i++) total += g_dex[i]->ncls;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), total);
    arr->cls = tl_jni_class("[Ljava/lang/String;");
    arr->refs = 1u << 30;
    uint32_t k = 0;
    for (int i = first; i < first + count; i++) {
        dvm_dex *d = g_dex[i];
        for (uint32_t j = 0; j < d->ncls; j++) {
            char nm[512]; desc_to_name(dex_type(d, rd32(d->b + d->cls_off + 32 * j)), nm, sizeof(nm));
            for (char *q = nm; *q; q++) if (*q == '/') *q = '.';
            arr->oarr.v[k++] = dvm_new_string_utf8(nm);
        }
    }
    ret->l = arr;
    return true;
}

static bool DexFile_true(jobj *self, const jvalue *a, jvalue *ret) { (void)self; (void)a; ret->j = 0; ret->z = true; return true; }
static bool DexFile_false(jobj *self, const jvalue *a, jvalue *ret) { (void)self; (void)a; ret->j = 0; return true; }

dvm_native_fn dvm_dexfile_native(const char *cls, const char *name, const char *sig);
dvm_native_fn dvm_dexfile_native(const char *cls, const char *name, const char *sig)
{
    (void)sig;
    if (strcmp(cls, "dalvik/system/DexFile")) return NULL;
    if (!strcmp(name, "openDexFileNative")) return DexFile_open;
    if (!strcmp(name, "openInMemoryDexFilesNative")) return DexFile_openInMemory;
    if (!strcmp(name, "defineClassNative")) return DexFile_define;
    if (!strcmp(name, "getClassNameList")) return DexFile_names;
    if (!strcmp(name, "closeDexFile")) return DexFile_true;
    if (!strcmp(name, "isBackedByOatFile") || !strcmp(name, "isDexOptNeeded") || !strcmp(name, "getDexOptNeeded")
        || !strcmp(name, "isReadOnlyJavaDclEnforced") || !strcmp(name, "setTrusted") || !strcmp(name, "verifyInBackgroundNative")
        || !strcmp(name, "getStaticSizeOfDexFile")) return DexFile_false;
    return NULL;
}

bool tl_jni_load_library(const char *base);
bool tl_dvm_load_library(const char *name_or_path);
bool tl_dvm_load_library(const char *name_or_path)
{
    /* a path to a library the app unpacked itself (SoLoader) loads from there; the APK's copy of the same name wins (the loader
     * looks there first) */
    const char *slash = strrchr(name_or_path, '/');
    struct stat st;
    if (slash && stat(name_or_path, &st) == 0 && st.st_size > 0) return tl_jni_load_library(name_or_path);
    return tl_jni_load_library(slash ? slash + 1 : name_or_path);
}

bool tl_dvm_load_natives(const char *libdir)
{
    tl_ld_add_search_dir(libdir);
    bool ok = tl_jni_load_library("libjavacore.so");
    ok = tl_jni_load_library("libopenjdk.so") && ok;
    /* core-icu4j's natives (charset converters, ICU's Java side), which ART loads with the runtime */
    if (!tl_jni_load_library("libicu_jni.so")) tl_log_line("dvm: libicu_jni.so did not load");
    if (tl_jni_pending()) { char b[300]; tl_log_line("dvm: loading libcore's natives threw %s", tl_dvm_describe_pending(b, sizeof(b)) ? b : "?"); tl_jni_clear(); }
    /* What ART initialises as it starts, before any app code: the core classes, in an order where each one's <clinit> finds what
     * it needs (System's pulls in most of java.lang and java.util.concurrent). */
    static const char *const order[] = { "java/lang/Object", "java/lang/Class", "java/lang/String", "java/lang/System",
                                         "java/lang/ThreadGroup", "java/lang/Thread", NULL };
    for (int i = 0; order[i]; i++) {
        dvm_class *c = dvm_class_of(dvm_class_named(order[i]));
        if (c && !dvm_ensure_init(c)) { char b[400]; tl_log_line("dvm: initialising %s threw %s", order[i], tl_dvm_describe_pending(b, sizeof(b)) ? b : "?"); tl_jni_clear(); }
    }
    dvm_current_thread();
    return ok;
}

bool tl_dvm_call_static(const char *cls, const char *name, const char *sig, const jvalue *args, jvalue *ret)
{
    tl_jclass *jc = dvm_class_named(cls);
    dvm_class *c = dvm_class_of(jc);
    if (!c) return dvm_throw("java/lang/NoClassDefFoundError", "%s", cls);
    dvm_method *m = dvm_find_method(c, name, sig, true);
    if (!m) return dvm_throw("java/lang/NoSuchMethodError", "%s.%s%s", cls, name, sig);
    if (!dvm_ensure_init(c)) return false;
    jvalue r; if (!ret) ret = &r;
    return dvm_call(m, NULL, args, ret);
}
