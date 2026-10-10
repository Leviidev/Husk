/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * JNI for guest native code, over a Java world that is implemented here.
 *
 * Native code reaches Java through a JNIEnv: it looks classes up by name, methods and
 * fields by name and signature, and calls them. Real Java is not running, so each class
 * the guest asks about is either one this file's companion (husk-tl-jni-hle.c)
 * implements in C, or one the APK defines and nothing here runs, or one that does not
 * exist. The distinction matters because the guest acts on it: Unity looks for optional
 * classes and treats NoClassDefFoundError as "not present".
 *
 *   - Classes and methods the APK's DEX defines are looked up in the DEX index, so a
 *     missing method throws NoSuchMethodError as Android would.
 *   - Framework classes (android/, java/) are assumed to exist; what they do is whatever
 *     the C implementation says, and a method with none logs once and returns zero.
 */
#ifndef HUSK_TL_JNI_H
#define HUSK_TL_JNI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef union jvalue {
    uint8_t z; int8_t b; uint16_t c; int16_t s; int32_t i; int64_t j; float f; double d; void *l;
} jvalue;

typedef struct jobj jobj;               /* every jobject, jclass, jstring, jarray handed to the guest */
typedef struct tl_jclass tl_jclass;

enum { TL_K_OBJECT, TL_K_CLASS, TL_K_STRING, TL_K_PRIM_ARRAY, TL_K_OBJ_ARRAY };

struct jobj {
    uint32_t refs;
    uint8_t kind;
    tl_jclass *cls;
    union {
        struct { char *utf8; } str;
        struct { void *data; uint32_t len; uint8_t esz; char etype; } arr;
        struct { jobj **v; uint32_t len; } oarr;
        struct { tl_jclass *jc; } klass;
    };
    jvalue *fields;                     /* instance fields, indexed by tl_jfield::index */
    uint32_t nfields;
    void *native;                       /* an implementation's own state */
    void *monitor;
};

/* What an implemented Java method receives. */
typedef struct tl_jcall {
    jobj *self;                         /* NULL for a static method */
    tl_jclass *cls;                     /* the class the method was found on */
    const jvalue *args;
    jvalue ret;                         /* written by the method */
} tl_jcall;

typedef void (*tl_jhle_fn)(tl_jcall *c);

typedef struct tl_jhle {
    const char *cls, *name, *sig;
    tl_jhle_fn fn;
} tl_jhle;

/* ----------------------------------------------------------------- setup */

void tl_jni_init(void);
void tl_jni_set_trace(int level);       /* 1: lookups; 2: every call */

/* Declare a class with its superclass (JNI names). Safe to repeat. */
tl_jclass *tl_jni_declare(const char *name, const char *super);
tl_jclass *tl_jni_class(const char *name);                    /* find or create; NULL never */
const char *tl_jni_class_name(const jobj *o);                 /* "java/lang/String", or "?" */
jobj *tl_jni_class_object(const char *name);                  /* the java.lang.Class object, as a jclass */
void tl_jni_register_hle(const tl_jhle *table);               /* NULL-terminated by cls == NULL */

void *tl_jni_vm(void);
void *tl_jni_env(void);

/* ---------------------------------------------------------------- objects */

jobj *tl_jni_new_object(tl_jclass *cls);
jobj *tl_jni_new_string(const char *utf8);
const char *tl_jni_string(const jobj *s);                     /* NULL for NULL / not a string */
jobj *tl_jni_new_prim_array(char etype, uint32_t len);
jobj *tl_jni_new_obj_array(tl_jclass *elem, uint32_t len);
jobj *tl_jni_ref(jobj *o);                                    /* a new reference to the same object */
void  tl_jni_unref(jobj *o);

/* Fields by name; created on first use. */
void tl_jni_set_field(jobj *o, const char *name, const char *sig, jvalue v);
jvalue tl_jni_get_field(jobj *o, const char *name, const char *sig);
void tl_jni_set_static_field(const char *cls, const char *name, const char *sig, jvalue v);
void tl_jni_set_static(const char *cls, const char *name, const char *sig, jvalue v);
jvalue tl_jni_get_static(const char *cls, const char *name, const char *sig);

/* An iterator over a fixed list of objects, for the Java collections implemented in C (each item gets a new reference as it is returned). */
jobj *tl_jni_new_list_iterator(jobj *const *items, uint32_t n);

/* Exceptions */
void tl_jni_throw(const char *cls, const char *msg);
bool tl_jni_pending(void);
void tl_jni_clear(void);

/* ---------------------------------------------------- what libraries register */

/* The native method a library registered with RegisterNatives (or NULL). */
void *tl_jni_native(const char *cls, const char *name, const char *sig);

/*
 * java.lang.reflect objects for methods and fields, as Unity's ReflectionHelper hands
 * them back to native code, which turns them into IDs with FromReflectedMethod/Field.
 * NULL when the class has no such member.
 */
jobj *tl_jni_reflect_method(jobj *cls, const char *name, const char *sig, bool is_static);
jobj *tl_jni_reflect_field(jobj *cls, const char *name, const char *sig, bool is_static);
const char *tl_jni_reflected_field_sig(const jobj *field);
jobj *tl_jni_class_super(jobj *cls);
void tl_jni_each_known_member(jobj *cls, bool methods, void (*fn)(const char *name, const char *sig, uint32_t flags, void *ctx), void *ctx);                          /* a Class's superclass, or NULL */
jobj *tl_jni_reflected_declaring_class(const jobj *member);
const char *tl_jni_reflected_name(const jobj *member);       /* a method's or field's name */
const char *tl_jni_reflected_sig(const jobj *member);        /* a method's or field's signature */

/* A Java proxy made by JNIBridge, whose methods run C# in the native library that registered JNIBridge.invoke. */


/* Call a Java method by name from C (HLE-implemented or not), as native code would. */
jvalue tl_jni_call(jobj *self_or_class, const char *name, const char *sig, const jvalue *args);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_JNI_H */
