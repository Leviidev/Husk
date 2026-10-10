/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Android's message loops, for the native code that drives itself with them.
 *
 * Unity paces its frames with a Choreographer on a HandlerThread of its own, and runs
 * work on the UI thread through Handlers. None of that is optional: its main thread
 * waits for the first vsync callback, so without a loop that delivers it the game
 * renders two frames and stops.
 *
 *   - A Looper is a queue of events ordered by time and a host thread that runs them.
 *     HandlerThread makes one with a thread of its own; the main looper's thread starts
 *     on first use.
 *   - A Handler posts Runnables and Messages to its Looper; the Looper delivers a
 *     Message to the Handler's Callback and runs a Runnable.
 *   - A Choreographer posts a frame callback for the next 60 Hz boundary.
 *   - Callbacks are Java proxies made by JNIBridge for C# delegates. Calling one means
 *     calling the native JNIBridge.invoke the library registered, with a java.lang.reflect.Method
 *     for the interface method, and the arguments boxed.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void tl_log_line(const char *fmt, ...);

#define NS 1000000000ll
#define VSYNC_NS (NS / 60)

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vj(int64_t j) { jvalue v; v.j = j; return v; }
#define C(name) tl_jni_class(name)

static int64_t now_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * NS + ts.tv_nsec; }

/* ----------------------------------------------------------------- boxing */

static jobj *box(const char *cls, const char *sig, jvalue v)
{
    jobj *o = tl_jni_new_object(C(cls));
    tl_jni_set_field(o, "value", sig, v);
    return o;
}
static jobj *box_long(int64_t j) { return box("java/lang/Long", "J", vj(j)); }

static void Long_init(tl_jcall *c)    { tl_jni_set_field(c->self, "value", "J", c->args[0]); }
static void Long_valueOf(tl_jcall *c) { c->ret = vl(box_long(c->args[0].j)); }
static void Long_value(tl_jcall *c)   { c->ret = tl_jni_get_field(c->self, "value", "J"); }
static void Int_init(tl_jcall *c)     { tl_jni_set_field(c->self, "value", "I", c->args[0]); }
static void Int_valueOf(tl_jcall *c)  { c->ret = vl(box("java/lang/Integer", "I", c->args[0])); }
static void Int_value(tl_jcall *c)    { c->ret = tl_jni_get_field(c->self, "value", "I"); }
static void Bool_init(tl_jcall *c)    { tl_jni_set_field(c->self, "value", "Z", c->args[0]); }
static void Bool_valueOf(tl_jcall *c) { c->ret = vl(box("java/lang/Boolean", "Z", c->args[0])); }
static void Bool_value(tl_jcall *c)   { c->ret = tl_jni_get_field(c->self, "value", "Z"); }
static void Float_init(tl_jcall *c)   { tl_jni_set_field(c->self, "value", "F", c->args[0]); }
static void Float_valueOf(tl_jcall *c){ c->ret = vl(box("java/lang/Float", "F", c->args[0])); }
static void Float_value(tl_jcall *c)  { c->ret = tl_jni_get_field(c->self, "value", "F"); }
static void Double_init(tl_jcall *c)  { tl_jni_set_field(c->self, "value", "D", c->args[0]); }
static void Double_valueOf(tl_jcall *c){ c->ret = vl(box("java/lang/Double", "D", c->args[0])); }
static void Double_value(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "value", "D"); }

/* ------------------------------------------------------------ reflection */

/* The Class a JNI type descriptor names, and how many characters of `d` it took. Primitives are classes named "int" and so on. */
static jobj *class_for_desc(const char *d, int *len)
{
    static const struct { char c; const char *name; } prim[] = {
        { 'Z', "boolean" }, { 'B', "byte" }, { 'C', "char" }, { 'S', "short" }, { 'I', "int" }, { 'J', "long" },
        { 'F', "float" }, { 'D', "double" }, { 'V', "void" },
    };
    for (size_t i = 0; i < sizeof(prim) / sizeof(prim[0]); i++) if (d[0] == prim[i].c) { *len = 1; return tl_jni_class_object(prim[i].name); }
    if (d[0] == 'L') {
        const char *e = strchr(d, ';');
        char name[200]; size_t n = e ? (size_t)(e - d - 1) : 0;
        if (!e || n >= sizeof(name)) { *len = 1; return tl_jni_class_object("java/lang/Object"); }
        memcpy(name, d + 1, n); name[n] = 0;
        *len = (int)(e - d) + 1;
        return tl_jni_class_object(name);
    }
    if (d[0] == '[') {
        int inner; class_for_desc(d + 1, &inner);
        *len = 1 + inner;
        char name[200]; snprintf(name, sizeof(name), "%.*s", *len, d);
        return tl_jni_class_object(name);
    }
    *len = 1;
    return tl_jni_class_object("java/lang/Object");
}

static void Method_getName(tl_jcall *c)
{
    const char *n = tl_jni_reflected_name(c->self);
    c->ret = vl(tl_jni_new_string(n ? n : ""));
}
static void Method_getReturnType(tl_jcall *c)
{
    const char *sig = tl_jni_reflected_sig(c->self);
    const char *r = sig ? strchr(sig, ')') : NULL;
    int n;
    c->ret = vl(r ? class_for_desc(r + 1, &n) : tl_jni_class_object("void"));
}
static void Method_getParameterTypes(tl_jcall *c)
{
    const char *sig = tl_jni_reflected_sig(c->self);
    jobj *types[40]; int nt = 0;
    if (sig && sig[0] == '(') for (const char *p = sig + 1; *p && *p != ')' && nt < 40; ) { int n; types[nt++] = class_for_desc(p, &n); p += n; }
    jobj *arr = tl_jni_new_obj_array(C("java/lang/Class"), (uint32_t)nt);
    for (int i = 0; i < nt; i++) arr->oarr.v[i] = types[i];
    c->ret = vl(arr);
}
/* ---- reflection over the APK's classes, as pyjnius's autoclass walks them: declared members per class, up the hierarchy */

#include "husk-tl-dexindex.h"

typedef struct { jobj *cls; jobj *items[1024]; int n; bool methods, ctors; } member_walk;
static void collect_member(const char *name, const char *sig, uint32_t flags, void *ctx)
{
    member_walk *w = ctx;
    if (w->n >= 1024) return;
    bool st = (flags & 8) != 0;
    jobj *o;
    if (!w->methods) o = tl_jni_reflect_field(w->cls, name, sig, st);
    else {
        bool ctor = !strcmp(name, "<init>");
        if (ctor != w->ctors || !strcmp(name, "<clinit>")) return;
        o = tl_jni_reflect_method(w->cls, name, sig, st);
    }
    if (o) { jvalue f; f.j = 0; f.i = (int)(flags | 0x10000u); tl_jni_set_field(o, "accessFlags", "I", f); w->items[w->n++] = o; }
}
static jobj *members(jobj *cls, bool methods, bool ctors, const char *elem)
{
    member_walk *w = calloc(1, sizeof(*w));
    w->cls = cls; w->methods = methods; w->ctors = ctors;
    const char *name = tl_jni_class_name(cls);
    if (tl_dexidx_has_class(name)) tl_dexidx_each_member(name, methods, collect_member, w);
    else tl_jni_each_known_member(cls, methods, collect_member, w);
    jobj *arr = tl_jni_new_obj_array(C(elem), (uint32_t)w->n);
    for (int i = 0; i < w->n; i++) arr->oarr.v[i] = w->items[i];
    free(w);
    return arr;
}
static void Class_getDeclaredMethods(tl_jcall *c) { c->ret = vl(members(c->self, true, false, "java/lang/reflect/Method")); }
static void Class_getDeclaredConstructors(tl_jcall *c) { c->ret = vl(members(c->self, true, true, "java/lang/reflect/Constructor")); }
static void Class_getDeclaredFields(tl_jcall *c) { c->ret = vl(members(c->self, false, false, "java/lang/reflect/Field")); }
static void Class_getInterfaces(tl_jcall *c) { c->ret = vl(tl_jni_new_obj_array(C("java/lang/Class"), 0)); }
static void Class_getSuperclass(tl_jcall *c)
{
    const char *name = tl_jni_class_name(c->self);
    char sup[260];
    if (tl_dexidx_super(name, sup, sizeof(sup))) { c->ret = vl(tl_jni_class_object(sup)); return; }
    c->ret = vl(strcmp(name, "java/lang/Object") ? tl_jni_class_super(c->self) : NULL);
}
static void Class_isInterface(tl_jcall *c) { jvalue v; v.j = 0; v.z = (tl_dexidx_class_flags(tl_jni_class_name(c->self)) & 0x200) != 0; c->ret = v; }
static void Class_isArray(tl_jcall *c) { jvalue v; v.j = 0; v.z = tl_jni_class_name(c->self)[0] == '['; c->ret = v; }
static void Class_isPrimitive(tl_jcall *c)
{
    static const char *const prim[] = { "boolean", "byte", "char", "short", "int", "long", "float", "double", "void" };
    const char *n = tl_jni_class_name(c->self);
    jvalue v; v.j = 0;
    for (size_t i = 0; i < sizeof(prim) / sizeof(prim[0]); i++) if (!strcmp(n, prim[i])) v.z = 1;
    c->ret = v;
}
static void Class_getComponentType(tl_jcall *c)
{
    const char *n = tl_jni_class_name(c->self);
    int len;
    c->ret = vl(n[0] == '[' ? class_for_desc(n + 1, &len) : NULL);
}
static void Class_getModifiers(tl_jcall *c) { jvalue v; v.j = 0; v.i = (int)(tl_dexidx_class_flags(tl_jni_class_name(c->self)) | 1); c->ret = v; }
/* A member's modifiers: the access flags it was listed with, or public (and static when it is) for one made otherwise. */
static void Member_getModifiers(tl_jcall *c)
{
    int f = tl_jni_get_field(c->self, "accessFlags", "I").i;
    jvalue v; v.j = 0; v.i = (f & 0x10000) ? (f & 0xFFFF) : 1;
    c->ret = v;
}
static void Member_isVarArgs(tl_jcall *c) { int f = tl_jni_get_field(c->self, "accessFlags", "I").i; jvalue v; v.j = 0; v.z = (f & 0x10000) && (f & 0x80); c->ret = v; }
static void Field_getType(tl_jcall *c) { int n; const char *sig = tl_jni_reflected_field_sig(c->self); c->ret = vl(sig ? class_for_desc(sig, &n) : tl_jni_class_object("java/lang/Object")); }

/* java.lang.reflect.Modifier's tests, on the access flags above. */
#define MODIFIER(fn, bit) static void fn(tl_jcall *c) { jvalue v; v.j = 0; v.z = (c->args[0].i & (bit)) != 0; c->ret = v; }
MODIFIER(Mod_isPublic, 0x1) MODIFIER(Mod_isPrivate, 0x2) MODIFIER(Mod_isProtected, 0x4) MODIFIER(Mod_isStatic, 0x8)
MODIFIER(Mod_isFinal, 0x10) MODIFIER(Mod_isSynchronized, 0x20) MODIFIER(Mod_isVolatile, 0x40) MODIFIER(Mod_isTransient, 0x80)
MODIFIER(Mod_isNative, 0x100) MODIFIER(Mod_isInterface, 0x200) MODIFIER(Mod_isAbstract, 0x400) MODIFIER(Mod_isStrict, 0x800)

static void Class_getName(tl_jcall *c)
{
    char buf[200];
    snprintf(buf, sizeof(buf), "%s", tl_jni_class_name(c->self));
    for (char *p = buf; *p; p++) if (*p == '/') *p = '.';
    c->ret = vl(tl_jni_new_string(buf));
}

/* ------------------------------------------------------------ the queue */

typedef struct ev {
    struct ev *next;
    int64_t when;
    jobj *handler, *msg, *run, *frame;
} ev;

typedef struct looper {
    pthread_mutex_t mu;
    pthread_cond_t cv;
    ev *head;
    bool started, quit;
    char name[48];
    jobj *mirror;                       /* the android.os.Looper object */
    jobj *choreographer;
} looper;

typedef struct handler { looper *lp; jobj *callback; } handler;

static __thread looper *t_looper;
static looper *g_main;
static pthread_mutex_t g_main_mu = PTHREAD_MUTEX_INITIALIZER;
static void *looper_thread(void *arg);

static looper *looper_new(const char *name)
{
    looper *l = calloc(1, sizeof(*l));
    pthread_mutex_init(&l->mu, NULL);
    pthread_cond_init(&l->cv, NULL);
    snprintf(l->name, sizeof(l->name), "%s", name);
    l->mirror = tl_jni_new_object(C("android/os/Looper"));
    l->mirror->native = l;
    tl_jni_ref(l->mirror);
    return l;
}

static void looper_start(looper *l)
{
    pthread_mutex_lock(&l->mu);
    bool go = !l->started;
    l->started = true;
    pthread_mutex_unlock(&l->mu);
    if (!go) return;
    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setstacksize(&a, 4u << 20);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_t t;
    if (pthread_create(&t, &a, looper_thread, l)) tl_log_line("looper: could not start '%s'", l->name);
    pthread_attr_destroy(&a);
}

static looper *main_looper(void)
{
    pthread_mutex_lock(&g_main_mu);
    if (!g_main) g_main = looper_new("main");
    looper *l = g_main;
    pthread_mutex_unlock(&g_main_mu);
    return l;
}

static void enqueue(looper *l, ev *e)
{
    looper_start(l);
    pthread_mutex_lock(&l->mu);
    ev **pp = &l->head;
    while (*pp && (*pp)->when <= e->when) pp = &(*pp)->next;
    e->next = *pp;
    *pp = e;
    pthread_cond_signal(&l->cv);
    pthread_mutex_unlock(&l->mu);
}

static void ev_free(ev *e)
{
    tl_jni_unref(e->handler); tl_jni_unref(e->msg); tl_jni_unref(e->run); tl_jni_unref(e->frame);
    free(e);
}

/* ----------------------------------------------------- Java proxy callbacks */

typedef jobj *(*bridge_invoke_fn)(void *env, void *cls, int64_t handle, void *iface, void *method, void *args);

/* Call `name` on a JNIBridge proxy as Java would: the native bridge turns it into a call on the C# delegate. */
static jobj *proxy_call(jobj *proxy, const char *iface, const char *name, const char *sig, jobj **args, int nargs)
{
    if (!proxy || strcmp(tl_jni_class_name(proxy), "java/lang/reflect/Proxy")) return NULL;
    if (tl_jni_get_field(proxy, "style", "I").i == 2) {
        typedef jobj *(*proxy_invoke_fn)(void *env, void *cls, int64_t handle, void *name, void *args);
        proxy_invoke_fn pf = tl_jni_native("com/unity3d/player/ReflectionHelper", "nativeProxyInvoke", "(JLjava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
        if (!pf) return NULL;
        jobj *arr2 = tl_jni_new_obj_array(C("java/lang/Object"), (uint32_t)nargs);
        for (int i = 0; i < nargs; i++) arr2->oarr.v[i] = tl_jni_ref(args[i]);
        jobj *nm = tl_jni_new_string(name);
        jobj *r2 = pf(tl_jni_env(), tl_jni_class_object("com/unity3d/player/ReflectionHelper"), tl_jni_get_field(proxy, "handle", "J").j, nm, arr2);
        if (tl_jni_pending()) tl_jni_clear();
        tl_jni_unref(nm); tl_jni_unref(arr2);
        return r2;
    }
    bridge_invoke_fn fn = tl_jni_native("bitter/jnibridge/JNIBridge", "invoke", "(JLjava/lang/Class;Ljava/lang/reflect/Method;[Ljava/lang/Object;)Ljava/lang/Object;");
    if (!fn) { tl_log_line("looper: JNIBridge.invoke is not registered; cannot call %s.%s", iface, name); return NULL; }
    jobj *icls = tl_jni_class_object(iface);
    jobj *method = tl_jni_reflect_method(icls, name, sig, false);
    jobj *arr = tl_jni_new_obj_array(C("java/lang/Object"), (uint32_t)nargs);
    for (int i = 0; i < nargs; i++) arr->oarr.v[i] = tl_jni_ref(args[i]);
    int64_t handle = tl_jni_get_field(proxy, "handle", "J").j;
    jobj *r = fn(tl_jni_env(), tl_jni_class_object("bitter/jnibridge/JNIBridge"), handle, icls, method, arr);
    if (tl_jni_pending()) tl_jni_clear();
    tl_jni_unref(method);
    tl_jni_unref(arr);
    return r;
}

/* For other parts of the runtime that hand a result back to a game's listener (Play services' Tasks, Billing). */
jobj *tl_proxy_call(jobj *proxy, const char *iface, const char *name, const char *sig, jobj **args, int nargs)
{
    return proxy_call(proxy, iface, name, sig, args, nargs);
}

static void run_runnable(jobj *run)
{
    if (!run) return;
    if (!strcmp(tl_jni_class_name(run), "java/lang/reflect/Proxy")) {
        jobj *r = proxy_call(run, "java/lang/Runnable", "run", "()V", NULL, 0);
        tl_jni_unref(r);
        return;
    }
    static int warned;
    if (warned++ < 8) tl_log_line("looper: a %s was posted; the app's own Java does not run here", tl_jni_class_name(run));
}

static void dispatch(ev *e)
{
    if (e->frame) {
        jobj *t = box_long(e->when);
        jobj *r = proxy_call(e->frame, "android/view/Choreographer$FrameCallback", "doFrame", "(J)V", &t, 1);
        tl_jni_unref(t); tl_jni_unref(r);
    } else if (e->run) {
        run_runnable(e->run);
    } else if (e->msg) {
        handler *h = e->handler ? e->handler->native : NULL;
        if (h && h->callback) {
            jobj *r = proxy_call(h->callback, "android/os/Handler$Callback", "handleMessage", "(Landroid/os/Message;)Z", &e->msg, 1);
            tl_jni_unref(r);
        } else {
            static int warned;
            if (warned++ < 8) tl_log_line("looper: a message went to a Handler with no callback; dropped");
        }
    }
}

static void *looper_thread(void *arg)
{
    looper *l = arg;
    t_looper = l;
    pthread_setname_np(l->name);
    pthread_mutex_lock(&l->mu);
    while (!l->quit) {
        ev *e = l->head;
        if (!e) { pthread_cond_wait(&l->cv, &l->mu); continue; }
        int64_t now = now_ns();
        if (e->when > now) {
            int64_t d = e->when - now;
            struct timespec rel = { .tv_sec = (time_t)(d / NS), .tv_nsec = (long)(d % NS) };
            pthread_cond_timedwait_relative_np(&l->cv, &l->mu, &rel);
            continue;
        }
        l->head = e->next;
        pthread_mutex_unlock(&l->mu);
        dispatch(e);
        ev_free(e);
        pthread_mutex_lock(&l->mu);
    }
    pthread_mutex_unlock(&l->mu);
    return NULL;
}

/* ------------------------------------------------------------- Looper */

static looper *current_or_main(void) { return t_looper ? t_looper : main_looper(); }
static looper *looper_of(const jobj *o) { return o && o->native ? o->native : NULL; }

static void Looper_getMainLooper(tl_jcall *c) { c->ret = vl(main_looper()->mirror); }
static void Looper_myLooper(tl_jcall *c) { c->ret = vl(t_looper ? t_looper->mirror : NULL); }
static void Looper_quit(tl_jcall *c)
{
    looper *l = looper_of(c->self);
    if (!l) return;
    pthread_mutex_lock(&l->mu); l->quit = true; pthread_cond_signal(&l->cv); pthread_mutex_unlock(&l->mu);
}
static void Looper_getThread(tl_jcall *c) { (void)c; }

/* A HandlerThread is a Thread whose run() is a Looper. Its looper exists from construction; start() gives it a thread. */
static void HT_init(tl_jcall *c)
{
    looper *l = looper_new(tl_jni_string(c->args[0].l) ? tl_jni_string(c->args[0].l) : "HandlerThread");
    c->self->native = l;
}
static void HT_start(tl_jcall *c) { looper *l = looper_of(c->self); if (l) looper_start(l); }
static void HT_getLooper(tl_jcall *c) { looper *l = looper_of(c->self); c->ret = vl(l ? l->mirror : NULL); }
static void HT_quit(tl_jcall *c) { Looper_quit(c); c->ret = vz(1); }

/* ------------------------------------------------------------ Message */

static jobj *new_message(void)
{
    jobj *m = tl_jni_new_object(C("android/os/Message"));
    tl_jni_set_field(m, "what", "I", vi(0)); tl_jni_set_field(m, "arg1", "I", vi(0)); tl_jni_set_field(m, "arg2", "I", vi(0));
    tl_jni_set_field(m, "obj", "Ljava/lang/Object;", vl(NULL)); tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(NULL));
    return m;
}
static void Message_obtain(tl_jcall *c)
{
    jobj *m = new_message();
    if (c->args && c->cls) { /* obtain(Handler[, what[, obj]]) variants set what they were given */ }
    c->ret = vl(m);
}
static void Message_obtainH(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->args[0].l)));
    c->ret = vl(m);
}
static void Message_obtainHI(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->args[0].l)));
    tl_jni_set_field(m, "what", "I", c->args[1]);
    c->ret = vl(m);
}

static void post_message(jobj *handler_obj, jobj *msg, int64_t delay_ns)
{
    handler *h = handler_obj ? handler_obj->native : NULL;
    if (!h || !msg) return;
    ev *e = calloc(1, sizeof(*e));
    e->when = now_ns() + delay_ns;
    e->handler = tl_jni_ref(handler_obj);
    e->msg = tl_jni_ref(msg);
    enqueue(h->lp, e);
}
static void Message_sendToTarget(tl_jcall *c)
{
    jobj *target = tl_jni_get_field(c->self, "target", "Landroid/os/Handler;").l;
    post_message(target, c->self, 0);
}

/* ------------------------------------------------------------ Handler */

static void handler_init(tl_jcall *c, looper *lp, jobj *callback)
{
    handler *h = calloc(1, sizeof(*h));
    h->lp = lp ? lp : current_or_main();
    h->callback = callback ? tl_jni_ref(callback) : NULL;
    c->self->native = h;
}
static void Handler_init0(tl_jcall *c)  { handler_init(c, NULL, NULL); }
static void Handler_initL(tl_jcall *c)  { handler_init(c, looper_of(c->args[0].l), NULL); }
static void Handler_initC(tl_jcall *c)  { handler_init(c, NULL, c->args[0].l); }
static void Handler_initLC(tl_jcall *c) { handler_init(c, looper_of(c->args[0].l), c->args[1].l); }
static void Handler_getLooper(tl_jcall *c) { handler *h = c->self->native; c->ret = vl(h ? h->lp->mirror : NULL); }

static bool post_runnable(jobj *self, jobj *run, int64_t delay_ns)
{
    handler *h = self ? self->native : NULL;
    if (!h || !run) return false;
    ev *e = calloc(1, sizeof(*e));
    e->when = now_ns() + delay_ns;
    e->handler = tl_jni_ref(self);
    e->run = tl_jni_ref(run);
    enqueue(h->lp, e);
    return true;
}
static void Handler_post(tl_jcall *c)         { c->ret = vz(post_runnable(c->self, c->args[0].l, 0)); }
static void Handler_postDelayed(tl_jcall *c)  { c->ret = vz(post_runnable(c->self, c->args[0].l, c->args[1].j * 1000000)); }
static void Handler_obtain0(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->self)));
    c->ret = vl(m);
}
static void Handler_obtainI(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->self)));
    tl_jni_set_field(m, "what", "I", c->args[0]);
    c->ret = vl(m);
}
static void Handler_obtainIII(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->self)));
    tl_jni_set_field(m, "what", "I", c->args[0]); tl_jni_set_field(m, "arg1", "I", c->args[1]); tl_jni_set_field(m, "arg2", "I", c->args[2]);
    c->ret = vl(m);
}
static void Handler_obtainIO(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->self)));
    tl_jni_set_field(m, "what", "I", c->args[0]); tl_jni_set_field(m, "obj", "Ljava/lang/Object;", vl(tl_jni_ref(c->args[1].l)));
    c->ret = vl(m);
}
static void Handler_sendMessage(tl_jcall *c)
{
    tl_jni_set_field(c->args[0].l, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->self)));
    post_message(c->self, c->args[0].l, 0);
    c->ret = vz(1);
}
static void Handler_sendMessageDelayed(tl_jcall *c)
{
    tl_jni_set_field(c->args[0].l, "target", "Landroid/os/Handler;", vl(tl_jni_ref(c->self)));
    post_message(c->self, c->args[0].l, c->args[1].j * 1000000);
    c->ret = vz(1);
}
static void Handler_sendEmpty(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "what", "I", c->args[0]);
    post_message(c->self, m, 0);
    tl_jni_unref(m);
    c->ret = vz(1);
}
static void Handler_sendEmptyDelayed(tl_jcall *c)
{
    jobj *m = new_message();
    tl_jni_set_field(m, "what", "I", c->args[0]);
    post_message(c->self, m, c->args[1].j * 1000000);
    tl_jni_unref(m);
    c->ret = vz(1);
}

/* Remove queued events for this handler that match: by message code, by runnable, or all. */
static bool remove_events(jobj *handler_obj, int what, jobj *run, bool all)
{
    handler *h = handler_obj ? handler_obj->native : NULL;
    if (!h) return false;
    looper *l = h->lp;
    bool any = false;
    pthread_mutex_lock(&l->mu);
    for (ev **pp = &l->head; *pp; ) {
        ev *e = *pp;
        bool hit = e->handler == handler_obj && !e->frame
                && (all || (run && e->run == run) || (!run && what >= 0 && e->msg && tl_jni_get_field(e->msg, "what", "I").i == what));
        if (hit) { *pp = e->next; ev_free(e); any = true; } else pp = &e->next;
    }
    pthread_mutex_unlock(&l->mu);
    return any;
}
static void Handler_removeCallbacks(tl_jcall *c)    { remove_events(c->self, -1, c->args[0].l, false); }
static void Handler_removeMessages(tl_jcall *c)     { remove_events(c->self, c->args[0].i, NULL, false); }
static void Handler_removeAll(tl_jcall *c)          { remove_events(c->self, -1, NULL, true); }
static void Handler_hasMessages(tl_jcall *c)
{
    handler *h = c->self->native;
    bool any = false;
    if (h) {
        pthread_mutex_lock(&h->lp->mu);
        for (ev *e = h->lp->head; e; e = e->next) if (e->handler == c->self && e->msg && tl_jni_get_field(e->msg, "what", "I").i == c->args[0].i) any = true;
        pthread_mutex_unlock(&h->lp->mu);
    }
    c->ret = vz(any);
}

/* The UI thread runs what is posted to it; from the UI thread itself it runs at once. */
static void Activity_runOnUiThread(tl_jcall *c)
{
    looper *m = main_looper();
    if (t_looper == m) { run_runnable(c->args[0].l); return; }
    ev *e = calloc(1, sizeof(*e));
    e->when = now_ns();
    e->run = tl_jni_ref(c->args[0].l);
    enqueue(m, e);
}

/* ------------------------------------------------------- Choreographer */

static void Choreo_getInstance(tl_jcall *c)
{
    looper *l = current_or_main();
    pthread_mutex_lock(&l->mu);
    if (!l->choreographer) {
        l->choreographer = tl_jni_new_object(C("android/view/Choreographer"));
        l->choreographer->native = l;
        tl_jni_ref(l->choreographer);
    }
    jobj *ch = l->choreographer;
    pthread_mutex_unlock(&l->mu);
    c->ret = vl(ch);
}

static void post_frame(jobj *self, jobj *cb, int64_t delay_ns)
{
    looper *l = looper_of(self);
    if (!l || !cb) return;
    int64_t t = now_ns() + delay_ns;
    ev *e = calloc(1, sizeof(*e));
    e->when = (t / VSYNC_NS + 1) * VSYNC_NS;       /* the next 60 Hz boundary after t */
    e->frame = tl_jni_ref(cb);
    enqueue(l, e);
}
static void Choreo_postFrameCallback(tl_jcall *c)        { post_frame(c->self, c->args[0].l, 0); }
static void Choreo_postFrameCallbackDelayed(tl_jcall *c) { post_frame(c->self, c->args[0].l, c->args[1].j * 1000000); }
static void Choreo_removeFrameCallback(tl_jcall *c)
{
    looper *l = looper_of(c->self);
    if (!l) return;
    pthread_mutex_lock(&l->mu);
    for (ev **pp = &l->head; *pp; ) {
        ev *e = *pp;
        if (e->frame && e->frame == c->args[0].l) { *pp = e->next; ev_free(e); } else pp = &e->next;
    }
    pthread_mutex_unlock(&l->mu);
}
static void Choreo_getFrameTimeNanos(tl_jcall *c)     { c->ret = vj(now_ns() / VSYNC_NS * VSYNC_NS); }
static void Choreo_getFrameIntervalNanos(tl_jcall *c) { c->ret = vj(VSYNC_NS); }

/* ------------------------------------------------------------ install */

static const struct { const char *name, *super; } k_loop_classes[] = {
    { "java/lang/Number", "java/lang/Object" },
    { "java/lang/Long", "java/lang/Number" }, { "java/lang/Integer", "java/lang/Number" }, { "java/lang/Float", "java/lang/Number" },
    { "java/lang/Double", "java/lang/Number" }, { "java/lang/Boolean", "java/lang/Object" },
    { "java/lang/Thread", "java/lang/Object" }, { "android/os/HandlerThread", "java/lang/Thread" },
    { "android/os/Looper", "java/lang/Object" }, { "android/os/Handler", "java/lang/Object" }, { "android/os/Message", "java/lang/Object" },
    { "android/os/Handler$Callback", "java/lang/Object" }, { "android/view/Choreographer", "java/lang/Object" },
    { "android/view/Choreographer$FrameCallback", "java/lang/Object" },
    { "int", "java/lang/Object" }, { "long", "java/lang/Object" }, { "boolean", "java/lang/Object" }, { "float", "java/lang/Object" },
    { "double", "java/lang/Object" }, { "byte", "java/lang/Object" }, { "char", "java/lang/Object" }, { "short", "java/lang/Object" }, { "void", "java/lang/Object" },
};

#define M(c, n, s, f) { c, n, s, f }
static const tl_jhle k_loop_hle[] = {
    M("java/lang/Long", "<init>", "(J)V", Long_init), M("java/lang/Long", "valueOf", "(J)Ljava/lang/Long;", Long_valueOf),
    M("java/lang/Long", "longValue", "()J", Long_value), M("java/lang/Integer", "<init>", "(I)V", Int_init),
    M("java/lang/Integer", "valueOf", "(I)Ljava/lang/Integer;", Int_valueOf), M("java/lang/Integer", "intValue", "()I", Int_value),
    M("java/lang/Boolean", "<init>", "(Z)V", Bool_init), M("java/lang/Boolean", "valueOf", "(Z)Ljava/lang/Boolean;", Bool_valueOf),
    M("java/lang/Boolean", "booleanValue", "()Z", Bool_value), M("java/lang/Float", "<init>", "(F)V", Float_init),
    M("java/lang/Float", "valueOf", "(F)Ljava/lang/Float;", Float_valueOf), M("java/lang/Float", "floatValue", "()F", Float_value),
    M("java/lang/Double", "<init>", "(D)V", Double_init), M("java/lang/Double", "valueOf", "(D)Ljava/lang/Double;", Double_valueOf),
    M("java/lang/Double", "doubleValue", "()D", Double_value),

    M("java/lang/reflect/Method", "getName", "()Ljava/lang/String;", Method_getName),
    M("java/lang/reflect/Method", "getReturnType", "()Ljava/lang/Class;", Method_getReturnType),
    M("java/lang/reflect/Method", "getParameterTypes", "()[Ljava/lang/Class;", Method_getParameterTypes),
    M("java/lang/Class", "getName", "()Ljava/lang/String;", Class_getName),
    M("java/lang/Class", "getDeclaredMethods", "()[Ljava/lang/reflect/Method;", Class_getDeclaredMethods),
    M("java/lang/Class", "getMethods", "()[Ljava/lang/reflect/Method;", Class_getDeclaredMethods),
    M("java/lang/Class", "getDeclaredConstructors", "()[Ljava/lang/reflect/Constructor;", Class_getDeclaredConstructors),
    M("java/lang/Class", "getConstructors", "()[Ljava/lang/reflect/Constructor;", Class_getDeclaredConstructors),
    M("java/lang/Class", "getDeclaredFields", "()[Ljava/lang/reflect/Field;", Class_getDeclaredFields),
    M("java/lang/Class", "getFields", "()[Ljava/lang/reflect/Field;", Class_getDeclaredFields),
    M("java/lang/Class", "getInterfaces", "()[Ljava/lang/Class;", Class_getInterfaces),
    M("java/lang/Class", "getSuperclass", "()Ljava/lang/Class;", Class_getSuperclass),
    M("java/lang/Class", "isInterface", "()Z", Class_isInterface),
    M("java/lang/Class", "isArray", "()Z", Class_isArray),
    M("java/lang/Class", "isPrimitive", "()Z", Class_isPrimitive),
    M("java/lang/Class", "getComponentType", "()Ljava/lang/Class;", Class_getComponentType),
    M("java/lang/Class", "getModifiers", "()I", Class_getModifiers),
    M("java/lang/reflect/Method", "getModifiers", "()I", Member_getModifiers),
    M("java/lang/reflect/Method", "isVarArgs", "()Z", Member_isVarArgs),
    M("java/lang/reflect/Constructor", "getModifiers", "()I", Member_getModifiers),
    M("java/lang/reflect/Constructor", "isVarArgs", "()Z", Member_isVarArgs),
    M("java/lang/reflect/Constructor", "getParameterTypes", "()[Ljava/lang/Class;", Method_getParameterTypes),
    M("java/lang/reflect/Field", "getName", "()Ljava/lang/String;", Method_getName),
    M("java/lang/reflect/Field", "getType", "()Ljava/lang/Class;", Field_getType),
    M("java/lang/reflect/Field", "getModifiers", "()I", Member_getModifiers),
    M("java/lang/reflect/Modifier", "isPublic", "(I)Z", Mod_isPublic), M("java/lang/reflect/Modifier", "isPrivate", "(I)Z", Mod_isPrivate),
    M("java/lang/reflect/Modifier", "isProtected", "(I)Z", Mod_isProtected), M("java/lang/reflect/Modifier", "isStatic", "(I)Z", Mod_isStatic),
    M("java/lang/reflect/Modifier", "isFinal", "(I)Z", Mod_isFinal), M("java/lang/reflect/Modifier", "isSynchronized", "(I)Z", Mod_isSynchronized),
    M("java/lang/reflect/Modifier", "isVolatile", "(I)Z", Mod_isVolatile), M("java/lang/reflect/Modifier", "isTransient", "(I)Z", Mod_isTransient),
    M("java/lang/reflect/Modifier", "isNative", "(I)Z", Mod_isNative), M("java/lang/reflect/Modifier", "isInterface", "(I)Z", Mod_isInterface),
    M("java/lang/reflect/Modifier", "isAbstract", "(I)Z", Mod_isAbstract), M("java/lang/reflect/Modifier", "isStrict", "(I)Z", Mod_isStrict),

    M("android/os/Looper", "getMainLooper", "()Landroid/os/Looper;", Looper_getMainLooper),
    M("android/os/Looper", "myLooper", "()Landroid/os/Looper;", Looper_myLooper),
    M("android/os/Looper", "quit", "()V", Looper_quit), M("android/os/Looper", "quitSafely", "()V", Looper_quit),
    M("android/os/Looper", "getThread", "()Ljava/lang/Thread;", Looper_getThread),
    M("android/os/HandlerThread", "<init>", "(Ljava/lang/String;)V", HT_init), M("android/os/HandlerThread", "<init>", "(Ljava/lang/String;I)V", HT_init),
    M("android/os/HandlerThread", "start", "()V", HT_start), M("android/os/HandlerThread", "getLooper", "()Landroid/os/Looper;", HT_getLooper),
    M("android/os/HandlerThread", "quit", "()Z", HT_quit), M("android/os/HandlerThread", "quitSafely", "()Z", HT_quit),

    M("android/os/Message", "obtain", "()Landroid/os/Message;", Message_obtain),
    M("android/os/Message", "obtain", "(Landroid/os/Handler;)Landroid/os/Message;", Message_obtainH),
    M("android/os/Message", "obtain", "(Landroid/os/Handler;I)Landroid/os/Message;", Message_obtainHI),
    M("android/os/Message", "sendToTarget", "()V", Message_sendToTarget),

    M("android/os/Handler", "<init>", "()V", Handler_init0), M("android/os/Handler", "<init>", "(Landroid/os/Looper;)V", Handler_initL),
    M("android/os/Handler", "<init>", "(Landroid/os/Handler$Callback;)V", Handler_initC),
    M("android/os/Handler", "<init>", "(Landroid/os/Looper;Landroid/os/Handler$Callback;)V", Handler_initLC),
    M("android/os/Handler", "getLooper", "()Landroid/os/Looper;", Handler_getLooper),
    M("android/os/Handler", "post", "(Ljava/lang/Runnable;)Z", Handler_post), M("android/os/Handler", "postDelayed", "(Ljava/lang/Runnable;J)Z", Handler_postDelayed),
    M("android/os/Handler", "obtainMessage", "()Landroid/os/Message;", Handler_obtain0), M("android/os/Handler", "obtainMessage", "(I)Landroid/os/Message;", Handler_obtainI),
    M("android/os/Handler", "obtainMessage", "(III)Landroid/os/Message;", Handler_obtainIII),
    M("android/os/Handler", "obtainMessage", "(ILjava/lang/Object;)Landroid/os/Message;", Handler_obtainIO),
    M("android/os/Handler", "sendMessage", "(Landroid/os/Message;)Z", Handler_sendMessage),
    M("android/os/Handler", "sendMessageDelayed", "(Landroid/os/Message;J)Z", Handler_sendMessageDelayed),
    M("android/os/Handler", "sendEmptyMessage", "(I)Z", Handler_sendEmpty), M("android/os/Handler", "sendEmptyMessageDelayed", "(IJ)Z", Handler_sendEmptyDelayed),
    M("android/os/Handler", "removeCallbacks", "(Ljava/lang/Runnable;)V", Handler_removeCallbacks),
    M("android/os/Handler", "removeMessages", "(I)V", Handler_removeMessages),
    M("android/os/Handler", "removeCallbacksAndMessages", "(Ljava/lang/Object;)V", Handler_removeAll),
    M("android/os/Handler", "hasMessages", "(I)Z", Handler_hasMessages),
    M("android/app/Activity", "runOnUiThread", "(Ljava/lang/Runnable;)V", Activity_runOnUiThread),

    M("android/view/Choreographer", "getInstance", "()Landroid/view/Choreographer;", Choreo_getInstance),
    M("android/view/Choreographer", "postFrameCallback", "(Landroid/view/Choreographer$FrameCallback;)V", Choreo_postFrameCallback),
    M("android/view/Choreographer", "postFrameCallbackDelayed", "(Landroid/view/Choreographer$FrameCallback;J)V", Choreo_postFrameCallbackDelayed),
    M("android/view/Choreographer", "removeFrameCallback", "(Landroid/view/Choreographer$FrameCallback;)V", Choreo_removeFrameCallback),
    M("android/view/Choreographer", "getFrameTimeNanos", "()J", Choreo_getFrameTimeNanos),
    M("android/view/Choreographer", "getFrameIntervalNanos", "()J", Choreo_getFrameIntervalNanos),
    { NULL, NULL, NULL, NULL }
};

jobj *tl_loop_main_looper(void) { return main_looper()->mirror; }

void tl_loop_install(void)
{
    for (size_t i = 0; i < sizeof(k_loop_classes) / sizeof(k_loop_classes[0]); i++) tl_jni_declare(k_loop_classes[i].name, k_loop_classes[i].super);
    tl_jni_register_hle(k_loop_hle);
    jobj *m = new_message();                    /* declares Message's fields for GetFieldID */
    tl_jni_unref(m);
}
