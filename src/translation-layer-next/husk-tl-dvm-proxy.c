/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * java.lang.reflect.Proxy: the classes Proxy.newProxyInstance asks for (Retrofit's services, annotation instances, any dynamic
 * interface implementation), made in the interpreter's own terms rather than from bytecode.
 *
 * libcore's Proxy collects the interfaces' methods and calls the native generateProxy(name, interfaces, loader, methods, exceptions);
 * ART answers with a class that extends Proxy, implements the interfaces, has a constructor taking the InvocationHandler, and for
 * each method a body that boxes the arguments, calls Proxy.invoke(proxy, method, args) (which is h.invoke), and unboxes the result,
 * wrapping a checked exception the method does not declare in UndeclaredThrowableException. Here that class is a dvm_class with
 * no dex: its constructor is an intrinsic, and its methods carry the Method they stand for, which dvm_call sends to proxy_invoke.
 */
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-dvm-internal.h"
#include "husk-tl-jni-internal.h"

void tl_log_line(const char *fmt, ...);

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue L(jobj *o) { jvalue r; r.j = 0; r.l = o; return r; }

static dvm_class *proxy_base(void) { return dvm_class_of(dvm_class_named("java/lang/reflect/Proxy")); }

/* the constructor: Proxy(InvocationHandler h) */
static bool proxy_init(jobj *self, const jvalue *a, jvalue *ret)
{
    ret->j = 0;
    static dvm_field *h;
    if (!h) h = dvm_find_field(proxy_base(), "h", false);
    if (!h) return dvm_throw("java/lang/InternalError", "java.lang.reflect.Proxy has no field h");
    dvm_slots(self)[h->slot] = L(a[0].l);
    return true;
}

/* ---- boxing, as the interpreter's own calls do it */

static const char *box_class(char k)
{
    switch (k) {
    case 'Z': return "java/lang/Boolean"; case 'B': return "java/lang/Byte"; case 'C': return "java/lang/Character";
    case 'S': return "java/lang/Short"; case 'I': return "java/lang/Integer"; case 'J': return "java/lang/Long";
    case 'F': return "java/lang/Float"; case 'D': return "java/lang/Double";
    }
    return NULL;
}

jobj *dvm_box(char k, jvalue v);
jobj *dvm_box(char k, jvalue v)
{
    const char *cls = box_class(k);
    if (!cls) return v.l;
    char sig[64];
    snprintf(sig, sizeof(sig), "(%c)L%s;", k, cls);
    jvalue r;
    if (!tl_dvm_call_static(cls, "valueOf", sig, &v, &r)) return NULL;
    return r.l;
}

/* The value a box holds, for a primitive of kind k: false (pending NPE / ClassCastException) when it is not that box. */
bool dvm_unbox(char k, jobj *o, jvalue *out);
bool dvm_unbox(char k, jobj *o, jvalue *out)
{
    out->j = 0;
    const char *cls = box_class(k);
    if (!cls) { out->l = o; return true; }
    if (!o) return dvm_throw("java/lang/NullPointerException", "null returned for a primitive %c", k);
    tl_jclass *bc = dvm_class_named(cls);
    if (!dvm_instance_of(o, bc)) return dvm_throw("java/lang/ClassCastException", "%s cannot be cast to %s", dvm_object_class(o)->name, cls);
    static const struct { char k; const char *m, *s; } get[] = {
        { 'Z', "booleanValue", "()Z" }, { 'B', "byteValue", "()B" }, { 'C', "charValue", "()C" }, { 'S', "shortValue", "()S" },
        { 'I', "intValue", "()I" }, { 'J', "longValue", "()J" }, { 'F', "floatValue", "()F" }, { 'D', "doubleValue", "()D" },
    };
    for (size_t i = 0; i < sizeof(get) / sizeof(get[0]); i++) {
        if (get[i].k != k) continue;
        dvm_method *m = dvm_find_virtual(bc, get[i].m, get[i].s);
        if (!m) return dvm_throw("java/lang/InternalError", "no %s.%s", cls, get[i].m);
        return dvm_call(m, o, NULL, out);
    }
    return true;
}

/* ---- a call to a proxy's method */

bool dvm_proxy_invoke(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret);
bool dvm_proxy_invoke(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret)
{
    ret->j = 0;
    static dvm_method *invoke;
    if (!invoke) invoke = dvm_find_method(proxy_base(), "invoke", "(Ljava/lang/reflect/Proxy;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;", true);
    if (!invoke) return dvm_throw("java/lang/InternalError", "java.lang.reflect.Proxy has no invoke");
    jobj *args = NULL;
    if (m->nparams > 0) {
        args = tl_jni_new_obj_array(tl_jni_class("java/lang/Object"), (uint32_t)m->nparams);
        args->cls = tl_jni_class("[Ljava/lang/Object;");
        args->refs = 1u << 30;
        for (int i = 0; i < m->nparams; i++) {
            char k = m->shorty[1 + i];
            jvalue v = params[i];
            /* the interpreter hands narrow values over in i */
            if (k == 'Z') { jvalue z; z.j = 0; z.z = v.z; v = z; }
            args->oarr.v[i] = k == 'L' ? v.l : dvm_box(k, v);
            if (k != 'L' && !args->oarr.v[i]) return false;
        }
    }
    jvalue p[3] = { L(self), L(m->proxy_method), L(args) }, r;
    if (!dvm_call(invoke, NULL, p, &r)) {
        /* a checked exception the method does not declare comes out wrapped */
        jobj *e = tl_jni_pending_object();
        tl_jclass *ec = e ? dvm_object_class(e) : NULL;
        bool ok = !ec || dvm_assignable(ec, dvm_class_named("java/lang/RuntimeException")) || dvm_assignable(ec, dvm_class_named("java/lang/Error"));
        for (uint32_t i = 0; !ok && m->proxy_throws && i < m->proxy_throws->oarr.len; i++) {
            jobj *t = m->proxy_throws->oarr.v[i];
            if (t && dvm_assignable(ec, t->klass.jc)) ok = true;
        }
        if (ok) return false;
        tl_jni_clear();
        tl_jclass *ut = dvm_class_named("java/lang/reflect/UndeclaredThrowableException");
        dvm_method *ctor = ut ? dvm_find_method(dvm_class_of(ut), "<init>", "(Ljava/lang/Throwable;)V", false) : NULL;
        if (!ctor) { tl_jni_set_pending(e); return false; }
        jobj *w = dvm_new_object(ut);
        jvalue cp[1] = { L(e) }, cr;
        if (!dvm_call(ctor, w, cp, &cr)) return false;
        tl_jni_set_pending(w);
        return false;
    }
    char rk = m->shorty[0];
    if (rk == 'V') return true;
    if (rk == 'L') { *ret = r; return true; }
    return dvm_unbox(rk, r.l, ret);
}

/* ---- Proxy.generateProxy(String name, Class<?>[] interfaces, ClassLoader loader, Method[] methods, Class<?>[][] exceptions) */

static void method_like(dvm_method *dst, const dvm_method *src)
{
    dst->name = src->name;
    dst->sig = src->sig;
    dst->shorty = src->shorty;
    dst->nparams = src->nparams;
}

dvm_method *dvm_method_of_reflect(jobj *exe);

NAT(Proxy_generateProxy)
{
    (void)self;
    const char *name = tl_jni_string(a[0].l);
    jobj *ifaces = a[1].l, *methods = a[3].l, *throws = a[4].l;
    if (!name || !methods) return dvm_throw("java/lang/NullPointerException", "generateProxy");
    char jname[300];
    snprintf(jname, sizeof(jname), "%s", name);
    for (char *q = jname; *q; q++) if (*q == '.') *q = '/';
    dvm_class *base = proxy_base();
    if (!base) return dvm_throw("java/lang/InternalError", "no java.lang.reflect.Proxy");

    tl_jclass *jc = tl_jni_declare(jname, "java/lang/reflect/Proxy");
    dvm_class *c = calloc(1, sizeof(*c));
    c->jc = jc; c->name = jc->name;
    pthread_mutex_init(&c->init_lock, NULL);
    c->flags = 0x11;                                          /* public final */
    c->super = base;
    jc->super = base->jc;
    uint32_t ni = ifaces ? ifaces->oarr.len : 0;
    c->ifaces = calloc(ni ? ni : 1, sizeof(*c->ifaces));
    for (uint32_t i = 0; i < ni; i++) if (ifaces->oarr.v[i]) c->ifaces[c->nifaces++] = ifaces->oarr.v[i]->klass.jc;
    c->nslots = base->nslots;

    c->dm = calloc(1, sizeof(dvm_method));
    dvm_method *init = &c->dm[c->ndm++];
    init->cls = c; init->name = "<init>"; init->vidx = -1;
    init->sig = "(Ljava/lang/reflect/InvocationHandler;)V";
    init->shorty = "VL";
    init->nparams = 1;
    init->flags = 0x1 | 0x100;                                /* public, native */
    init->intrinsic = proxy_init;

    uint32_t nm = methods->oarr.len;
    c->vm = calloc(nm ? nm : 1, sizeof(dvm_method));
    methods->refs = 1u << 30;
    if (throws) throws->refs = 1u << 30;
    for (uint32_t i = 0; i < nm; i++) {
        dvm_method *src = dvm_method_of_reflect(methods->oarr.v[i]);
        if (!src) continue;
        dvm_method *m = &c->vm[c->nvm++];
        method_like(m, src);
        m->cls = c; m->vidx = -1;
        m->flags = 0x1 | 0x10;                                /* public final */
        m->proxy_method = methods->oarr.v[i];
        m->proxy_throws = throws && i < throws->oarr.len ? throws->oarr.v[i] : NULL;
    }
    /* the vtable: Proxy's (Object's), these replacing what they override, the rest appended */
    c->vtab = calloc((size_t)(base->nvtab + c->nvm + 1), sizeof(*c->vtab));
    if (base->nvtab) memcpy(c->vtab, base->vtab, (size_t)base->nvtab * sizeof(*c->vtab));
    c->nvtab = base->nvtab;
    for (int i = 0; i < c->nvm; i++) {
        dvm_method *m = &c->vm[i];
        int slot = -1;
        for (int k = 0; k < base->nvtab; k++) if (c->vtab[k] && !strcmp(c->vtab[k]->name, m->name) && !strcmp(c->vtab[k]->sig, m->sig)) { slot = k; break; }
        if (slot < 0) slot = c->nvtab++;
        c->vtab[slot] = m;
        m->vidx = slot;
    }
    atomic_store(&c->state, CS_INITIALIZED);
    jc->in_dex = true;
    jc->dvm = c;
    if (g_dvm_trace >= 1) tl_log_line("dvm: proxy %s, %u interfaces, %d methods", jname, ni, c->nvm);
    *ret = L(jc->mirror);
    return true;
}

dvm_native_fn dvm_proxy_native(const char *cls, const char *name, const char *sig);
dvm_native_fn dvm_proxy_native(const char *cls, const char *name, const char *sig)
{
    if (!strcmp(cls, "java/lang/reflect/Proxy") && !strcmp(name, "generateProxy")
        && !strcmp(sig, "(Ljava/lang/String;[Ljava/lang/Class;Ljava/lang/ClassLoader;[Ljava/lang/reflect/Method;[[Ljava/lang/Class;)Ljava/lang/Class;"))
        return Proxy_generateProxy;
    return NULL;
}
