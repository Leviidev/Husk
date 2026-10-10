/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-bionic.h"
#include "husk-tl-dexindex.h"
#include "husk-tl-va.h"

/* ---------------------------------------------------------------- types */

#include "husk-tl-jni-internal.h"

const tl_dvm_hooks *tl_dvm;

#define NBUCKETS 512
static tl_jclass *g_classes[NBUCKETS];
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static int g_trace;
static const tl_jhle *g_hle[32];
static int g_nhle;

void tl_jni_set_trace(int level) { g_trace = level; }

static uint32_t hash_str(const char *s) { uint32_t h = 2166136261u; while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; } return h; }

#define TRACE(...) do { if (g_trace >= 1) tl_log_line(__VA_ARGS__); } while (0)
#define TRACE2(...) do { if (g_trace >= 2) tl_log_line(__VA_ARGS__); } while (0)

/* --------------------------------------------------------------- objects */

static jobj *obj_alloc(uint8_t kind, tl_jclass *cls)
{
    jobj *o = calloc(1, sizeof(*o));
    o->refs = 1;
    o->kind = kind;
    o->cls = cls;
    return o;
}

jobj *tl_jni_ref(jobj *o) { if (o) __atomic_add_fetch(&o->refs, 1, __ATOMIC_RELAXED); return o; }

void tl_jni_unref(jobj *o)
{
    if (!o || o->kind == TL_K_CLASS) return;
    /* With the Java runtime up, objects live as long as Java can reach them, which reference counts from native code do not
     * know about: nothing is freed here (a tracing collector's job). */
    if (tl_dvm) return;
    if (__atomic_sub_fetch(&o->refs, 1, __ATOMIC_ACQ_REL) != 0) return;
    switch (o->kind) {
    case TL_K_STRING: free(o->str.utf8); free(o->str.u16); break;
    case TL_K_PRIM_ARRAY: free(o->arr.data); break;
    case TL_K_OBJ_ARRAY: free(o->oarr.v); break;
    default: break;
    }
    free(o->fields);
    free(o);
}

static tl_jclass *find_class_locked(const char *name)
{
    for (tl_jclass *c = g_classes[hash_str(name) % NBUCKETS]; c; c = c->next) if (!strcmp(c->name, name)) return c;
    return NULL;
}

static tl_jclass *tl_jni_declare_linked(const char *name, const char *super);

/* Declaring a class and linking its bytecode is one step to every other thread: a class is visible from the moment it is declared
 * (the linking itself looks it up, through superclasses that refer back), so lookups that find one mid-link wait on this lock. */
static pthread_mutex_t g_link;
static pthread_once_t g_link_once = PTHREAD_ONCE_INIT;
static void link_init(void)
{
    pthread_mutexattr_t a; pthread_mutexattr_init(&a); pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&g_link, &a);
}
void tl_jni_wait_linked(tl_jclass *c)
{
    if (!c || !c->linking) return;
    pthread_once(&g_link_once, link_init);
    pthread_mutex_lock(&g_link);
    pthread_mutex_unlock(&g_link);
}

tl_jclass *tl_jni_declare(const char *name, const char *super)
{
    if (tl_dvm) { pthread_once(&g_link_once, link_init); pthread_mutex_lock(&g_link); }
    tl_jclass *c = tl_jni_declare_linked(name, super);
    if (tl_dvm) pthread_mutex_unlock(&g_link);
    tl_jni_wait_linked(c);
    return c;
}

static tl_jclass *tl_jni_declare_linked(const char *name, const char *super)
{
    pthread_mutex_lock(&g_lock);
    tl_jclass *c = find_class_locked(name);
    if (!c) {
        c = calloc(1, sizeof(*c));
        c->name = strdup(name);
        uint32_t h = hash_str(name) % NBUCKETS;
        c->next = g_classes[h];
        g_classes[h] = c;
        c->in_dex = tl_dexidx_has_class(name);
        c->mirror = obj_alloc(TL_K_CLASS, NULL);
        c->mirror->klass.jc = c;
        c->mirror->refs = 1u << 30;
        /* A class the interpreter has bytecode for: it says what the superclass is, and keeps the statics. */
        if (tl_dvm) {
            c->linking = true;
            pthread_mutex_unlock(&g_lock);
            bool attached = tl_dvm->attach(c);
            pthread_mutex_lock(&g_lock);
            c->linking = false;
            if (attached) { c->in_dex = true; pthread_mutex_unlock(&g_lock); return c; }
        }
    }
    if (super && !c->super) {
        pthread_mutex_unlock(&g_lock);
        tl_jclass *s = tl_jni_declare(super, strcmp(super, "java/lang/Object") ? "java/lang/Object" : NULL);
        pthread_mutex_lock(&g_lock);
        if (!c->super && s != c) c->super = s;
    }
    pthread_mutex_unlock(&g_lock);
    return c;
}

tl_jclass *tl_jni_class(const char *name)
{
    tl_jclass *c;
    pthread_mutex_lock(&g_lock);
    c = find_class_locked(name);
    pthread_mutex_unlock(&g_lock);
    if (c) { tl_jni_wait_linked(c); return c; }
    /* Not declared: the APK may define it, and its superclass is whatever the DEX says. */
    char sup[1024];
    const char *s = tl_dexidx_super(name, sup, sizeof(sup));
    return tl_jni_declare(name, s ? s : (strcmp(name, "java/lang/Object") ? "java/lang/Object" : NULL));
}

const char *tl_jni_class_name(const jobj *o) { return o && o->kind == TL_K_CLASS ? o->klass.jc->name : (o && o->cls ? o->cls->name : "?"); }

jobj *tl_jni_class_object(const char *name) { return tl_jni_class(name)->mirror; }

jobj *tl_jni_new_object(tl_jclass *cls)
{
    jobj *o = obj_alloc(TL_K_OBJECT, cls);
    if (tl_dvm && cls && cls->dvm) {
        uint32_t n = tl_dvm->instance_slots(cls);
        if (n) { o->fields = calloc(n, sizeof(jvalue)); o->nfields = n; }
    }
    return o;
}

jobj *tl_jni_new_string(const char *utf8)
{
    jobj *o = obj_alloc(TL_K_STRING, tl_jni_class("java/lang/String"));
    o->str.utf8 = strdup(utf8 ? utf8 : "");
    return o;
}

const char *tl_jni_string(const jobj *s) { return (s && s->kind == TL_K_STRING) ? s->str.utf8 : NULL; }

static uint8_t esz_of(char t)
{
    switch (t) { case 'Z': case 'B': return 1; case 'C': case 'S': return 2; case 'I': case 'F': return 4; default: return 8; }
}

jobj *tl_jni_new_prim_array(char etype, uint32_t len)
{
    if (etype == 'Z' || etype == 'B' || etype == 'C' || etype == 'S' || etype == 'I' || etype == 'J' || etype == 'F' || etype == 'D') { /* ok */ }
    char cn[4] = { '[', etype, 0 };
    jobj *o = obj_alloc(TL_K_PRIM_ARRAY, tl_jni_class(cn));
    o->arr.esz = esz_of(etype); o->arr.etype = etype; o->arr.len = len;
    o->arr.data = calloc(len ? len : 1, o->arr.esz);
    return o;
}

jobj *tl_jni_new_obj_array(tl_jclass *elem, uint32_t len)
{
    char cn[1024];
    snprintf(cn, sizeof(cn), "[L%s;", elem ? elem->name : "java/lang/Object");
    jobj *o = obj_alloc(TL_K_OBJ_ARRAY, tl_jni_class(cn));
    o->oarr.len = len;
    o->oarr.v = calloc(len ? len : 1, sizeof(jobj *));
    return o;
}

/* ----------------------------------------------------------- signatures */

static const char *skip_type(const char *p)
{
    while (*p == '[') p++;
    if (*p == 'L') { while (*p && *p != ';') p++; }
    return *p ? p + 1 : p;
}

static void parse_sig(tl_jmeth *m)
{
    const char *p = m->sig;
    m->nargs = 0; m->retk = 'V';
    if (*p != '(') return;
    p++;
    while (*p && *p != ')' && m->nargs < (int)sizeof(m->argk) - 1) {
        char k = *p;
        m->argk[m->nargs++] = (k == '[') ? 'L' : k;
        p = skip_type(p);
    }
    if (*p == ')') { p++; m->retk = (*p == '[') ? 'L' : *p; }
    m->argk[m->nargs] = 0;
}

/* ------------------------------------------------- the implemented classes */

void tl_jni_register_hle(const tl_jhle *table)
{
    if (g_nhle < 32) g_hle[g_nhle++] = table;
}

static tl_jhle_fn hle_find(const char *cls, const char *name, const char *sig)
{
    for (int t = 0; t < g_nhle; t++)
        for (const tl_jhle *e = g_hle[t]; e->cls; e++)
            if (!strcmp(e->cls, cls) && !strcmp(e->name, name) && (!e->sig || !strcmp(e->sig, sig))) return e->fn;
    return NULL;
}

/* Whether two signatures differ only in which object types they name: what Unity's reflection fallback treats as the same call. */
static bool sig_loosely_equal(const char *a, const char *b)
{
    if (*a != '(' || *b != '(') return false;
    while (*a && *b) {
        if ((*a == 'L' || *a == '[') && (*b == 'L' || *b == '[')) {
            while (*a == '[') a++;
            while (*b == '[') b++;
            if (*a == 'L') { while (*a && *a != ';') a++; if (*a) a++; } else if (*a) a++;
            if (*b == 'L') { while (*b && *b != ';') b++; if (*b) b++; } else if (*b) b++;
            continue;
        }
        if (*a != *b) return false;
        a++; b++;
    }
    return *a == *b;
}

static tl_jhle_fn hle_find_loose(const char *cls, const char *name, const char *sig)
{
    for (int t = 0; t < g_nhle; t++)
        for (const tl_jhle *e = g_hle[t]; e->cls; e++)
            if (!strcmp(e->cls, cls) && !strcmp(e->name, name) && e->sig && sig_loosely_equal(e->sig, sig)) return e->fn;
    return NULL;
}

/* Find or make the method `name`/`sig`, walking superclasses for an implementation. */
static tl_jmeth *lookup_method(tl_jclass *cls, const char *name, const char *sig, bool is_static)
{
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < cls->nmeths; i++) {
        tl_jmeth *m = cls->meths[i];
        if (m->is_static == is_static && !strcmp(m->name, name) && !strcmp(m->sig, sig)) { pthread_mutex_unlock(&g_lock); return m; }
    }
    pthread_mutex_unlock(&g_lock);

    /* A class the interpreter runs: its bytecode (or an intrinsic) is the method, wherever up the chain it is declared. */
    if (tl_dvm && cls->dvm) {
        void *dm = tl_dvm->find_method(cls, name, sig, is_static);
        if (dm) {
            tl_jmeth *m = calloc(1, sizeof(*m));
            m->cls = cls; m->name = strdup(name); m->sig = strdup(sig); m->is_static = is_static;
            m->dvm = dm; m->exists = true;
            parse_sig(m);
            pthread_mutex_lock(&g_lock);
            if (cls->nmeths == cls->capm) { cls->capm = cls->capm ? cls->capm * 2 : 8; cls->meths = realloc(cls->meths, (size_t)cls->capm * sizeof(*cls->meths)); }
            cls->meths[cls->nmeths++] = m;
            pthread_mutex_unlock(&g_lock);
            return m;
        }
    }
    /* Where could this method come from? An implementation here, the APK's DEX, or a
     * framework class somewhere up the chain (which cannot be checked, so is believed). */
    tl_jhle_fn fn = NULL; bool dex_decl = false, framework = false;
    for (tl_jclass *c = cls; c; c = c->super) {
        if (!fn) fn = hle_find(c->name, name, sig);
        if (!fn) fn = hle_find_loose(c->name, name, sig);
        if (c->in_dex) { if (!dex_decl && tl_dexidx_declares_method(c->name, name, sig, NULL)) dex_decl = true; }
        else if (strcmp(c->name, "java/lang/Object")) framework = true;
        else if (!strcmp(name, "<init>") || !strcmp(name, "toString") || !strcmp(name, "hashCode") || !strcmp(name, "equals")
                 || !strcmp(name, "getClass") || !strcmp(name, "notify") || !strcmp(name, "notifyAll") || !strcmp(name, "wait")) dex_decl = true;
    }
    /* A class the APK defines that has a method of this name, but not with this signature, does not have
     * this method: ART says NoSuchMethodError and Unity falls back to reflection, which is what is wanted. */
    bool dex_other_sig = false;
    if (!fn && !dex_decl) for (tl_jclass *c = cls; c; c = c->super) if (c->in_dex && tl_dexidx_method_named(c->name, name)) { dex_other_sig = true; break; }
    bool exists = fn || dex_decl || (framework && !dex_other_sig);

    tl_jmeth *m = calloc(1, sizeof(*m));
    m->cls = cls; m->name = strdup(name); m->sig = strdup(sig); m->is_static = is_static;
    m->fn = fn; m->exists = exists;
    parse_sig(m);
    pthread_mutex_lock(&g_lock);
    if (cls->nmeths == cls->capm) { cls->capm = cls->capm ? cls->capm * 2 : 8; cls->meths = realloc(cls->meths, (size_t)cls->capm * sizeof(*cls->meths)); }
    cls->meths[cls->nmeths++] = m;
    pthread_mutex_unlock(&g_lock);
    return m;
}

/* The signature the APK declares for a field of this name, anywhere up the chain of classes it defines. */
static bool dex_field_real_sig(tl_jclass *cls, const char *name, char *out, size_t n)
{
    for (tl_jclass *c = cls; c; c = c->super) if (c->in_dex && tl_dexidx_field_sig(c->name, name, out, n)) return true;
    return false;
}

static tl_jfield *lookup_field(tl_jclass *cls, const char *name, const char *sig, bool is_static, bool create)
{
    if (tl_dvm && cls->dvm) {
        /* The interpreter lays its classes out: the field is a slot of the class that declares it. */
        tl_jclass *decl; uint32_t slot;
        if (tl_dvm->find_field(cls, name, sig, is_static, &decl, &slot)) {
            pthread_mutex_lock(&g_lock);
            for (int i = 0; i < decl->nfields; i++) {
                tl_jfield *f = decl->fields[i];
                if (f->is_static == is_static && f->index == slot && !strcmp(f->name, name)) { pthread_mutex_unlock(&g_lock); return f; }
            }
            tl_jfield *f = calloc(1, sizeof(*f));
            f->cls = decl; f->name = strdup(name); f->sig = strdup(sig); f->is_static = is_static; f->index = slot;
            if (decl->nfields == decl->capf) { decl->capf = decl->capf ? decl->capf * 2 : 8; decl->fields = realloc(decl->fields, (size_t)decl->capf * sizeof(*decl->fields)); }
            decl->fields[decl->nfields++] = f;
            pthread_mutex_unlock(&g_lock);
            return f;
        }
        if (!create) return NULL;
        /* not a field of the class: a detached one, so the object's slots and the class's statics are never reallocated under the
           interpreter */
        pthread_mutex_lock(&g_lock);
        for (int i = 0; i < cls->nfields; i++) {
            tl_jfield *f = cls->fields[i];
            if (f->detached && f->is_static == is_static && !strcmp(f->name, name) && !strcmp(f->sig, sig)) { pthread_mutex_unlock(&g_lock); return f; }
        }
        tl_jfield *f = calloc(1, sizeof(*f));
        f->cls = cls; f->name = strdup(name); f->sig = strdup(sig); f->is_static = is_static; f->detached = true;
        if (cls->nfields == cls->capf) { cls->capf = cls->capf ? cls->capf * 2 : 8; cls->fields = realloc(cls->fields, (size_t)cls->capf * sizeof(*cls->fields)); }
        cls->fields[cls->nfields++] = f;
        pthread_mutex_unlock(&g_lock);
        tl_log_line("jni: %s has no field %s %s: kept apart from its objects", cls->name, name, sig);
        return f;
    }
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < cls->nfields; i++) {
        tl_jfield *f = cls->fields[i];
        if (f->is_static == is_static && !strcmp(f->name, name) && !strcmp(f->sig, sig)) { pthread_mutex_unlock(&g_lock); return f; }
    }
    pthread_mutex_unlock(&g_lock);
    if (!create) {
        /* Existence: an app class's fields can be checked; a framework class's are believed. */
        bool found = false;
        for (tl_jclass *c = cls; c && !found; c = c->super) {
            if (c->in_dex) found = tl_dexidx_declares_field(c->name, name, sig, NULL);
            else if (strcmp(c->name, "java/lang/Object")) found = true;
        }
        if (!found) return NULL;
    }
    tl_jfield *f = calloc(1, sizeof(*f));
    f->cls = cls; f->name = strdup(name); f->sig = strdup(sig); f->is_static = is_static;
    pthread_mutex_lock(&g_lock);
    f->index = (uint32_t)(is_static ? cls->nstatics++ : cls->nfields - 0);
    if (is_static) {
        cls->statics = realloc(cls->statics, (size_t)cls->nstatics * sizeof(jvalue));
        memset(&cls->statics[cls->nstatics - 1], 0, sizeof(jvalue));
    } else {
        uint32_t ni = 0;
        for (int i = 0; i < cls->nfields; i++) if (!cls->fields[i]->is_static) ni++;
        f->index = ni;
    }
    if (cls->nfields == cls->capf) { cls->capf = cls->capf ? cls->capf * 2 : 8; cls->fields = realloc(cls->fields, (size_t)cls->capf * sizeof(*cls->fields)); }
    cls->fields[cls->nfields++] = f;
    pthread_mutex_unlock(&g_lock);
    return f;
}

static jvalue *field_slot(jobj *o, tl_jfield *f)
{
    if (f->detached) return &f->dval;
    if (f->is_static) return &f->cls->statics[f->index];
    /* an interpreter object's slots are its own: never grown from here */
    if (tl_dvm && o && o->cls && o->cls->dvm && f->index >= o->nfields) {
        static __thread jvalue scratch;
        scratch.j = 0;
        return &scratch;
    }
    if (f->index >= o->nfields) {
        uint32_t n = f->index + 8;
        if (tl_dvm) { jvalue *dvm_grow_slots(jobj *, uint32_t); return &dvm_grow_slots(o, n)[f->index]; }
        o->fields = realloc(o->fields, n * sizeof(jvalue));
        memset(o->fields + o->nfields, 0, (n - o->nfields) * sizeof(jvalue));
        o->nfields = n;
    }
    return &o->fields[f->index];
}

void tl_jni_set_field(jobj *o, const char *name, const char *sig, jvalue v)
{
    tl_jfield *f = lookup_field(o->cls, name, sig, false, true);
    *field_slot(o, f) = v;
}
/* A static field, set from C: what an activity's Java would have assigned before the game's code reads it. */
void tl_jni_set_static_field(const char *cls, const char *name, const char *sig, jvalue v)
{
    tl_jfield *f = lookup_field(tl_jni_class(cls), name, sig, true, true);
    if (f) *field_slot(NULL, f) = v;
}

jvalue tl_jni_get_field(jobj *o, const char *name, const char *sig)
{
    tl_jfield *f = lookup_field(o->cls, name, sig, false, true);
    return *field_slot(o, f);
}
void tl_jni_set_static(const char *cls, const char *name, const char *sig, jvalue v)
{
    tl_jclass *c = tl_jni_class(cls);
    tl_jfield *f = lookup_field(c, name, sig, true, true);
    *field_slot(NULL, f) = v;
}
jvalue tl_jni_get_static(const char *cls, const char *name, const char *sig)
{
    tl_jclass *c = tl_jni_class(cls);
    tl_jfield *f = lookup_field(c, name, sig, true, true);
    return *field_slot(NULL, f);
}

/* ------------------------------------------------------------- exceptions */

static __thread jobj *t_pending;

void tl_jni_throw(const char *cls, const char *msg)
{
    jobj *e = tl_jni_new_object(tl_jni_class(cls));
    jvalue v; v.l = tl_jni_new_string(msg ? msg : "");
    tl_jni_set_field(e, "detailMessage", "Ljava/lang/String;", v);
    t_pending = e;
    tl_log_line("jni: throwing %s: %s", cls, msg ? msg : "");
}
bool tl_jni_pending(void) { return t_pending != NULL; }
jobj *tl_jni_pending_object(void) { return t_pending; }
void tl_jni_set_pending(jobj *e) { t_pending = e; }
void tl_jni_clear(void) { t_pending = NULL; }

/* --------------------------------------------------------------- calling */

static void jni_ExceptionDescribe(void *env);
static jvalue g_zero;

static jvalue invoke(jobj *self, tl_jmeth *m, bool nonvirtual, const jvalue *args)
{
    if (m->dvm && tl_dvm) {
        jvalue r = tl_dvm->invoke(m->dvm, self, nonvirtual, args);
        if (g_trace >= 3) {
            tl_log_line("jni: java call %s.%s%s on %p -> %#llx%s", m->cls->name, m->name, m->sig, (void *)self, (unsigned long long)r.j, t_pending ? " (threw)" : "");
            if (t_pending) jni_ExceptionDescribe(NULL);
        }
        if (m->retk == 'L' && r.l && ((jobj *)r.l)->kind != TL_K_CLASS) tl_jni_ref(r.l);
        return r;
    }
    tl_jcall c = { .self = self, .cls = m->cls, .args = args };
    c.ret.j = 0;
    tl_jhle_fn fn = m->fn;
    if (!nonvirtual && self && self->cls && self->cls != m->cls) {
        /* virtual dispatch: an override in a subclass's implementation wins */
        for (tl_jclass *k = self->cls; k && k != m->cls; k = k->super) {
            tl_jhle_fn o = hle_find(k->name, m->name, m->sig);
            if (o) { fn = o; c.cls = k; break; }
        }
    }
    if (fn) {
        if (g_trace >= 2) tl_log_line("jni: call %s.%s%s", m->cls->name, m->name, m->sig);
        fn(&c);
        /* A method that returns an object hands the caller a new local reference. The implementations
         * return objects they keep (the Activity, the Display), so without this a caller releasing its
         * reference would free an object that is still in use. */
        if (m->retk == 'L' && c.ret.l && ((jobj *)c.ret.l)->kind != TL_K_CLASS) tl_jni_ref(c.ret.l);
        return c.ret;
    }
    if (!m->warned) {
        m->warned = true;
        tl_log_line("jni: UNIMPLEMENTED Java method %s.%s%s (returning zero)", m->cls->name, m->name, m->sig);
    }
    return g_zero;
}

static void args_from_va(const tl_jmeth *m, tl_va_list *ap, jvalue *out)
{
    for (int i = 0; i < m->nargs; i++) {
        out[i].j = 0;
        switch (m->argk[i]) {
        case 'Z': out[i].z = (uint8_t)tl_va_arg_u64(ap); break;
        case 'B': out[i].b = (int8_t)tl_va_arg_u64(ap); break;
        case 'C': out[i].c = (uint16_t)tl_va_arg_u64(ap); break;
        case 'S': out[i].s = (int16_t)tl_va_arg_u64(ap); break;
        case 'I': out[i].i = (int32_t)tl_va_arg_u64(ap); break;
        case 'J': out[i].j = (int64_t)tl_va_arg_u64(ap); break;
        case 'F': out[i].f = (float)tl_va_arg_f64(ap); break;
        case 'D': out[i].d = tl_va_arg_f64(ap); break;
        default:  out[i].l = (void *)(uintptr_t)tl_va_arg_u64(ap); break;
        }
    }
}

static tl_jmeth *mid_ok(void *mid) { return (tl_jmeth *)mid; }

/* The real work behind every Call*Method: kind 0 instance, 1 nonvirtual, 2 static. */
static jvalue null_receiver(const tl_jmeth *m)
{
    char msg[300];
    snprintf(msg, sizeof(msg), "Attempt to invoke virtual method '%s.%s%s' on a null object reference", m->cls->name, m->name, m->sig);
    tl_jni_throw("java/lang/NullPointerException", msg);
    return g_zero;
}
static jvalue call_a(int kind, jobj *self, void *mid, const jvalue *args)
{
    tl_jmeth *m = mid_ok(mid);
    if (!m) return g_zero;
    if (kind != 2 && !self) return null_receiver(m);
    return invoke(kind == 2 ? NULL : self, m, kind == 1, args);
}
static jvalue call_va(int kind, jobj *self, void *mid, tl_va_list *ap)
{
    tl_jmeth *m = mid_ok(mid);
    if (!m) return g_zero;
    if (kind != 2 && !self) return null_receiver(m);
    jvalue args[40];
    args_from_va(m, ap, args);
    return invoke(kind == 2 ? NULL : self, m, kind == 1, args);
}

jvalue tl_jni_call(jobj *self_or_class, const char *name, const char *sig, const jvalue *args)
{
    bool is_class = self_or_class && self_or_class->kind == TL_K_CLASS;
    tl_jclass *cls = is_class ? self_or_class->klass.jc : self_or_class->cls;
    tl_jmeth *m = lookup_method(cls, name, sig, is_class);
    return invoke(is_class ? NULL : self_or_class, m, false, args);
}

/* ------------------------------------------------- the JNI function table */

typedef jobj *jo;

static uint32_t jni_GetVersion(void *env) { (void)env; return 0x00010006; }

static jo jni_FindClass(void *env, const char *name)
{
    (void)env;
    bool array = name[0] == '[';
    bool framework = !strncmp(name, "android/", 8) || !strncmp(name, "java/", 5) || !strncmp(name, "javax/", 6)
                  || !strncmp(name, "dalvik/", 7) || !strncmp(name, "libcore/", 8) || !strncmp(name, "sun/", 4)
                  || !strncmp(name, "org/json/", 9) || !strncmp(name, "org/xml/", 8) || !strncmp(name, "org/w3c/", 8);
    tl_jclass *known;
    pthread_mutex_lock(&g_lock); known = find_class_locked(name); pthread_mutex_unlock(&g_lock);
    if (tl_dvm && !known && !array) {
        /* the Java runtime is up: a class exists when some DEX on its class paths defines it */
        tl_jclass *c = tl_jni_class(name);
        if (c->dvm || framework) return c->mirror;
        char msg[200]; snprintf(msg, sizeof(msg), "%s", name);
        tl_jni_throw("java/lang/NoClassDefFoundError", msg);
        return NULL;
    }
    if (array || framework || known || tl_dexidx_has_class(name)) {
        TRACE("jni: FindClass(%s)", name);
        return tl_jni_class(name)->mirror;
    }
    TRACE("jni: FindClass(%s) -> NoClassDefFoundError", name);
    char msg[200]; snprintf(msg, sizeof(msg), "%s", name);
    tl_jni_throw("java/lang/NoClassDefFoundError", msg);
    return NULL;
}

static jo jni_GetSuperclass(void *env, jo cls) { (void)env; return cls && cls->kind == TL_K_CLASS && cls->klass.jc->super ? cls->klass.jc->super->mirror : NULL; }

bool dvm_assignable(tl_jclass *sub, tl_jclass *sup);
/* superclasses, interfaces (the interpreter knows a class's) and arrays: Unity asks IsInstanceOf(trustManager, X509TrustManager) */
static bool assignable(tl_jclass *sub, tl_jclass *sup)
{
    for (tl_jclass *c = sub; c; c = c->super) if (c == sup) return true;
    return sub && sup && dvm_assignable(sub, sup);
}
static uint8_t jni_IsAssignableFrom(void *env, jo a, jo b) { (void)env; return (a && b && a->kind == TL_K_CLASS && b->kind == TL_K_CLASS) ? assignable(a->klass.jc, b->klass.jc) : 0; }
static uint8_t jni_IsInstanceOf(void *env, jo obj, jo cls)
{
    (void)env;
    if (!obj) return 1;
    if (!cls || cls->kind != TL_K_CLASS) return 0;
    if (!obj->cls) return 0;
    return assignable(obj->cls, cls->klass.jc);
}

static int32_t jni_Throw(void *env, jo t) { (void)env; t_pending = t; return 0; }
static int32_t jni_ThrowNew(void *env, jo cls, const char *msg) { (void)env; tl_jni_throw(cls && cls->kind == TL_K_CLASS ? cls->klass.jc->name : "java/lang/Error", msg); return 0; }
static void jni_ExceptionDescribe(void *env);
static jo jni_ExceptionOccurred(void *env)
{
    static __thread jobj *t_seen;
    if (t_pending && g_trace >= 1 && t_pending != t_seen) { t_seen = t_pending; jni_ExceptionDescribe(env); }
    return tl_jni_ref(t_pending);
}
static void jni_ExceptionDescribe(void *env)
{
    (void)env;
    if (!t_pending) return;
    jvalue m = tl_jni_get_field(t_pending, "detailMessage", "Ljava/lang/String;");
    tl_log_line("jni: pending exception %s: %s", t_pending->cls->name, tl_jni_string(m.l) ? tl_jni_string(m.l) : "");
}
static void jni_ExceptionClear(void *env) { if (t_pending && g_trace >= 2) { tl_log_line("jni: ExceptionClear of:"); jni_ExceptionDescribe(env); } t_pending = NULL; }
static void jni_FatalError(void *env, const char *msg) { (void)env; tl_log_line("jni: FatalError: %s", msg); abort(); }
static int32_t jni_PushLocalFrame(void *env, int32_t cap) { (void)env; (void)cap; return 0; }
static jo jni_PopLocalFrame(void *env, jo r) { (void)env; return r; }
static jo jni_NewGlobalRef(void *env, jo o) { (void)env; return tl_jni_ref(o); }
static void jni_DeleteGlobalRef(void *env, jo o) { (void)env; tl_jni_unref(o); }
static void jni_DeleteLocalRef(void *env, jo o) { (void)env; tl_jni_unref(o); }
static uint8_t jni_IsSameObject(void *env, jo a, jo b) { (void)env; return a == b; }
static jo jni_NewLocalRef(void *env, jo o) { (void)env; return tl_jni_ref(o); }
static int32_t jni_EnsureLocalCapacity(void *env, int32_t n) { (void)env; (void)n; return 0; }
static uint32_t jni_GetObjectRefType(void *env, jo o) { (void)env; (void)o; return 1; }
static jo jni_AllocObject(void *env, jo cls) { (void)env; return (cls && cls->kind == TL_K_CLASS) ? tl_jni_new_object(cls->klass.jc) : NULL; }
static jo jni_GetObjectClass(void *env, jo o)
{
    (void)env;
    if (!o) return NULL;
    if (o->kind == TL_K_CLASS) return tl_jni_class("java/lang/Class")->mirror;
    return o->cls ? o->cls->mirror : NULL;
}

static void *jni_GetMethodID_impl(jo cls, const char *name, const char *sig, bool is_static)
{
    if (!cls || cls->kind != TL_K_CLASS) {
        tl_log_line("jni: Get%sMethodID(NULL class, %s%s)", is_static ? "Static" : "", name, sig);
        tl_jni_throw("java/lang/NullPointerException", "class is null");
        return NULL;
    }
    tl_jmeth *m = lookup_method(cls->klass.jc, name, sig, is_static);
    TRACE("jni: Get%sMethodID(%s, %s%s) -> %s", is_static ? "Static" : "", cls->klass.jc->name, name, sig,
          m->fn ? "implemented" : m->exists ? "exists, not implemented" : "NOT FOUND");
    if (!m->exists) {
        char msg[300]; snprintf(msg, sizeof(msg), "no %smethod \"%s\" %s in class L%s;", is_static ? "static " : "", name, sig, cls->klass.jc->name);
        tl_jni_throw("java/lang/NoSuchMethodError", msg);
        return NULL;
    }
    return m;
}
static void *jni_GetMethodID(void *env, jo cls, const char *name, const char *sig) { (void)env; return jni_GetMethodID_impl(cls, name, sig, false); }
static void *jni_GetStaticMethodID(void *env, jo cls, const char *name, const char *sig) { (void)env; return jni_GetMethodID_impl(cls, name, sig, true); }

/* --- Call*Method families --- */

#define JV_TYPES(X) \
    X(Object, jo, l) X(Boolean, uint8_t, z) X(Byte, int8_t, b) X(Char, uint16_t, c) X(Short, int16_t, s) \
    X(Int, int32_t, i) X(Long, int64_t, j) X(Float, float, f) X(Double, double, d)

#define GEN_CALLS(T, CT, F) \
    CT jni_Call##T##MethodA(void *env, jo o, void *mid, const jvalue *a) { (void)env; return (CT)call_a(0, o, mid, a).F; } \
    CT jni_Call##T##MethodV(void *env, jo o, void *mid, tl_va_list *ap) { (void)env; return (CT)call_va(0, o, mid, ap).F; } \
    CT tl_vai_jni_Call##T##Method(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 3, 0, &ap); return (CT)call_va(0, (jo)f->gp[1], (void *)f->gp[2], &ap).F; } \
    TL_VA_STUB(tl_va_jni_Call##T##Method, tl_vai_jni_Call##T##Method); \
    CT jni_CallNonvirtual##T##MethodA(void *env, jo o, jo c, void *mid, const jvalue *a) { (void)env; (void)c; return (CT)call_a(1, o, mid, a).F; } \
    CT jni_CallNonvirtual##T##MethodV(void *env, jo o, jo c, void *mid, tl_va_list *ap) { (void)env; (void)c; return (CT)call_va(1, o, mid, ap).F; } \
    CT tl_vai_jni_CallNonvirtual##T##Method(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 4, 0, &ap); return (CT)call_va(1, (jo)f->gp[1], (void *)f->gp[3], &ap).F; } \
    TL_VA_STUB(tl_va_jni_CallNonvirtual##T##Method, tl_vai_jni_CallNonvirtual##T##Method); \
    CT jni_CallStatic##T##MethodA(void *env, jo c, void *mid, const jvalue *a) { (void)env; (void)c; return (CT)call_a(2, NULL, mid, a).F; } \
    CT jni_CallStatic##T##MethodV(void *env, jo c, void *mid, tl_va_list *ap) { (void)env; (void)c; return (CT)call_va(2, NULL, mid, ap).F; } \
    CT tl_vai_jni_CallStatic##T##Method(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 3, 0, &ap); return (CT)call_va(2, NULL, (void *)f->gp[2], &ap).F; } \
    TL_VA_STUB(tl_va_jni_CallStatic##T##Method, tl_vai_jni_CallStatic##T##Method); \
    extern void tl_va_jni_Call##T##Method(void); extern void tl_va_jni_CallNonvirtual##T##Method(void); extern void tl_va_jni_CallStatic##T##Method(void);

JV_TYPES(GEN_CALLS)

/* void is its own case: nothing to return */
void jni_CallVoidMethodA(void *env, jo o, void *mid, const jvalue *a) { (void)env; call_a(0, o, mid, a); }
void jni_CallVoidMethodV(void *env, jo o, void *mid, tl_va_list *ap) { (void)env; call_va(0, o, mid, ap); }
int tl_vai_jni_CallVoidMethod(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 3, 0, &ap); call_va(0, (jo)f->gp[1], (void *)f->gp[2], &ap); return 0; }
TL_VA_STUB(tl_va_jni_CallVoidMethod, tl_vai_jni_CallVoidMethod);
void jni_CallNonvirtualVoidMethodA(void *env, jo o, jo c, void *mid, const jvalue *a) { (void)env; (void)c; call_a(1, o, mid, a); }
void jni_CallNonvirtualVoidMethodV(void *env, jo o, jo c, void *mid, tl_va_list *ap) { (void)env; (void)c; call_va(1, o, mid, ap); }
int tl_vai_jni_CallNonvirtualVoidMethod(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 4, 0, &ap); call_va(1, (jo)f->gp[1], (void *)f->gp[3], &ap); return 0; }
TL_VA_STUB(tl_va_jni_CallNonvirtualVoidMethod, tl_vai_jni_CallNonvirtualVoidMethod);
void jni_CallStaticVoidMethodA(void *env, jo c, void *mid, const jvalue *a) { (void)env; (void)c; call_a(2, NULL, mid, a); }
void jni_CallStaticVoidMethodV(void *env, jo c, void *mid, tl_va_list *ap) { (void)env; (void)c; call_va(2, NULL, mid, ap); }
int tl_vai_jni_CallStaticVoidMethod(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 3, 0, &ap); call_va(2, NULL, (void *)f->gp[2], &ap); return 0; }
TL_VA_STUB(tl_va_jni_CallStaticVoidMethod, tl_vai_jni_CallStaticVoidMethod);
extern void tl_va_jni_CallVoidMethod(void); extern void tl_va_jni_CallNonvirtualVoidMethod(void); extern void tl_va_jni_CallStaticVoidMethod(void);

/* --- NewObject: allocate, then run the constructor if one is implemented --- */

static jo new_object(jo cls, void *mid, int how, const jvalue *a, tl_va_list *ap)
{
    if (!cls || cls->kind != TL_K_CLASS) { tl_jni_throw("java/lang/NullPointerException", "class is null"); return NULL; }
    jobj *o = tl_jni_new_object(cls->klass.jc);
    tl_jmeth *m = mid_ok(mid);
    if (m) {
        jvalue args[40];
        if (how == 1) args_from_va(m, ap, args);
        const jvalue *use = how == 1 ? args : a;
        /* new String(...) through JNI: ART makes it with StringFactory, as bytecode does. */
        if (cls->klass.jc->dvm && !strcmp(cls->klass.jc->name, "java/lang/String") && !strcmp(m->name, "<init>")) {
            bool dvm_string_init(const char *sig, const jvalue *p, jvalue *ret);
            jvalue r = {0};
            tl_jni_unref(o);
            return dvm_string_init(m->sig, use, &r) ? r.l : NULL;
        }
        jvalue r = invoke(o, m, true, use);
        /* A constructor can stand in for the object it was given: String is not an ordinary object. */
        if (r.l && ((jobj *)r.l)->kind != TL_K_OBJECT) { tl_jni_unref(o); return r.l; }
        if (r.l && r.l != o) { tl_jni_unref(o); return r.l; }
    }
    return o;
}
static jo jni_NewObjectA(void *env, jo cls, void *mid, const jvalue *a) { (void)env; return new_object(cls, mid, 2, a, NULL); }
static jo jni_NewObjectV(void *env, jo cls, void *mid, tl_va_list *ap) { (void)env; return new_object(cls, mid, 1, NULL, ap); }
jo tl_vai_jni_NewObject(tl_va_frame *f) { tl_va_list ap; tl_va_start(f, 3, 0, &ap); return new_object((jo)f->gp[1], (void *)f->gp[2], 1, NULL, &ap); }
TL_VA_STUB(tl_va_jni_NewObject, tl_vai_jni_NewObject);
extern void tl_va_jni_NewObject(void);

/* --- fields --- */

static void *jni_GetFieldID_impl(jo cls, const char *name, const char *sig, bool is_static)
{
    if (!cls || cls->kind != TL_K_CLASS) {
        tl_log_line("jni: Get%sFieldID(NULL class, %s %s)", is_static ? "Static" : "", name, sig);
        tl_jni_throw("java/lang/NullPointerException", "class is null");
        return NULL;
    }
    char real[1024];
    bool mismatch = dex_field_real_sig(cls->klass.jc, name, real, sizeof(real)) && strcmp(real, sig) != 0;
    tl_jfield *f = mismatch ? NULL : lookup_field(cls->klass.jc, name, sig, is_static, false);
    TRACE("jni: Get%sFieldID(%s, %s %s) -> %s", is_static ? "Static" : "", cls->klass.jc->name, name, sig, f ? "ok" : "NOT FOUND");
    if (!f) { char msg[300]; snprintf(msg, sizeof(msg), "no field \"%s\" %s", name, sig); tl_jni_throw("java/lang/NoSuchFieldError", msg); }
    return f;
}
static void *jni_GetFieldID(void *env, jo cls, const char *n, const char *s) { (void)env; return jni_GetFieldID_impl(cls, n, s, false); }
static void *jni_GetStaticFieldID(void *env, jo cls, const char *n, const char *s) { (void)env; return jni_GetFieldID_impl(cls, n, s, true); }

/* ------------------------------------------------------------- reflection */

static jobj *reflected(const char *cls, void *member)
{
    jobj *o = tl_jni_new_object(tl_jni_class(cls));
    o->native = member;
    return o;
}

jobj *tl_jni_reflect_method(jobj *cls, const char *name, const char *sig, bool is_static)
{
    if (!cls || cls->kind != TL_K_CLASS) return NULL;
    tl_jmeth *m = lookup_method(cls->klass.jc, name, sig, is_static);
    if (!m || !m->exists) {
        /* The real helper matches by name and parameters, with an Object argument fitting any object parameter. */
        char real[512]; bool st = false;
        for (tl_jclass *c = cls->klass.jc; c; c = c->super)
            if (c->in_dex && tl_dexidx_find_method_lenient(c->name, name, sig, real, sizeof(real), &st) && st == is_static) {
                m = lookup_method(cls->klass.jc, name, real, is_static);
                break;
            }
    }
    if (!m || !m->exists) return NULL;
    return reflected(!strcmp(name, "<init>") ? "java/lang/reflect/Constructor" : "java/lang/reflect/Method", m);
}

jobj *tl_jni_reflect_field(jobj *cls, const char *name, const char *sig, bool is_static)
{
    if (!cls || cls->kind != TL_K_CLASS) return NULL;
    /* The real helper finds a field by name; Unity passes "Ljava/lang/Object;" for any object-typed field. */
    char real[1024];
    if (dex_field_real_sig(cls->klass.jc, name, real, sizeof(real)) && strcmp(real, sig) != 0
        && (real[0] == 'L' || real[0] == '[') && (sig[0] == 'L' || sig[0] == '['))
        sig = real;
    tl_jfield *f = lookup_field(cls->klass.jc, name, sig, is_static, false);
    return f ? reflected("java/lang/reflect/Field", f) : NULL;
}

/* The members of a framework class (one the APK does not define) that this Java world knows: the methods implemented for it and the
 * fields its objects have been given. What reflection over such a class can honestly list. */
void tl_jni_each_known_member(jobj *cls, bool methods, void (*fn)(const char *name, const char *sig, uint32_t flags, void *ctx), void *ctx)
{
    if (!cls || cls->kind != TL_K_CLASS) return;
    tl_jclass *jc = cls->klass.jc;
    if (methods) {
        for (int t = 0; t < g_nhle; t++)
            for (const tl_jhle *e = g_hle[t]; e->cls; e++)
                if (!strcmp(e->cls, jc->name)) fn(e->name, e->sig, 1 /* public */, ctx);
        return;
    }
    pthread_mutex_lock(&g_lock);
    int n = jc->nfields;
    tl_jfield **copy = malloc((size_t)(n ? n : 1) * sizeof(*copy));
    memcpy(copy, jc->fields, (size_t)n * sizeof(*copy));
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < n; i++) fn(copy[i]->name, copy[i]->sig, 1u | (copy[i]->is_static ? 8u : 0u), ctx);
    free(copy);
}

/* A class's superclass as a Class object, or NULL for java/lang/Object and anything without one. */
jobj *tl_jni_class_super(jobj *cls)
{
    if (!cls || cls->kind != TL_K_CLASS || !cls->klass.jc->super) return NULL;
    return tl_jni_class_object(cls->klass.jc->super->name);
}

const char *tl_jni_reflected_field_sig(const jobj *field) { return field && field->native ? ((const tl_jfield *)field->native)->sig : NULL; }

jobj *tl_jni_reflected_declaring_class(const jobj *member)
{
    if (!member || !member->native) return NULL;
    const char *k = member->cls ? member->cls->name : "";
    tl_jclass *c = !strcmp(k, "java/lang/reflect/Field") ? ((const tl_jfield *)member->native)->cls : ((const tl_jmeth *)member->native)->cls;
    return c ? c->mirror : NULL;
}

const char *tl_jni_reflected_name(const jobj *member)
{
    if (!member || !member->native) return NULL;
    return !strcmp(member->cls->name, "java/lang/reflect/Field") ? ((const tl_jfield *)member->native)->name : ((const tl_jmeth *)member->native)->name;
}
const char *tl_jni_reflected_sig(const jobj *member)
{
    if (!member || !member->native) return NULL;
    return !strcmp(member->cls->name, "java/lang/reflect/Field") ? ((const tl_jfield *)member->native)->sig : ((const tl_jmeth *)member->native)->sig;
}

/* With the Java runtime up, Method/Constructor/Field objects are its own (Class.getMethod and the like): their interpreter member
 * is turned into the JNI id for it, and ids back into its objects */
struct dvm_method; struct dvm_field;
struct dvm_method *dvm_method_of_reflect(jo exe);
jo dvm_make_executable(struct dvm_method *m);
jo dvm_make_field_object(struct dvm_field *f);
bool dvm_member_info(const void *member, bool field, tl_jclass **cls, const char **name, const char **sig, bool *is_static);
void *dvm_field_of_jfield(tl_jclass *cls, const char *name, bool is_static);
static void *jni_FromReflectedMethod(void *env, jo m)
{
    (void)env;
    if (tl_dvm && m && m->kind == TL_K_OBJECT) {
        struct dvm_method *dm = dvm_method_of_reflect(m);
        tl_jclass *c; const char *n, *sg; bool st;
        if (dm && dvm_member_info(dm, false, &c, &n, &sg, &st)) return lookup_method(c, n, sg, st);
    }
    if (!m || !m->native) { TRACE("jni: FromReflectedMethod(%s) -> NPE", m ? "no member" : "NULL"); tl_jni_throw("java/lang/NullPointerException", "reflected method is null"); return NULL; }
    TRACE("jni: FromReflectedMethod -> %s.%s%s", ((tl_jmeth *)m->native)->cls->name, ((tl_jmeth *)m->native)->name, ((tl_jmeth *)m->native)->sig);
    return m->native;
}
static void *jni_FromReflectedField(void *env, jo f)
{
    (void)env;
    if (tl_dvm && f && f->native && f->kind == TL_K_OBJECT && f->cls && f->cls->dvm) {
        tl_jclass *c; const char *n, *sg; bool st;
        if (dvm_member_info(f->native, true, &c, &n, &sg, &st)) return lookup_field(c, n, sg, st, true);
    }
    if (!f || !f->native) { tl_jni_throw("java/lang/NullPointerException", "reflected field is null"); return NULL; }
    return f->native;
}
static jo jni_ToReflectedMethod(void *env, jo cls, void *mid, uint8_t is_static)
{
    (void)env; (void)cls; (void)is_static;
    tl_jmeth *m = mid;
    if (tl_dvm && m && m->dvm) return dvm_make_executable(m->dvm);
    return m ? reflected(!strcmp(m->name, "<init>") ? "java/lang/reflect/Constructor" : "java/lang/reflect/Method", m) : NULL;
}
static jo jni_ToReflectedField(void *env, jo cls, void *fid, uint8_t is_static)
{
    (void)env; (void)cls; (void)is_static;
    if (tl_dvm && fid) {
        tl_jfield *f = fid;
        void *df = dvm_field_of_jfield(f->cls, f->name, f->is_static);
        if (df) return dvm_make_field_object(df);
    }
    return fid ? reflected("java/lang/reflect/Field", fid) : NULL;
}

#define REF_Object(x) ((jo)tl_jni_ref((jo)(x)))
#define REF_Boolean(x) (x)
#define REF_Byte(x) (x)
#define REF_Char(x) (x)
#define REF_Short(x) (x)
#define REF_Int(x) (x)
#define REF_Long(x) (x)
#define REF_Float(x) (x)
#define REF_Double(x) (x)
/*
 * An enum constant (or a singleton named like one) is an object of its own class held in a static field of it, made by the class initialiser, which nothing here
 * runs. Native code that reads one expects the object, and some libraries (Epic Online Services') treat null as a broken install, so the first read makes one.
 */
static void enum_constant_fallback(void *fid, jvalue *slot)
{
    tl_jfield *f = fid;
    if (slot->l || !f || !f->cls || f->sig[0] != 'L') return;
    size_t n = strlen(f->cls->name);
    if (strncmp(f->sig + 1, f->cls->name, n) || f->sig[n + 1] != ';' || f->sig[n + 2]) return;
    for (const char *p = f->name; *p; p++) if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) return;
    slot->l = tl_jni_new_object(f->cls);
    TRACE("jni: static %s.%s was never set: made an object of its own class", f->cls->name, f->name);
}
/* A field ID is NULL when its lookup failed (and left an exception pending); native code that goes on to use it must not take the process down. */
#define GEN_FIELDS(T, CT, F) \
    static CT jni_Get##T##Field(void *env, jo o, void *fid) { (void)env; if (!fid || !o) return (CT)0; return (CT)REF_##T(field_slot(o, fid)->F); } \
    static void jni_Set##T##Field(void *env, jo o, void *fid, CT v) { (void)env; if (!fid || !o) return; field_slot(o, fid)->F = v; } \
    static CT jni_GetStatic##T##Field(void *env, jo c, void *fid) { (void)env; (void)c; if (!fid) return (CT)0; jvalue *sl = field_slot(NULL, fid); if (#T[0] == 'O') enum_constant_fallback(fid, sl); return (CT)REF_##T(sl->F); } \
    static void jni_SetStatic##T##Field(void *env, jo c, void *fid, CT v) { (void)env; (void)c; if (!fid) return; field_slot(NULL, fid)->F = v; }
JV_TYPES(GEN_FIELDS)

/* --- strings --- */

static size_t utf16_len(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        if (*p < 0x80) { p += 1; n++; } else if ((*p >> 5) == 6) { p += 2; n++; }
        else if ((*p >> 4) == 14) { p += 3; n++; } else { p += 4; n += 2; }
    }
    return n;
}
static uint16_t *to_utf16(const char *s, size_t *outlen)
{
    size_t cap = utf16_len(s), k = 0;
    uint16_t *u = malloc((cap + 1) * 2);
    for (const unsigned char *p = (const unsigned char *)s; *p;) {
        uint32_t cp;
        if (*p < 0x80) { cp = *p; p += 1; }
        else if ((*p >> 5) == 6) { cp = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f); p += 2; }
        else if ((*p >> 4) == 14) { cp = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f); p += 3; }
        else { cp = ((p[0] & 7) << 18) | ((p[1] & 0x3f) << 12) | ((p[2] & 0x3f) << 6) | (p[3] & 0x3f); p += 4; }
        if (cp >= 0x10000) { cp -= 0x10000; u[k++] = (uint16_t)(0xD800 + (cp >> 10)); u[k++] = (uint16_t)(0xDC00 + (cp & 0x3ff)); }
        else u[k++] = (uint16_t)cp;
    }
    u[k] = 0;
    if (outlen) *outlen = k;
    return u;
}
static char *from_utf16(const uint16_t *u, size_t n)
{
    char *out = malloc(n * 3 + 1); size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t c = u[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n) { c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00); i++; }
        if (c < 0x80) out[k++] = (char)c;
        else if (c < 0x800) { out[k++] = (char)(0xC0 | (c >> 6)); out[k++] = (char)(0x80 | (c & 0x3f)); }
        else if (c < 0x10000) { out[k++] = (char)(0xE0 | (c >> 12)); out[k++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[k++] = (char)(0x80 | (c & 0x3f)); }
        else { out[k++] = (char)(0xF0 | (c >> 18)); out[k++] = (char)(0x80 | ((c >> 12) & 0x3f)); out[k++] = (char)(0x80 | ((c >> 6) & 0x3f)); out[k++] = (char)(0x80 | (c & 0x3f)); }
    }
    out[k] = 0;
    return out;
}

static jo jni_NewString(void *env, const uint16_t *chars, int32_t len) { (void)env; char *s = from_utf16(chars, (size_t)len); jo o = tl_jni_new_string(s); free(s); return o; }
static int32_t jni_GetStringLength(void *env, jo s) { (void)env; return (int32_t)utf16_len(tl_jni_string(s) ? tl_jni_string(s) : ""); }
static const uint16_t *jni_GetStringChars(void *env, jo s, uint8_t *copy) { (void)env; if (copy) *copy = 1; return to_utf16(tl_jni_string(s) ? tl_jni_string(s) : "", NULL); }
static void jni_ReleaseStringChars(void *env, jo s, const uint16_t *c) { (void)env; (void)s; free((void *)c); }
static jo jni_NewStringUTF(void *env, const char *utf) { (void)env; return utf ? tl_jni_new_string(utf) : NULL; }
static int32_t jni_GetStringUTFLength(void *env, jo s) { (void)env; return (int32_t)strlen(tl_jni_string(s) ? tl_jni_string(s) : ""); }
static const char *jni_GetStringUTFChars(void *env, jo s, uint8_t *copy) { (void)env; if (copy) *copy = 1; return strdup(tl_jni_string(s) ? tl_jni_string(s) : ""); }
static void jni_ReleaseStringUTFChars(void *env, jo s, const char *c) { (void)env; (void)s; free((void *)c); }
static void jni_GetStringRegion(void *env, jo s, int32_t start, int32_t len, uint16_t *buf)
{
    (void)env;
    size_t n; uint16_t *u = to_utf16(tl_jni_string(s) ? tl_jni_string(s) : "", &n);
    if (start >= 0 && len >= 0 && (size_t)(start + len) <= n) memcpy(buf, u + start, (size_t)len * 2);
    free(u);
}
static void jni_GetStringUTFRegion(void *env, jo s, int32_t start, int32_t len, char *buf)
{
    (void)env;
    size_t n; uint16_t *u = to_utf16(tl_jni_string(s) ? tl_jni_string(s) : "", &n);
    if (start >= 0 && len >= 0 && (size_t)(start + len) <= n) { char *o = from_utf16(u + start, (size_t)len); strcpy(buf, o); free(o); }
    free(u);
}

/* --- arrays --- */

static int32_t jni_GetArrayLength(void *env, jo a) { (void)env; return a ? (int32_t)(a->kind == TL_K_OBJ_ARRAY ? a->oarr.len : a->arr.len) : 0; }
static jo jni_NewObjectArray(void *env, int32_t len, jo cls, jo init)
{
    (void)env;
    jo a = tl_jni_new_obj_array(cls ? cls->klass.jc : NULL, (uint32_t)len);
    for (int32_t i = 0; init && i < len; i++) a->oarr.v[i] = tl_jni_ref(init);
    return a;
}
static jo jni_GetObjectArrayElement(void *env, jo a, int32_t i) { (void)env; return (a && i >= 0 && (uint32_t)i < a->oarr.len) ? tl_jni_ref(a->oarr.v[i]) : NULL; }
static void jni_SetObjectArrayElement(void *env, jo a, int32_t i, jo v)
{
    (void)env;
    if (!a || i < 0 || (uint32_t)i >= a->oarr.len) return;
    tl_jni_unref(a->oarr.v[i]);
    a->oarr.v[i] = tl_jni_ref(v);
}

#define PRIM_ARRAYS(X) \
    X(Boolean, uint8_t, 'Z') X(Byte, int8_t, 'B') X(Char, uint16_t, 'C') X(Short, int16_t, 'S') \
    X(Int, int32_t, 'I') X(Long, int64_t, 'J') X(Float, float, 'F') X(Double, double, 'D')

#define GEN_ARRAYS(T, CT, K) \
    static jo jni_New##T##Array(void *env, int32_t len) { (void)env; return tl_jni_new_prim_array(K, (uint32_t)len); } \
    static CT *jni_Get##T##ArrayElements(void *env, jo a, uint8_t *copy) { (void)env; if (copy) *copy = 0; return a ? a->arr.data : NULL; } \
    static void jni_Release##T##ArrayElements(void *env, jo a, CT *e, int32_t mode) { (void)env; (void)a; (void)e; (void)mode; } \
    static void jni_Get##T##ArrayRegion(void *env, jo a, int32_t s, int32_t n, CT *buf) { (void)env; \
        if (a && s >= 0 && n >= 0 && (uint32_t)(s + n) <= a->arr.len) memcpy(buf, (CT *)a->arr.data + s, (size_t)n * sizeof(CT)); } \
    static void jni_Set##T##ArrayRegion(void *env, jo a, int32_t s, int32_t n, const CT *buf) { (void)env; \
        if (a && s >= 0 && n >= 0 && (uint32_t)(s + n) <= a->arr.len) memcpy((CT *)a->arr.data + s, buf, (size_t)n * sizeof(CT)); }
PRIM_ARRAYS(GEN_ARRAYS)

static void *jni_GetPrimitiveArrayCritical(void *env, jo a, uint8_t *copy) { (void)env; if (copy) *copy = 0; return a ? a->arr.data : NULL; }
static void jni_ReleasePrimitiveArrayCritical(void *env, jo a, void *p, int32_t mode) { (void)env; (void)a; (void)p; (void)mode; }
static const uint16_t *jni_GetStringCritical(void *env, jo s, uint8_t *copy) { return jni_GetStringChars(env, s, copy); }
static void jni_ReleaseStringCritical(void *env, jo s, const uint16_t *c) { jni_ReleaseStringChars(env, s, c); }

/* --- natives, monitors, misc --- */

typedef struct { const char *name, *sig; void *fn; } native_method;

static int32_t jni_RegisterNatives(void *env, jo cls, const native_method *m, int32_t n)
{
    (void)env;
    if (!cls || cls->kind != TL_K_CLASS) return -1;
    tl_jclass *given = cls->klass.jc;
    bool tl_dvm_declares(tl_jclass *jc, const char *name, const char *sig);
    pthread_mutex_lock(&g_lock);
    for (int32_t i = 0; i < n; i++) {
        /* the class that declares the method, the given one or a superclass (ART's FindClassMethod): Reanimated registers
           NativeProxyCommon's natives through its subclass NativeProxy */
        tl_jclass *c = given;
        if (!tl_dvm_declares(c, m[i].name, m[i].sig))
            for (tl_jclass *k = given->super; k; k = k->super) if (tl_dvm_declares(k, m[i].name, m[i].sig)) { c = k; break; }
        c->natives = realloc(c->natives, (size_t)(c->nnatives + 1) * sizeof(*c->natives));
        c->natives[c->nnatives].name = strdup(m[i].name);
        c->natives[c->nnatives].sig = strdup(m[i].sig);
        c->natives[c->nnatives].fn = m[i].fn;
        c->nnatives++;
        TRACE("jni: RegisterNatives %s.%s%s -> %p", c->name, m[i].name, m[i].sig, m[i].fn);
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}
static int32_t jni_UnregisterNatives(void *env, jo cls) { (void)env; (void)cls; return 0; }

void *tl_jni_native(const char *cls, const char *name, const char *sig)
{
    tl_jclass *c = tl_jni_class(cls);
    void *r = NULL;
    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < c->nnatives; i++) if (!strcmp(c->natives[i].name, name) && (!sig || !strcmp(c->natives[i].sig, sig))) r = c->natives[i].fn;
    pthread_mutex_unlock(&g_lock);
    return r;
}

void dvm_monitor_enter(jobj *o);
bool dvm_monitor_exit(jobj *o);
static int32_t jni_MonitorEnter(void *env, jo o) { (void)env; if (o) dvm_monitor_enter((jobj *)o); return 0; }
static int32_t jni_MonitorExit(void *env, jo o) { (void)env; if (o) dvm_monitor_exit((jobj *)o); return 0; }

static jo jni_NewDirectByteBuffer(void *env, void *addr, int64_t cap)
{
    (void)env;
    if (tl_dvm) {
        /* the Java runtime's own DirectByteBuffer, made as ART makes it for JNI: DirectByteBuffer(long address, int capacity) */
        tl_jclass *c = tl_jni_class("java/nio/DirectByteBuffer");
        jobj *o = tl_jni_new_object(c);
        tl_jmeth *m = lookup_method(c, "<init>", "(JI)V", false);
        jvalue args[2]; args[0].j = (int64_t)(uintptr_t)addr; args[1].j = 0; args[1].i = (int32_t)cap;
        invoke(o, m, true, args);
        return o;
    }
    jobj *o = tl_jni_new_object(tl_jni_class("java/nio/DirectByteBuffer"));
    jvalue a, c; a.l = addr; c.j = cap;
    tl_jni_set_field(o, "address", "J", a);
    tl_jni_set_field(o, "capacity", "J", c);
    return o;
}
static void *jni_GetDirectBufferAddress(void *env, jo o) { (void)env; return o ? tl_jni_get_field(o, "address", "J").l : NULL; }
static int64_t jni_GetDirectBufferCapacity(void *env, jo o) { (void)env; return o ? tl_jni_get_field(o, "capacity", "J").j : -1; }

/* ------------------------------------------------------------- the VM */

typedef struct { const void *functions; } env_t;
static env_t g_env, g_vm;
static const void *g_env_fns[233];

static int32_t vm_DestroyJavaVM(void *vm) { (void)vm; return 0; }
static int32_t vm_Attach(void *vm, void **penv, void *args) { (void)vm; (void)args; *penv = &g_env; return 0; }
static int32_t vm_Detach(void *vm) { (void)vm; return 0; }
static int32_t vm_GetEnv(void *vm, void **penv, int32_t version) { (void)vm; (void)version; *penv = &g_env; return 0; }
static const void *g_vm_fns[8] = { NULL, NULL, NULL, vm_DestroyJavaVM, vm_Attach, vm_Detach, vm_GetEnv, vm_Attach };
static int32_t jni_GetJavaVM(void *env, void **vm) { (void)env; *vm = &g_vm; return 0; }

void *tl_jni_vm(void) { tl_jni_init(); return &g_vm; }
void *tl_jni_env(void) { tl_jni_init(); return &g_env; }

#define SLOT(n, fn) g_env_fns[n] = (const void *)(fn)
#define CALL_SLOTS(base, T) \
    SLOT(base, tl_va_jni_Call##T##Method); SLOT(base + 1, jni_Call##T##MethodV); SLOT(base + 2, jni_Call##T##MethodA); \
    SLOT(base + 30, tl_va_jni_CallNonvirtual##T##Method); SLOT(base + 31, jni_CallNonvirtual##T##MethodV); SLOT(base + 32, jni_CallNonvirtual##T##MethodA); \
    SLOT(base + 80, tl_va_jni_CallStatic##T##Method); SLOT(base + 81, jni_CallStatic##T##MethodV); SLOT(base + 82, jni_CallStatic##T##MethodA);
#define FIELD_SLOTS(i, T) SLOT(95 + i, jni_Get##T##Field); SLOT(104 + i, jni_Set##T##Field); SLOT(145 + i, jni_GetStatic##T##Field); SLOT(154 + i, jni_SetStatic##T##Field);
#define ARRAY_SLOTS(i, T) SLOT(175 + i, jni_New##T##Array); SLOT(183 + i, jni_Get##T##ArrayElements); SLOT(191 + i, jni_Release##T##ArrayElements); \
    SLOT(199 + i, jni_Get##T##ArrayRegion); SLOT(207 + i, jni_Set##T##ArrayRegion);

static pthread_once_t g_init_once = PTHREAD_ONCE_INIT;
static void build_tables(void)
{
    SLOT(4, jni_GetVersion); SLOT(6, jni_FindClass); SLOT(10, jni_GetSuperclass); SLOT(11, jni_IsAssignableFrom);
    SLOT(7, jni_FromReflectedMethod); SLOT(8, jni_FromReflectedField); SLOT(9, jni_ToReflectedMethod); SLOT(12, jni_ToReflectedField);
    SLOT(13, jni_Throw); SLOT(14, jni_ThrowNew); SLOT(15, jni_ExceptionOccurred); SLOT(16, jni_ExceptionDescribe);
    SLOT(17, jni_ExceptionClear); SLOT(18, jni_FatalError); SLOT(19, jni_PushLocalFrame); SLOT(20, jni_PopLocalFrame);
    SLOT(21, jni_NewGlobalRef); SLOT(22, jni_DeleteGlobalRef); SLOT(23, jni_DeleteLocalRef); SLOT(24, jni_IsSameObject);
    SLOT(25, jni_NewLocalRef); SLOT(26, jni_EnsureLocalCapacity); SLOT(27, jni_AllocObject);
    SLOT(28, tl_va_jni_NewObject); SLOT(29, jni_NewObjectV); SLOT(30, jni_NewObjectA);
    SLOT(31, jni_GetObjectClass); SLOT(32, jni_IsInstanceOf); SLOT(33, jni_GetMethodID);
    CALL_SLOTS(34, Object) CALL_SLOTS(37, Boolean) CALL_SLOTS(40, Byte) CALL_SLOTS(43, Char) CALL_SLOTS(46, Short)
    CALL_SLOTS(49, Int) CALL_SLOTS(52, Long) CALL_SLOTS(55, Float) CALL_SLOTS(58, Double) CALL_SLOTS(61, Void)
    SLOT(94, jni_GetFieldID); SLOT(113, jni_GetStaticMethodID); SLOT(144, jni_GetStaticFieldID);
    FIELD_SLOTS(0, Object) FIELD_SLOTS(1, Boolean) FIELD_SLOTS(2, Byte) FIELD_SLOTS(3, Char) FIELD_SLOTS(4, Short)
    FIELD_SLOTS(5, Int) FIELD_SLOTS(6, Long) FIELD_SLOTS(7, Float) FIELD_SLOTS(8, Double)
    SLOT(163, jni_NewString); SLOT(164, jni_GetStringLength); SLOT(165, jni_GetStringChars); SLOT(166, jni_ReleaseStringChars);
    SLOT(167, jni_NewStringUTF); SLOT(168, jni_GetStringUTFLength); SLOT(169, jni_GetStringUTFChars); SLOT(170, jni_ReleaseStringUTFChars);
    SLOT(171, jni_GetArrayLength); SLOT(172, jni_NewObjectArray); SLOT(173, jni_GetObjectArrayElement); SLOT(174, jni_SetObjectArrayElement);
    ARRAY_SLOTS(0, Boolean) ARRAY_SLOTS(1, Byte) ARRAY_SLOTS(2, Char) ARRAY_SLOTS(3, Short)
    ARRAY_SLOTS(4, Int) ARRAY_SLOTS(5, Long) ARRAY_SLOTS(6, Float) ARRAY_SLOTS(7, Double)
    SLOT(215, jni_RegisterNatives); SLOT(216, jni_UnregisterNatives); SLOT(217, jni_MonitorEnter); SLOT(218, jni_MonitorExit);
    SLOT(219, jni_GetJavaVM); SLOT(220, jni_GetStringRegion); SLOT(221, jni_GetStringUTFRegion);
    SLOT(222, jni_GetPrimitiveArrayCritical); SLOT(223, jni_ReleasePrimitiveArrayCritical);
    SLOT(224, jni_GetStringCritical); SLOT(225, jni_ReleaseStringCritical);
    SLOT(226, jni_NewGlobalRef); SLOT(227, jni_DeleteGlobalRef);
    SLOT(228, jni_ExceptionOccurred);   /* replaced below: ExceptionCheck returns a boolean */
    SLOT(229, jni_NewDirectByteBuffer); SLOT(230, jni_GetDirectBufferAddress); SLOT(231, jni_GetDirectBufferCapacity);
    SLOT(232, jni_GetObjectRefType);
    g_env.functions = g_env_fns;
    g_vm.functions = g_vm_fns;
}

static uint8_t jni_ExceptionCheck(void *env)
{
    static __thread jobj *t_seen;
    if (t_pending && g_trace >= 1 && t_pending != t_seen) { t_seen = t_pending; jni_ExceptionDescribe(env); }
    return t_pending != NULL;
}

void tl_jni_init(void)
{
    pthread_once(&g_init_once, build_tables);
    g_env_fns[228] = (const void *)jni_ExceptionCheck;
}

/* ------------------------------------------------------------- for the interpreter */

tl_jmeth *tl_jni_method(tl_jclass *cls, const char *name, const char *sig, bool is_static) { return lookup_method(cls, name, sig, is_static); }
jvalue tl_jni_invoke(jobj *self, tl_jmeth *m, bool nonvirtual, const jvalue *args) { return invoke(self, m, nonvirtual, args); }
tl_jfield *tl_jni_field(tl_jclass *cls, const char *name, const char *sig, bool is_static) { return lookup_field(cls, name, sig, is_static, true); }
jvalue *tl_jni_field_slot(jobj *o, tl_jfield *f) { return field_slot(o, f); }
void tl_jni_each_declared(void (*fn)(tl_jclass *jc))
{
    int cap = 8192, n = 0;
    tl_jclass **list = malloc((size_t)cap * sizeof(*list));
    pthread_mutex_lock(&g_lock);
    for (int b = 0; b < NBUCKETS; b++) for (tl_jclass *c = g_classes[b]; c; c = c->next) {
        if (n == cap) { cap *= 2; list = realloc(list, (size_t)cap * sizeof(*list)); }
        list[n++] = c;
    }
    pthread_mutex_unlock(&g_lock);
    for (int i = 0; i < n; i++) fn(list[i]);
    free(list);
}

tl_jclass *tl_jni_find_declared(const char *name)
{
    pthread_mutex_lock(&g_lock);
    tl_jclass *c = find_class_locked(name);
    pthread_mutex_unlock(&g_lock);
    tl_jni_wait_linked(c);
    return c;
}
