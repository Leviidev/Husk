/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ARM32 → AArch64 translation. Guest code lives in a 32-bit address space; the
 * JIT region holds AArch64 that implements it. See husk-tl-arm32.h.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-arm32.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-dvm-internal.h"
#include "husk-tl-internal.h"
#include "husk-tl-jni.h"
#include "husk-tl-jni-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-xmem.h"

void *tl_nwindow_get(void);

#define A32_TAG  0xA320000000000000ull
#define GSIZE    (256u << 20)
#define GHEAP    0x00100000u
#define GSTACK   (GSIZE - 0x20000u)
#define IMP_BASE 0x0F000000u
#define IMP_SLOT 16u
#define JNI_ENV  0x0F800000u
#define JNI_VM   0x0F810000u
#define SENTINEL 0x0FFF0000u
#define MAX_IMP  1024

enum { R_SP = 13, R_LR = 14, R_PC = 15 };

typedef struct {
    uint32_t r[16];
    uint32_t cpsr;          /* NZCV in 31..28, T in bit 5 */
    uint32_t fpscr;
    uint64_t d[16];
} a32_cpu;

static uint8_t *Gmem;
static size_t Gcap;
static uint32_t Gheap;
static a32_cpu CPU;
static pthread_mutex_t Glk = PTHREAD_MUTEX_INITIALIZER;
static _Thread_local int Gdepth;
static int Gunk;
static uint8_t ITn, ITcond, ITmask;
static int Gnimp;
static void *Gimp_fn[MAX_IMP];
static char Gimp_name[MAX_IMP][96];
static bool Gused;
static int nM;
static void *Hptr[512];
static int nHptr = 1;

static jobj *Ghand[4096];
static int Gnhand = 1;

static uint8_t *host(uint32_t va)
{
    if (!Gmem || va >= Gcap) return NULL;
    return Gmem + va;
}

static uint32_t halloc(uint32_t n)
{
    n = (n + 15u) & ~15u;
    if (Gheap + n >= IMP_BASE) return 0;
    uint32_t p = Gheap; Gheap += n; return p;
}

static uint32_t to_h(jobj *o)
{
    if (!o) return 0;
    for (int i = 1; i < Gnhand; i++) if (Ghand[i] == o) return (uint32_t)i;
    if (Gnhand >= 4096) Gnhand = 1;
    int i = Gnhand++;
    Ghand[i] = o;
    o->refs++;
    return (uint32_t)i;
}
static jobj *from_h(uint32_t h) { return h && h < 4096 ? Ghand[h] : NULL; }

void *tl_a32_sym_tag(uint32_t va) { return va ? (void *)(uintptr_t)(A32_TAG | va) : NULL; }
bool tl_a32_is_fn(const void *p) { return p && ((uintptr_t)p >> 32) == 0xA3200000u; }
uint32_t tl_a32_va(const void *p) { return (uint32_t)(uintptr_t)p; }
bool tl_a32_active(void) { return Gused; }
uint8_t *tl_a32_host(uint32_t va) { return host(va); }
uint32_t tl_a32_alloc(uint32_t n) { return Gmem ? halloc(n) : 0; }
uint32_t tl_a32_intern_host(void *p)
{
    if (!p) return 0;
    for (int i = 1; i < nHptr; i++) if (Hptr[i] == p) return (uint32_t)i;
    if (nHptr >= 512) return 0;
    int i = nHptr++;
    Hptr[i] = p;
    return (uint32_t)i;
}
void *tl_a32_hostptr(uint32_t h) { return h && h < 512 ? Hptr[h] : NULL; }
uint32_t tl_a32_env_va(void) { return JNI_ENV; }
uint32_t tl_a32_vm_va(void) { return JNI_VM; }
uint32_t tl_a32_ref(jobj *o) { return to_h(o); }

void tl_a32_reset(void)
{
    Gheap = GHEAP;
    Gused = false;
    Gnhand = 1;
    nM = 0;
    nHptr = 1;
    memset(Ghand, 0, sizeof(Ghand));
    memset(Hptr, 0, sizeof(Hptr));
    memset(&CPU, 0, sizeof(CPU));
    Gunk = 0; ITn = 0;
}

bool tl_a32_is_elf(const uint8_t *f, size_t n)
{
    return f && n >= 52 && memcmp(f, "\x7f""ELF", 4) == 0 && f[4] == 1 && f[5] == 1
        && f[18] == 40 && f[19] == 0;   /* EM_ARM little-endian ELF32 */
}

/* ------------------------------------------------------------------ imports */

static uint32_t imp_va(int i) { return IMP_BASE + (uint32_t)i * IMP_SLOT; }

static uint32_t load32(uint32_t a);
static int32_t a32_call_inner(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3);

static int intern_imp(const char *name, void *fn, int kind)
{
    (void)kind;
    for (int i = 0; i < Gnimp; i++) if (!strcmp(Gimp_name[i], name)) return i;
    if (Gnimp >= MAX_IMP) { tl_log_line("a32: import table full (%s)", name); return -1; }
    int i = Gnimp++;
    snprintf(Gimp_name[i], sizeof(Gimp_name[0]), "%s", name);
    Gimp_fn[i] = fn;
    return i;
}

static void *wrap_memcpy(uint32_t d, uint32_t s, uint32_t n)
{
    uint8_t *D = host(d), *S = host(s);
    if (D && S) memcpy(D, S, n);
    return (void *)(uintptr_t)d;
}
static void *wrap_memmove(uint32_t d, uint32_t s, uint32_t n)
{
    uint8_t *D = host(d), *S = host(s);
    if (D && S) memmove(D, S, n);
    return (void *)(uintptr_t)d;
}
static void *wrap_memset(uint32_t d, uint32_t c, uint32_t n)
{
    uint8_t *D = host(d); if (D) memset(D, (int)c, n); return (void *)(uintptr_t)d;
}
static uint32_t wrap_strlen(uint32_t s) { const char *p = (const char *)host(s); return p ? (uint32_t)strlen(p) : 0; }
static int wrap_strcmp(uint32_t a, uint32_t b)
{
    const char *A = (const char *)host(a), *B = (const char *)host(b);
    return strcmp(A ? A : "", B ? B : "");
}
static uint32_t wrap_malloc(uint32_t n) { return halloc(n ? n : 1); }
static void wrap_free(uint32_t p) { (void)p; }
static uint32_t wrap_calloc(uint32_t n, uint32_t sz)
{
    uint64_t b = (uint64_t)n * sz; if (b > 0x1000000) return 0;
    uint32_t p = halloc((uint32_t)b); if (p) memset(host(p), 0, (size_t)b); return p;
}
static int wrap_memcmp(uint32_t a, uint32_t b, uint32_t n)
{
    const uint8_t *A = host(a), *B = host(b);
    return (A && B) ? memcmp(A, B, n) : 0;
}
static void wrap_abort(void) { tl_log_line("a32: abort()"); CPU.r[R_PC] = SENTINEL; }
static int wrap_log(int prio, uint32_t tag, uint32_t msg)
{
    (void)prio;
    tl_log_line("a32: %s: %s", host(tag) ? (char *)host(tag) : "?", host(msg) ? (char *)host(msg) : "");
    return 0;
}
static uint32_t wrap_ok(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { (void)a; (void)b; (void)c; (void)d; return 0; }
static uint32_t wrap_aeabi_atexit(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { (void)a; (void)b; (void)c; (void)d; return 0; }
static uint32_t wrap_uidiv(uint32_t n, uint32_t d, uint32_t a, uint32_t b) { (void)a; (void)b; return d ? n / d : 0; }
static uint32_t wrap_uidivmod(uint32_t n, uint32_t d, uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    if (!d) { CPU.r[1] = 0; return 0; }
    CPU.r[1] = n % d;
    return n / d;
}
static uint32_t wrap_idiv(uint32_t n, uint32_t d, uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    if (!d) return 0;
    return (uint32_t)((int32_t)n / (int32_t)d);
}
static uint32_t wrap_idivmod(uint32_t n, uint32_t d, uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    if (!d) { CPU.r[1] = 0; return 0; }
    CPU.r[1] = (uint32_t)((int32_t)n % (int32_t)d);
    return (uint32_t)((int32_t)n / (int32_t)d);
}
static uint32_t wrap_uldivmod(uint32_t nlo, uint32_t nhi, uint32_t dlo, uint32_t dhi)
{
    uint64_t n = ((uint64_t)nhi << 32) | nlo, d = ((uint64_t)dhi << 32) | dlo;
    uint64_t q = d ? n / d : 0, r = d ? n % d : 0;
    CPU.r[1] = (uint32_t)(q >> 32);
    CPU.r[2] = (uint32_t)r;
    CPU.r[3] = (uint32_t)(r >> 32);
    return (uint32_t)q;
}
static uint32_t wrap_memclr(uint32_t d, uint32_t n, uint32_t a, uint32_t b)
{
    (void)a; (void)b; wrap_memset(d, 0, n); return d;
}
static uint32_t wrap_aeabi_memset(uint32_t d, uint32_t n, uint32_t c, uint32_t a)
{
    (void)a; wrap_memset(d, c, n); return d;
}
static uint32_t wrap_strcpy(uint32_t d, uint32_t s, uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    char *D = (char *)host(d); const char *S = (const char *)host(s);
    if (D && S) strcpy(D, S);
    return d;
}
static uint32_t wrap_strncpy(uint32_t d, uint32_t s, uint32_t n, uint32_t a)
{
    (void)a;
    char *D = (char *)host(d); const char *S = (const char *)host(s);
    if (D && S) strncpy(D, S, n);
    return d;
}
static uint32_t wrap_strncmp(uint32_t a, uint32_t b, uint32_t n, uint32_t x)
{
    (void)x;
    const char *A = (const char *)host(a), *B = (const char *)host(b);
    return (uint32_t)strncmp(A ? A : "", B ? B : "", n);
}
static uint32_t wrap_realloc(uint32_t p, uint32_t n, uint32_t a, uint32_t b)
{
    (void)a; (void)b; (void)p;
    return wrap_malloc(n);
}
static uint32_t wrap_pipe(uint32_t out, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    int fd[2];
    if (pipe(fd) != 0) return (uint32_t)-1;
    uint32_t *o = (uint32_t *)host(out);
    if (o) { o[0] = (uint32_t)fd[0]; o[1] = (uint32_t)fd[1]; }
    return 0;
}
static uint32_t wrap_close(uint32_t fd, uint32_t a, uint32_t b, uint32_t c)
{ (void)a; (void)b; (void)c; return (uint32_t)close((int)fd); }
static uint32_t wrap_read(uint32_t fd, uint32_t buf, uint32_t n, uint32_t a)
{
    (void)a;
    uint8_t *p = host(buf);
    return p ? (uint32_t)read((int)fd, p, n) : (uint32_t)-1;
}
static uint32_t wrap_write(uint32_t fd, uint32_t buf, uint32_t n, uint32_t a)
{
    (void)a;
    uint8_t *p = host(buf);
    return p ? (uint32_t)write((int)fd, p, n) : (uint32_t)-1;
}
static uint32_t wrap_fcntl(uint32_t fd, uint32_t cmd, uint32_t arg, uint32_t a)
{
    (void)a;
    return (uint32_t)fcntl((int)fd, (int)cmd, (long)arg);
}

typedef struct { uint32_t pc, arg; } a32_tramp;
static void *a32_thread(void *p);

static uint32_t wrap_pthread_create(uint32_t thr, uint32_t attr, uint32_t start, uint32_t arg)
{
    (void)attr;
    a32_tramp *t = calloc(1, sizeof(*t));
    t->pc = start; t->arg = arg;
    pthread_t pt;
    if (pthread_create(&pt, NULL, a32_thread, t) != 0) { free(t); return 1; }
    uint32_t id = tl_a32_intern_host((void *)pt);
    uint32_t *o = (uint32_t *)host(thr);
    if (o) *o = id;
    return 0;
}
static uint32_t wrap_mutex_init(uint32_t mu, uint32_t attr, uint32_t a, uint32_t b)
{
    (void)attr; (void)a; (void)b;
    pthread_mutex_t *m = calloc(1, sizeof(*m));
    pthread_mutex_init(m, NULL);
    uint32_t *o = (uint32_t *)host(mu);
    if (o) *o = tl_a32_intern_host(m);
    return 0;
}
static pthread_mutex_t *guest_mutex(uint32_t mu)
{
    uint32_t *o = (uint32_t *)host(mu);
    return o ? (pthread_mutex_t *)tl_a32_hostptr(*o) : NULL;
}
static uint32_t wrap_mutex_lock(uint32_t mu, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    pthread_mutex_t *m = guest_mutex(mu);
    return m ? (uint32_t)pthread_mutex_lock(m) : 0;
}
static uint32_t wrap_mutex_unlock(uint32_t mu, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    pthread_mutex_t *m = guest_mutex(mu);
    return m ? (uint32_t)pthread_mutex_unlock(m) : 0;
}
static uint32_t wrap_mutex_destroy(uint32_t mu, uint32_t a, uint32_t b, uint32_t c)
{ (void)mu; (void)a; (void)b; (void)c; return 0; }
static uint32_t wrap_cond_init(uint32_t cv, uint32_t attr, uint32_t a, uint32_t b)
{
    (void)attr; (void)a; (void)b;
    pthread_cond_t *c = calloc(1, sizeof(*c));
    pthread_cond_init(c, NULL);
    uint32_t *o = (uint32_t *)host(cv);
    if (o) *o = tl_a32_intern_host(c);
    return 0;
}
static pthread_cond_t *guest_cond(uint32_t cv)
{
    uint32_t *o = (uint32_t *)host(cv);
    return o ? (pthread_cond_t *)tl_a32_hostptr(*o) : NULL;
}
static uint32_t wrap_cond_signal(uint32_t cv, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    pthread_cond_t *x = guest_cond(cv);
    return x ? (uint32_t)pthread_cond_signal(x) : 0;
}
static uint32_t wrap_cond_broadcast(uint32_t cv, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    pthread_cond_t *x = guest_cond(cv);
    return x ? (uint32_t)pthread_cond_broadcast(x) : 0;
}
static uint32_t wrap_cond_wait(uint32_t cv, uint32_t mu, uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    pthread_cond_t *x = guest_cond(cv);
    pthread_mutex_t *m = guest_mutex(mu);
    if (!x || !m) return 0;
    int depth = Gdepth;
    Gdepth = 0;
    pthread_mutex_unlock(&Glk);
    int r = pthread_cond_wait(x, m);
    pthread_mutex_lock(&Glk);
    Gdepth = depth;
    return (uint32_t)r;
}
static uint32_t wrap_attr_init(uint32_t attr, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    uint32_t *o = (uint32_t *)host(attr);
    if (o) memset(o, 0, 36);
    return 0;
}
static uint32_t wrap_attr_set(uint32_t attr, uint32_t v, uint32_t a, uint32_t b) { (void)attr; (void)v; (void)a; (void)b; return 0; }
static uint32_t wrap_self(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)a; (void)b; (void)c; (void)d;
    return tl_a32_intern_host((void *)pthread_self());
}
static uint32_t wrap_pthread_once(uint32_t once, uint32_t fn, uint32_t a, uint32_t b)
{
    (void)a; (void)b;
    uint32_t *o = (uint32_t *)host(once);
    if (!o || *o) return 0;
    *o = 1;
    if (fn) a32_call_inner(fn, 0, 0, 0, 0);
    return 0;
}

static uint32_t wrap_looper_prepare(uint32_t opts, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    void *(*prep)(int) = tl_bionic_find("ALooper_prepare");
    void *l = prep ? prep((int)opts) : NULL;
    return tl_a32_intern_host(l);
}
static uint32_t wrap_looper_addfd(uint32_t lo, uint32_t fd, uint32_t ident, uint32_t events)
{
    int (*add)(void *, int, int, int, void *, void *) = tl_bionic_find("ALooper_addFd");
    void *l = tl_a32_hostptr(lo);
    /* glue passes callback = NULL and a guest source pointer as data; pollAll returns the ident */
    uint32_t data = load32(CPU.r[R_SP] + 4);
    return add && l ? (uint32_t)add(l, (int)fd, (int)ident, (int)events, NULL, (void *)(uintptr_t)data) : -1u;
}
static uint32_t wrap_looper_poll(uint32_t timeout, uint32_t out_fd, uint32_t out_ev, uint32_t out_data)
{
    int (*poll)(int, int *, int *, void **) = tl_bionic_find("ALooper_pollAll");
    int fd = 0, ev = 0; void *data = NULL;
    int r = poll ? poll((int)timeout, &fd, &ev, &data) : -1;
    uint32_t *a = (uint32_t *)host(out_fd); if (a) *a = (uint32_t)fd;
    uint32_t *b = (uint32_t *)host(out_ev); if (b) *b = (uint32_t)ev;
    uint32_t *c = (uint32_t *)host(out_data); if (c) *c = (uint32_t)(uintptr_t)data;
    return (uint32_t)r;
}
static uint32_t wrap_asset_open(uint32_t am, uint32_t name, uint32_t mode, uint32_t a)
{
    (void)a;
    void *(*open)(void *, const char *, int) = tl_bionic_find("AAssetManager_open");
    const char *n = (const char *)host(name);
    void *mgr = tl_a32_hostptr(am);
    if (!mgr) mgr = tl_bionic_find("AAssetManager_fromJava") ? ((void *(*)(void *, void *))tl_bionic_find("AAssetManager_fromJava"))(tl_jni_env(), NULL) : NULL;
    void *asset = open && n ? open(mgr, n, (int)mode) : NULL;
    return tl_a32_intern_host(asset);
}
static uint32_t wrap_asset_read(uint32_t ast, uint32_t buf, uint32_t n, uint32_t a)
{
    (void)a;
    int (*rd)(void *, void *, size_t) = tl_bionic_find("AAsset_read");
    uint8_t *p = host(buf);
    void *h = tl_a32_hostptr(ast);
    return rd && h && p ? (uint32_t)rd(h, p, n) : 0;
}
static uint32_t wrap_asset_close(uint32_t ast, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    void (*cl)(void *) = tl_bionic_find("AAsset_close");
    void *h = tl_a32_hostptr(ast);
    if (cl && h) cl(h);
    return 0;
}
static uint32_t wrap_asset_len(uint32_t ast, uint32_t a, uint32_t b, uint32_t c)
{
    (void)a; (void)b; (void)c;
    off_t (*ln)(void *) = tl_bionic_find("AAsset_getLength");
    void *h = tl_a32_hostptr(ast);
    return ln && h ? (uint32_t)ln(h) : 0;
}
static uint32_t wrap_egl(const char *name, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    void *fn = tl_egl_resolve(name);
    if (!fn) fn = tl_bionic_find(name);
    if (!fn) { tl_log_line("a32: egl %s missing", name); return 0; }
    if (!strcmp(name, "eglGetDisplay")) {
        void *d = ((void *(*)(void *))fn)((void *)(uintptr_t)r0);
        return tl_a32_intern_host(d);
    }
    if (!strcmp(name, "eglInitialize")) {
        int (*f)(void *, int32_t *, int32_t *) = fn;
        return (uint32_t)f(tl_a32_hostptr(r0), (int32_t *)host(r1), (int32_t *)host(r2));
    }
    if (!strcmp(name, "eglChooseConfig")) {
        int (*f)(void *, const int *, void *, int, int *) = fn;
        void *cfg = NULL;
        int n = 0;
        int ok = f(tl_a32_hostptr(r0), (const int *)host(r1), &cfg, 1, &n);
        uint32_t *out = (uint32_t *)host(r2); if (out && n) *out = tl_a32_intern_host(cfg);
        int32_t *num = (int32_t *)host(load32(CPU.r[R_SP])); if (num) *num = n;
        return (uint32_t)ok;
    }
    if (!strcmp(name, "eglCreateWindowSurface")) {
        void *(*f)(void *, void *, void *, const int *) = fn;
        void *s = f(tl_a32_hostptr(r0), tl_a32_hostptr(r1), tl_a32_hostptr(r2), (const int *)host(r3));
        return tl_a32_intern_host(s);
    }
    if (!strcmp(name, "eglCreateContext")) {
        void *(*f)(void *, void *, void *, const int *) = fn;
        void *s = f(tl_a32_hostptr(r0), tl_a32_hostptr(r1), tl_a32_hostptr(r2), (const int *)host(r3));
        return tl_a32_intern_host(s);
    }
    if (!strcmp(name, "eglMakeCurrent")) {
        int (*f)(void *, void *, void *, void *) = fn;
        uint32_t ctx = load32(CPU.r[R_SP]);
        return (uint32_t)f(tl_a32_hostptr(r0), tl_a32_hostptr(r1), tl_a32_hostptr(r2), tl_a32_hostptr(ctx));
    }
    if (!strcmp(name, "eglSwapBuffers") || !strcmp(name, "eglDestroySurface") || !strcmp(name, "eglDestroyContext")) {
        int (*f)(void *, void *) = fn;
        return (uint32_t)f(tl_a32_hostptr(r0), tl_a32_hostptr(r1));
    }
    if (!strcmp(name, "eglGetProcAddress")) {
        const char *n = (const char *)host(r0);
        void *p = n ? ((void *(*)(const char *))fn)(n) : NULL;
        return tl_a32_intern_host(p);
    }
    typedef uint32_t (*f3)(uint32_t, uint32_t, uint32_t);
    return ((f3)fn)(r0, r1, r2);
}

static void seed_imports(void)
{
    intern_imp("memcpy", (void *)(uintptr_t)wrap_memcpy, 0);
    intern_imp("memmove", (void *)(uintptr_t)wrap_memmove, 0);
    intern_imp("memset", (void *)(uintptr_t)wrap_memset, 0);
    intern_imp("memcmp", (void *)(uintptr_t)wrap_memcmp, 0);
    intern_imp("strlen", (void *)(uintptr_t)wrap_strlen, 0);
    intern_imp("strcmp", (void *)(uintptr_t)wrap_strcmp, 0);
    intern_imp("strcpy", (void *)(uintptr_t)wrap_strcpy, 0);
    intern_imp("strncpy", (void *)(uintptr_t)wrap_strncpy, 0);
    intern_imp("strncmp", (void *)(uintptr_t)wrap_strncmp, 0);
    intern_imp("malloc", (void *)(uintptr_t)wrap_malloc, 0);
    intern_imp("free", (void *)(uintptr_t)wrap_free, 0);
    intern_imp("calloc", (void *)(uintptr_t)wrap_calloc, 0);
    intern_imp("realloc", (void *)(uintptr_t)wrap_realloc, 0);
    intern_imp("abort", (void *)(uintptr_t)wrap_abort, 0);
    intern_imp("__stack_chk_fail", (void *)(uintptr_t)wrap_abort, 0);
    intern_imp("__android_log_print", (void *)(uintptr_t)wrap_log, 0);
    intern_imp("__android_log_write", (void *)(uintptr_t)wrap_log, 0);
    intern_imp("__aeabi_atexit", (void *)(uintptr_t)wrap_aeabi_atexit, 0);
    intern_imp("__cxa_atexit", (void *)(uintptr_t)wrap_aeabi_atexit, 0);
    intern_imp("__cxa_finalize", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pipe", (void *)(uintptr_t)wrap_pipe, 0);
    intern_imp("close", (void *)(uintptr_t)wrap_close, 0);
    intern_imp("read", (void *)(uintptr_t)wrap_read, 0);
    intern_imp("write", (void *)(uintptr_t)wrap_write, 0);
    intern_imp("fcntl", (void *)(uintptr_t)wrap_fcntl, 0);
    intern_imp("pthread_create", (void *)(uintptr_t)wrap_pthread_create, 0);
    intern_imp("pthread_mutex_init", (void *)(uintptr_t)wrap_mutex_init, 0);
    intern_imp("pthread_mutex_lock", (void *)(uintptr_t)wrap_mutex_lock, 0);
    intern_imp("pthread_mutex_unlock", (void *)(uintptr_t)wrap_mutex_unlock, 0);
    intern_imp("pthread_mutex_destroy", (void *)(uintptr_t)wrap_mutex_destroy, 0);
    intern_imp("pthread_cond_init", (void *)(uintptr_t)wrap_cond_init, 0);
    intern_imp("pthread_cond_wait", (void *)(uintptr_t)wrap_cond_wait, 0);
    intern_imp("pthread_cond_signal", (void *)(uintptr_t)wrap_cond_signal, 0);
    intern_imp("pthread_cond_broadcast", (void *)(uintptr_t)wrap_cond_broadcast, 0);
    intern_imp("pthread_cond_destroy", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_mutexattr_init", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_mutexattr_settype", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_mutexattr_destroy", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_attr_init", (void *)(uintptr_t)wrap_attr_init, 0);
    intern_imp("pthread_attr_setdetachstate", (void *)(uintptr_t)wrap_attr_set, 0);
    intern_imp("pthread_attr_setstacksize", (void *)(uintptr_t)wrap_attr_set, 0);
    intern_imp("pthread_attr_setschedparam", (void *)(uintptr_t)wrap_attr_set, 0);
    intern_imp("pthread_attr_destroy", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_attr_getdetachstate", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_self", (void *)(uintptr_t)wrap_self, 0);
    intern_imp("pthread_detach", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_join", (void *)(uintptr_t)wrap_ok, 0);
    intern_imp("pthread_once", (void *)(uintptr_t)wrap_pthread_once, 0);
    intern_imp("ALooper_prepare", (void *)(uintptr_t)wrap_looper_prepare, 0);
    intern_imp("ALooper_pollAll", (void *)(uintptr_t)wrap_looper_poll, 0);
    intern_imp("ALooper_addFd", (void *)(uintptr_t)wrap_looper_addfd, 0);
    intern_imp("AAssetManager_open", (void *)(uintptr_t)wrap_asset_open, 0);
    intern_imp("AAsset_read", (void *)(uintptr_t)wrap_asset_read, 0);
    intern_imp("AAsset_close", (void *)(uintptr_t)wrap_asset_close, 0);
    intern_imp("AAsset_getLength", (void *)(uintptr_t)wrap_asset_len, 0);
    intern_imp("AAsset_getLength64", (void *)(uintptr_t)wrap_asset_len, 0);
    intern_imp("__aeabi_uidiv", (void *)(uintptr_t)wrap_uidiv, 0);
    intern_imp("__aeabi_uidivmod", (void *)(uintptr_t)wrap_uidivmod, 0);
    intern_imp("__aeabi_idiv", (void *)(uintptr_t)wrap_idiv, 0);
    intern_imp("__aeabi_idivmod", (void *)(uintptr_t)wrap_idivmod, 0);
    intern_imp("__aeabi_uldivmod", (void *)(uintptr_t)wrap_uldivmod, 0);
    intern_imp("__aeabi_memcpy", (void *)(uintptr_t)wrap_memcpy, 0);
    intern_imp("__aeabi_memcpy4", (void *)(uintptr_t)wrap_memcpy, 0);
    intern_imp("__aeabi_memcpy8", (void *)(uintptr_t)wrap_memcpy, 0);
    intern_imp("__aeabi_memmove", (void *)(uintptr_t)wrap_memmove, 0);
    intern_imp("__aeabi_memmove4", (void *)(uintptr_t)wrap_memmove, 0);
    intern_imp("__aeabi_memmove8", (void *)(uintptr_t)wrap_memmove, 0);
    intern_imp("__aeabi_memset", (void *)(uintptr_t)wrap_aeabi_memset, 0);
    intern_imp("__aeabi_memset4", (void *)(uintptr_t)wrap_aeabi_memset, 0);
    intern_imp("__aeabi_memset8", (void *)(uintptr_t)wrap_aeabi_memset, 0);
    intern_imp("__aeabi_memclr", (void *)(uintptr_t)wrap_memclr, 0);
    intern_imp("__aeabi_memclr4", (void *)(uintptr_t)wrap_memclr, 0);
    intern_imp("__aeabi_memclr8", (void *)(uintptr_t)wrap_memclr, 0);
}

static void *resolve_imp(const char *name)
{
    for (int i = 0; i < Gnimp; i++) if (!strcmp(Gimp_name[i], name)) return (void *)(uintptr_t)imp_va(i);
    void *fn = (void *)(uintptr_t)wrap_ok;
    if (!strncmp(name, "AAsset", 6) && strstr(name, "open")) fn = (void *)(uintptr_t)wrap_asset_open;
    else if (!strcmp(name, "ALooper_pollOnce")) fn = (void *)(uintptr_t)wrap_looper_poll;
    int i = intern_imp(name, fn, 0);
    if (i >= 0 && strncmp(name, "egl", 3) && strncmp(name, "__aeabi_", 8) && strncmp(name, "pthread_", 8)
        && name[0] != 'A' && strncmp(name, "__cxa_", 6) && strncmp(name, "__gnu_", 6)
        && strncmp(name, "_Unwind", 7))
        tl_log_line("a32: import %s stubbed", name);
    return i >= 0 ? (void *)(uintptr_t)imp_va(i) : NULL;
}

/* ------------------------------------------------------------------ JNI 32-bit */

enum {
    J_GetVersion = 4, J_FindClass = 6, J_ThrowNew = 14, J_ExceptionOccurred = 15,
    J_ExceptionDescribe = 16, J_ExceptionClear = 17, J_ExceptionCheck = 228,
    J_NewGlobalRef = 21, J_DeleteLocalRef = 23, J_IsSameObject = 24,
    J_NewObject = 28, J_GetObjectClass = 31, J_GetMethodID = 33,
    J_CallObjectMethod = 34, J_CallBooleanMethod = 37, J_CallIntMethod = 49, J_CallVoidMethod = 61,
    J_GetFieldID = 94, J_GetObjectField = 95, J_GetIntField = 100, J_SetObjectField = 104,
    J_GetStaticMethodID = 113, J_CallStaticObjectMethod = 114, J_CallStaticIntMethod = 129,
    J_CallStaticVoidMethod = 141, J_GetStaticFieldID = 144, J_GetStaticObjectField = 145,
    J_SetStaticObjectField = 154, J_GetStringUTFChars = 169,
    J_ReleaseStringUTFChars = 170, J_GetArrayLength = 171, J_RegisterNatives = 215,
    J_GetJavaVM = 219, J_NewStringUTF = 167
};

static tl_jmeth *Gmids[1024];
static tl_jfield *Gfids[1024];
static int nGmids = 1, nGfids = 1;
static uint32_t to_mid(tl_jmeth *m)
{
    if (!m) return 0;
    for (int i = 1; i < nGmids; i++) if (Gmids[i] == m) return (uint32_t)i;
    if (nGmids >= 1024) return 0;
    int i = nGmids++; Gmids[i] = m; return (uint32_t)i;
}
static tl_jmeth *from_mid(uint32_t h) { return h && h < 1024 ? Gmids[h] : NULL; }
static uint32_t to_fid(tl_jfield *f)
{
    if (!f) return 0;
    for (int i = 1; i < nGfids; i++) if (Gfids[i] == f) return (uint32_t)i;
    if (nGfids >= 1024) return 0;
    int i = nGfids++; Gfids[i] = f; return (uint32_t)i;
}
static tl_jfield *from_fid(uint32_t h) { return h && h < 1024 ? Gfids[h] : NULL; }

static uint32_t pack_ret(tl_jmeth *m, jvalue r)
{
    if (!m) return 0;
    if (m->retk == 'L' || m->retk == '[') return to_h(r.l);
    if (m->retk == 'J' || m->retk == 'D') { CPU.r[1] = (uint32_t)(r.j >> 32); return (uint32_t)r.j; }
    if (m->retk == 'Z') return r.z;
    if (m->retk == 'V') return 0;
    return (uint32_t)r.i;
}
static uint32_t jni_invoke(int kind, uint32_t *a)
{
    /* kind 0 instance, 2 static. a[1]=obj/class, a[2]=mid, a[3...] args */
    tl_jmeth *m = from_mid(a[2]);
    if (!m) return 0;
    jobj *self = kind == 2 ? NULL : from_h(a[1]);
    jvalue args[16]; memset(args, 0, sizeof(args));
    int ng = 3;
    for (int i = 0; i < m->nargs && i < 16; i++) {
        char k = m->argk[i];
        if (k == 'J' || k == 'D') {
            args[i].j = ((uint64_t)a[ng + 1] << 32) | a[ng]; ng += 2;
        } else if (k == 'L' || k == '[') args[i].l = from_h(a[ng++]);
        else args[i].i = (int32_t)a[ng++];
    }
    return pack_ret(m, tl_jni_invoke(self, m, false, args));
}

static uint32_t jcall(int id, uint32_t *a)
{
    (void)tl_jni_env();
    switch (id) {
    case J_GetVersion: return 0x00010006;
    case J_FindClass: {
        const char *n = (const char *)host(a[1]);
        jobj *c = n ? tl_jni_class_object(n) : NULL;
        if (!c) tl_log_line("a32: FindClass %s failed", n ? n : "?");
        return to_h(c);
    }
    case J_ExceptionCheck: return tl_jni_pending() ? 1 : 0;
    case J_ExceptionClear: tl_jni_clear(); return 0;
    case J_ExceptionOccurred: return to_h(tl_jni_pending_object());
    case J_NewGlobalRef: return a[1];
    case J_DeleteLocalRef: return 0;
    case J_IsSameObject: return a[1] == a[2];
    case J_GetObjectClass: {
        jobj *o = from_h(a[1]);
        return o && o->cls && o->cls->mirror ? to_h(o->cls->mirror) : 0;
    }
    case J_GetMethodID:
    case J_GetStaticMethodID: {
        jobj *c = from_h(a[1]);
        const char *nm = (const char *)host(a[2]), *sg = (const char *)host(a[3]);
        if (!c || c->kind != TL_K_CLASS || !nm || !sg) return 0;
        tl_jmeth *m = tl_jni_method(c->klass.jc, nm, sg, id == J_GetStaticMethodID);
        return to_mid(m);
    }
    case J_GetFieldID:
    case J_GetStaticFieldID: {
        jobj *c = from_h(a[1]);
        const char *nm = (const char *)host(a[2]), *sg = (const char *)host(a[3]);
        if (!c || c->kind != TL_K_CLASS || !nm) return 0;
        tl_jfield *f = tl_jni_field(c->klass.jc, nm, sg ? sg : "", id == J_GetStaticFieldID);
        return to_fid(f);
    }
    case J_CallObjectMethod:
    case J_CallBooleanMethod:
    case J_CallIntMethod:
    case J_CallVoidMethod:
        return jni_invoke(0, a);
    case J_CallStaticObjectMethod:
    case J_CallStaticIntMethod:
    case J_CallStaticVoidMethod:
        return jni_invoke(2, a);
    case J_NewObject: {
        jobj *c = from_h(a[1]);
        if (!c || c->kind != TL_K_CLASS) return 0;
        jobj *o = tl_jni_new_object(c->klass.jc);
        uint32_t saved = a[1];
        a[1] = to_h(o);
        jni_invoke(0, a);
        a[1] = saved;
        return to_h(o);
    }
    case J_ThrowNew: {
        jobj *c = from_h(a[1]);
        const char *msg = (const char *)host(a[2]);
        tl_jni_throw(c && c->kind == TL_K_CLASS ? c->klass.jc->name : "java/lang/Error", msg ? msg : "");
        return 0;
    }
    case J_ExceptionDescribe:
        tl_log_line("a32: JNI exception pending");
        return 0;
    case J_GetObjectField:
    case J_GetIntField: {
        jobj *o = from_h(a[1]); tl_jfield *f = from_fid(a[2]);
        jvalue *s = o && f ? tl_jni_field_slot(o, f) : NULL;
        if (!s) return 0;
        return (id == J_GetObjectField) ? to_h(s->l) : (uint32_t)s->i;
    }
    case J_SetObjectField: {
        jobj *o = from_h(a[1]); tl_jfield *f = from_fid(a[2]);
        jvalue *s = o && f ? tl_jni_field_slot(o, f) : NULL;
        if (s) s->l = from_h(a[3]);
        return 0;
    }
    case J_GetStaticObjectField: {
        tl_jfield *f = from_fid(a[2]);
        jobj *c = from_h(a[1]);
        jvalue *s = c && f ? tl_jni_field_slot(c, f) : NULL;
        return s ? to_h(s->l) : 0;
    }
    case J_SetStaticObjectField: {
        tl_jfield *f = from_fid(a[2]);
        jobj *c = from_h(a[1]);
        jvalue *s = c && f ? tl_jni_field_slot(c, f) : NULL;
        if (s) s->l = from_h(a[3]);
        return 0;
    }
    case J_NewStringUTF: {
        const char *s = (const char *)host(a[1]);
        return to_h(tl_jni_new_string(s ? s : ""));
    }
    case J_GetStringUTFChars: {
        jobj *s = from_h(a[1]);
        const char *u = tl_jni_string(s);
        if (!u) return 0;
        uint32_t p = halloc((uint32_t)strlen(u) + 1);
        if (p) memcpy(host(p), u, strlen(u) + 1);
        return p;
    }
    case J_ReleaseStringUTFChars: return 0;
    case J_GetArrayLength: {
        jobj *o = from_h(a[1]);
        return o && (o->kind == TL_K_PRIM_ARRAY || o->kind == TL_K_OBJ_ARRAY) ? o->arr.len : 0;
    }
    case J_GetJavaVM: {
        uint32_t *out = (uint32_t *)host(a[1]);
        if (out) *out = JNI_VM;
        return 0;
    }
    case J_RegisterNatives: {
        jobj *c = from_h(a[1]);
        uint32_t tbl = a[2]; int n = (int)a[3];
        if (!c || c->kind != TL_K_CLASS) return -1;
        for (int i = 0; i < n; i++) {
            uint32_t *e = (uint32_t *)host(tbl + (uint32_t)i * 12);
            if (!e) continue;
            const char *nm_ = (const char *)host(e[0]);
            const char *sg = (const char *)host(e[1]);
            tl_jni_register_one(c, nm_, sg, tl_a32_sym_tag(e[2]));
        }
        return 0;
    }
    default:
        tl_log_line("a32: JNI #%d not translated yet", id);
        return 0;
    }
}

/* ------------------------------------------------------------------ ELF32 */

typedef struct {
    uint8_t e[16]; uint16_t type, mach; uint32_t ver, entry, phoff, shoff, flags;
    uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
} ehdr32;
typedef struct { uint32_t type, off, vaddr, paddr, filesz, memsz, flags, align; } phdr32;
typedef struct { uint32_t tag, val; } dyn32;
typedef struct { uint32_t name, value, size; uint8_t info, other; uint16_t shndx; } sym32;
typedef struct { uint32_t off, info; } rel32;

static struct a32_mapped {
    tl_lib *L;
    uint32_t strtab, symtab, syment, slide, init, init_array, init_arraysz, nsyms;
} M[64];

static uint32_t lookup_export(const char *nm)
{
    if (!nm || !nm[0]) return 0;
    for (int i = 0; i < nM; i++) {
        uint32_t strtab = M[i].strtab, symtab = M[i].symtab, ent = M[i].syment, slide = M[i].slide;
        uint32_t n = M[i].nsyms ? M[i].nsyms : 65536u;
        for (uint32_t s = 1; s < n; s++) {
            uint8_t *p = host(symtab + s * ent);
            if (!p) break;
            sym32 sy; memcpy(&sy, p, sizeof(sy));
            const char *n2 = sy.name ? (const char *)host(strtab + sy.name) : NULL;
            if (n2 && !strcmp(n2, nm) && sy.shndx) return sy.value + slide;
        }
    }
    return 0;
}

static struct a32_mapped *meta(tl_lib *L)
{
    for (int i = 0; i < nM; i++) if (M[i].L == L) return &M[i];
    return NULL;
}

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static uint16_t rd16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }

static uint32_t gwrite(uint32_t va, const void *src, uint32_t n)
{
    uint8_t *d = host(va); if (!d) return 0; memcpy(d, src, n); return n;
}

bool tl_a32_map(tl_lib *L, const char *name, uint8_t *file, size_t flen)
{
    if (!tl_a32_is_elf(file, flen)) return false;
    if (!Gmem) {
        Gmem = calloc(1, GSIZE);
        if (!Gmem) return false;
        Gcap = GSIZE; Gheap = GHEAP;
        seed_imports();
        /* JNIEnv / JavaVM: pointer to vtable of import-like JNI slots */
        uint32_t env_ptr = JNI_ENV, vt = JNI_ENV + 16;
        uint32_t vm_ptr = JNI_VM, vvt = JNI_VM + 16;
        gwrite(env_ptr, &vt, 4);
        gwrite(vm_ptr, &vvt, 4);
        for (int i = 0; i < 256; i++) {
            uint32_t slot = IMP_BASE + (uint32_t)(200 + i) * IMP_SLOT; /* JNI helpers start at 200 */
            uint32_t *e = (uint32_t *)host(vt + (uint32_t)i * 4);
            if (e) *e = slot;
        }
        uint32_t vmfn = IMP_BASE + 199u * IMP_SLOT;
        uint32_t *ve = (uint32_t *)host(vvt + 4 * 6); /* GetEnv is index 6 on JavaVM */
        if (ve) *ve = vmfn;
    }
    ehdr32 eh; memcpy(&eh, file, sizeof(eh));
    if (eh.phoff + (uint32_t)eh.phnum * eh.phentsize > flen) return false;

    uint32_t lo = ~0u, hi = 0;
    for (int i = 0; i < eh.phnum; i++) {
        phdr32 p; memcpy(&p, file + eh.phoff + (size_t)i * eh.phentsize, sizeof(p));
        if (p.type != 1 || !p.memsz) continue;
        if (p.vaddr < lo) lo = p.vaddr;
        if (p.vaddr + p.memsz > hi) hi = p.vaddr + p.memsz;
    }
    if (lo == ~0u) return false;
    uint32_t need = (hi - (lo & ~0xFFFu)) + 0x1000;
    uint32_t base = halloc(need);
    if (!base) { tl_log_line("a32: no guest space for %s", name); return false; }
    uint32_t slide = base - (lo & ~0xFFFu);

    uint32_t dyn_v = 0, dyn_sz = 0;
    for (int i = 0; i < eh.phnum; i++) {
        phdr32 p; memcpy(&p, file + eh.phoff + (size_t)i * eh.phentsize, sizeof(p));
        if (p.type == 1 && p.filesz) {
            uint32_t va = p.vaddr + slide;
            if (p.off + p.filesz <= flen) gwrite(va, file + p.off, p.filesz);
        } else if (p.type == 2) { dyn_v = p.vaddr + slide; dyn_sz = p.filesz; }
    }

    uint32_t strtab = 0, symtab = 0, syment = 16, rel = 0, relsz = 0, pltrel = 0, jmprel = 0, pltrelsz = 0;
    uint32_t init = 0, init_array = 0, init_arraysz = 0, soname = 0, nsyms = 0;
    uint32_t needed[32]; int nneeded = 0;
    if (dyn_v && dyn_sz) {
        for (uint32_t o = 0; o + 8 <= dyn_sz; o += 8) {
            dyn32 d; memcpy(&d, host(dyn_v + o), 8);
            if (d.tag == 0) break;
            else if (d.tag == 5) strtab = d.val + slide;
            else if (d.tag == 6) symtab = d.val + slide;
            else if (d.tag == 11) syment = d.val;
            else if (d.tag == 17) rel = d.val + slide;
            else if (d.tag == 18) relsz = d.val;
            else if (d.tag == 20) pltrel = d.val;
            else if (d.tag == 23) jmprel = d.val + slide;
            else if (d.tag == 2) pltrelsz = d.val;
            else if (d.tag == 12) init = d.val + slide;
            else if (d.tag == 25) init_array = d.val + slide;
            else if (d.tag == 27) init_arraysz = d.val;
            else if (d.tag == 14) soname = d.val;
            else if (d.tag == 4) {
                uint32_t *h = (uint32_t *)host(d.val + slide);
                if (h) nsyms = h[1];
            }
            else if (d.tag == 1 && nneeded < 32) needed[nneeded++] = d.val;
        }
    }

    /* relocations */
    uint32_t rels[2] = { rel, jmprel }; uint32_t rsz[2] = { relsz, pltrelsz };
    for (int t = 0; t < 2; t++) {
        for (uint32_t o = 0; o + 8 <= rsz[t]; o += 8) {
            rel32 r; memcpy(&r, host(rels[t] + o), 8);
            uint32_t *loc = (uint32_t *)host(r.off + slide);
            if (!loc) continue;
            int type = (int)(r.info & 0xff), symi = (int)(r.info >> 8);
            uint32_t S = 0;
            const char *nm = NULL;
            if (symi && symtab && strtab) {
                sym32 s; memcpy(&s, host(symtab + (uint32_t)symi * syment), sizeof(s));
                nm = (const char *)host(strtab + s.name);
                if (s.shndx) S = s.value + slide;
                else if (nm && nm[0]) {
                    uint32_t ex = lookup_export(nm);
                    if (ex) S = ex;
                    else {
                        void *im = resolve_imp(nm);
                        S = (uint32_t)(uintptr_t)im;
                    }
                }
            }
            switch (type) {
            case 2:  *loc += S; break;                  /* ABS32 */
            case 3:  *loc += S - (r.off + slide); break; /* REL32 */
            case 21: *loc = S; break;                   /* GLOB_DAT */
            case 22: *loc = S; break;                   /* JUMP_SLOT */
            case 23: *loc += slide; break;              /* RELATIVE */
            default: break;
            }
            (void)pltrel;
        }
    }

    if (nM >= 64) return false;
    M[nM] = (struct a32_mapped){
        .L = L, .strtab = strtab, .symtab = symtab, .syment = syment ? syment : 16,
        .slide = slide, .init = init, .init_array = init_array, .init_arraysz = init_arraysz, .nsyms = nsyms,
    };
    nM++;
    Gused = true;
    tl_log_line("a32: mapped %s at guest %#x (%u KiB), %d imports", name, base, need / 1024, Gnimp);
    (void)nneeded; (void)needed; (void)soname;
    return true;
}

void *tl_a32_sym(tl_lib *L, const char *name)
{
    struct a32_mapped *m = meta(L);
    if (!m || !name) return NULL;
    uint32_t strtab = m->strtab, symtab = m->symtab;
    uint32_t ent = m->syment;
    uint32_t slide = m->slide;
    if (!symtab || !strtab) return NULL;
    uint32_t n = m->nsyms ? m->nsyms : 65536u;
    for (uint32_t i = 1; i < n; i++) {
        uint8_t *p = host(symtab + i * ent);
        if (!p) break;
        sym32 s; memcpy(&s, p, sizeof(s));
        const char *nm = s.name ? (const char *)host(strtab + s.name) : NULL;
        if (nm && !strcmp(nm, name) && s.shndx)
            return tl_a32_sym_tag(s.value + slide);
    }
    return NULL;
}

/* ------------------------------------------------------------------ CPU */

static bool cond_ok(uint32_t cond)
{
    uint32_t n = (CPU.cpsr >> 31) & 1, z = (CPU.cpsr >> 30) & 1, c = (CPU.cpsr >> 29) & 1, v = (CPU.cpsr >> 28) & 1;
    switch (cond) {
    case 0: return z; case 1: return !z; case 2: return c; case 3: return !c;
    case 4: return n; case 5: return !n; case 6: return v; case 7: return !v;
    case 8: return c && !z; case 9: return !c || z;
    case 10: return n == v; case 11: return n != v;
    case 12: return !z && n == v; case 13: return z || n != v;
    case 14: return true; default: return false;
    }
}

static uint32_t ror32(uint32_t x, unsigned n) { n &= 31; return n ? (x >> n) | (x << (32 - n)) : x; }

static uint32_t shift_reg(uint32_t val, int type, unsigned amt, int *carry)
{
    switch (type & 3) {
    case 0: /* LSL */
        if (amt == 0) return val;
        if (amt < 32) { if (carry) *carry = (val >> (32 - amt)) & 1; return val << amt; }
        if (carry) *carry = amt == 32 ? (val & 1) : 0; return 0;
    case 1: /* LSR */
        if (amt == 0) amt = 32;
        if (amt < 32) { if (carry) *carry = (val >> (amt - 1)) & 1; return val >> amt; }
        if (carry) *carry = amt == 32 ? (int)(val >> 31) : 0; return 0;
    case 2: /* ASR */
        if (amt == 0) amt = 32;
        if (amt >= 32) { if (carry) *carry = (int)(val >> 31); return (int32_t)val < 0 ? 0xFFFFFFFFu : 0; }
        if (carry) *carry = (val >> (amt - 1)) & 1; return (uint32_t)((int32_t)val >> amt);
    default:
        if (amt == 0) { if (carry) *carry = val & 1; return ((CPU.cpsr >> 29) & 1) << 31 | (val >> 1); }
        if (carry) *carry = (val >> (amt - 1)) & 1; return ror32(val, amt);
    }
}

static void set_nz(uint32_t x)
{
    CPU.cpsr = (CPU.cpsr & 0x3FFFFFFFu) | (x & 0x80000000u) | (x == 0 ? 0x40000000u : 0);
}
static void set_nzcv_add(uint32_t a, uint32_t b, uint32_t r)
{
    uint32_t c = ((uint64_t)a + b) >> 32, v = (~(a ^ b) & (a ^ r)) >> 31;
    CPU.cpsr = (CPU.cpsr & 0x0FFFFFFFu) | (r & 0x80000000u) | (r == 0 ? 0x40000000u : 0) | (c << 29) | (v << 28);
}
static void set_nzcv_sub(uint32_t a, uint32_t b, uint32_t r)
{
    uint32_t c = a >= b, v = ((a ^ b) & (a ^ r)) >> 31;
    CPU.cpsr = (CPU.cpsr & 0x0FFFFFFFu) | (r & 0x80000000u) | (r == 0 ? 0x40000000u : 0) | (c << 29) | (v << 28);
}

static uint32_t load32(uint32_t a) { uint32_t v = 0; uint8_t *p = host(a); if (p) memcpy(&v, p, 4); return v; }
static uint32_t load16(uint32_t a) { uint16_t v = 0; uint8_t *p = host(a); if (p) memcpy(&v, p, 2); return v; }
static uint32_t load8(uint32_t a) { uint8_t *p = host(a); return p ? *p : 0; }
static void store32(uint32_t a, uint32_t v) { uint8_t *p = host(a); if (p) memcpy(p, &v, 4); }
static void store16(uint32_t a, uint32_t v) { uint8_t *p = host(a); uint16_t x = (uint16_t)v; if (p) memcpy(p, &x, 2); }
static void store8(uint32_t a, uint32_t v) { uint8_t *p = host(a); if (p) *p = (uint8_t)v; }

static uint32_t pc_val(void)
{
    /* After the decoder has already advanced PC to the next instruction. */
    return CPU.r[R_PC] + ((CPU.cpsr & 0x20) ? 2 : 4);
}

static void do_import(int idx)
{
    if (idx == 199) {
        uint32_t *out = (uint32_t *)host(CPU.r[1]);
        if (out) *out = JNI_ENV;
        CPU.r[0] = 0;
        CPU.r[R_PC] = CPU.r[R_LR];
        return;
    }
    if (idx >= 200) {
        /* JNI: r0 = env, r1.. args. id = idx - 200 */
        uint32_t args[8] = { CPU.r[0], CPU.r[1], CPU.r[2], CPU.r[3], load32(CPU.r[R_SP]), load32(CPU.r[R_SP]+4), load32(CPU.r[R_SP]+8), load32(CPU.r[R_SP]+12) };
        CPU.r[0] = jcall(idx - 200, args);
        CPU.r[R_PC] = CPU.r[R_LR];
        return;
    }
    if (idx < 0 || idx >= Gnimp) { CPU.r[R_PC] = CPU.r[R_LR]; return; }
    const char *nm = Gimp_name[idx];
    uint32_t r0 = CPU.r[0], r1 = CPU.r[1], r2 = CPU.r[2], r3 = CPU.r[3];
    typedef uint32_t (*f3)(uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*f1)(uint32_t);
    typedef uint32_t (*imp4)(uint32_t, uint32_t, uint32_t, uint32_t);
    typedef void (*f0)(void);
    if (!strncmp(nm, "egl", 3)) CPU.r[0] = wrap_egl(nm, r0, r1, r2, r3);
    else if (!strcmp(nm, "memcpy") || !strcmp(nm, "memmove") || !strcmp(nm, "memset")
             || !strncmp(nm, "__aeabi_memcpy", 14) || !strncmp(nm, "__aeabi_memmove", 15))
        CPU.r[0] = ((f3)Gimp_fn[idx])(r0, r1, r2);
    else if (!strcmp(nm, "strlen")) CPU.r[0] = ((f1)Gimp_fn[idx])(r0);
    else if (!strcmp(nm, "strcmp") || !strcmp(nm, "memcmp"))
        CPU.r[0] = (uint32_t)((int (*)(uint32_t, uint32_t, uint32_t))Gimp_fn[idx])(r0, r1, r2);
    else if (!strcmp(nm, "malloc")) CPU.r[0] = ((f1)Gimp_fn[idx])(r0);
    else if (!strcmp(nm, "free")) ((f1)Gimp_fn[idx])(r0);
    else if (!strcmp(nm, "calloc")) CPU.r[0] = ((f3)Gimp_fn[idx])(r0, r1, 0);
    else if (!strcmp(nm, "abort") || !strcmp(nm, "__stack_chk_fail")) ((f0)Gimp_fn[idx])();
    else if (!strncmp(nm, "__android_log", 13)) CPU.r[0] = (uint32_t)wrap_log((int)r0, r1, r2);
    else {
        void *h = Gimp_fn[idx];
        if (h && h != wrap_abort) CPU.r[0] = ((imp4)h)(r0, r1, r2, r3);
        else tl_log_line("a32: import %s unhooked", nm);
    }
    CPU.r[R_PC] = CPU.r[R_LR];
}

static int exec_arm(uint32_t insn);
static int exec_thumb(void);

/* 32-bit Thumb first half: 11101…, 11110…, or 1111100x / 1111101x (F800–FBFF).
 * 0xFCxx–0xFFxx is the second half of BL, not a new 32-bit insn. */
static int thumb_is_32(uint16_t h)
{
    if ((h & 0xF800) == 0xE800 || (h & 0xF800) == 0xF000) return 1;
    if ((h & 0xFC00) == 0xF800) return 1;
    return 0;
}
static int thumb_width(uint32_t pc)
{
    return thumb_is_32((uint16_t)load16(pc)) ? 4 : 2;
}

static int it_advance_and_skip(uint32_t pc)
{
    if (!ITn) return 0;
    int skip = !cond_ok(ITcond);
    ITn--;
    if (ITn) {
        ITcond = (uint8_t)((ITcond & 0xE) | ((ITmask >> 4) & 1));
        ITmask = (uint8_t)(ITmask << 1);
    }
    if (!skip) return 0;
    CPU.r[R_PC] = pc + (uint32_t)thumb_width(pc);
    return 1;
}

static int exec_one(void)
{
    uint32_t pc = CPU.r[R_PC];
    if (pc == SENTINEL || pc == 0) return 0;
    if (pc >= IMP_BASE && pc < JNI_ENV) {
        int idx = (int)((pc - IMP_BASE) / IMP_SLOT);
        do_import(idx);
        return 1;
    }
    if (CPU.cpsr & 0x20) {
        if (it_advance_and_skip(pc)) { Gunk = 0; return 1; }
        int before = Gunk;
        int r = exec_thumb();
        if (r && Gunk == before) Gunk = 0;
        return r;
    }
    uint32_t insn = load32(pc);
    CPU.r[R_PC] = pc + 4;
    int before = Gunk;
    int r = exec_arm(insn);
    if (r && Gunk == before) Gunk = 0;
    return r;
}

static int exec_arm(uint32_t insn)
{
    unsigned cond = insn >> 28;
    if (cond != 0xE && cond != 0xF && !cond_ok(cond)) return 1;
    /* branch */
    if ((insn & 0x0E000000u) == 0x0A000000u && cond != 0xF) {
        int32_t off = ((int32_t)(insn << 8) >> 6);
        if (insn & (1u << 24)) CPU.r[R_LR] = CPU.r[R_PC];
        CPU.r[R_PC] = (pc_val() + (uint32_t)off) & ~1u;
        return 1;
    }
    /* bx / blx reg */
    if ((insn & 0x0FFFFFF0u) == 0x012FFF10u) {
        uint32_t t = CPU.r[insn & 15];
        if ((insn & 0xF0) == 0x30) CPU.r[R_LR] = CPU.r[R_PC]; /* blx */
        CPU.cpsr = (CPU.cpsr & ~0x20u) | ((t & 1) << 5);
        CPU.r[R_PC] = t & ~1u;
        return 1;
    }
    /* blx imm */
    if ((insn & 0xFE000000u) == 0xFA000000u) {
        int32_t off = ((int32_t)(insn << 8) >> 6) | ((insn >> 23) & 2);
        CPU.r[R_LR] = CPU.r[R_PC];
        CPU.cpsr |= 0x20;
        CPU.r[R_PC] = (pc_val() + (uint32_t)off) & ~1u;
        return 1;
    }
    /* svc: Linux syscalls are not implemented; counting them as success reset the
     * stuck-insn abort and left constructors spinning for minutes. */
    if ((insn & 0x0F000000u) == 0x0F000000u) {
        if (++Gunk < 8) tl_log_line("a32: svc #%u", insn & 0xFFFFFFu);
        if (Gunk > 32) { CPU.r[R_PC] = SENTINEL; return 0; }
        return 1;
    }
    /* ldr/str */
    if ((insn & 0x0C000000u) == 0x04000000u) {
        int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
        int p = (insn >> 24) & 1, u = (insn >> 23) & 1, b = (insn >> 22) & 1, w = (insn >> 21) & 1, l = (insn >> 20) & 1;
        uint32_t off, base = (rn == 15) ? pc_val() : CPU.r[rn];
        if (insn & (1u << 25)) {
            unsigned sh = (insn >> 5) & 3, amt = (insn >> 7) & 31;
            off = shift_reg(CPU.r[insn & 15], (int)sh, amt, NULL);
        } else off = insn & 0xFFF;
        uint32_t addr = p ? (u ? base + off : base - off) : base;
        if (l) {
            uint32_t v = b ? load8(addr) : load32(addr);
            if (rd == 15) { CPU.cpsr = (CPU.cpsr & ~0x20u) | ((v & 1) << 5); CPU.r[R_PC] = v & ~1u; }
            else CPU.r[rd] = v;
        } else {
            uint32_t v = (rd == 15) ? pc_val() : CPU.r[rd];
            if (b) store8(addr, v); else store32(addr, v);
        }
        if (!p) addr = u ? base + off : base - off;
        if ((!p || w) && rn != 15) CPU.r[rn] = addr;
        return 1;
    }
    /* extra ld/st half */
    if ((insn & 0x0E000090u) == 0x00000090u && ((insn >> 5) & 3)) {
        int rn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
        int p = (insn >> 24) & 1, u = (insn >> 23) & 1, w = (insn >> 21) & 1, l = (insn >> 20) & 1;
        int op = (insn >> 5) & 3;
        uint32_t off = (insn & (1u << 22)) ? ((insn >> 4) & 0xF0) | (insn & 0xF) : CPU.r[insn & 15];
        uint32_t base = (rn == 15) ? pc_val() : CPU.r[rn];
        uint32_t addr = p ? (u ? base + off : base - off) : base;
        if (l) {
            if (op == 1) CPU.r[rd] = load16(addr);
            else if (op == 2) CPU.r[rd] = (int32_t)(int8_t)load8(addr);
            else CPU.r[rd] = (int32_t)(int16_t)load16(addr);
        } else if (op == 1) store16(addr, CPU.r[rd]);
        if (!p) addr = u ? base + off : base - off;
        if ((!p || w) && rn != 15) CPU.r[rn] = addr;
        return 1;
    }
    /* ldm/stm */
    if ((insn & 0x0E000000u) == 0x08000000u) {
        int rn = (insn >> 16) & 15, p = (insn >> 24) & 1, u = (insn >> 23) & 1, w = (insn >> 21) & 1, l = (insn >> 20) & 1;
        uint32_t addr = CPU.r[rn], list = insn & 0xFFFF;
        int n = 0; for (int i = 0; i < 16; i++) if (list & (1u << i)) n++;
        uint32_t start = u ? (p ? addr + 4 : addr) : (p ? addr - (uint32_t)n * 4 : addr - (uint32_t)n * 4 + 4);
        if (!u && p) start = addr - (uint32_t)n * 4;
        if (!u && !p) start = addr - (uint32_t)n * 4 + 4;
        if (u && p) start = addr + 4;
        if (u && !p) start = addr;
        uint32_t a = start;
        for (int i = 0; i < 16; i++) if (list & (1u << i)) {
            if (l) {
                uint32_t v = load32(a);
                if (i == 15) { CPU.cpsr = (CPU.cpsr & ~0x20u) | ((v & 1) << 5); CPU.r[15] = v & ~1u; }
                else CPU.r[i] = v;
            } else store32(a, CPU.r[i]);
            a += 4;
        }
        if (w) CPU.r[rn] = u ? CPU.r[rn] + (uint32_t)n * 4 : CPU.r[rn] - (uint32_t)n * 4;
        return 1;
    }
    /* mul */
    if ((insn & 0x0FC000F0u) == 0x00000090u) {
        int rd = (insn >> 16) & 15, rn = (insn >> 12) & 15, rs = (insn >> 8) & 15, rm = insn & 15;
        uint32_t r = CPU.r[rm] * CPU.r[rs] + ((insn & (1u << 21)) ? CPU.r[rn] : 0);
        CPU.r[rd] = r; if (insn & (1u << 20)) set_nz(r); return 1;
    }
    /* data processing */
    if ((insn & 0x0C000000u) == 0) {
        int opc = (insn >> 21) & 15, s = (insn >> 20) & 1, rn = (insn >> 16) & 15, rd = (insn >> 12) & 15;
        int carry = (CPU.cpsr >> 29) & 1;
        uint32_t op2, a = (rn == 15) ? pc_val() : CPU.r[rn];
        if (insn & (1u << 25)) {
            unsigned rot = ((insn >> 8) & 15) * 2;
            op2 = ror32(insn & 0xFF, rot);
            if (rot) carry = (int)(op2 >> 31);
        } else {
            unsigned sh = (insn >> 5) & 3, rm = insn & 15;
            uint32_t val = (rm == 15) ? pc_val() : CPU.r[rm];
            unsigned amt = (insn & (1u << 4)) ? (CPU.r[(insn >> 8) & 15] & 0xFF) : ((insn >> 7) & 31);
            op2 = shift_reg(val, (int)sh, amt, &carry);
        }
        uint32_t r = 0; int logic = 0;
        switch (opc) {
        case 0: r = a & op2; logic = 1; break;
        case 1: r = a ^ op2; logic = 1; break;
        case 2: r = a - op2; if (s) set_nzcv_sub(a, op2, r); break;
        case 3: r = op2 - a; if (s) set_nzcv_sub(op2, a, r); break;
        case 4: r = a + op2; if (s) set_nzcv_add(a, op2, r); break;
        case 5: r = a + op2 + ((CPU.cpsr >> 29) & 1); if (s) set_nzcv_add(a, op2, r); break;
        case 6: r = a - op2 - !((CPU.cpsr >> 29) & 1); if (s) set_nzcv_sub(a, op2, r); break;
        case 7: r = op2 - a - !((CPU.cpsr >> 29) & 1); break;
        case 8: r = a & op2; logic = 1; rd = 16; break; /* TST */
        case 9: r = a ^ op2; logic = 1; rd = 16; break;
        case 10: r = a - op2; if (s) set_nzcv_sub(a, op2, r); rd = 16; break;
        case 11: r = a + op2; if (s) set_nzcv_add(a, op2, r); rd = 16; break;
        case 12: r = a | op2; logic = 1; break;
        case 13: r = op2; logic = 1; break;
        case 14: r = a & ~op2; logic = 1; break;
        case 15: r = ~op2; logic = 1; break;
        }
        if (rd < 16) {
            if (rd == 15) { CPU.cpsr = (CPU.cpsr & ~0x20u) | ((r & 1) << 5); CPU.r[15] = r & ~1u; }
            else CPU.r[rd] = r;
        }
        if (s && logic) { set_nz(r); CPU.cpsr = (CPU.cpsr & ~(1u << 29)) | ((uint32_t)carry << 29); }
        return 1;
    }
    /* movw / movt */
    if ((insn & 0x0FF00000u) == 0x03000000u || (insn & 0x0FF00000u) == 0x03400000u) {
        int rd = (insn >> 12) & 15;
        uint32_t imm = (insn & 0xFFF) | ((insn >> 4) & 0xF000);
        if ((insn & 0x0FF00000u) == 0x03400000u) CPU.r[rd] = (CPU.r[rd] & 0xFFFF) | (imm << 16);
        else CPU.r[rd] = imm;
        return 1;
    }
    if (++Gunk < 8) tl_log_line("a32: arm %#x at %#x not translated", insn, CPU.r[R_PC] - 4);
    if (Gunk > 32) { CPU.r[R_PC] = SENTINEL; return 0; }
    return 1;
}

static int exec_thumb(void)
{
    uint32_t pc = CPU.r[R_PC];
    uint16_t h = (uint16_t)load16(pc);
    /* 32-bit thumb */
    if (thumb_is_32(h)) {
        uint16_t l = (uint16_t)load16(pc + 2);
        uint32_t w = ((uint32_t)h << 16) | l;
        CPU.r[R_PC] = pc + 4;
        /* bl / blx */
        if ((h & 0xF800) == 0xF000 && ((l & 0xC000) == 0xC000 || (l & 0xD000) == 0xC000)) {
            uint32_t s = (h >> 10) & 1, j1 = (l >> 13) & 1, j2 = (l >> 11) & 1;
            uint32_t i1 = !(j1 ^ s), i2 = !(j2 ^ s);
            int32_t off = (int32_t)((s << 24) | (i1 << 23) | (i2 << 22) | ((h & 0x3FF) << 12) | ((l & 0x7FF) << 1));
            off = (off << 7) >> 7;
            CPU.r[R_LR] = (pc + 4) | 1;
            if (!(l & (1u << 12))) { CPU.cpsr &= ~0x20u; CPU.r[R_PC] = (pc + 4 + (uint32_t)off) & ~3u; }
            else CPU.r[R_PC] = (pc + 4 + (uint32_t)off) & ~1u;
            return 1;
        }
        /* b.w */
        if ((h & 0xF800) == 0xF000 && (l & 0xD000) == 0x9000) {
            uint32_t s = (h >> 10) & 1, j1 = (l >> 13) & 1, j2 = (l >> 11) & 1;
            uint32_t i1 = !(j1 ^ s), i2 = !(j2 ^ s);
            int32_t off = (int32_t)((s << 24) | (i1 << 23) | (i2 << 22) | ((h & 0x3FF) << 12) | ((l & 0x7FF) << 1));
            off = (off << 7) >> 7;
            CPU.r[R_PC] = (pc + 4 + (uint32_t)off) & ~1u;
            return 1;
        }
        /* movw/movt */
        if ((h & 0xFBF0) == 0xF240 || (h & 0xFBF0) == 0xF2C0) {
            int rd = (l >> 8) & 0xF;
            uint32_t imm = ((h & 0xF) << 12) | ((h & 0x0400) << 1) | ((l >> 4) & 0xF00) | (l & 0xFF);
            if (h & 0x0080) CPU.r[rd] = (CPU.r[rd] & 0xFFFF) | (imm << 16);
            else CPU.r[rd] = imm;
            return 1;
        }
        /* ldr.w [pc, #imm] */
        if ((h & 0xFF7F) == 0xF85F) {
            int rt = (l >> 12) & 0xF; uint32_t imm = l & 0xFFF;
            uint32_t addr = (pc + 4) & ~3u;
            addr = (h & 0x80) ? addr + imm : addr - imm;
            CPU.r[rt] = load32(addr);
            return 1;
        }
        /* dmb / dsb / isb */
        if ((h & 0xFFF0) == 0xF3B0) return 1;
        /* ldrd / strd  1110100 P U 1 W L Rn */
        if ((h & 0xFE40) == 0xE840) {
            int rn = h & 0xF, P = (h >> 8) & 1, U = (h >> 7) & 1, W = (h >> 5) & 1, Lbit = (h >> 4) & 1;
            int rt = (l >> 12) & 0xF, rt2 = (l >> 8) & 0xF;
            uint32_t imm = (uint32_t)(l & 0xFF) * 4, base = CPU.r[rn];
            uint32_t addr = P ? (U ? base + imm : base - imm) : base;
            if (Lbit) { CPU.r[rt] = load32(addr); CPU.r[rt2] = load32(addr + 4); }
            else { store32(addr, CPU.r[rt]); store32(addr + 4, CPU.r[rt2]); }
            if ((!P || W) && rn != 15) CPU.r[rn] = U ? base + imm : base - imm;
            return 1;
        }
        /* ldmia/stmia.w 1110100010WL, ldmdb/stmdb.w 1110100100WL (push.w / pop.w) */
        if ((h & 0xFF80) == 0xE880 || (h & 0xFF80) == 0xE900) {
            int rn = h & 0xF, W = (h >> 5) & 1, Lbit = (h >> 4) & 1, up = ((h & 0xFF80) == 0xE880);
            uint32_t list = l, n = 0;
            for (int i = 0; i < 16; i++) if (list & (1u << i)) n++;
            uint32_t a = up ? CPU.r[rn] : CPU.r[rn] - n * 4;
            for (int i = 0; i < 16; i++) if (list & (1u << i)) {
                if (Lbit) {
                    uint32_t v = load32(a);
                    if (i == 15) { CPU.cpsr = (CPU.cpsr & ~0x20u) | ((v & 1) << 5); CPU.r[15] = v & ~1u; }
                    else CPU.r[i] = v;
                } else store32(a, CPU.r[i]);
                a += 4;
            }
            if (W) CPU.r[rn] = up ? CPU.r[rn] + n * 4 : CPU.r[rn] - n * 4;
            return 1;
        }
        /* ldr/str.w [Rn, Rm, lsl #shift]  (bit7 of hw1 clear) */
        if ((h & 0xFE80) == 0xF800 && (l & 0x0FC0) == 0) {
            int rn = h & 0xF, rt = (l >> 12) & 0xF, rm = l & 0xF;
            int Lbit = (h >> 4) & 1, size = (h >> 5) & 3, sh = (l >> 4) & 3;
            uint32_t addr = CPU.r[rn] + (CPU.r[rm] << sh);
            if (Lbit) {
                if (size == 2) CPU.r[rt] = load32(addr);
                else if (size == 1) CPU.r[rt] = load16(addr);
                else CPU.r[rt] = load8(addr);
            } else {
                if (size == 2) store32(addr, CPU.r[rt]);
                else if (size == 1) store16(addr, CPU.r[rt]);
                else store8(addr, CPU.r[rt]);
            }
            return 1;
        }
        /* ldr/str.w [Rn, #imm12]  (bit7 of hw1 set: F88..F8D) */
        if ((h & 0xFE80) == 0xF880) {
            int rn = h & 0xF, rt = (l >> 12) & 0xF, Lbit = (h >> 4) & 1;
            uint32_t imm = l & 0xFFF, addr = CPU.r[rn] + imm;
            int size = (h >> 5) & 3;
            if (Lbit) {
                if (size == 2) CPU.r[rt] = load32(addr);
                else if (size == 1) CPU.r[rt] = load16(addr);
                else CPU.r[rt] = load8(addr);
            } else {
                if (size == 2) store32(addr, CPU.r[rt]);
                else if (size == 1) store16(addr, CPU.r[rt]);
                else store8(addr, CPU.r[rt]);
            }
            return 1;
        }
        /* data-processing w/ modified imm */
        if ((h & 0xFA00) == 0xF000) {
            int rd = (l >> 8) & 0xF, rn = h & 0xF, op = (h >> 4) & 0xF;
            uint32_t imm8 = l & 0xFF, i = (h >> 10) & 1, imm3 = (l >> 12) & 7;
            uint32_t imm12 = (i << 11) | (imm3 << 8) | imm8, op2;
            if ((imm12 >> 10) == 0) {
                unsigned t = (imm12 >> 8) & 3;
                if (t == 0) op2 = imm8;
                else if (t == 1) op2 = imm8 | (imm8 << 16);
                else if (t == 2) op2 = (imm8 << 8) | (imm8 << 24);
                else op2 = imm8 * 0x01010101u;
            } else {
                unsigned rot = (imm12 >> 7) & 0x1F; uint32_t un = (1u << 7) | (imm12 & 0x7F);
                op2 = ror32(un, rot);
            }
            uint32_t a = CPU.r[rn], r = 0;
            if (op == 2) r = a - op2;
            else if (op == 4) r = a + op2;
            else if (op == 0) r = a & op2;
            else if (op == 8) r = a ^ op2;
            else if (op == 10) r = a | op2;
            else if (op == 13) r = op2; /* MOV */
            else if (op == 3) r = op2 - a;
            else { tl_log_line("a32: thumb2 dp op %d at %#x", op, pc); r = op2; }
            if (rd != 15) CPU.r[rd] = r;
            return 1;
        }
        if (++Gunk < 8) tl_log_line("a32: thumb32 %#x at %#x not translated", w, pc);
        if (Gunk > 32) { CPU.r[R_PC] = SENTINEL; return 0; }
        return 1;
    }
    CPU.r[R_PC] = pc + 2;
    /* cbz / cbnz */
    if ((h & 0xF500) == 0xB100) {
        int rn = h & 7, nz = (h >> 11) & 1;
        uint32_t imm = (uint32_t)(((h >> 3) & 0x1F) | ((h >> 4) & 0x40)) << 1;
        int z = CPU.r[rn] == 0;
        if (nz ? !z : z) CPU.r[R_PC] = pc + 4 + imm;
        return 1;
    }
    /* it */
    if ((h & 0xFF00) == 0xBF00 && (h & 0xF)) {
        ITcond = (uint8_t)((h >> 4) & 0xF);
        ITmask = (uint8_t)((h & 0xF) << 4);
        ITn = 0;
        uint8_t m = (uint8_t)(h & 0xF);
        while (m) { ITn++; m = (uint8_t)((m << 1) & 0xF); }
        return 1;
    }
    /* 16-bit */
    if ((h & 0xF800) == 0x0000 || (h & 0xF800) == 0x0800 || (h & 0xF800) == 0x1000) {
        int rd = h & 7, rm = (h >> 3) & 7, amt = (h >> 6) & 31, op = (h >> 11) & 3;
        int c = 0; CPU.r[rd] = shift_reg(CPU.r[rm], op == 0 ? 0 : op == 1 ? 1 : 2, amt ? amt : (op ? 32 : 0), &c);
        set_nz(CPU.r[rd]); return 1;
    }
    if ((h & 0xFC00) == 0x1800) {
        int rd = h & 7, rn = (h >> 3) & 7, rm = (h >> 6) & 7;
        uint32_t r = (h & 0x200) ? CPU.r[rn] - CPU.r[rm] : CPU.r[rn] + CPU.r[rm];
        if (h & 0x200) set_nzcv_sub(CPU.r[rn], CPU.r[rm], r); else set_nzcv_add(CPU.r[rn], CPU.r[rm], r);
        CPU.r[rd] = r; return 1;
    }
    if ((h & 0xFC00) == 0x1C00) {
        int rd = h & 7, rn = (h >> 3) & 7, imm = (h >> 6) & 7;
        uint32_t r = (h & 0x200) ? CPU.r[rn] - imm : CPU.r[rn] + imm;
        if (h & 0x200) set_nzcv_sub(CPU.r[rn], imm, r); else set_nzcv_add(CPU.r[rn], imm, r);
        CPU.r[rd] = r; return 1;
    }
    if ((h & 0xE000) == 0x2000) {
        int rd = (h >> 8) & 7, imm = h & 0xFF, op = (h >> 11) & 3;
        if (op == 0) { CPU.r[rd] = imm; set_nz(imm); }
        else if (op == 1) { uint32_t r = CPU.r[rd] - imm; set_nzcv_sub(CPU.r[rd], imm, r); }
        else if (op == 2) { uint32_t r = CPU.r[rd] + imm; set_nzcv_add(CPU.r[rd], imm, r); CPU.r[rd] = r; }
        else { uint32_t r = CPU.r[rd] - imm; set_nzcv_sub(CPU.r[rd], imm, r); CPU.r[rd] = r; }
        return 1;
    }
    if ((h & 0xFC00) == 0x4000) {
        int rd = h & 7, rm = (h >> 3) & 7, op = (h >> 6) & 15;
        uint32_t a = CPU.r[rd], b = CPU.r[rm], r = 0;
        switch (op) {
        case 0: r = a & b; break; case 1: r = a ^ b; break;
        case 2: r = shift_reg(a, 0, b & 0xFF, NULL); break;
        case 3: r = shift_reg(a, 1, b & 0xFF, NULL); break;
        case 4: r = shift_reg(a, 2, b & 0xFF, NULL); break;
        case 5: r = a + b + ((CPU.cpsr >> 29) & 1); break;
        case 6: r = a - b - !((CPU.cpsr >> 29) & 1); break;
        case 7: r = shift_reg(a, 3, b & 0xFF, NULL); break;
        case 8: r = a & b; set_nz(r); return 1;
        case 9: r = -b; break;
        case 10: r = a - b; set_nzcv_sub(a, b, r); return 1;
        case 11: r = a + b; set_nzcv_add(a, b, r); return 1;
        case 12: r = a | b; break; case 13: r = a * b; break;
        case 14: r = a & ~b; break; case 15: r = ~b; break;
        }
        CPU.r[rd] = r; set_nz(r); return 1;
    }
    if ((h & 0xFC00) == 0x4400) {
        int rd = (h & 7) | ((h >> 4) & 8), rm = (h >> 3) & 15, op = (h >> 8) & 3;
        uint32_t a = (rd == 15) ? pc_val() : CPU.r[rd], b = (rm == 15) ? pc_val() : CPU.r[rm];
        if (op == 0) CPU.r[rd] = a + b;
        else if (op == 1) { uint32_t r = a - b; set_nzcv_sub(a, b, r); }
        else if (op == 2) { if (rd == 15) { CPU.cpsr = (CPU.cpsr & ~0x20u) | ((b & 1) << 5); CPU.r[15] = b & ~1u; } else CPU.r[rd] = b; }
        else { CPU.cpsr = (CPU.cpsr & ~0x20u) | ((b & 1) << 5); CPU.r[R_PC] = b & ~1u; }
        return 1;
    }
    if ((h & 0xF800) == 0x4800) { CPU.r[(h >> 8) & 7] = load32(((pc + 4) & ~3u) + (uint32_t)(h & 0xFF) * 4); return 1; }
    if ((h & 0xF200) == 0x5000 || (h & 0xF200) == 0x5200) {
        int rd = h & 7, rn = (h >> 3) & 7, rm = (h >> 6) & 7, l = (h >> 11) & 1, b = (h >> 10) & 1;
        uint32_t addr = CPU.r[rn] + CPU.r[rm];
        if (l) CPU.r[rd] = b ? load8(addr) : load32(addr);
        else if (b) store8(addr, CPU.r[rd]); else store32(addr, CPU.r[rd]);
        return 1;
    }
    if ((h & 0xE000) == 0x6000) {
        int rd = h & 7, rn = (h >> 3) & 7, imm = ((h >> 6) & 31) * ((h & 0x1000) ? 1 : 4);
        int l = (h >> 11) & 1, b = (h >> 12) & 1;
        uint32_t addr = CPU.r[rn] + (uint32_t)imm;
        if (l) CPU.r[rd] = b ? load8(addr) : load32(addr);
        else if (b) store8(addr, CPU.r[rd]); else store32(addr, CPU.r[rd]);
        return 1;
    }
    if ((h & 0xF000) == 0x8000) {
        int rd = h & 7, rn = (h >> 3) & 7, imm = ((h >> 6) & 31) * 2, l = (h >> 11) & 1;
        uint32_t addr = CPU.r[rn] + (uint32_t)imm;
        if (l) CPU.r[rd] = load16(addr); else store16(addr, CPU.r[rd]);
        return 1;
    }
    if ((h & 0xF000) == 0x9000) {
        int rd = (h >> 8) & 7, l = (h >> 11) & 1; uint32_t addr = CPU.r[R_SP] + (uint32_t)(h & 0xFF) * 4;
        if (l) CPU.r[rd] = load32(addr); else store32(addr, CPU.r[rd]);
        return 1;
    }
    if ((h & 0xF000) == 0xA000) {
        int rd = (h >> 8) & 7;
        CPU.r[rd] = ((h & 0x800) ? CPU.r[R_SP] : ((pc + 4) & ~3u)) + (uint32_t)(h & 0xFF) * 4;
        return 1;
    }
    if ((h & 0xFF00) == 0xB000) {
        uint32_t imm = (uint32_t)(h & 0x7F) * 4;
        CPU.r[R_SP] = (h & 0x80) ? CPU.r[R_SP] - imm : CPU.r[R_SP] + imm;
        return 1;
    }
    if ((h & 0xF600) == 0xB400) {
        unsigned list = h & 0xFF; int lr = (h >> 8) & 1, l = (h >> 11) & 1;
        if (!l) {
            if (lr) { CPU.r[R_SP] -= 4; store32(CPU.r[R_SP], CPU.r[R_LR]); }
            for (int i = 7; i >= 0; i--) if (list & (1u << i)) { CPU.r[R_SP] -= 4; store32(CPU.r[R_SP], CPU.r[i]); }
        } else {
            for (int i = 0; i < 8; i++) if (list & (1u << i)) { CPU.r[i] = load32(CPU.r[R_SP]); CPU.r[R_SP] += 4; }
            if (lr) { uint32_t v = load32(CPU.r[R_SP]); CPU.r[R_SP] += 4; CPU.cpsr = (CPU.cpsr & ~0x20u) | ((v & 1) << 5); CPU.r[R_PC] = v & ~1u; }
        }
        return 1;
    }
    if ((h & 0xF000) == 0xC000) {
        int rn = (h >> 8) & 7, l = (h >> 11) & 1; unsigned list = h & 0xFF;
        uint32_t a = CPU.r[rn];
        for (int i = 0; i < 8; i++) if (list & (1u << i)) {
            if (l) CPU.r[i] = load32(a); else store32(a, CPU.r[i]);
            a += 4;
        }
        CPU.r[rn] = a; return 1;
    }
    if ((h & 0xF000) == 0xD000 && ((h >> 8) & 0xF) < 14) {
        if (cond_ok((h >> 8) & 0xF)) CPU.r[R_PC] = pc + 4 + (uint32_t)(int8_t)(h & 0xFF) * 2;
        return 1;
    }
    if ((h & 0xF800) == 0xE000) { CPU.r[R_PC] = pc + 4 + (uint32_t)((int16_t)((h & 0x7FF) << 5) >> 4); return 1; }
    if ((h & 0xFF00) == 0xDF00) { tl_log_line("a32: thumb svc #%u", h & 0xFF); return 1; }
    if (++Gunk < 8) tl_log_line("a32: thumb %#x at %#x not translated", h, pc);
    if (Gunk > 32) { CPU.r[R_PC] = SENTINEL; return 0; }
    return 1;
}

static int32_t a32_call_inner(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    uint32_t save[16]; uint32_t cpsr = CPU.cpsr;
    uint32_t sp = CPU.r[R_SP];
    memcpy(save, CPU.r, sizeof(save));
    memset(CPU.r, 0, sizeof(CPU.r));
    CPU.r[0] = r0; CPU.r[1] = r1; CPU.r[2] = r2; CPU.r[3] = r3;
    CPU.r[R_SP] = (sp >= GHEAP && sp <= GSTACK) ? sp : GSTACK;
    CPU.r[R_LR] = SENTINEL;
    CPU.r[R_PC] = pc & ~1u;
    CPU.cpsr = (pc & 1) ? 0x20 : 0;
    Gunk = 0;
    for (int n = 0; n < 2000000; n++) {
        if (!exec_one()) break;
        if (CPU.r[R_PC] == SENTINEL) break;
    }
    int32_t r = (int32_t)CPU.r[0];
    memcpy(CPU.r, save, sizeof(save)); CPU.cpsr = cpsr;
    return r;
}

int32_t tl_a32_call(uint32_t pc, uint32_t r0, uint32_t r1, uint32_t r2, uint32_t r3)
{
    int nested = Gdepth++;
    if (!nested) pthread_mutex_lock(&Glk);
    int32_t r = a32_call_inner(pc, r0, r1, r2, r3);
    if (--Gdepth == 0) pthread_mutex_unlock(&Glk);
    return r;
}

static void *a32_thread(void *p)
{
    a32_tramp *t = p;
    tl_a32_call(t->pc, t->arg, 0, 0, 0);
    free(t);
    return NULL;
}

int32_t tl_a32_jni_onload(tl_lib *L)
{
    void *p = tl_a32_sym(L, "JNI_OnLoad");
    if (!p) return 0;
    return tl_a32_call(tl_a32_va(p), JNI_VM, 0, 0, 0);
}

bool tl_a32_init(tl_lib *L)
{
    /* Static constructors of a 32-bit game are Thumb-2 C++ and not a complete
     * ISA yet; running them is what left Minecraft on "loading the engine"
     * for minutes. NativeActivity still starts from ANativeActivity_onCreate. */
    (void)L;
    return true;
}

bool tl_a32_jni_native(void *fn, dvm_method *m, jobj *self, const jvalue *params, jvalue *ret)
{
    uint32_t pc = tl_a32_va(fn);
    uint32_t r0 = JNI_ENV;
    uint32_t r1 = (m->flags & 8) ? to_h(m->cls->jc->mirror) : to_h(self);
    uint32_t r2 = 0, r3 = 0;
    uint32_t extra[16]; int ne = 0;
    int ng = 2; /* env, this/class already in r0 r1 */
    uint32_t rs[4] = { r0, r1, 0, 0 };
    for (int i = 0; i < m->nparams; i++) {
        char k = m->shorty[1 + i];
        uint32_t lo, hi = 0; int wide = 0;
        if (k == 'J' || k == 'D') { wide = 1; lo = (uint32_t)params[i].j; hi = (uint32_t)(params[i].j >> 32); }
        else if (k == 'F') lo = (uint32_t)params[i].i;
        else if (k == 'L' || k == '[') lo = to_h(params[i].l);
        else lo = (uint32_t)params[i].i;
        if (ng < 4) rs[ng++] = lo; else extra[ne++] = lo;
        if (wide) { if (ng < 4) rs[ng++] = hi; else extra[ne++] = hi; }
    }
    /* push extras */
    uint32_t sp = GSTACK;
    for (int i = ne - 1; i >= 0; i--) { sp -= 4; store32(sp, extra[i]); }
    uint32_t save_sp = CPU.r[R_SP];
    int32_t r = tl_a32_call(pc, rs[0], rs[1], rs[2], rs[3]);
    (void)save_sp;
    char rk = m->shorty[0];
    if (rk == 'L' || rk == '[') ret->l = from_h((uint32_t)r);
    else if (rk == 'Z') { ret->j = 0; ret->z = (uint8_t)r; }
    else if (rk == 'J' || rk == 'D') ret->j = (int64_t)(uint32_t)r;
    else ret->i = r;
    return !tl_jni_pending();
}

