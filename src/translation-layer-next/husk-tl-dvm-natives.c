/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The native methods ART implements inside itself rather than in libjavacore/libopenjdk: Object, String's character access,
 * StringFactory, System, Thread, Throwable, Class and reflection, VMRuntime, Unsafe, Reference, Array, Float/Double bits, Math.
 * Each takes the receiver and the parameters (one jvalue per parameter) and returns false when it threw.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-dvm-internal.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"

static jvalue J(int64_t v) { jvalue r; r.j = v; return r; }
static jvalue I(int32_t v) { jvalue r; r.j = (uint32_t)v; return r; }
static jvalue Z(bool v) { jvalue r; r.j = v ? 1 : 0; return r; }
static jvalue L(void *p) { jvalue r; r.j = 0; r.l = p; return r; }
static jvalue F(float f) { jvalue r; r.j = 0; r.f = f; return r; }
static jvalue D(double d) { jvalue r; r.d = d; return r; }
#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
#define UNUSED (void)self; (void)a; (void)ret

static const uint16_t *chars(jobj *s, int32_t *n) { return tl_dvm_string_chars(s, n); }
static bool npe(const char *what) { return dvm_throw("java/lang/NullPointerException", "%s", what); }

/* ================================================================== Object */

NAT(Object_getClass) { (void)a; *ret = L(dvm_object_class(self)->mirror); return true; }
NAT(Object_identityHashCode) { (void)self; *ret = I(a[0].l ? (int32_t)(((uintptr_t)a[0].l >> 4) * 2654435761u) : 0); return true; }
NAT(Object_internalClone)
{
    (void)a;
    jobj *c;
    if (self->kind == TL_K_PRIM_ARRAY) {
        c = tl_jni_new_prim_array(self->arr.etype, self->arr.len);
        memcpy(c->arr.data, self->arr.data, (size_t)self->arr.len * self->arr.esz);
    } else if (self->kind == TL_K_OBJ_ARRAY) {
        c = tl_jni_new_obj_array(NULL, self->oarr.len);
        c->cls = self->cls;
        memcpy(c->oarr.v, self->oarr.v, (size_t)self->oarr.len * sizeof(jobj *));
    } else if (self->kind == TL_K_STRING) {
        int32_t n; const uint16_t *s = chars(self, &n); c = tl_dvm_string_u16(s, n);
    } else {
        c = dvm_new_object(self->cls);
        if (self->nfields) { c->fields = calloc(self->nfields, sizeof(jvalue)); memcpy(c->fields, self->fields, self->nfields * sizeof(jvalue)); c->nfields = self->nfields; }
    }
    c->refs = 1u << 30;
    *ret = L(c);
    return true;
}
void dvm_monitor_wait(jobj *o, int64_t ms, int32_t ns);
void dvm_monitor_notify(jobj *o, bool all);
NAT(Object_notify) { UNUSED; dvm_monitor_notify(self, false); return true; }
NAT(Object_notifyAll) { UNUSED; dvm_monitor_notify(self, true); return true; }
NAT(Object_wait) { (void)ret; dvm_monitor_wait(self, a[0].j, a[1].i); return true; }

/* ================================================================== String */

NAT(String_charAt)
{
    int32_t n; const uint16_t *s = chars(self, &n);
    if (a[0].i < 0 || a[0].i >= n) return dvm_throw("java/lang/StringIndexOutOfBoundsException", "index=%d length=%d", a[0].i, n);
    *ret = I(s[a[0].i]);
    return true;
}
NAT(String_compareTo)
{
    if (!a[0].l) return npe("compareTo(null)");
    int32_t n1, n2; const uint16_t *s1 = chars(self, &n1), *s2 = chars(a[0].l, &n2);
    int32_t n = n1 < n2 ? n1 : n2;
    for (int32_t i = 0; i < n; i++) if (s1[i] != s2[i]) { *ret = I((int32_t)s1[i] - (int32_t)s2[i]); return true; }
    *ret = I(n1 - n2);
    return true;
}
NAT(String_concat)
{
    if (!a[0].l) return npe("concat(null)");
    int32_t n1, n2; const uint16_t *s1 = chars(self, &n1), *s2 = chars(a[0].l, &n2);
    if (!n2) { *ret = L(self); return true; }
    uint16_t *b = malloc((size_t)(n1 + n2) * 2);
    memcpy(b, s1, (size_t)n1 * 2); memcpy(b + n1, s2, (size_t)n2 * 2);
    *ret = L(tl_dvm_string_u16(b, n1 + n2));
    free(b);
    return true;
}
NAT(String_doRepeat)
{
    int32_t n; const uint16_t *s = chars(self, &n);
    int64_t total = (int64_t)n * a[0].i;
    uint16_t *b = malloc((size_t)(total ? total : 1) * 2);
    for (int32_t i = 0; i < a[0].i; i++) memcpy(b + (int64_t)i * n, s, (size_t)n * 2);
    *ret = L(tl_dvm_string_u16(b, (int32_t)total));
    free(b);
    return true;
}
NAT(String_doReplace)
{
    int32_t n; const uint16_t *s = chars(self, &n);
    uint16_t *b = malloc((size_t)(n ? n : 1) * 2);
    for (int32_t i = 0; i < n; i++) b[i] = s[i] == a[0].c ? a[1].c : s[i];
    *ret = L(tl_dvm_string_u16(b, n));
    free(b);
    return true;
}
NAT(String_fastSubstring)
{
    int32_t n; const uint16_t *s = chars(self, &n);
    int32_t start = a[0].i, len = a[1].i;
    if (start < 0 || len < 0 || start + len > n) return dvm_throw("java/lang/StringIndexOutOfBoundsException", "substring");
    *ret = L(tl_dvm_string_u16(s + start, len));
    return true;
}
NAT(String_fillBytesLatin1)
{
    (void)ret;
    int32_t n; const uint16_t *s = chars(self, &n);
    jobj *b = a[0].l;
    for (int32_t i = 0; i < n && a[1].i + i < (int32_t)b->arr.len; i++) ((uint8_t *)b->arr.data)[a[1].i + i] = (uint8_t)s[i];
    return true;
}
NAT(String_fillBytesUTF16)
{
    (void)ret;
    int32_t n; const uint16_t *s = chars(self, &n);
    jobj *b = a[0].l;
    for (int32_t i = 0; i < n && (a[1].i + i) * 2 + 1 < (int32_t)b->arr.len; i++) {
        ((uint8_t *)b->arr.data)[(a[1].i + i) * 2] = (uint8_t)(s[i] & 0xFF);
        ((uint8_t *)b->arr.data)[(a[1].i + i) * 2 + 1] = (uint8_t)(s[i] >> 8);
    }
    return true;
}
NAT(String_getCharsNoCheck)
{
    (void)ret;
    int32_t n; const uint16_t *s = chars(self, &n);
    jobj *dst = a[2].l;
    int32_t start = a[0].i, end = a[1].i, at = a[3].i;
    if (!dst) return npe("getChars");
    for (int32_t i = start; i < end && i < n; i++) ((uint16_t *)dst->arr.data)[at + i - start] = s[i];
    return true;
}
static jobj *g_interned[65536];
static pthread_mutex_t g_intern_lock = PTHREAD_MUTEX_INITIALIZER;
NAT(String_intern)
{
    (void)a;
    int32_t n; const uint16_t *s = chars(self, &n);
    uint32_t h = 0; for (int32_t i = 0; i < n; i++) h = h * 31 + s[i];
    pthread_mutex_lock(&g_intern_lock);
    for (uint32_t k = 0; k < 65536; k++) {
        uint32_t slot = (h + k) & 0xFFFF;
        jobj *o = g_interned[slot];
        if (!o) { g_interned[slot] = self; *ret = L(self); break; }
        int32_t m; const uint16_t *t = chars(o, &m);
        if (m == n && !memcmp(s, t, (size_t)n * 2)) { *ret = L(o); break; }
    }
    pthread_mutex_unlock(&g_intern_lock);
    return true;
}
NAT(String_toCharArray)
{
    (void)a;
    int32_t n; const uint16_t *s = chars(self, &n);
    jobj *arr = tl_jni_new_prim_array('C', (uint32_t)n);
    arr->refs = 1u << 30;
    memcpy(arr->arr.data, s, (size_t)n * 2);
    *ret = L(arr);
    return true;
}
/* intrinsics ART has for speed; the bytecode would do the same, slower */
NAT(String_length) { (void)a; int32_t n; chars(self, &n); *ret = I(n); return true; }
NAT(String_equals)
{
    jobj *o = a[0].l;
    if (o == self) { *ret = Z(true); return true; }
    if (!o || o->kind != TL_K_STRING) { *ret = Z(false); return true; }
    int32_t n1, n2; const uint16_t *s1 = chars(self, &n1), *s2 = chars(o, &n2);
    *ret = Z(n1 == n2 && !memcmp(s1, s2, (size_t)n1 * 2));
    return true;
}
NAT(String_hashCode)
{
    (void)a;
    int32_t n; const uint16_t *s = chars(self, &n);
    uint32_t h = 0; for (int32_t i = 0; i < n; i++) h = h * 31 + s[i];
    *ret = I((int32_t)h);
    return true;
}
NAT(String_indexOfChar)
{
    int32_t n; const uint16_t *s = chars(self, &n);
    int32_t c = a[0].i, from = a[1].i < 0 ? 0 : a[1].i;
    for (int32_t i = from; i < n; i++) {
        if (c < 0x10000 ? s[i] == c : (i + 1 < n && s[i] == (0xD800 | ((c - 0x10000) >> 10)) && s[i + 1] == (0xDC00 | ((c - 0x10000) & 0x3FF)))) { *ret = I(i); return true; }
    }
    *ret = I(-1);
    return true;
}

/* new String(...) in bytecode, and StringFactory: the forms a string is made from */
static jobj *from_bytes(jobj *b, int32_t off, int32_t len, int32_t hibyte)
{
    uint16_t *u = malloc((size_t)(len ? len : 1) * 2);
    for (int32_t i = 0; i < len; i++) u[i] = (uint16_t)((hibyte & 0xFF) << 8 | ((uint8_t *)b->arr.data)[off + i]);
    jobj *s = tl_dvm_string_u16(u, len);
    free(u);
    return s;
}
static jobj *from_utf8(const uint8_t *p, int32_t len)
{
    char *z = malloc((size_t)len + 1);
    memcpy(z, p, (size_t)len); z[len] = 0;
    jobj *s = dvm_new_string_utf8(z);
    free(z);
    return s;
}
NAT(SF_fromBytes) { (void)self; if (!a[0].l) return npe("bytes"); *ret = L(from_bytes(a[0].l, a[2].i, a[3].i, a[1].i)); return true; }
NAT(SF_fromChars) { (void)self; jobj *c = a[2].l; if (!c) return npe("chars"); *ret = L(tl_dvm_string_u16((uint16_t *)c->arr.data + a[0].i, a[1].i)); return true; }
NAT(SF_fromString) { (void)self; int32_t n; const uint16_t *s = chars(a[0].l, &n); *ret = L(tl_dvm_string_u16(s, n)); return true; }
NAT(SF_fromUtf16Bytes)
{
    (void)self;
    jobj *b = a[0].l; int32_t off = a[1].i, n = a[2].i;
    uint16_t *u = malloc((size_t)(n ? n : 1) * 2);
    for (int32_t i = 0; i < n; i++) u[i] = (uint16_t)(((uint8_t *)b->arr.data)[off + 2 * i] | ((uint8_t *)b->arr.data)[off + 2 * i + 1] << 8);
    *ret = L(tl_dvm_string_u16(u, n));
    free(u);
    return true;
}
NAT(SF_fromUtf8Bytes) { (void)self; jobj *b = a[0].l; if (!b) return npe("bytes"); *ret = L(from_utf8((uint8_t *)b->arr.data + a[1].i, a[2].i)); return true; }

/* String's constructors, as StringFactory's methods of the same parameters. Encoded bytes go through StringFactory's Java. */
bool dvm_string_init(const char *sig, const jvalue *p, jvalue *ret)
{
    if (!strcmp(sig, "()V")) { *ret = L(tl_dvm_string_u16(NULL, 0)); return true; }
    if (!strcmp(sig, "(Ljava/lang/String;)V")) return SF_fromString(NULL, p, ret);
    if (!strcmp(sig, "([C)V")) { jobj *c = p[0].l; if (!c) return npe("chars"); *ret = L(tl_dvm_string_u16(c->arr.data, (int32_t)c->arr.len)); return true; }
    if (!strcmp(sig, "([CII)V")) { jobj *c = p[0].l; if (!c) return npe("chars"); *ret = L(tl_dvm_string_u16((uint16_t *)c->arr.data + p[1].i, p[2].i)); return true; }
    if (!strcmp(sig, "(II[C)V")) return SF_fromChars(NULL, p, ret);
    /* everything else (charsets, code points, builders): StringFactory.newStringFrom<same parameters> */
    char fsig[256];
    snprintf(fsig, sizeof(fsig), "%.*s)Ljava/lang/String;", (int)(strchr(sig, ')') - sig), sig);
    tl_jclass *sf = dvm_class_named("java/lang/StringFactory");
    dvm_class *c = dvm_class_of(sf);
    static const char *const names[] = { "newStringFromBytes", "newStringFromChars", "newStringFromString", "newStringFromStringBuffer",
                                         "newStringFromCodePoints", "newStringFromStringBuilder", NULL };
    for (int i = 0; c && names[i]; i++) {
        dvm_method *m = dvm_find_method(c, names[i], fsig, true);
        if (m) { if (!dvm_ensure_init(c)) return false; return dvm_call(m, NULL, p, ret); }
    }
    return dvm_throw("java/lang/UnsupportedOperationException", "String constructor %s", sig);
}

/* ================================================================== System */

static int64_t now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (int64_t)t.tv_sec * 1000000000ll + t.tv_nsec; }
NAT(System_nanoTime) { UNUSED; *ret = J(now_ns()); return true; }
NAT(System_currentTimeMillis) { UNUSED; struct timeval tv; gettimeofday(&tv, NULL); *ret = J((int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000); return true; }
NAT(System_arraycopy)
{
    (void)self; (void)ret;
    jobj *src = a[0].l, *dst = a[2].l;
    int32_t sp = a[1].i, dp = a[3].i, n = a[4].i;
    if (!src || !dst) return npe("arraycopy");
    bool so = src->kind == TL_K_OBJ_ARRAY, dobj = dst->kind == TL_K_OBJ_ARRAY;
    if ((src->kind != TL_K_PRIM_ARRAY && !so) || (dst->kind != TL_K_PRIM_ARRAY && !dobj) || so != dobj
        || (!so && src->arr.etype != dst->arr.etype))
        return dvm_throw("java/lang/ArrayStoreException", "arraycopy: incompatible arrays");
    uint32_t sl = so ? src->oarr.len : src->arr.len, dl = dobj ? dst->oarr.len : dst->arr.len;
    if (sp < 0 || dp < 0 || n < 0 || (uint32_t)sp + (uint32_t)n > sl || (uint32_t)dp + (uint32_t)n > dl)
        return dvm_throw("java/lang/ArrayIndexOutOfBoundsException", "arraycopy: src %u+%d, dst %u+%d, length %d", sl, sp, dl, dp, n);
    if (so) memmove(dst->oarr.v + dp, src->oarr.v + sp, (size_t)n * sizeof(jobj *));
    else memmove((uint8_t *)dst->arr.data + (size_t)dp * dst->arr.esz, (uint8_t *)src->arr.data + (size_t)sp * src->arr.esz, (size_t)n * src->arr.esz);
    return true;
}
NAT(System_log)
{
    (void)self; (void)ret;
    int32_t n; const uint16_t *s = chars(a[1].l, &n);
    char *u = malloc((size_t)n * 3 + 1); size_t k = 0;
    for (int32_t i = 0; i < n; i++) u[k++] = s[i] < 0x80 ? (char)s[i] : '?';
    u[k] = 0;
    tl_log_line("java %c: %s", (char)a[0].c, u);
    free(u);
    return true;
}
NAT(System_mapLibraryName)
{
    (void)self;
    const char *n = tl_jni_string(a[0].l);
    if (!n) return npe("libname");
    char buf[512]; snprintf(buf, sizeof(buf), "lib%s.so", n);
    *ret = L(dvm_new_string_utf8(buf));
    return true;
}
static jobj *string_array(const char *const *v, int n)
{
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/String;");
    arr->refs = 1u << 30;
    for (int i = 0; i < n; i++) arr->oarr.v[i] = dvm_new_string_utf8(v[i]);
    return arr;
}
NAT(System_specialProperties)
{
    UNUSED;
    char cwd[1024]; if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), "/");
    char ud[1100]; snprintf(ud, sizeof(ud), "user.dir=%s", cwd);
    const char *v[] = { ud, "android.zlib.version=1.2.13", "android.openssl.version=OpenSSL 3.0", NULL };
    *ret = L(string_array(v, 3));
    return true;
}
NAT(System_setStream) { (void)self; (void)ret; (void)a; return true; }   /* setIn0/setOut0/setErr0: the Java side keeps the field */

/* ================================================================== VMRuntime */

NAT(VMR_properties)
{
    UNUSED;
    const char *v[] = { "java.vm.version=2.1.0", "java.vm.name=Dalvik", "java.home=/system", "java.io.tmpdir=/data/local/tmp",
                        "java.library.path=/system/lib64", "file.encoding=UTF-8", "line.separator=\n", "path.separator=:",
                        "file.separator=/", "os.arch=aarch64", "os.name=Linux", "os.version=6.1", "user.home=", "user.name=u0_a100",
                        "java.vendor=The Android Project", "java.version=0", "java.specification.version=0.9", "android.icu.library.version=74",
                        "android.icu.unicode.version=15.1", "http.agent=Dalvik/2.1.0 (Linux; U; Android 14)" };
    *ret = L(string_array(v, (int)(sizeof(v) / sizeof(v[0]))));
    return true;
}
NAT(VMR_getRuntime) { UNUSED; return true; }
NAT(VMR_zero) { UNUSED; ret->j = 0; return true; }
NAT(VMR_true) { UNUSED; *ret = Z(true); return true; }
NAT(VMR_isa) { UNUSED; *ret = L(dvm_new_string_utf8("arm64")); return true; }
NAT(VMR_vmVersion) { UNUSED; *ret = L(dvm_new_string_utf8("2.1.0")); return true; }
NAT(VMR_vmLibrary) { UNUSED; *ret = L(dvm_new_string_utf8("libart.so")); return true; }
NAT(VMR_heapUtil) { UNUSED; *ret = F(0.75f); return true; }
NAT(VMR_sdk) { (void)self; *ret = I(a[0].i ? a[0].i : 34); return true; }
NAT(VMR_intProp) { (void)self; *ret = I(a[1].i); return true; }
NAT(VMR_newArray)
{
    (void)self;
    jobj *cls = a[0].l; int32_t n = a[1].i;
    if (!cls || cls->kind != TL_K_CLASS) return npe("component type");
    dvm_class *c = dvm_class_of(cls->klass.jc);
    jobj *arr;
    if (c && c->prim) arr = tl_jni_new_prim_array(c->prim, (uint32_t)n);
    else {
        arr = tl_jni_new_obj_array(cls->klass.jc, (uint32_t)n);
        char nm[400]; snprintf(nm, sizeof(nm), cls->klass.jc->name[0] == '[' ? "[%s" : "[L%s;", cls->klass.jc->name);
        arr->cls = tl_jni_class(nm);
    }
    arr->refs = 1u << 30;
    *ret = L(arr);
    return true;
}
NAT(VMR_addressOf)
{
    (void)self;
    jobj *o = a[0].l;
    *ret = J(o && o->kind == TL_K_PRIM_ARRAY ? (int64_t)(uintptr_t)o->arr.data : 0);
    return true;
}
NAT(VMR_notifyInterval) { UNUSED; *ret = I(384); return true; }   /* ART's kNotifyNativeInterval: a divisor */
NAT(VMR_finalizerTimeout) { UNUSED; *ret = J(10000); return true; }
NAT(VMR_classPath) { UNUSED; *ret = L(dvm_new_string_utf8("")); return true; }

/* ================================================================== Thread */

static _Thread_local jobj *t_thread;
static pthread_mutex_t g_tid_lock = PTHREAD_MUTEX_INITIALIZER;

jobj *dvm_current_thread(void)
{
    if (t_thread) return t_thread;
    /* A thread Java has not seen (the main thread, a native thread calling in): a Thread for it, in the main group, as ART
     * makes when it attaches one. */
    tl_jclass *tc = dvm_class_named("java/lang/Thread");
    dvm_class *c = dvm_class_of(tc);
    if (!c) return NULL;
    jobj *t = dvm_new_object(tc);
    dvm_slots(t);
    t_thread = t;
    pthread_mutex_lock(&g_tid_lock);
    pthread_mutex_unlock(&g_tid_lock);
    dvm_class *tg = dvm_class_of(dvm_class_named("java/lang/ThreadGroup"));
    jobj *group = NULL;
    if (tg && dvm_ensure_init(tg)) {
        dvm_field *f = dvm_find_field(tg, "mainThreadGroup", true);
        if (f) group = tg->jc->statics[f->slot].l;
    }
    dvm_method *init = dvm_find_method(c, "<init>", "(Ljava/lang/ThreadGroup;Ljava/lang/String;IZ)V", false);
    char tn[64] = ""; pthread_getname_np(pthread_self(), tn, sizeof(tn));
    jvalue p[4] = { L(group), L(dvm_new_string_utf8(tn[0] ? tn : "main")), I(5), Z(false) };
    jvalue r;
    if (init && group && !dvm_call(init, t, p, &r)) {
        char buf[300]; tl_log_line("dvm: making the Thread for %s threw %s", tn, tl_dvm_describe_pending(buf, sizeof(buf)) ? buf : "?");
        tl_jni_set_pending(NULL);
    }
    return t;
}
NAT(Thread_currentThread) { UNUSED; *ret = L(dvm_current_thread()); return true; }
NAT(Thread_sleep)
{
    (void)self; (void)ret;
    int64_t ms = a[1].j; int32_t ns = a[2].i;
    struct timespec ts = { (time_t)(ms / 1000), (long)((ms % 1000) * 1000000 + ns) };
    nanosleep(&ts, NULL);
    return true;
}
NAT(Thread_false) { UNUSED; *ret = Z(false); return true; }
NAT(Thread_noop) { UNUSED; return true; }
NAT(Thread_nice) { (void)self; *ret = I(a[0].i <= 1 ? 19 : a[0].i >= 10 ? -8 : 10 - 2 * a[0].i); return true; }
NAT(Thread_status) { UNUSED; *ret = I(1 /* RUNNABLE */); return true; }
NAT(Thread_holdsLock) { UNUSED; *ret = Z(true); return true; }

typedef struct { jobj *thread; } start_arg;
static void *thread_main(void *p)
{
    start_arg *s = p;
    t_thread = s->thread;
    free(s);
    jobj *t = t_thread;
    jvalue nm; dvm_class *c = dvm_class_of(dvm_object_class(t));
    dvm_field *nf = c ? dvm_find_field(c, "name", false) : NULL;
    if (nf) { nm = t->fields[nf->slot]; const char *n = tl_jni_string(nm.l); if (n) pthread_setname_np(n); }
    dvm_method *run = dvm_find_virtual(dvm_object_class(t), "run", "()V");
    jvalue r;
    if (run && !dvm_call(run, t, NULL, &r)) {
        char buf[400];
        tl_log_line("dvm: uncaught exception in a thread: %s", tl_dvm_describe_pending(buf, sizeof(buf)) ? buf : "?");
        tl_jni_set_pending(NULL);
    }
    return NULL;
}
NAT(Thread_nativeCreate)
{
    (void)self; (void)ret;
    start_arg *s = malloc(sizeof(*s));
    s->thread = a[0].l;
    pthread_attr_t at; pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 32u << 20);
    pthread_attr_setdetachstate(&at, PTHREAD_CREATE_DETACHED);
    pthread_t th;
    if (pthread_create(&th, &at, thread_main, s) != 0) { free(s); return dvm_throw("java/lang/OutOfMemoryError", "pthread_create"); }
    return true;
}

/* ================================================================== Throwable */

NAT(Throwable_fill) { UNUSED; *ret = L(NULL); return true; }
NAT(Throwable_getStack)
{
    UNUSED;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/StackTraceElement"), 0);
    arr->cls = tl_jni_class("[Ljava/lang/StackTraceElement;");
    *ret = L(arr);
    return true;
}

/* ================================================================== Runtime */

NAT(Runtime_freeMemory) { UNUSED; *ret = J(256ll << 20); return true; }
NAT(Runtime_maxMemory) { UNUSED; *ret = J(512ll << 20); return true; }
NAT(Runtime_gc) { UNUSED; return true; }
NAT(Runtime_exit) { (void)self; (void)ret; tl_log_line("dvm: Runtime.exit(%d)", a[0].i); _exit(a[0].i); }
NAT(Runtime_nativeLoad)
{
    (void)self;
    const char *path = tl_jni_string(a[0].l);
    if (!path) return npe("path");
    const char *slash = strrchr(path, '/');
    jvalue n; n.l = dvm_new_string_utf8(slash ? slash + 1 : path);
    /* through Husk's own System.loadLibrary, which finds the library in the APK and runs its JNI_OnLoad */
    tl_jmeth *m = tl_jni_method(tl_jni_class("husk/Loader"), "loadLibrary", "(Ljava/lang/String;)V", true);
    (void)m;
    extern bool tl_dvm_load_library(const char *name);
    *ret = L(tl_dvm_load_library(path) ? NULL : dvm_new_string_utf8("could not load library"));
    return true;
}

/* ================================================================== Float, Double, Math */

NAT(Float_toBits) { (void)self; *ret = I(a[0].i); return true; }
NAT(Float_fromBits) { (void)self; *ret = I(a[0].i); return true; }
NAT(Double_toBits) { (void)self; *ret = J(a[0].j); return true; }
NAT(Double_fromBits) { (void)self; *ret = J(a[0].j); return true; }
#define MATH1(n, f) NAT(n) { (void)self; *ret = D(f(a[0].d)); return true; }
#define MATH2(n, f) NAT(n) { (void)self; *ret = D(f(a[0].d, a[1].d)); return true; }
MATH1(M_sin, sin) MATH1(M_cos, cos) MATH1(M_tan, tan) MATH1(M_asin, asin) MATH1(M_acos, acos) MATH1(M_atan, atan)
MATH1(M_exp, exp) MATH1(M_log, log) MATH1(M_log10, log10) MATH1(M_sqrt, sqrt) MATH1(M_cbrt, cbrt) MATH1(M_ceil, ceil)
MATH1(M_floor, floor) MATH1(M_rint, rint) MATH1(M_sinh, sinh) MATH1(M_cosh, cosh) MATH1(M_tanh, tanh) MATH1(M_expm1, expm1)
MATH1(M_log1p, log1p)
MATH2(M_atan2, atan2) MATH2(M_pow, pow) MATH2(M_hypot, hypot) MATH2(M_IEEEremainder, remainder) MATH2(M_nextafter, nextafter)

/* ================================================================== Class */

static int prim_code(char p)
{
    switch (p) { case 'Z': return 1; case 'B': return 2; case 'C': return 3; case 'S': return 4; case 'I': return 5;
                 case 'J': return 6; case 'F': return 7; case 'D': return 8; case 'V': return 9; default: return 0; }
}

/* What a Class mirror's fields hold, filled when the interpreter first looks inside one. */
void dvm_fill_mirror(jobj *mirror);
void dvm_fill_mirror(jobj *mirror)
{
    tl_jclass *jc = mirror->klass.jc;
    dvm_class *cc = dvm_class_of(tl_jni_class("java/lang/Class"));
    dvm_class *c = dvm_class_of(jc);
    if (!cc) return;
    jvalue *s = mirror->fields;
    dvm_field *f;
    uint32_t flags = c ? c->flags : 1;
    if (!c) flags = !strncmp(jc->name, "android/", 8) ? 1 : 1;
    if ((f = dvm_find_field(cc, "accessFlags", false))) s[f->slot] = I((int32_t)(flags & 0xFFFF));
    if ((f = dvm_find_field(cc, "superClass", false)) && jc->super && !(flags & 0x200)) s[f->slot] = L(jc->super->mirror);
    if ((f = dvm_find_field(cc, "primitiveType", false)) && c && c->prim) {
        int code = prim_code(c->prim);
        int shift = c->prim == 'J' || c->prim == 'D' ? 3 : c->prim == 'I' || c->prim == 'F' ? 2 : c->prim == 'C' || c->prim == 'S' ? 1 : 0;
        s[f->slot] = I(code | shift << 16);
    }
    if ((f = dvm_find_field(cc, "componentType", false)) && jc->name[0] == '[') {
        const char *e = jc->name + 1;
        tl_jclass *ec;
        switch (*e) {
        case 'I': ec = tl_jni_class("int"); break; case 'J': ec = tl_jni_class("long"); break; case 'F': ec = tl_jni_class("float"); break;
        case 'D': ec = tl_jni_class("double"); break; case 'Z': ec = tl_jni_class("boolean"); break; case 'B': ec = tl_jni_class("byte"); break;
        case 'C': ec = tl_jni_class("char"); break; case 'S': ec = tl_jni_class("short"); break;
        case '[': ec = tl_jni_class(e); break;
        default: { char n[400]; snprintf(n, sizeof(n), "%.*s", (int)strlen(e) - 2, e + 1); ec = tl_jni_class(n); }
        }
        s[f->slot] = L(ec->mirror);
    }
    if ((f = dvm_find_field(cc, "status", false))) s[f->slot] = I(c && c->state == CS_INITIALIZED ? 0x0F : 0x0A);
}

static void dotted(const char *n, char *out, size_t sz)
{
    snprintf(out, sz, "%s", n);
    if (n[0] != '[') { for (char *p = out; *p; p++) if (*p == '/') *p = '.'; }
    else for (char *p = out; *p; p++) if (*p == '/') *p = '.';
}
NAT(Class_getNameNative)
{
    (void)a;
    char buf[512]; dotted(self->klass.jc->name, buf, sizeof(buf));
    *ret = L(dvm_new_string_utf8(buf));
    return true;
}
NAT(Class_classForName)
{
    (void)self;
    const char *n = tl_jni_string(a[0].l);
    if (!n) return npe("className");
    char jn[512]; snprintf(jn, sizeof(jn), "%s", n);
    for (char *p = jn; *p; p++) if (*p == '.') *p = '/';
    tl_jclass *jc = dvm_class_named(jn);
    if (!jc && (!strncmp(jn, "android/", 8) || !strncmp(jn, "java/", 5))) jc = tl_jni_find_declared(jn);
    if (!jc) return dvm_throw("java/lang/ClassNotFoundException", "%s", n);
    if (a[1].z && dvm_class_of(jc) && !dvm_ensure_init(dvm_class_of(jc))) return false;
    *ret = L(jc->mirror);
    return true;
}
NAT(Class_getPrimitiveClass)
{
    (void)self;
    const char *n = tl_jni_string(a[0].l);
    *ret = L(n ? tl_jni_class(n)->mirror : NULL);
    return true;
}
NAT(Class_getInterfacesInternal)
{
    (void)a;
    dvm_class *c = dvm_class_of(self->klass.jc);
    int n = c ? c->nifaces : 0;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/Class"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/Class;");
    for (int i = 0; i < n; i++) arr->oarr.v[i] = c->ifaces[i]->mirror;
    *ret = L(n ? arr : NULL);
    return true;
}
NAT(Class_newInstance)
{
    (void)a;
    tl_jclass *jc = self->klass.jc;
    dvm_class *c = dvm_class_of(jc);
    if (!c) return dvm_throw("java/lang/InstantiationException", "%s", jc->name);
    if (!dvm_ensure_init(c)) return false;
    dvm_method *init = dvm_find_method(c, "<init>", "()V", false);
    if (!init || init->cls != c) return dvm_throw("java/lang/InstantiationException", "%s has no zero-argument constructor", jc->name);
    jobj *o = dvm_new_object(jc); dvm_slots(o);
    jvalue r;
    if (!dvm_call(init, o, NULL, &r)) return false;
    *ret = L(o);
    return true;
}
NAT(Class_null) { UNUSED; *ret = L(NULL); return true; }
NAT(Class_false) { UNUSED; *ret = Z(false); return true; }
NAT(Class_zero) { UNUSED; *ret = I(0); return true; }
NAT(Class_getSimpleNameNative)
{
    (void)a;
    const char *n = self->klass.jc->name, *s = strrchr(n, '/'), *d = strrchr(n, '$');
    const char *b = d && (!s || d > s) ? d + 1 : s ? s + 1 : n;
    *ret = L(dvm_new_string_utf8(b));
    return true;
}

/* ---- reflection objects (Field, Method, Constructor) over the interpreter's own members */

static jobj *make_field(dvm_field *f)
{
    tl_jclass *fc = tl_jni_class("java/lang/reflect/Field");
    jobj *o = dvm_new_object(fc); jvalue *s = dvm_slots(o);
    dvm_class *c = dvm_class_of(fc);
    dvm_field *x;
    if ((x = dvm_find_field(c, "declaringClass", false))) s[x->slot] = L(f->cls->jc->mirror);
    if ((x = dvm_find_field(c, "accessFlags", false))) s[x->slot] = I((int32_t)f->flags);
    if ((x = dvm_find_field(c, "dexFieldIndex", false))) s[x->slot] = I((int32_t)f->idx);
    if ((x = dvm_find_field(c, "offset", false))) s[x->slot] = I((int32_t)(f->slot * 8 + 16));
    if ((x = dvm_find_field(c, "type", false))) {
        const char *t = f->type; char nm[400];
        if (t[0] == 'L') snprintf(nm, sizeof(nm), "%.*s", (int)strlen(t) - 2, t + 1);
        else if (t[0] == '[') snprintf(nm, sizeof(nm), "%s", t);
        else snprintf(nm, sizeof(nm), "%s", t[0] == 'I' ? "int" : t[0] == 'J' ? "long" : t[0] == 'Z' ? "boolean" : t[0] == 'F' ? "float" : t[0] == 'D' ? "double" : t[0] == 'B' ? "byte" : t[0] == 'C' ? "char" : "short");
        s[x->slot] = L(tl_jni_class(nm)->mirror);
    }
    o->native = f;
    return o;
}
static dvm_field *field_of(jobj *fo) { return fo ? fo->native : NULL; }
NAT(Class_getDeclaredField)
{
    dvm_class *c = dvm_class_of(self->klass.jc);
    const char *n = tl_jni_string(a[0].l);
    if (!n) return npe("name");
    for (int st = 0; c && st < 2; st++) {
        dvm_field *list = st ? c->sf : c->inf; int cnt = st ? c->nsf : c->ninf;
        for (int i = 0; i < cnt; i++) if (!strcmp(list[i].name, n)) { *ret = L(make_field(&list[i])); return true; }
    }
    *ret = L(NULL);
    return true;
}
NAT(Class_getDeclaredFields)
{
    (void)a;
    dvm_class *c = dvm_class_of(self->klass.jc);
    int n = c ? c->nsf + c->ninf : 0;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/reflect/Field"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/reflect/Field;");
    int k = 0;
    for (int i = 0; c && i < c->ninf; i++) arr->oarr.v[k++] = make_field(&c->inf[i]);
    for (int i = 0; c && i < c->nsf; i++) arr->oarr.v[k++] = make_field(&c->sf[i]);
    *ret = L(arr);
    return true;
}
static jvalue *field_place(dvm_field *f, jobj *obj)
{
    if (f->flags & 8) { dvm_ensure_init(f->cls); return &f->cls->jc->statics[f->slot]; }
    return obj ? &dvm_slots(obj)[f->slot] : NULL;
}
NAT(Field_getInt) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = I(p->i); return true; }
NAT(Field_getLong) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = J(p->j); return true; }
NAT(Field_getBoolean) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = Z(p->z); return true; }
NAT(Field_setInt) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = 0; p->i = a[1].i; return true; }
NAT(Field_setLong) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = a[1].j; return true; }
NAT(Field_setBoolean) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = 0; p->z = a[1].z; return true; }
NAT(Field_get)
{
    dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l);
    if (!p) return npe("object");
    if (f->type[0] == 'L' || f->type[0] == '[') { *ret = L(p->l); return true; }
    /* a primitive, boxed */
    const char *box; const char *sig;
    switch (f->type[0]) {
    case 'I': box = "java/lang/Integer"; sig = "(I)Ljava/lang/Integer;"; break;
    case 'J': box = "java/lang/Long"; sig = "(J)Ljava/lang/Long;"; break;
    case 'Z': box = "java/lang/Boolean"; sig = "(Z)Ljava/lang/Boolean;"; break;
    case 'F': box = "java/lang/Float"; sig = "(F)Ljava/lang/Float;"; break;
    case 'D': box = "java/lang/Double"; sig = "(D)Ljava/lang/Double;"; break;
    case 'B': box = "java/lang/Byte"; sig = "(B)Ljava/lang/Byte;"; break;
    case 'C': box = "java/lang/Character"; sig = "(C)Ljava/lang/Character;"; break;
    default: box = "java/lang/Short"; sig = "(S)Ljava/lang/Short;"; break;
    }
    return tl_dvm_call_static(box, "valueOf", sig, p, ret);
}
NAT(Field_set)
{
    (void)ret;
    dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l);
    if (!p) return npe("object");
    if (f->type[0] == 'L' || f->type[0] == '[') { p->l = a[1].l; return true; }
    jobj *b = a[1].l;
    if (!b) return npe("value");
    dvm_class *bc = dvm_class_of(dvm_object_class(b));
    dvm_field *v = bc ? dvm_find_field(bc, "value", false) : NULL;
    if (v) *p = dvm_slots(b)[v->slot];
    return true;
}


/* ---- Constructor and Method objects: an Executable whose artMethod is the interpreter's method */

static tl_jclass *class_of_desc(const char *d, int *len)
{
    static const struct { char c; const char *n; } prim[] = { { 'Z', "boolean" }, { 'B', "byte" }, { 'C', "char" }, { 'S', "short" },
        { 'I', "int" }, { 'J', "long" }, { 'F', "float" }, { 'D', "double" }, { 'V', "void" } };
    for (size_t i = 0; i < sizeof(prim) / sizeof(prim[0]); i++) if (*d == prim[i].c) { *len = 1; return tl_jni_class(prim[i].n); }
    if (*d == '[') {
        int inner; class_of_desc(d + 1, &inner);
        *len = 1 + inner;
        char n[400]; snprintf(n, sizeof(n), "%.*s", *len, d);
        return tl_jni_class(n);
    }
    const char *e = strchr(d, ';');
    *len = e ? (int)(e - d) + 1 : (int)strlen(d);
    char n[400]; snprintf(n, sizeof(n), "%.*s", e ? (int)(e - d - 1) : 0, d + 1);
    tl_jclass *c = dvm_class_named(n);
    return c ? c : tl_jni_class(n);
}

/* A Class mirror as the descriptor a signature names it by. */
static void desc_of(jobj *mirror, char *out, size_t n)
{
    tl_jclass *jc = mirror->klass.jc;
    dvm_class *c = dvm_class_of(jc);
    if (c && c->prim) snprintf(out, n, "%c", c->prim);
    else if (jc->name[0] == '[') snprintf(out, n, "%s", jc->name);
    else snprintf(out, n, "L%s;", jc->name);
}

static bool params_match(const dvm_method *m, jobj *types)
{
    char want[256] = "("; size_t k = 1;
    uint32_t n = types ? types->oarr.len : 0;
    for (uint32_t i = 0; i < n; i++) {
        char d[200]; if (!types->oarr.v[i]) return false;
        desc_of(types->oarr.v[i], d, sizeof(d));
        k += (size_t)snprintf(want + k, sizeof(want) - k, "%s", d);
    }
    snprintf(want + k, sizeof(want) - k, ")");
    return !strncmp(m->sig, want, strlen(want));
}

static jobj *make_executable(dvm_method *m)
{
    bool ctor = !strcmp(m->name, "<init>");
    tl_jclass *ec = tl_jni_class(ctor ? "java/lang/reflect/Constructor" : "java/lang/reflect/Method");
    jobj *o = dvm_new_object(ec); jvalue *s = dvm_slots(o);
    dvm_class *c = dvm_class_of(ec);
    dvm_field *x;
    if ((x = dvm_find_field(c, "artMethod", false))) s[x->slot] = J((int64_t)(uintptr_t)m);
    if ((x = dvm_find_field(c, "declaringClass", false))) s[x->slot] = L(m->cls->jc->mirror);
    if ((x = dvm_find_field(c, "declaringClassOfOverriddenMethod", false))) s[x->slot] = L(m->cls->jc->mirror);
    if ((x = dvm_find_field(c, "accessFlags", false))) s[x->slot] = I((int32_t)(m->flags & 0xFFFF));
    if ((x = dvm_find_field(c, "dexMethodIndex", false))) s[x->slot] = I((int32_t)m->idx);
    return o;
}
static dvm_method *method_of(jobj *exe)
{
    if (!exe) return NULL;
    dvm_class *c = dvm_class_of(dvm_object_class(exe));
    dvm_field *x = c ? dvm_find_field(c, "artMethod", false) : NULL;
    return x ? (dvm_method *)(uintptr_t)dvm_slots(exe)[x->slot].j : NULL;
}

static jobj *exe_array(dvm_method **list, int n, bool ctor)
{
    jobj *arr = tl_jni_new_obj_array(tl_jni_class(ctor ? "java/lang/reflect/Constructor" : "java/lang/reflect/Method"), (uint32_t)n);
    arr->cls = tl_jni_class(ctor ? "[Ljava/lang/reflect/Constructor;" : "[Ljava/lang/reflect/Method;");
    arr->refs = 1u << 30;
    for (int i = 0; i < n; i++) arr->oarr.v[i] = make_executable(list[i]);
    return arr;
}

NAT(Class_getDeclaredConstructorInternal)
{
    dvm_class *c = dvm_class_of(self->klass.jc);
    for (int i = 0; c && i < c->ndm; i++)
        if (!strcmp(c->dm[i].name, "<init>") && params_match(&c->dm[i], a[0].l)) { *ret = L(make_executable(&c->dm[i])); return true; }
    *ret = L(NULL);
    return true;
}
NAT(Class_getDeclaredConstructorsInternal)
{
    dvm_class *c = dvm_class_of(self->klass.jc);
    dvm_method *list[256]; int n = 0;
    for (int i = 0; c && i < c->ndm && n < 256; i++)
        if (!strcmp(c->dm[i].name, "<init>") && (!a[0].z || (c->dm[i].flags & 1))) list[n++] = &c->dm[i];
    *ret = L(exe_array(list, n, true));
    return true;
}
NAT(Class_getDeclaredMethodInternal)
{
    dvm_class *c = dvm_class_of(self->klass.jc);
    const char *name = tl_jni_string(a[0].l);
    if (!name) return npe("name");
    dvm_method *best = NULL;
    for (int g = 0; c && g < 2; g++) {
        dvm_method *list = g ? c->vm : c->dm; int cnt = g ? c->nvm : c->ndm;
        for (int i = 0; i < cnt; i++)
            if (!strcmp(list[i].name, name) && params_match(&list[i], a[1].l)) {
                /* a bridge method loses to the real one */
                if (!best || ((best->flags & 0x40) && !(list[i].flags & 0x40))) best = &list[i];
            }
    }
    *ret = L(best ? make_executable(best) : NULL);
    return true;
}
NAT(Class_getDeclaredMethodsUnchecked)
{
    dvm_class *c = dvm_class_of(self->klass.jc);
    dvm_method *list[2048]; int n = 0;
    for (int g = 0; c && g < 2; g++) {
        dvm_method *ms = g ? c->vm : c->dm; int cnt = g ? c->nvm : c->ndm;
        for (int i = 0; i < cnt && n < 2048; i++) {
            if (ms[i].name[0] == '<') continue;
            if (a[0].z && !(ms[i].flags & 1)) continue;
            list[n++] = &ms[i];
        }
    }
    *ret = L(exe_array(list, n, false));
    return true;
}
NAT(Class_getPublicDeclaredFields)
{
    (void)a;
    dvm_class *c = dvm_class_of(self->klass.jc);
    int n = 0;
    for (int i = 0; c && i < c->ninf; i++) if (c->inf[i].flags & 1) n++;
    for (int i = 0; c && i < c->nsf; i++) if (c->sf[i].flags & 1) n++;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/reflect/Field"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/reflect/Field;");
    int k = 0;
    for (int i = 0; c && i < c->ninf; i++) if (c->inf[i].flags & 1) arr->oarr.v[k++] = make_field(&c->inf[i]);
    for (int i = 0; c && i < c->nsf; i++) if (c->sf[i].flags & 1) arr->oarr.v[k++] = make_field(&c->sf[i]);
    *ret = L(arr);
    return true;
}
NAT(Class_getPublicFieldRecursive)
{
    const char *n = tl_jni_string(a[0].l);
    if (!n) return npe("name");
    for (dvm_class *c = dvm_class_of(self->klass.jc); c; c = c->super) {
        dvm_field *f = dvm_find_field(c, n, false);
        if (!f) f = dvm_find_field(c, n, true);
        if (f && (f->flags & 1)) { *ret = L(make_field(f)); return true; }
    }
    *ret = L(NULL);
    return true;
}

NAT(Exe_getParameterTypes)
{
    (void)a;
    dvm_method *m = method_of(self);
    if (!m) return npe("method");
    jobj *types[64]; int n = 0;
    for (const char *p = m->sig + 1; *p && *p != ')' && n < 64; ) { int l; types[n++] = class_of_desc(p, &l)->mirror; p += l; }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/Class"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/Class;");
    for (int i = 0; i < n; i++) arr->oarr.v[i] = types[i];
    *ret = L(arr);
    return true;
}
NAT(Exe_getParameterCount) { (void)a; dvm_method *m = method_of(self); *ret = I(m ? m->nparams : 0); return true; }
NAT(Exe_getName) { (void)a; dvm_method *m = method_of(self); *ret = L(m ? dvm_new_string_utf8(m->name) : NULL); return true; }
NAT(Exe_getReturnType) { (void)a; dvm_method *m = method_of(self); int l; *ret = L(m ? class_of_desc(strchr(m->sig, ')') + 1, &l)->mirror : NULL); return true; }
NAT(Exe_compareParams)
{
    dvm_method *x = method_of(self), *y = method_of(a[0].l);
    if (!x || !y) { *ret = I(0); return true; }
    const char *px = x->sig, *py = y->sig;
    size_t lx = strchr(px, ')') - px, ly = strchr(py, ')') - py;
    int c = strncmp(px, py, lx < ly ? lx : ly);
    *ret = I(c ? c : (int)lx - (int)ly);
    return true;
}
NAT(Exe_emptyAnnotations)
{
    UNUSED;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/annotation/Annotation"), 0);
    arr->cls = tl_jni_class("[Ljava/lang/annotation/Annotation;");
    *ret = L(arr);
    return true;
}
NAT(Exe_paramAnnotations)
{
    (void)a;
    dvm_method *m = method_of(self);
    int n = m ? m->nparams : 0;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("[Ljava/lang/annotation/Annotation;"), (uint32_t)n);
    arr->cls = tl_jni_class("[[Ljava/lang/annotation/Annotation;");
    for (int i = 0; i < n; i++) { jvalue r; Exe_emptyAnnotations(NULL, NULL, &r); arr->oarr.v[i] = r.l; }
    *ret = L(arr);
    return true;
}
NAT(Exe_noClasses)
{
    UNUSED;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/Class"), 0);
    arr->cls = tl_jni_class("[Ljava/lang/Class;");
    *ret = L(arr);
    return true;
}

/* Arguments from an Object[] (unboxing primitives) into jvalues for the method's parameters. */
static bool unbox_args(dvm_method *m, jobj *arr, jvalue *out)
{
    uint32_t n = arr ? arr->oarr.len : 0;
    if ((int)n != m->nparams) return dvm_throw("java/lang/IllegalArgumentException", "Wrong number of arguments; expected %d, got %u", m->nparams, n);
    for (uint32_t i = 0; i < n; i++) {
        char k = m->shorty[1 + i];
        jobj *v = arr->oarr.v[i];
        out[i].j = 0;
        if (k == 'L') { out[i].l = v; continue; }
        if (!v) return dvm_throw("java/lang/IllegalArgumentException", "primitive argument %u is null", i);
        dvm_class *bc = dvm_class_of(dvm_object_class(v));
        dvm_field *f = bc ? dvm_find_field(bc, "value", false) : NULL;
        if (!f) return dvm_throw("java/lang/IllegalArgumentException", "argument %u is not a boxed primitive", i);
        jvalue x = dvm_slots(v)[f->slot];
        char have = f->type[0];
        /* widening, as reflection allows */
        double dv = have == 'J' ? (double)x.j : have == 'F' ? x.f : have == 'D' ? x.d : have == 'Z' ? x.z : have == 'C' ? x.c : (double)x.i;
        int64_t lv = have == 'J' ? x.j : have == 'Z' ? x.z : have == 'C' ? x.c : have == 'F' || have == 'D' ? (int64_t)dv : x.i;
        switch (k) {
        case 'Z': out[i].z = x.z; break;
        case 'J': out[i].j = lv; break;
        case 'F': out[i].f = (float)dv; break;
        case 'D': out[i].d = dv; break;
        case 'C': out[i].c = (uint16_t)lv; break;
        default: out[i].i = (int32_t)lv; break;
        }
    }
    return true;
}
static bool box(char k, jvalue v, jvalue *ret)
{
    const char *cls, *sig;
    switch (k) {
    case 'V': *ret = L(NULL); return true;
    case 'L': case '[': *ret = v; return true;
    case 'I': cls = "java/lang/Integer"; sig = "(I)Ljava/lang/Integer;"; break;
    case 'J': cls = "java/lang/Long"; sig = "(J)Ljava/lang/Long;"; break;
    case 'Z': cls = "java/lang/Boolean"; sig = "(Z)Ljava/lang/Boolean;"; break;
    case 'F': cls = "java/lang/Float"; sig = "(F)Ljava/lang/Float;"; break;
    case 'D': cls = "java/lang/Double"; sig = "(D)Ljava/lang/Double;"; break;
    case 'B': cls = "java/lang/Byte"; sig = "(B)Ljava/lang/Byte;"; break;
    case 'C': cls = "java/lang/Character"; sig = "(C)Ljava/lang/Character;"; break;
    default: cls = "java/lang/Short"; sig = "(S)Ljava/lang/Short;"; break;
    }
    return tl_dvm_call_static(cls, "valueOf", sig, &v, ret);
}
/* Reflection wraps what the method threw in InvocationTargetException. */
static bool wrap_target_exception(void)
{
    jobj *cause = tl_jni_pending_object();
    tl_jni_set_pending(NULL);
    tl_jclass *ic = dvm_class_named("java/lang/reflect/InvocationTargetException");
    dvm_class *c = dvm_class_of(ic);
    dvm_method *init = c ? dvm_find_method(c, "<init>", "(Ljava/lang/Throwable;)V", false) : NULL;
    if (!init) { tl_jni_set_pending(cause); return false; }
    jobj *e = dvm_new_object(ic); dvm_slots(e);
    jvalue p = L(cause), r;
    if (dvm_ensure_init(c) && dvm_call(init, e, &p, &r)) tl_jni_set_pending(e);
    return false;
}
NAT(Method_invoke)
{
    dvm_method *m = method_of(self);
    if (!m) return npe("method");
    jvalue args[64];
    if (!unbox_args(m, a[1].l, args)) return false;
    jobj *recv = a[0].l;
    if (!(m->flags & 8)) {
        if (!recv) return npe("receiver");
        if (!(m->flags & 2) && m->name[0] != '<') { dvm_method *v = dvm_find_virtual(dvm_object_class(recv), m->name, m->sig); if (v) m = v; }
    } else if (!dvm_ensure_init(m->cls)) return false;
    jvalue r;
    if (!dvm_call(m, (m->flags & 8) ? NULL : recv, args, &r)) return wrap_target_exception();
    return box(m->shorty[0], r, ret);
}
NAT(Ctor_newInstance0)
{
    dvm_method *m = method_of(self);
    if (!m) return npe("constructor");
    jvalue args[64];
    if (!unbox_args(m, a[0].l, args)) return false;
    if (!dvm_ensure_init(m->cls)) return false;
    jobj *o = dvm_new_object(m->cls->jc); dvm_slots(o);
    jvalue r;
    if (!dvm_call(m, o, args, &r)) return wrap_target_exception();
    *ret = L(o);
    return true;
}


NAT(Field_getName) { (void)a; dvm_field *f = field_of(self); *ret = L(f ? dvm_new_string_utf8(f->name) : NULL); return true; }
NAT(Field_getArtField) { (void)a; *ret = J((int64_t)(uintptr_t)field_of(self)); return true; }
NAT(Field_getByte) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = I(p->b); return true; }
NAT(Field_getChar) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = I(p->c); return true; }
NAT(Field_getShort) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = I(p->s); return true; }
NAT(Field_getFloat) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = F(p->f); return true; }
NAT(Field_getDouble) { dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); *ret = D(p->d); return true; }
NAT(Field_setByte) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = 0; p->b = a[1].b; return true; }
NAT(Field_setChar) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = 0; p->c = a[1].c; return true; }
NAT(Field_setShort) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = 0; p->s = a[1].s; return true; }
NAT(Field_setFloat) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->j = 0; p->f = a[1].f; return true; }
NAT(Field_setDouble) { (void)ret; dvm_field *f = field_of(self); jvalue *p = field_place(f, a[0].l); if (!p) return npe("object"); p->d = a[1].d; return true; }

/* ================================================================== Unsafe */

/* Offsets are what Field objects say (slot * 8 + 16), and arrays start at 16 with their element size as the scale. */
static jvalue *unsafe_place(jobj *o, int64_t off, int size)
{
    if (!o) return (jvalue *)(uintptr_t)off;                     /* raw memory */
    if (o->kind == TL_K_PRIM_ARRAY) return (jvalue *)((uint8_t *)o->arr.data + (off - 16));
    if (o->kind == TL_K_OBJ_ARRAY) return (jvalue *)&o->oarr.v[(off - 16) / 8];
    if (o->kind == TL_K_CLASS) {
        /* a static field: Unsafe.staticFieldOffset gives slot*8+16 of the class's statics */
        return &o->klass.jc->statics[(off - 16) / 8];
    }
    (void)size;
    return &dvm_slots(o)[(off - 16) / 8];
}
NAT(U_objectFieldOffset) { (void)self; dvm_field *f = field_of(a[0].l); *ret = J(f ? f->slot * 8 + 16 : 0); return true; }
NAT(U_objectFieldOffset2)
{
    (void)self;
    jobj *cls = a[0].l; const char *n = tl_jni_string(a[1].l);
    dvm_class *c = cls ? dvm_class_of(cls->klass.jc) : NULL;
    dvm_field *f = c && n ? dvm_find_field(c, n, false) : NULL;
    if (!f) return dvm_throw("java/lang/InternalError", "objectFieldOffset %s", n ? n : "?");
    *ret = J(f->slot * 8 + 16);
    return true;
}
NAT(U_arrayBaseOffset) { UNUSED; *ret = I(16); return true; }
NAT(U_arrayIndexScale)
{
    (void)self;
    jobj *cls = a[0].l;
    const char *n = cls ? cls->klass.jc->name : "[L";
    int s = n[1] == 'J' || n[1] == 'D' || n[1] == 'L' || n[1] == '[' ? 8 : n[1] == 'I' || n[1] == 'F' ? 4 : n[1] == 'C' || n[1] == 'S' ? 2 : 1;
    *ret = I(s);
    return true;
}
NAT(U_getInt) { (void)self; *ret = I(*(int32_t *)unsafe_place(a[0].l, a[1].j, 4)); return true; }
NAT(U_putInt) { (void)self; (void)ret; *(int32_t *)unsafe_place(a[0].l, a[1].j, 4) = a[2].i; return true; }
NAT(U_getLong) { (void)self; *ret = J(*(int64_t *)unsafe_place(a[0].l, a[1].j, 8)); return true; }
NAT(U_putLong) { (void)self; (void)ret; *(int64_t *)unsafe_place(a[0].l, a[1].j, 8) = a[2].j; return true; }
NAT(U_getObject) { (void)self; *ret = L(*(void **)unsafe_place(a[0].l, a[1].j, 8)); return true; }
NAT(U_putObject) { (void)self; (void)ret; *(void **)unsafe_place(a[0].l, a[1].j, 8) = a[2].l; return true; }
NAT(U_getBoolean) { (void)self; *ret = Z(*(uint8_t *)unsafe_place(a[0].l, a[1].j, 1)); return true; }
NAT(U_putBoolean) { (void)self; (void)ret; *(uint8_t *)unsafe_place(a[0].l, a[1].j, 1) = a[2].z; return true; }
NAT(U_getByte) { (void)self; *ret = I(*(int8_t *)unsafe_place(a[0].l, a[1].j, 1)); return true; }
NAT(U_putByte) { (void)self; (void)ret; *(int8_t *)unsafe_place(a[0].l, a[1].j, 1) = a[2].b; return true; }
NAT(U_casInt)
{
    (void)self;
    _Atomic int32_t *p = (_Atomic int32_t *)unsafe_place(a[0].l, a[1].j, 4);
    int32_t expect = a[2].i;
    *ret = Z(atomic_compare_exchange_strong(p, &expect, a[3].i));
    return true;
}
NAT(U_casLong)
{
    (void)self;
    _Atomic int64_t *p = (_Atomic int64_t *)unsafe_place(a[0].l, a[1].j, 8);
    int64_t expect = a[2].j;
    *ret = Z(atomic_compare_exchange_strong(p, &expect, a[3].j));
    return true;
}
NAT(U_casObject)
{
    (void)self;
    _Atomic(void *) *p = (_Atomic(void *) *)unsafe_place(a[0].l, a[1].j, 8);
    void *expect = a[2].l;
    *ret = Z(atomic_compare_exchange_strong(p, &expect, a[3].l));
    return true;
}
NAT(U_getAndAddInt) { (void)self; *ret = I(atomic_fetch_add((_Atomic int32_t *)unsafe_place(a[0].l, a[1].j, 4), a[2].i)); return true; }
NAT(U_getAndAddLong) { (void)self; *ret = J(atomic_fetch_add((_Atomic int64_t *)unsafe_place(a[0].l, a[1].j, 8), a[2].j)); return true; }
NAT(U_getAndSetInt) { (void)self; *ret = I(atomic_exchange((_Atomic int32_t *)unsafe_place(a[0].l, a[1].j, 4), a[2].i)); return true; }
NAT(U_getAndSetLong) { (void)self; *ret = J(atomic_exchange((_Atomic int64_t *)unsafe_place(a[0].l, a[1].j, 8), a[2].j)); return true; }
NAT(U_getAndSetObject) { (void)self; *ret = L(atomic_exchange((_Atomic(void *) *)unsafe_place(a[0].l, a[1].j, 8), a[2].l)); return true; }
NAT(U_fence) { UNUSED; atomic_thread_fence(memory_order_seq_cst); return true; }
NAT(U_allocateMemory) { (void)self; *ret = J((int64_t)(uintptr_t)malloc((size_t)a[0].j)); return true; }
NAT(U_freeMemory) { (void)self; (void)ret; free((void *)(uintptr_t)a[0].j); return true; }
NAT(U_park)
{
    (void)self; (void)ret;
    /* park(isAbsolute, time): a short sleep is a correct (if lazy) park -- callers re-check their condition in a loop */
    int64_t t = a[1].j;
    if (a[0].z) { struct timeval tv; gettimeofday(&tv, NULL); t = (t - ((int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000)) * 1000000; }
    if (t <= 0 || t > 2000000) t = 2000000;
    struct timespec ts = { 0, (long)t }; nanosleep(&ts, NULL);
    return true;
}
NAT(U_unpark) { UNUSED; return true; }
NAT(U_allocateInstance)
{
    (void)self;
    jobj *cls = a[0].l;
    if (!cls) return npe("class");
    jobj *o = dvm_new_object(cls->klass.jc); dvm_slots(o);
    *ret = L(o);
    return true;
}


NAT(U_addressSize) { UNUSED; *ret = I(8); return true; }
NAT(U_pageSize) { UNUSED; *ret = I(16384); return true; }
NAT(U_componentScale)
{
    (void)self;
    jobj *cls = a[0].l;
    dvm_class *c = cls ? dvm_class_of(cls->klass.jc) : NULL;
    char p = c ? c->prim : 0;
    *ret = I(p == 'J' || p == 'D' || !p ? 8 : p == 'I' || p == 'F' ? 4 : p == 'C' || p == 'S' ? 2 : 1);
    return true;
}
NAT(U_casxLong)
{
    (void)self;
    _Atomic int64_t *p = (_Atomic int64_t *)unsafe_place(a[0].l, a[1].j, 8);
    int64_t expect = a[2].j;
    atomic_compare_exchange_strong(p, &expect, a[3].j);
    *ret = J(expect);
    return true;
}
NAT(U_copyMemory) { (void)self; (void)ret; memmove((void *)(uintptr_t)a[1].j, (void *)(uintptr_t)a[0].j, (size_t)a[2].j); return true; }
NAT(U_copyMemory0) { (void)self; (void)ret; memmove(unsafe_place(a[2].l, a[3].j, 1), unsafe_place(a[0].l, a[1].j, 1), (size_t)a[4].j); return true; }
NAT(U_copyFromArray) { (void)self; (void)ret; memmove((void *)(uintptr_t)a[2].j, unsafe_place(a[0].l, a[1].j, 1), (size_t)a[3].j); return true; }
NAT(U_copyToArray) { (void)self; (void)ret; memmove(unsafe_place(a[1].l, a[2].j, 1), (void *)(uintptr_t)a[0].j, (size_t)a[3].j); return true; }
NAT(U_setMemory) { (void)self; (void)ret; memset((void *)(uintptr_t)a[0].j, a[2].b, (size_t)a[1].j); return true; }
NAT(U_getChar) { (void)self; *ret = I(*(uint16_t *)unsafe_place(a[0].l, a[1].j, 2)); return true; }
NAT(U_putChar) { (void)self; (void)ret; *(uint16_t *)unsafe_place(a[0].l, a[1].j, 2) = a[2].c; return true; }
NAT(U_getShort) { (void)self; *ret = I(*(int16_t *)unsafe_place(a[0].l, a[1].j, 2)); return true; }
NAT(U_putShort) { (void)self; (void)ret; *(int16_t *)unsafe_place(a[0].l, a[1].j, 2) = a[2].s; return true; }
NAT(U_getFloat) { (void)self; *ret = F(*(float *)unsafe_place(a[0].l, a[1].j, 4)); return true; }
NAT(U_putFloat) { (void)self; (void)ret; *(float *)unsafe_place(a[0].l, a[1].j, 4) = a[2].f; return true; }
NAT(U_getDouble) { (void)self; *ret = D(*(double *)unsafe_place(a[0].l, a[1].j, 8)); return true; }
NAT(U_putDouble) { (void)self; (void)ret; *(double *)unsafe_place(a[0].l, a[1].j, 8) = a[2].d; return true; }
/* raw addresses */
NAT(U_rgetByte) { (void)self; *ret = I(*(int8_t *)(uintptr_t)a[0].j); return true; }
NAT(U_rputByte) { (void)self; (void)ret; *(int8_t *)(uintptr_t)a[0].j = a[1].b; return true; }
NAT(U_rgetChar) { (void)self; *ret = I(*(uint16_t *)(uintptr_t)a[0].j); return true; }
NAT(U_rputChar) { (void)self; (void)ret; *(uint16_t *)(uintptr_t)a[0].j = a[1].c; return true; }
NAT(U_rgetShort) { (void)self; *ret = I(*(int16_t *)(uintptr_t)a[0].j); return true; }
NAT(U_rputShort) { (void)self; (void)ret; *(int16_t *)(uintptr_t)a[0].j = a[1].s; return true; }
NAT(U_rgetInt) { (void)self; *ret = I(*(int32_t *)(uintptr_t)a[0].j); return true; }
NAT(U_rputInt) { (void)self; (void)ret; *(int32_t *)(uintptr_t)a[0].j = a[1].i; return true; }
NAT(U_rgetLong) { (void)self; *ret = J(*(int64_t *)(uintptr_t)a[0].j); return true; }
NAT(U_rputLong) { (void)self; (void)ret; *(int64_t *)(uintptr_t)a[0].j = a[1].j; return true; }
NAT(U_rgetFloat) { (void)self; *ret = F(*(float *)(uintptr_t)a[0].j); return true; }
NAT(U_rputFloat) { (void)self; (void)ret; *(float *)(uintptr_t)a[0].j = a[1].f; return true; }
NAT(U_rgetDouble) { (void)self; *ret = D(*(double *)(uintptr_t)a[0].j); return true; }
NAT(U_rputDouble) { (void)self; (void)ret; *(double *)(uintptr_t)a[0].j = a[1].d; return true; }

/* ================================================================== Reference, Array, VMStack, ClassLoader */

NAT(Ref_get)
{
    (void)a;
    dvm_class *c = dvm_class_of(dvm_object_class(self));
    dvm_field *f = c ? dvm_find_field(c, "referent", false) : NULL;
    *ret = L(f ? dvm_slots(self)[f->slot].l : NULL);
    return true;
}
NAT(Ref_clear)
{
    UNUSED;
    dvm_class *c = dvm_class_of(dvm_object_class(self));
    dvm_field *f = c ? dvm_find_field(c, "referent", false) : NULL;
    if (f) dvm_slots(self)[f->slot].l = NULL;
    return true;
}
NAT(Ref_refersTo)
{
    dvm_class *c = dvm_class_of(dvm_object_class(self));
    dvm_field *f = c ? dvm_find_field(c, "referent", false) : NULL;
    *ret = Z(f && dvm_slots(self)[f->slot].l == a[0].l);
    return true;
}
NAT(Array_createObjectArray)
{
    (void)self;
    jobj *cls = a[0].l;
    jobj *arr = tl_jni_new_obj_array(cls->klass.jc, (uint32_t)a[1].i);
    char nm[400]; snprintf(nm, sizeof(nm), cls->klass.jc->name[0] == '[' ? "[%s" : "[L%s;", cls->klass.jc->name);
    arr->cls = tl_jni_class(nm);
    arr->refs = 1u << 30;
    *ret = L(arr);
    return true;
}
NAT(VMStack_null) { UNUSED; *ret = L(NULL); return true; }
/* getStackClass2: the caller of the method that called the method asking (frames: getStackClass2, getCallerClass, its caller, X). */
NAT(VMStack_getStackClass2)
{
    UNUSED;
    int i = t_depth - 4;
    *ret = L(i >= 0 && i < 8192 && t_frames[i] ? t_frames[i]->cls->jc->mirror : NULL);
    return true;
}
NAT(VMStack_fill) { UNUSED; *ret = I(0); return true; }
NAT(VMCL_findLoaded)
{
    (void)self;
    const char *n = tl_jni_string(a[1].l);
    if (!n) { *ret = L(NULL); return true; }
    char jn[512]; snprintf(jn, sizeof(jn), "%s", n);
    for (char *p = jn; *p; p++) if (*p == '.') *p = '/';
    tl_jclass *jc = dvm_class_named(jn);
    *ret = L(jc ? jc->mirror : NULL);
    return true;
}
NAT(VMCL_bootEntries) { UNUSED; *ret = L(string_array(NULL, 0)); return true; }

/* ================================================================== the table */

typedef struct { const char *cls, *name, *sig; dvm_native_fn fn; } entry;
static const entry k_natives[] = {
    { "java/lang/Object", "getClass", "()Ljava/lang/Class;", Object_getClass },
    { "java/lang/Object", "identityHashCodeNative", "(Ljava/lang/Object;)I", Object_identityHashCode },
    { "java/lang/Object", "internalClone", "()Ljava/lang/Object;", Object_internalClone },
    { "java/lang/Object", "notify", "()V", Object_notify },
    { "java/lang/Object", "notifyAll", "()V", Object_notifyAll },
    { "java/lang/Object", "wait", "(JI)V", Object_wait },

    { "java/lang/String", "charAt", "(I)C", String_charAt },
    { "java/lang/String", "compareTo", "(Ljava/lang/String;)I", String_compareTo },
    { "java/lang/String", "concat", "(Ljava/lang/String;)Ljava/lang/String;", String_concat },
    { "java/lang/String", "doRepeat", "(I)Ljava/lang/String;", String_doRepeat },
    { "java/lang/String", "doReplace", "(CC)Ljava/lang/String;", String_doReplace },
    { "java/lang/String", "fastSubstring", "(II)Ljava/lang/String;", String_fastSubstring },
    { "java/lang/String", "fillBytesLatin1", "([BI)V", String_fillBytesLatin1 },
    { "java/lang/String", "fillBytesUTF16", "([BI)V", String_fillBytesUTF16 },
    { "java/lang/String", "getCharsNoCheck", "(II[CI)V", String_getCharsNoCheck },
    { "java/lang/String", "intern", "()Ljava/lang/String;", String_intern },
    { "java/lang/String", "toCharArray", "()[C", String_toCharArray },
    { "java/lang/StringFactory", "newStringFromBytes", "([BIII)Ljava/lang/String;", SF_fromBytes },
    { "java/lang/StringFactory", "newStringFromChars", "(II[C)Ljava/lang/String;", SF_fromChars },
    { "java/lang/StringFactory", "newStringFromString", "(Ljava/lang/String;)Ljava/lang/String;", SF_fromString },
    { "java/lang/StringFactory", "newStringFromUtf16Bytes", "([BII)Ljava/lang/String;", SF_fromUtf16Bytes },
    { "java/lang/StringFactory", "newStringFromUtf8Bytes", "([BII)Ljava/lang/String;", SF_fromUtf8Bytes },

    { "java/lang/System", "arraycopy", "(Ljava/lang/Object;ILjava/lang/Object;II)V", System_arraycopy },
    { "java/lang/System", "currentTimeMillis", "()J", System_currentTimeMillis },
    { "java/lang/System", "nanoTime", "()J", System_nanoTime },
    { "java/lang/System", "log", "(CLjava/lang/String;Ljava/lang/Throwable;)V", System_log },
    { "java/lang/System", "mapLibraryName", "(Ljava/lang/String;)Ljava/lang/String;", System_mapLibraryName },
    { "java/lang/System", "specialProperties", "()[Ljava/lang/String;", System_specialProperties },
    { "java/lang/System", "setIn0", "(Ljava/io/InputStream;)V", System_setStream },
    { "java/lang/System", "setOut0", "(Ljava/io/PrintStream;)V", System_setStream },
    { "java/lang/System", "setErr0", "(Ljava/io/PrintStream;)V", System_setStream },

    { "dalvik/system/VMRuntime", "properties", "()[Ljava/lang/String;", VMR_properties },
    { "dalvik/system/VMRuntime", "is64Bit", "()Z", VMR_true },
    { "dalvik/system/VMRuntime", "vmInstructionSet", "()Ljava/lang/String;", VMR_isa },
    { "dalvik/system/VMRuntime", "getCurrentInstructionSet", "()Ljava/lang/String;", VMR_isa },
    { "dalvik/system/VMRuntime", "vmVersion", "()Ljava/lang/String;", VMR_vmVersion },
    { "dalvik/system/VMRuntime", "vmLibrary", "()Ljava/lang/String;", VMR_vmLibrary },
    { "dalvik/system/VMRuntime", "getTargetHeapUtilization", "()F", VMR_heapUtil },
    { "dalvik/system/VMRuntime", "getSdkVersionNative", "(I)I", VMR_sdk },
    { "dalvik/system/VMRuntime", "getIntSystemProperty", "(Ljava/lang/String;I)I", VMR_intProp },
    { "dalvik/system/VMRuntime", "newNonMovableArray", "(Ljava/lang/Class;I)Ljava/lang/Object;", VMR_newArray },
    { "dalvik/system/VMRuntime", "newUnpaddedArray", "(Ljava/lang/Class;I)Ljava/lang/Object;", VMR_newArray },
    { "dalvik/system/VMRuntime", "addressOf", "(Ljava/lang/Object;)J", VMR_addressOf },
    { "dalvik/system/VMRuntime", "classPath", "()Ljava/lang/String;", VMR_classPath },
    { "dalvik/system/VMRuntime", "getNotifyNativeInterval", "()I", VMR_notifyInterval },
    { "dalvik/system/VMRuntime", "getFinalizerTimeoutMs", "()J", VMR_finalizerTimeout },
    { "dalvik/system/VMRuntime", "bootClassPath", "()Ljava/lang/String;", VMR_classPath },

    { "java/lang/Thread", "currentThread", "()Ljava/lang/Thread;", Thread_currentThread },
    { "java/lang/Thread", "currentCarrierThread", "()Ljava/lang/Thread;", Thread_currentThread },
    { "java/lang/Thread", "sleep", "(Ljava/lang/Object;JI)V", Thread_sleep },
    { "java/lang/Thread", "interrupted", "()Z", Thread_false },
    { "java/lang/Thread", "isInterrupted", "()Z", Thread_false },
    { "java/lang/Thread", "interrupt0", "()V", Thread_noop },
    { "java/lang/Thread", "holdsLock", "(Ljava/lang/Object;)Z", Thread_holdsLock },
    { "java/lang/Thread", "nativeCreate", "(Ljava/lang/Thread;JZ)V", Thread_nativeCreate },
    { "java/lang/Thread", "nativeGetStatus", "(Z)I", Thread_status },
    { "java/lang/Thread", "nicenessForPriority", "(I)I", Thread_nice },
    { "java/lang/Thread", "setNativeName", "(Ljava/lang/String;)V", Thread_noop },
    { "java/lang/Thread", "setPriority0", "(II)V", Thread_noop },
    { "java/lang/Thread", "setNiceness0", "(I)I", Thread_noop },
    { "java/lang/Thread", "yield0", "()V", Thread_noop },

    { "java/lang/Throwable", "nativeFillInStackTrace", "()Ljava/lang/Object;", Throwable_fill },
    { "java/lang/Throwable", "nativeGetStackTrace", "(Ljava/lang/Object;)[Ljava/lang/StackTraceElement;", Throwable_getStack },

    { "java/lang/Runtime", "freeMemory", "()J", Runtime_freeMemory },
    { "java/lang/Runtime", "totalMemory", "()J", Runtime_maxMemory },
    { "java/lang/Runtime", "maxMemory", "()J", Runtime_maxMemory },
    { "java/lang/Runtime", "nativeGc", "()V", Runtime_gc },
    { "java/lang/Runtime", "nativeExit", "(I)V", Runtime_exit },
    { "java/lang/Runtime", "nativeLoad", "(Ljava/lang/String;Ljava/lang/ClassLoader;)Ljava/lang/String;", Runtime_nativeLoad },

    { "java/lang/Float", "floatToRawIntBits", "(F)I", Float_toBits },
    { "java/lang/Float", "intBitsToFloat", "(I)F", Float_fromBits },
    { "java/lang/Double", "doubleToRawLongBits", "(D)J", Double_toBits },
    { "java/lang/Double", "longBitsToDouble", "(J)D", Double_fromBits },
    { "java/lang/Math", "sin", "(D)D", M_sin }, { "java/lang/Math", "cos", "(D)D", M_cos }, { "java/lang/Math", "tan", "(D)D", M_tan },
    { "java/lang/Math", "asin", "(D)D", M_asin }, { "java/lang/Math", "acos", "(D)D", M_acos }, { "java/lang/Math", "atan", "(D)D", M_atan },
    { "java/lang/Math", "exp", "(D)D", M_exp }, { "java/lang/Math", "log", "(D)D", M_log }, { "java/lang/Math", "log10", "(D)D", M_log10 },
    { "java/lang/Math", "sqrt", "(D)D", M_sqrt }, { "java/lang/Math", "cbrt", "(D)D", M_cbrt }, { "java/lang/Math", "ceil", "(D)D", M_ceil },
    { "java/lang/Math", "floor", "(D)D", M_floor }, { "java/lang/Math", "rint", "(D)D", M_rint }, { "java/lang/Math", "sinh", "(D)D", M_sinh },
    { "java/lang/Math", "cosh", "(D)D", M_cosh }, { "java/lang/Math", "tanh", "(D)D", M_tanh }, { "java/lang/Math", "expm1", "(D)D", M_expm1 },
    { "java/lang/Math", "log1p", "(D)D", M_log1p }, { "java/lang/Math", "atan2", "(DD)D", M_atan2 }, { "java/lang/Math", "pow", "(DD)D", M_pow },
    { "java/lang/Math", "hypot", "(DD)D", M_hypot }, { "java/lang/Math", "IEEEremainder", "(DD)D", M_IEEEremainder },
    { "java/lang/Math", "nextAfter", "(DD)D", M_nextafter },
    { "java/lang/StrictMath", "sin", "(D)D", M_sin }, { "java/lang/StrictMath", "cos", "(D)D", M_cos }, { "java/lang/StrictMath", "tan", "(D)D", M_tan },
    { "java/lang/StrictMath", "asin", "(D)D", M_asin }, { "java/lang/StrictMath", "acos", "(D)D", M_acos }, { "java/lang/StrictMath", "atan", "(D)D", M_atan },
    { "java/lang/StrictMath", "exp", "(D)D", M_exp }, { "java/lang/StrictMath", "log", "(D)D", M_log }, { "java/lang/StrictMath", "log10", "(D)D", M_log10 },
    { "java/lang/StrictMath", "sqrt", "(D)D", M_sqrt }, { "java/lang/StrictMath", "cbrt", "(D)D", M_cbrt },
    { "java/lang/StrictMath", "sinh", "(D)D", M_sinh }, { "java/lang/StrictMath", "cosh", "(D)D", M_cosh }, { "java/lang/StrictMath", "tanh", "(D)D", M_tanh },
    { "java/lang/StrictMath", "expm1", "(D)D", M_expm1 }, { "java/lang/StrictMath", "log1p", "(D)D", M_log1p },
    { "java/lang/StrictMath", "atan2", "(DD)D", M_atan2 }, { "java/lang/StrictMath", "pow", "(DD)D", M_pow },
    { "java/lang/StrictMath", "hypot", "(DD)D", M_hypot }, { "java/lang/StrictMath", "IEEEremainder", "(DD)D", M_IEEEremainder },

    { "java/lang/Class", "getNameNative", "()Ljava/lang/String;", Class_getNameNative },
    { "java/lang/Class", "classForName", "(Ljava/lang/String;ZLjava/lang/ClassLoader;)Ljava/lang/Class;", Class_classForName },
    { "java/lang/Class", "getPrimitiveClass", "(Ljava/lang/String;)Ljava/lang/Class;", Class_getPrimitiveClass },
    { "java/lang/Class", "getInterfacesInternal", "()[Ljava/lang/Class;", Class_getInterfacesInternal },
    { "java/lang/Class", "newInstance", "()Ljava/lang/Object;", Class_newInstance },
    { "java/lang/Class", "getDeclaredField", "(Ljava/lang/String;)Ljava/lang/reflect/Field;", Class_getDeclaredField },
    { "java/lang/Class", "getDeclaredFields", "()[Ljava/lang/reflect/Field;", Class_getDeclaredFields },
    { "java/lang/Class", "getDeclaredFieldsUnchecked", "(Z)[Ljava/lang/reflect/Field;", Class_getDeclaredFields },
    { "java/lang/Class", "getSimpleNameNative", "()Ljava/lang/String;", Class_getSimpleNameNative },
    { "java/lang/Class", "getDeclaringClass", "()Ljava/lang/Class;", Class_null },
    { "java/lang/Class", "getEnclosingClass", "()Ljava/lang/Class;", Class_null },
    { "java/lang/Class", "getSignatureAnnotation", "()[Ljava/lang/String;", Class_null },
    { "java/lang/Class", "getDeclaredAnnotation", "(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;", Class_null },
    { "java/lang/Class", "isDeclaredAnnotationPresent", "(Ljava/lang/Class;)Z", Class_false },
    { "java/lang/Class", "isAnonymousClass", "()Z", Class_false },
    { "java/lang/Class", "isRecord0", "()Z", Class_false },
    { "java/lang/Class", "getInnerClassFlags", "(I)I", Class_zero },

    { "java/lang/Class", "getDeclaredConstructorInternal", "([Ljava/lang/Class;)Ljava/lang/reflect/Constructor;", Class_getDeclaredConstructorInternal },
    { "java/lang/Class", "getDeclaredConstructorsInternal", "(Z)[Ljava/lang/reflect/Constructor;", Class_getDeclaredConstructorsInternal },
    { "java/lang/Class", "getDeclaredMethodInternal", "(Ljava/lang/String;[Ljava/lang/Class;)Ljava/lang/reflect/Method;", Class_getDeclaredMethodInternal },
    { "java/lang/Class", "getDeclaredMethodsUnchecked", "(Z)[Ljava/lang/reflect/Method;", Class_getDeclaredMethodsUnchecked },
    { "java/lang/Class", "getPublicDeclaredFields", "()[Ljava/lang/reflect/Field;", Class_getPublicDeclaredFields },
    { "java/lang/Class", "getPublicFieldRecursive", "(Ljava/lang/String;)Ljava/lang/reflect/Field;", Class_getPublicFieldRecursive },
    { "java/lang/Class", "getDeclaredAnnotations", "()[Ljava/lang/annotation/Annotation;", Exe_emptyAnnotations },
    { "java/lang/Class", "getDeclaredClasses", "()[Ljava/lang/Class;", Exe_noClasses },
    { "java/lang/reflect/Executable", "getParameterTypesInternal", "()[Ljava/lang/Class;", Exe_getParameterTypes },
    { "java/lang/reflect/Executable", "getParameterCountInternal", "()I", Exe_getParameterCount },
    { "java/lang/reflect/Executable", "getMethodNameInternal", "()Ljava/lang/String;", Exe_getName },
    { "java/lang/reflect/Executable", "getMethodReturnTypeInternal", "()Ljava/lang/Class;", Exe_getReturnType },
    { "java/lang/reflect/Executable", "compareMethodParametersInternal", "(Ljava/lang/reflect/Method;)I", Exe_compareParams },
    { "java/lang/reflect/Executable", "getDeclaredAnnotationsNative", "()[Ljava/lang/annotation/Annotation;", Exe_emptyAnnotations },
    { "java/lang/reflect/Executable", "getAnnotationNative", "(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;", Class_null },
    { "java/lang/reflect/Executable", "isAnnotationPresentNative", "(Ljava/lang/Class;)Z", Class_false },
    { "java/lang/reflect/Executable", "getParameterAnnotationsNative", "()[[Ljava/lang/annotation/Annotation;", Exe_paramAnnotations },
    { "java/lang/reflect/Executable", "getSignatureAnnotation", "()[Ljava/lang/String;", Class_null },
    { "java/lang/reflect/Executable", "getParameters0", "()[Ljava/lang/reflect/Parameter;", Class_null },
    { "java/lang/reflect/Method", "getExceptionTypes", "()[Ljava/lang/Class;", Exe_noClasses },
    { "java/lang/reflect/Constructor", "getExceptionTypes", "()[Ljava/lang/Class;", Exe_noClasses },
    { "java/lang/reflect/Method", "invoke", "(Ljava/lang/Object;[Ljava/lang/Object;)Ljava/lang/Object;", Method_invoke },
    { "java/lang/reflect/Method", "getDefaultValue", "()Ljava/lang/Object;", Class_null },
    { "java/lang/reflect/Constructor", "newInstance0", "([Ljava/lang/Object;)Ljava/lang/Object;", Ctor_newInstance0 },
    { "java/lang/reflect/Field", "getDeclaredAnnotations", "()[Ljava/lang/annotation/Annotation;", Exe_emptyAnnotations },
    { "java/lang/reflect/Field", "getAnnotationNative", "(Ljava/lang/Class;)Ljava/lang/annotation/Annotation;", Class_null },
    { "java/lang/reflect/Field", "isAnnotationPresentNative", "(Ljava/lang/Class;)Z", Class_false },
    { "java/lang/reflect/Field", "getSignatureAnnotation", "()[Ljava/lang/String;", Class_null },
    { "java/lang/reflect/Field", "getNameInternal", "()Ljava/lang/String;", Field_getName },
    { "java/lang/reflect/Field", "getArtField", "()J", Field_getArtField },
    { "java/lang/reflect/Field", "isMonotonic0", "()Z", Class_false },
    { "java/lang/reflect/Field", "getByte", "(Ljava/lang/Object;)B", Field_getByte },
    { "java/lang/reflect/Field", "getChar", "(Ljava/lang/Object;)C", Field_getChar },
    { "java/lang/reflect/Field", "getShort", "(Ljava/lang/Object;)S", Field_getShort },
    { "java/lang/reflect/Field", "getFloat", "(Ljava/lang/Object;)F", Field_getFloat },
    { "java/lang/reflect/Field", "getDouble", "(Ljava/lang/Object;)D", Field_getDouble },
    { "java/lang/reflect/Field", "setByte", "(Ljava/lang/Object;B)V", Field_setByte },
    { "java/lang/reflect/Field", "setChar", "(Ljava/lang/Object;C)V", Field_setChar },
    { "java/lang/reflect/Field", "setShort", "(Ljava/lang/Object;S)V", Field_setShort },
    { "java/lang/reflect/Field", "setFloat", "(Ljava/lang/Object;F)V", Field_setFloat },
    { "java/lang/reflect/Field", "setDouble", "(Ljava/lang/Object;D)V", Field_setDouble },
    { "java/lang/reflect/Field", "get", "(Ljava/lang/Object;)Ljava/lang/Object;", Field_get },
    { "java/lang/reflect/Field", "set", "(Ljava/lang/Object;Ljava/lang/Object;)V", Field_set },
    { "java/lang/reflect/Field", "getInt", "(Ljava/lang/Object;)I", Field_getInt },
    { "java/lang/reflect/Field", "getLong", "(Ljava/lang/Object;)J", Field_getLong },
    { "java/lang/reflect/Field", "getBoolean", "(Ljava/lang/Object;)Z", Field_getBoolean },
    { "java/lang/reflect/Field", "setInt", "(Ljava/lang/Object;I)V", Field_setInt },
    { "java/lang/reflect/Field", "setLong", "(Ljava/lang/Object;J)V", Field_setLong },
    { "java/lang/reflect/Field", "setBoolean", "(Ljava/lang/Object;Z)V", Field_setBoolean },

    { "java/lang/ref/Reference", "getReferent", "()Ljava/lang/Object;", Ref_get },
    { "java/lang/ref/Reference", "clearReferent", "()V", Ref_clear },
    { "java/lang/ref/Reference", "refersTo0", "(Ljava/lang/Object;)Z", Ref_refersTo },
    { "java/lang/reflect/Array", "createObjectArray", "(Ljava/lang/Class;I)Ljava/lang/Object;", Array_createObjectArray },
    { "dalvik/system/VMStack", "getCallingClassLoader", "()Ljava/lang/ClassLoader;", VMStack_null },
    { "dalvik/system/VMStack", "getClosestUserClassLoader", "()Ljava/lang/ClassLoader;", VMStack_null },
    { "dalvik/system/VMStack", "getStackClass2", "()Ljava/lang/Class;", VMStack_getStackClass2 },
    { "dalvik/system/VMStack", "fillStackTraceElements", "(Ljava/lang/Thread;[Ljava/lang/StackTraceElement;)I", VMStack_fill },
    { "java/lang/VMClassLoader", "findLoadedClass", "(Ljava/lang/ClassLoader;Ljava/lang/String;)Ljava/lang/Class;", VMCL_findLoaded },
    { "java/lang/VMClassLoader", "getBootClassPathEntries", "()[Ljava/lang/String;", VMCL_bootEntries },
    { NULL, NULL, NULL, NULL },
};

/* Unsafe exists twice (sun.misc and jdk.internal.misc), with the same natives; matched by name for both. */
static const entry k_unsafe[] = {
    { NULL, "objectFieldOffset0", "(Ljava/lang/reflect/Field;)J", U_objectFieldOffset },
    { NULL, "objectFieldOffset", "(Ljava/lang/reflect/Field;)J", U_objectFieldOffset },
    { NULL, "objectFieldOffset1", "(Ljava/lang/Class;Ljava/lang/String;)J", U_objectFieldOffset2 },
    { NULL, "arrayBaseOffset", "(Ljava/lang/Class;)I", U_arrayBaseOffset },
    { NULL, "arrayIndexScale", "(Ljava/lang/Class;)I", U_arrayIndexScale },
    { NULL, "getInt", "(Ljava/lang/Object;J)I", U_getInt }, { NULL, "getIntVolatile", "(Ljava/lang/Object;J)I", U_getInt },
    { NULL, "putInt", "(Ljava/lang/Object;JI)V", U_putInt }, { NULL, "putIntVolatile", "(Ljava/lang/Object;JI)V", U_putInt },
    { NULL, "putOrderedInt", "(Ljava/lang/Object;JI)V", U_putInt },
    { NULL, "getLong", "(Ljava/lang/Object;J)J", U_getLong }, { NULL, "getLongVolatile", "(Ljava/lang/Object;J)J", U_getLong },
    { NULL, "putLong", "(Ljava/lang/Object;JJ)V", U_putLong }, { NULL, "putLongVolatile", "(Ljava/lang/Object;JJ)V", U_putLong },
    { NULL, "putOrderedLong", "(Ljava/lang/Object;JJ)V", U_putLong },
    { NULL, "getObject", "(Ljava/lang/Object;J)Ljava/lang/Object;", U_getObject },
    { NULL, "getObjectVolatile", "(Ljava/lang/Object;J)Ljava/lang/Object;", U_getObject },
    { NULL, "getReference", "(Ljava/lang/Object;J)Ljava/lang/Object;", U_getObject },
    { NULL, "getReferenceVolatile", "(Ljava/lang/Object;J)Ljava/lang/Object;", U_getObject },
    { NULL, "putObject", "(Ljava/lang/Object;JLjava/lang/Object;)V", U_putObject },
    { NULL, "putObjectVolatile", "(Ljava/lang/Object;JLjava/lang/Object;)V", U_putObject },
    { NULL, "putOrderedObject", "(Ljava/lang/Object;JLjava/lang/Object;)V", U_putObject },
    { NULL, "putReference", "(Ljava/lang/Object;JLjava/lang/Object;)V", U_putObject },
    { NULL, "putReferenceVolatile", "(Ljava/lang/Object;JLjava/lang/Object;)V", U_putObject },
    { NULL, "putReferenceRelease", "(Ljava/lang/Object;JLjava/lang/Object;)V", U_putObject },
    { NULL, "getBoolean", "(Ljava/lang/Object;J)Z", U_getBoolean }, { NULL, "putBoolean", "(Ljava/lang/Object;JZ)V", U_putBoolean },
    { NULL, "getByte", "(Ljava/lang/Object;J)B", U_getByte }, { NULL, "putByte", "(Ljava/lang/Object;JB)V", U_putByte },
    { NULL, "compareAndSwapInt", "(Ljava/lang/Object;JII)Z", U_casInt }, { NULL, "compareAndSetInt", "(Ljava/lang/Object;JII)Z", U_casInt },
    { NULL, "compareAndSwapLong", "(Ljava/lang/Object;JJJ)Z", U_casLong }, { NULL, "compareAndSetLong", "(Ljava/lang/Object;JJJ)Z", U_casLong },
    { NULL, "compareAndSwapObject", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z", U_casObject },
    { NULL, "compareAndSetObject", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z", U_casObject },
    { NULL, "compareAndSetReference", "(Ljava/lang/Object;JLjava/lang/Object;Ljava/lang/Object;)Z", U_casObject },
    { NULL, "getAndAddInt", "(Ljava/lang/Object;JI)I", U_getAndAddInt }, { NULL, "getAndAddLong", "(Ljava/lang/Object;JJ)J", U_getAndAddLong },
    { NULL, "getAndSetInt", "(Ljava/lang/Object;JI)I", U_getAndSetInt }, { NULL, "getAndSetLong", "(Ljava/lang/Object;JJ)J", U_getAndSetLong },
    { NULL, "getAndSetObject", "(Ljava/lang/Object;JLjava/lang/Object;)Ljava/lang/Object;", U_getAndSetObject },
    { NULL, "getAndSetReference", "(Ljava/lang/Object;JLjava/lang/Object;)Ljava/lang/Object;", U_getAndSetObject },
    { NULL, "loadFence", "()V", U_fence }, { NULL, "storeFence", "()V", U_fence }, { NULL, "fullFence", "()V", U_fence },
    { NULL, "allocateMemory", "(J)J", U_allocateMemory }, { NULL, "freeMemory", "(J)V", U_freeMemory },
    { NULL, "park", "(ZJ)V", U_park }, { NULL, "unpark", "(Ljava/lang/Object;)V", U_unpark },
    { NULL, "allocateInstance", "(Ljava/lang/Class;)Ljava/lang/Object;", U_allocateInstance },
    { NULL, "addressSize", "()I", U_addressSize }, { NULL, "pageSize", "()I", U_pageSize },
    { NULL, "getArrayBaseOffsetForComponentType", "(Ljava/lang/Class;)I", U_arrayBaseOffset },
    { NULL, "getArrayIndexScaleForComponentType", "(Ljava/lang/Class;)I", U_componentScale },
    { NULL, "compareAndExchangeLong", "(Ljava/lang/Object;JJJ)J", U_casxLong },
    { NULL, "copyMemory", "(JJJ)V", U_copyMemory }, { NULL, "copyMemory0", "(Ljava/lang/Object;JLjava/lang/Object;JJ)V", U_copyMemory0 },
    { NULL, "copyMemoryFromPrimitiveArray", "(Ljava/lang/Object;JJJ)V", U_copyFromArray },
    { NULL, "copyMemoryToPrimitiveArray", "(JLjava/lang/Object;JJ)V", U_copyToArray },
    { NULL, "setMemory", "(JJB)V", U_setMemory },
    { NULL, "getBooleanVolatile", "(Ljava/lang/Object;J)Z", U_getBoolean }, { NULL, "putBooleanVolatile", "(Ljava/lang/Object;JZ)V", U_putBoolean },
    { NULL, "getByteVolatile", "(Ljava/lang/Object;J)B", U_getByte }, { NULL, "putByteVolatile", "(Ljava/lang/Object;JB)V", U_putByte },
    { NULL, "getChar", "(Ljava/lang/Object;J)C", U_getChar }, { NULL, "getCharVolatile", "(Ljava/lang/Object;J)C", U_getChar },
    { NULL, "putChar", "(Ljava/lang/Object;JC)V", U_putChar }, { NULL, "putCharVolatile", "(Ljava/lang/Object;JC)V", U_putChar },
    { NULL, "getShort", "(Ljava/lang/Object;J)S", U_getShort }, { NULL, "getShortVolatile", "(Ljava/lang/Object;J)S", U_getShort },
    { NULL, "putShort", "(Ljava/lang/Object;JS)V", U_putShort }, { NULL, "putShortVolatile", "(Ljava/lang/Object;JS)V", U_putShort },
    { NULL, "getFloat", "(Ljava/lang/Object;J)F", U_getFloat }, { NULL, "getFloatVolatile", "(Ljava/lang/Object;J)F", U_getFloat },
    { NULL, "putFloat", "(Ljava/lang/Object;JF)V", U_putFloat }, { NULL, "putFloatVolatile", "(Ljava/lang/Object;JF)V", U_putFloat },
    { NULL, "getDouble", "(Ljava/lang/Object;J)D", U_getDouble }, { NULL, "getDoubleVolatile", "(Ljava/lang/Object;J)D", U_getDouble },
    { NULL, "putDouble", "(Ljava/lang/Object;JD)V", U_putDouble }, { NULL, "putDoubleVolatile", "(Ljava/lang/Object;JD)V", U_putDouble },
    { NULL, "getByte", "(J)B", U_rgetByte }, { NULL, "putByte", "(JB)V", U_rputByte }, { NULL, "getChar", "(J)C", U_rgetChar },
    { NULL, "putChar", "(JC)V", U_rputChar }, { NULL, "getShort", "(J)S", U_rgetShort }, { NULL, "putShort", "(JS)V", U_rputShort },
    { NULL, "getInt", "(J)I", U_rgetInt }, { NULL, "putInt", "(JI)V", U_rputInt }, { NULL, "getLong", "(J)J", U_rgetLong },
    { NULL, "putLong", "(JJ)V", U_rputLong }, { NULL, "getFloat", "(J)F", U_rgetFloat }, { NULL, "putFloat", "(JF)V", U_rputFloat },
    { NULL, "getDouble", "(J)D", U_rgetDouble }, { NULL, "putDouble", "(JD)V", U_rputDouble },
    { NULL, NULL, NULL, NULL },
};

dvm_native_fn dvm_intrinsic(const char *cls, const char *name, const char *sig)
{
    for (const entry *e = k_natives; e->cls; e++) if (!strcmp(e->cls, cls) && !strcmp(e->name, name) && !strcmp(e->sig, sig)) return e->fn;
    if (!strcmp(cls, "sun/misc/Unsafe") || !strcmp(cls, "jdk/internal/misc/Unsafe"))
        for (const entry *e = k_unsafe; e->name; e++) if (!strcmp(e->name, name) && !strcmp(e->sig, sig)) return e->fn;
    /* the runtime's housekeeping (heap tuning, debugging hooks): nothing to do here, and zero is the quiet answer */
    if (!strcmp(cls, "dalvik/system/VMRuntime") || !strcmp(cls, "dalvik/system/VMDebug") || !strcmp(cls, "dalvik/system/ZygoteHooks"))
        return VMR_zero;
    return NULL;
}

/* Intrinsics for methods that have bytecode too, but which ART runs natively: much faster, same answers. */
dvm_native_fn dvm_fast_path(const char *cls, const char *name, const char *sig);
extern bool tl_dvm_load_library(const char *name_or_path);
NAT(System_loadLibrary)
{
    (void)self; (void)ret;
    const char *n = tl_jni_string(a[0].l);
    if (!n) return npe("libname");
    char base[300]; snprintf(base, sizeof(base), "lib%s.so", n);
    if (!tl_dvm_load_library(base)) return dvm_throw("java/lang/UnsatisfiedLinkError", "dlopen failed: library \"%s\" not found", base);
    return !tl_jni_pending();
}
NAT(System_load)
{
    (void)self; (void)ret;
    const char *n = tl_jni_string(a[0].l);
    if (!n) return npe("path");
    if (!tl_dvm_load_library(n)) return dvm_throw("java/lang/UnsatisfiedLinkError", "dlopen failed: library \"%s\" not found", n);
    return !tl_jni_pending();
}

dvm_native_fn dvm_fast_path(const char *cls, const char *name, const char *sig)
{
    if (!strcmp(cls, "java/lang/Object") && !strcmp(name, "getClass") && !strcmp(sig, "()Ljava/lang/Class;")) return Object_getClass;
    if (!strcmp(cls, "java/lang/System") && !strcmp(name, "loadLibrary") && !strcmp(sig, "(Ljava/lang/String;)V")) return System_loadLibrary;
    if (!strcmp(cls, "java/lang/System") && !strcmp(name, "load") && !strcmp(sig, "(Ljava/lang/String;)V")) return System_load;
    if (strcmp(cls, "java/lang/String")) return NULL;
    if (!strcmp(name, "length") && !strcmp(sig, "()I")) return String_length;
    if (!strcmp(name, "equals") && !strcmp(sig, "(Ljava/lang/Object;)Z")) return String_equals;
    if (!strcmp(name, "hashCode") && !strcmp(sig, "()I")) return String_hashCode;
    if (!strcmp(name, "indexOf") && !strcmp(sig, "(II)I")) return String_indexOfChar;
    return NULL;
}

void dvm_natives_init(void) {}
