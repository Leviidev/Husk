/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The JNI world's own structures, shared with the Dalvik interpreter (husk-tl-dvm.c), whose classes, methods and fields are these
 * same objects: a class the interpreter runs is a tl_jclass with `dvm` set, and native code reaching it through JNI ends up in the
 * interpreter by way of the hooks below.
 */
#ifndef HUSK_TL_JNI_INTERNAL_H
#define HUSK_TL_JNI_INTERNAL_H

#include <stdatomic.h>
#include "husk-tl-jni.h"

typedef struct tl_jmeth {
    tl_jclass *cls;                    /* the class the method was looked up on */
    char *name, *sig;
    bool is_static;
    tl_jhle_fn fn;                     /* NULL: no implementation */
    char argk[40];                     /* one kind letter per argument: Z B C S I J F D L */
    int nargs;
    char retk;                         /* kind of the return value, or V */
    bool exists;                       /* declared by something we can see */
    bool warned;
    void *dvm;                         /* the interpreter's method, when the class is one it runs */
} tl_jmeth;

typedef struct tl_jfield {
    tl_jclass *cls;
    char *name, *sig;
    bool is_static;
    uint32_t index;
    /* a field the interpreter's class does not declare, asked for by name from C: kept here, never in the object's slots (resizing
       those under the interpreter frees memory it is using) */
    bool detached;
    jvalue dval;
} tl_jfield;

struct tl_jclass {
    char *name;                        /* any length: Dagger's generated factories run past 160 characters */
    tl_jclass *super;
    jobj *mirror;
    bool in_dex;                       /* the APK defines it */
    tl_jmeth **meths; int nmeths, capm;
    tl_jfield **fields; int nfields, capf;
    jvalue *statics; int nstatics;
    struct { char *name, *sig; void *fn; } *natives; int nnatives;
    tl_jclass *next;
    void *dvm;                         /* the interpreter's class (husk-tl-dvm.c), or NULL */
    _Atomic bool linking;              /* declared, its bytecode still being linked (by the thread holding the link lock) */
};

/* A class another thread is still linking: wait for it (returns at once on the linking thread itself). */
void tl_jni_wait_linked(tl_jclass *c);

/* What the interpreter provides, once it is running (tl_dvm_start). */
typedef struct tl_dvm_hooks {
    /* A class the interpreter has bytecode for: attach it (sets jc->dvm, jc->super, statics). */
    bool (*attach)(tl_jclass *jc);
    /* The method name/sig on (or above) a class it runs. */
    void *(*find_method)(tl_jclass *jc, const char *name, const char *sig, bool is_static);
    /* Call one of its methods from native code. */
    jvalue (*invoke)(void *method, jobj *self, bool nonvirtual, const jvalue *args);
    /* A field of a class it runs: the class that declares it and its slot. */
    bool (*find_field)(tl_jclass *jc, const char *name, const char *sig, bool is_static, tl_jclass **decl, uint32_t *slot);
    /* Instance slots an object of the class needs. */
    uint32_t (*instance_slots)(tl_jclass *jc);
} tl_dvm_hooks;
extern const tl_dvm_hooks *tl_dvm;

/* Exceptions, for the interpreter. */
jobj *tl_jni_pending_object(void);
void tl_jni_set_pending(jobj *e);

/* JNI's method/field lookups, for the interpreter's own use of HLE classes. */
tl_jmeth *tl_jni_method(tl_jclass *cls, const char *name, const char *sig, bool is_static);
jvalue tl_jni_invoke(jobj *self, tl_jmeth *m, bool nonvirtual, const jvalue *args);
tl_jfield *tl_jni_field(tl_jclass *cls, const char *name, const char *sig, bool is_static);
jvalue *tl_jni_field_slot(jobj *o, tl_jfield *f);
tl_jclass *tl_jni_find_declared(const char *name);   /* already declared, or NULL */
void tl_jni_each_declared(void (*fn)(tl_jclass *jc));

#endif
