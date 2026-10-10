/* SPDX-License-Identifier: GPL-2.0-or-later */
/* The Dalvik runtime's structures, shared between the interpreter (husk-tl-dvm.c) and its natives (husk-tl-dvm-natives.c). */
#ifndef HUSK_TL_DVM_INTERNAL_H
#define HUSK_TL_DVM_INTERNAL_H

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "husk-tl-dvm.h"
#include "husk-tl-jni-internal.h"

typedef struct dvm_dex dvm_dex;
typedef struct dvm_class dvm_class;
typedef struct dvm_method dvm_method;
typedef struct dvm_field dvm_field;

/* A native method ART implements itself: params in declaration order (wide values one jvalue each), self NULL for static.
 * Returns false when it threw (the exception is pending). */
typedef bool (*dvm_native_fn)(jobj *self, const jvalue *args, jvalue *ret);

struct dvm_dex {
    const uint8_t *b;
    size_t size;
    uint32_t nstr, ntype, nproto, nfield, nmeth, ncls;
    uint32_t str_off, type_off, proto_off, field_off, meth_off, cls_off;
    dvm_method **mcache;            /* by method_idx */
    dvm_field **fcache;             /* by field_idx */
    tl_jclass **tcache;             /* by type_idx */
    jobj **scache;                  /* interned constant strings, by string_idx */
    char name[96];
    int ns;                         /* 0: the boot class path and the app; else the namespace of a DexFile opened at run time */
    jobj *loader;                   /* a run-time dex: the class loader its classes were first defined through (Class.getClassLoader) */
};

struct dvm_field {
    dvm_class *cls;
    const char *name;
    const char *type;               /* descriptor */
    uint32_t flags;
    uint32_t slot;                  /* instance: index into the object's jvalue slots; static: into the class's statics */
    uint32_t idx;                   /* field_idx in its dex */
};

struct dvm_method {
    dvm_class *cls;
    const char *name;
    const char *sig;                /* "(...)R": as long as the descriptor is (a Kotlin constructor's runs to thousands) */
    const char *shorty;             /* return kind first, then each parameter: L I J F D Z B C S V */
    uint32_t flags;
    uint32_t idx;                   /* method_idx in its dex */
    uint16_t regs, ins, outs, ntries;
    uint32_t ninsns;
    const uint16_t *insns;          /* NULL: abstract or native */
    const uint8_t *code;            /* the code item */
    int vidx;                       /* vtable slot, or -1 */
    int nparams;
    dvm_native_fn intrinsic;        /* an ART-internal native, or NULL */
    void *jni;                      /* a JNI native's address, once found */
    bool jni_looked;
    signed char critical;           /* @CriticalNative: 1 yes, -1 no, 0 not looked at yet */
    jobj *proxy_method;             /* a Proxy class's method: the interface Method it hands the InvocationHandler */
    jobj *proxy_throws;             /* and the Class[] of checked exceptions it declares */
};

enum { CS_LINKED = 0, CS_INITIALIZING = 1, CS_INITIALIZED = 2, CS_FAILED = -1 };

struct dvm_class {
    tl_jclass *jc;
    const char *name;               /* "java/lang/String" or "[I" */
    dvm_dex *dex;                   /* NULL for array and primitive classes */
    uint32_t def;
    uint32_t flags;
    dvm_class *super;               /* the superclass when the interpreter runs it too */
    tl_jclass **ifaces; int nifaces;
    dvm_field *sf; int nsf;
    dvm_field *inf; int ninf;
    uint32_t nslots;                /* instance slots, the superclasses' included */
    dvm_method *dm; int ndm;        /* direct: static, private, constructors */
    dvm_method *vm; int nvm;        /* virtual */
    dvm_method **vtab; int nvtab;
    _Atomic int state;
    pthread_mutex_t init_lock;
    pthread_t init_thread;
    char elem;                      /* arrays: the element's descriptor letter ('L' or '[' for objects) */
    char prim;                      /* primitive classes: their letter */
    bool art_tables;                /* inf/sf and dm/vm are each one length-prefixed block (DVM_MEMBERS_HDR before them) */
};

#define DVM_MEMBERS_HDR 16

/* ---- the interpreter, for its natives */
dvm_class *dvm_class_of(tl_jclass *jc);
tl_jclass *dvm_class_named(const char *jni_name);       /* load or find; NULL when nothing defines it */
bool dvm_ensure_init(dvm_class *c);
bool dvm_call(dvm_method *m, jobj *self, const jvalue *params, jvalue *ret);
dvm_method *dvm_find_method(dvm_class *c, const char *name, const char *sig, bool want_static);
dvm_method *dvm_find_virtual(tl_jclass *receiver, const char *name, const char *sig);
dvm_field *dvm_find_field(dvm_class *c, const char *name, bool want_static);
jobj *dvm_new_object(tl_jclass *jc);
jobj *dvm_new_string_utf8(const char *s);
bool dvm_instance_of(jobj *o, tl_jclass *c);
bool dvm_assignable(tl_jclass *sub, tl_jclass *sup);
tl_jclass *dvm_object_class(jobj *o);                   /* what getClass() says, arrays and classes included */
bool dvm_throw(const char *cls, const char *fmt, ...) __attribute__((format(printf, 2, 3)));   /* always false */
jvalue *dvm_slots(jobj *o);                             /* the object's instance slots (made for Class mirrors on demand) */
void dvm_monitor_enter(jobj *o);
bool dvm_monitor_exit(jobj *o);
jobj *dvm_current_thread(void);
extern int g_dvm_trace;
extern _Thread_local dvm_method *t_frames[8192];
extern _Thread_local int t_depth;

/* ---- the natives (husk-tl-dvm-natives.c) */
dvm_native_fn dvm_intrinsic(const char *cls, const char *name, const char *sig);
void dvm_natives_init(void);

#endif
