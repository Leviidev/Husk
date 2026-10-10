/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Annotations, read from the dex as ART reads them (runtime/dex/dex_file_annotations.cc): a class's annotations_directory_item
 * lists annotation sets for the class, its fields, its methods and its methods' parameters. Each annotation_item has a visibility
 * (build, runtime, system) and an encoded_annotation: its type and (name, encoded_value) elements.
 *
 * Runtime-visible ones become annotation objects the way ART makes them: an AnnotationMember per element, given to libcore's
 * AnnotationFactory.createAnnotation, which makes a Proxy (husk-tl-dvm-proxy.c) of the annotation interface; elements left out get
 * their defaults through Method.getDefaultValue, from the annotation class's dalvik.annotation.AnnotationDefault. The system ones
 * answer the rest of reflection: Signature (generic types: Gson's TypeToken, Retrofit's return types), Throws
 * (getExceptionTypes, and so which exceptions a Proxy method may throw), InnerClass / EnclosingClass / EnclosingMethod /
 * MemberClasses (getSimpleName, isAnonymousClass, getEnclosingClass, getDeclaredClasses).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-dvm-internal.h"
#include "husk-tl-jni-internal.h"
#include "husk-tl-dvm.h"

void tl_log_line(const char *fmt, ...);
const char *dvm_dex_str(const dvm_dex *d, uint32_t idx);
const char *dvm_dex_type(const dvm_dex *d, uint32_t idx);
void dvm_dex_proto(const dvm_dex *d, uint32_t proto, char *sig, size_t n);
jobj *dvm_dex_string(dvm_dex *d, uint32_t idx);
jobj *dvm_make_executable(dvm_method *m);
dvm_method *dvm_method_of_reflect(jobj *exe);
dvm_field *dvm_field_of_reflect(jobj *f);
jobj *dvm_box(char k, jvalue v);

#define NAT(fn) bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue L(jobj *o) { jvalue r; r.j = 0; r.l = o; return r; }
static jvalue Z(bool b) { jvalue r; r.j = 0; r.z = b; return r; }
static jvalue I(int32_t v) { jvalue r; r.j = (uint32_t)v; return r; }

enum { VIS_BUILD = 0, VIS_RUNTIME = 1, VIS_SYSTEM = 2 };

static inline uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t uleb(const uint8_t **pp)
{
    const uint8_t *p = *pp; uint32_t r = 0; int s = 0;
    for (;;) { uint8_t b = *p++; r |= (uint32_t)(b & 0x7f) << s; if (!(b & 0x80)) break; s += 7; if (s > 28) break; }
    *pp = p; return r;
}

/* ---- classes by descriptor */

static tl_jclass *class_by_desc(const char *d)
{
    static const struct { char c; const char *n; } prim[] = { { 'Z', "boolean" }, { 'B', "byte" }, { 'C', "char" }, { 'S', "short" },
        { 'I', "int" }, { 'J', "long" }, { 'F', "float" }, { 'D', "double" }, { 'V', "void" } };
    if (!d || !*d) return NULL;
    for (size_t i = 0; i < sizeof(prim) / sizeof(prim[0]); i++) if (d[0] == prim[i].c && !d[1]) return tl_jni_class(prim[i].n);
    char n[400];
    if (d[0] == 'L') snprintf(n, sizeof(n), "%.*s", (int)strlen(d) - 2, d + 1);
    else snprintf(n, sizeof(n), "%s", d);
    tl_jclass *c = dvm_class_named(n);
    return c ? c : (d[0] == '[' ? tl_jni_class(n) : NULL);
}
static void desc_of_class(tl_jclass *jc, char *out, size_t n)
{
    dvm_class *c = dvm_class_of(jc);
    if (c && c->prim) snprintf(out, n, "%c", c->prim);
    else if (jc->name[0] == '[') snprintf(out, n, "%s", jc->name);
    else snprintf(out, n, "L%s;", jc->name);
}

/* ---- the annotations directory */

static const uint8_t *directory(dvm_class *c)
{
    if (!c || !c->dex) return NULL;
    const uint8_t *def = c->dex->b + c->dex->cls_off + 32 * c->def;
    uint32_t off = rd32(def + 20);
    return off ? c->dex->b + off : NULL;
}
static const uint8_t *class_set(dvm_class *c)
{
    const uint8_t *dir = directory(c);
    uint32_t off = dir ? rd32(dir) : 0;
    return off ? c->dex->b + off : NULL;
}
/* kind 0: fields, 1: methods, 2: method parameters (an annotation_set_ref_list) */
static const uint8_t *member_set(dvm_class *c, int kind, uint32_t idx)
{
    const uint8_t *dir = directory(c);
    if (!dir) return NULL;
    uint32_t nf = rd32(dir + 4), nm = rd32(dir + 8), np = rd32(dir + 12);
    const uint8_t *e = dir + 16;
    uint32_t n = nf;
    if (kind >= 1) { e += 8 * nf; n = nm; }
    if (kind == 2) { e += 8 * nm; n = np; }
    for (uint32_t i = 0; i < n; i++, e += 8) if (rd32(e) == idx) { uint32_t off = rd32(e + 4); return off ? c->dex->b + off : NULL; }
    return NULL;
}

/* the annotation_item of a type (a descriptor) in a set, with its visibility; its encoded annotation follows the byte */
static const uint8_t *find_item(dvm_dex *d, const uint8_t *set, const char *desc, int vis)
{
    if (!set) return NULL;
    uint32_t n = rd32(set);
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *item = d->b + rd32(set + 4 + 4 * i);
        if (item[0] != vis) continue;
        const uint8_t *p = item + 1;
        if (!strcmp(dvm_dex_type(d, uleb(&p)), desc)) return item;
    }
    return NULL;
}

/* ---- encoded values */

enum { V_BYTE = 0x00, V_SHORT = 0x02, V_CHAR = 0x03, V_INT = 0x04, V_LONG = 0x06, V_FLOAT = 0x10, V_DOUBLE = 0x11, V_METHOD_TYPE = 0x15,
       V_METHOD_HANDLE = 0x16, V_STRING = 0x17, V_TYPE = 0x18, V_FIELD = 0x19, V_METHOD = 0x1a, V_ENUM = 0x1b, V_ARRAY = 0x1c,
       V_ANNOTATION = 0x1d, V_NULL = 0x1e, V_BOOLEAN = 0x1f };

static void skip_value(const uint8_t **pp);
static void skip_annotation(const uint8_t **pp)
{
    uleb(pp);
    uint32_t n = uleb(pp);
    for (uint32_t i = 0; i < n; i++) { uleb(pp); skip_value(pp); }
}
static void skip_value(const uint8_t **pp)
{
    uint8_t h = *(*pp)++;
    int type = h & 0x1f, arg = h >> 5;
    switch (type) {
    case V_ARRAY: { uint32_t n = uleb(pp); for (uint32_t i = 0; i < n; i++) skip_value(pp); break; }
    case V_ANNOTATION: skip_annotation(pp); break;
    case V_NULL: case V_BOOLEAN: break;
    default: *pp += arg + 1; break;
    }
}
static uint64_t raw(const uint8_t **pp, int size)
{
    uint64_t v = 0;
    for (int i = 0; i < size; i++) v |= (uint64_t)(*pp)[i] << (8 * i);
    *pp += size;
    return v;
}
static int64_t sext(uint64_t v, int size) { int sh = 64 - 8 * size; return (int64_t)(v << sh) >> sh; }

/* one value as the primitive (letter k) an int[] / long[] / ... element needs */
static jvalue prim_value(const uint8_t **pp, char k)
{
    uint8_t h = *(*pp)++;
    int type = h & 0x1f, size = (h >> 5) + 1;
    jvalue v; v.j = 0;
    if (type == V_BOOLEAN) { v.z = (h >> 5) != 0; return v; }
    if (type == V_NULL) return v;
    uint64_t r = raw(pp, size);
    double dv = 0; int64_t iv = 0;
    switch (type) {
    case V_FLOAT: { union { uint32_t u; float f; } u; u.u = (uint32_t)(r << (8 * (4 - size))); dv = u.f; iv = (int64_t)dv; break; }
    case V_DOUBLE: { union { uint64_t u; double d; } u; u.u = r << (8 * (8 - size)); dv = u.d; iv = (int64_t)dv; break; }
    case V_CHAR: iv = (int64_t)r; dv = (double)iv; break;
    default: iv = sext(r, size); dv = (double)iv; break;
    }
    switch (k) {
    case 'F': v.f = (float)dv; break;
    case 'D': v.d = dv; break;
    case 'J': v.j = iv; break;
    case 'Z': v.z = iv != 0; break;
    case 'B': v.i = (int8_t)iv; break;
    case 'S': v.i = (int16_t)iv; break;
    case 'C': v.i = (uint16_t)iv; break;
    default: v.i = (int32_t)iv; break;
    }
    return v;
}

static jobj *make_annotation(dvm_dex *d, const uint8_t **pp);

static jobj *static_field_value(tl_jclass *jc, const char *name)
{
    dvm_class *c = dvm_class_of(jc);
    dvm_field *f = c ? dvm_find_field(c, name, true) : NULL;
    if (!f) return NULL;
    if (!dvm_ensure_init(f->cls)) return NULL;
    return f->cls->jc->statics ? f->cls->jc->statics[f->slot].l : NULL;
}

static jobj *method_object(dvm_dex *d, uint32_t midx)
{
    const uint8_t *mi = d->b + d->meth_off + 8 * midx;
    tl_jclass *jc = class_by_desc(dvm_dex_type(d, rd16(mi)));
    dvm_class *c = dvm_class_of(jc);
    if (!c) return NULL;
    char sig[8192];
    dvm_dex_proto(d, rd16(mi + 2), sig, sizeof(sig));
    const char *name = dvm_dex_str(d, rd32(mi + 4));
    dvm_method *m = dvm_find_method(c, name, sig, false);
    if (!m) m = dvm_find_method(c, name, sig, true);
    return m ? dvm_make_executable(m) : NULL;
}

/* A value as an object, of the type want (a descriptor) when that is known: primitives boxed, arrays of their element type. */
static jobj *value_object(dvm_dex *d, const uint8_t **pp, const char *want)
{
    const uint8_t *start = *pp;
    uint8_t h = **pp;
    int type = h & 0x1f, size = (h >> 5) + 1;
    switch (type) {
    case V_BYTE: case V_SHORT: case V_CHAR: case V_INT: case V_LONG: case V_FLOAT: case V_DOUBLE: case V_BOOLEAN: {
        char k = type == V_BYTE ? 'B' : type == V_SHORT ? 'S' : type == V_CHAR ? 'C' : type == V_INT ? 'I' : type == V_LONG ? 'J'
               : type == V_FLOAT ? 'F' : type == V_DOUBLE ? 'D' : 'Z';
        if (want && want[0] && !want[1] && strchr("BSCIJFDZ", want[0])) k = want[0];
        return dvm_box(k, prim_value(pp, k));
    }
    case V_NULL: (*pp)++; return NULL;
    }
    (*pp)++;
    switch (type) {
    case V_STRING: return dvm_dex_string(d, (uint32_t)raw(pp, size));
    case V_TYPE: { tl_jclass *c = class_by_desc(dvm_dex_type(d, (uint32_t)raw(pp, size))); return c ? c->mirror : NULL; }
    case V_ENUM: {
        uint32_t fidx = (uint32_t)raw(pp, size);
        const uint8_t *fi = d->b + d->field_off + 8 * fidx;
        tl_jclass *c = class_by_desc(dvm_dex_type(d, rd16(fi)));
        return c ? static_field_value(c, dvm_dex_str(d, rd32(fi + 4))) : NULL;
    }
    case V_METHOD: return method_object(d, (uint32_t)raw(pp, size));
    case V_ANNOTATION: return make_annotation(d, pp);
    case V_ARRAY: {
        uint32_t n = uleb(pp);
        const char *elem = want && want[0] == '[' ? want + 1 : NULL;
        if (elem && elem[0] && !elem[1] && strchr("BSCIJFDZ", elem[0])) {
            jobj *arr = tl_jni_new_prim_array(elem[0], n);
            arr->refs = 1u << 30;
            for (uint32_t i = 0; i < n; i++) {
                jvalue v = prim_value(pp, elem[0]);
                uint8_t *dst = (uint8_t *)arr->arr.data + (size_t)i * arr->arr.esz;
                switch (elem[0]) {
                case 'Z': case 'B': *(uint8_t *)dst = (uint8_t)(elem[0] == 'Z' ? v.z : (uint8_t)v.i); break;
                case 'S': case 'C': *(uint16_t *)dst = (uint16_t)v.i; break;
                case 'I': case 'F': memcpy(dst, &v.i, 4); break;
                default: memcpy(dst, &v.j, 8); break;
                }
            }
            return arr;
        }
        tl_jclass *ec = elem ? class_by_desc(elem) : NULL;
        if (!ec) ec = tl_jni_class("java/lang/Object");
        char an[420];
        if (elem && elem[0] == 'L') snprintf(an, sizeof(an), "[%s", elem); else if (elem) snprintf(an, sizeof(an), "[%s", elem); else snprintf(an, sizeof(an), "[Ljava/lang/Object;");
        jobj *arr = tl_jni_new_obj_array(ec, n);
        arr->cls = tl_jni_class(an);
        arr->refs = 1u << 30;
        for (uint32_t i = 0; i < n; i++) arr->oarr.v[i] = value_object(d, pp, elem);
        return arr;
    }
    default:
        *pp = start;
        skip_value(pp);
        return NULL;
    }
}

/* ---- annotation objects */

static dvm_method *element_method(dvm_class *ac, const char *name)
{
    for (int i = 0; ac && i < ac->nvm; i++) if (!strcmp(ac->vm[i].name, name) && !strncmp(ac->vm[i].sig, "()", 2)) return &ac->vm[i];
    return NULL;
}

/* An encoded_annotation (at *pp) as the object reflection hands out; NULL when its class is not there. */
static jobj *make_annotation(dvm_dex *d, const uint8_t **pp)
{
    const uint8_t *p = *pp;
    const char *desc = dvm_dex_type(d, uleb(&p));
    uint32_t n = uleb(&p);
    tl_jclass *ac = class_by_desc(desc);
    dvm_class *adc = dvm_class_of(ac);
    tl_jclass *mc = dvm_class_named("libcore/reflect/AnnotationMember");
    dvm_method *mctor = mc ? dvm_find_method(dvm_class_of(mc), "<init>", "(Ljava/lang/String;Ljava/lang/Object;Ljava/lang/Class;Ljava/lang/reflect/Method;)V", false) : NULL;
    if (!adc || !mctor) {
        for (uint32_t i = 0; i < n; i++) { uleb(&p); skip_value(&p); }
        *pp = p;
        return NULL;
    }
    jobj *members = tl_jni_new_obj_array(mc, n);
    members->cls = tl_jni_class("[Llibcore/reflect/AnnotationMember;");
    members->refs = 1u << 30;
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        const char *name = dvm_dex_str(d, uleb(&p));
        dvm_method *em = element_method(adc, name);
        const char *rt = em ? em->sig + 2 : NULL;
        jobj *v = value_object(d, &p, rt);
        if (!em) continue;
        if (tl_jni_pending()) { tl_jni_clear(); continue; }
        tl_jclass *rc = class_by_desc(rt);
        jobj *m = dvm_new_object(mc);
        jvalue args[4] = { L(dvm_new_string_utf8(name)), L(v), L(rc ? rc->mirror : NULL), L(dvm_make_executable(em)) }, r;
        if (!dvm_call(mctor, m, args, &r)) { tl_jni_clear(); continue; }
        members->oarr.v[k++] = m;
    }
    members->oarr.len = k;
    *pp = p;
    jvalue args[2] = { L(ac->mirror), L(members) }, r;
    if (!tl_dvm_call_static("libcore/reflect/AnnotationFactory", "createAnnotation",
                            "(Ljava/lang/Class;[Llibcore/reflect/AnnotationMember;)Ljava/lang/annotation/Annotation;", args, &r)) {
        char buf[300];
        tl_log_line("dvm: making a %s annotation threw %s", desc, tl_dvm_describe_pending(buf, sizeof(buf)) ? buf : "?");
        tl_jni_clear();
        return NULL;
    }
    if (r.l) ((jobj *)r.l)->refs = 1u << 30;
    return r.l;
}

static jobj *annotation_array(uint32_t n)
{
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/annotation/Annotation"), n);
    arr->cls = tl_jni_class("[Ljava/lang/annotation/Annotation;");
    arr->refs = 1u << 30;
    return arr;
}

/* every runtime-visible annotation of a set */
static jobj *all_runtime(dvm_dex *d, const uint8_t *set)
{
    uint32_t n = set ? rd32(set) : 0, k = 0;
    jobj *arr = annotation_array(n);
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *item = d->b + rd32(set + 4 + 4 * i);
        if (item[0] != VIS_RUNTIME) continue;
        const uint8_t *p = item + 1;
        jobj *o = make_annotation(d, &p);
        if (o) arr->oarr.v[k++] = o;
    }
    arr->oarr.len = k;
    return arr;
}
static jobj *one_runtime(dvm_dex *d, const uint8_t *set, jobj *type)
{
    if (!set || !type) return NULL;
    char desc[420];
    desc_of_class(type->klass.jc, desc, sizeof(desc));
    const uint8_t *item = find_item(d, set, desc, VIS_RUNTIME);
    if (!item) return NULL;
    const uint8_t *p = item + 1;
    return make_annotation(d, &p);
}
static bool has_runtime(dvm_dex *d, const uint8_t *set, jobj *type)
{
    if (!set || !type) return false;
    char desc[420];
    desc_of_class(type->klass.jc, desc, sizeof(desc));
    return find_item(d, set, desc, VIS_RUNTIME) != NULL;
}

/* A system annotation's element: positions *pp at its encoded value. */
static bool system_element(dvm_dex *d, const uint8_t *set, const char *desc, const char *elem, const uint8_t **pp)
{
    const uint8_t *item = find_item(d, set, desc, VIS_SYSTEM);
    if (!item) return false;
    const uint8_t *p = item + 1;
    uleb(&p);
    uint32_t n = uleb(&p);
    for (uint32_t i = 0; i < n; i++) {
        const char *name = dvm_dex_str(d, uleb(&p));
        if (!strcmp(name, elem)) { *pp = p; return true; }
        skip_value(&p);
    }
    return false;
}
static jobj *system_value(dvm_dex *d, const uint8_t *set, const char *desc, const char *elem, const char *want)
{
    const uint8_t *p;
    return system_element(d, set, desc, elem, &p) ? value_object(d, &p, want) : NULL;
}

static jobj *class_array(uint32_t n)
{
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/Class"), n);
    arr->cls = tl_jni_class("[Ljava/lang/Class;");
    arr->refs = 1u << 30;
    return arr;
}

/* ---- java.lang.Class */

static dvm_class *self_class(jobj *self) { return self && self->kind == TL_K_CLASS ? dvm_class_of(self->klass.jc) : NULL; }

NAT(An_Class_getDeclaredAnnotation) { dvm_class *c = self_class(self); *ret = L(c && c->dex ? one_runtime(c->dex, class_set(c), a[0].l) : NULL); return !tl_jni_pending(); }
NAT(An_Class_isDeclaredAnnotationPresent) { dvm_class *c = self_class(self); *ret = Z(c && c->dex && has_runtime(c->dex, class_set(c), a[0].l)); return true; }
NAT(An_Class_getDeclaredAnnotations) { (void)a; dvm_class *c = self_class(self); *ret = L(c && c->dex ? all_runtime(c->dex, class_set(c)) : annotation_array(0)); return !tl_jni_pending(); }
NAT(An_Class_getSignatureAnnotation)
{
    (void)a;
    dvm_class *c = self_class(self);
    *ret = L(c && c->dex ? system_value(c->dex, class_set(c), "Ldalvik/annotation/Signature;", "value", "[Ljava/lang/String;") : NULL);
    return !tl_jni_pending();
}
static bool inner_class(dvm_class *c, bool *anonymous, jobj **name, int32_t *flags)
{
    const uint8_t *set = c && c->dex ? class_set(c) : NULL;
    const uint8_t *item = set ? find_item(c->dex, set, "Ldalvik/annotation/InnerClass;", VIS_SYSTEM) : NULL;
    if (!item) return false;
    const uint8_t *p = item + 1;
    uleb(&p);
    uint32_t n = uleb(&p);
    *anonymous = true;
    for (uint32_t i = 0; i < n; i++) {
        const char *el = dvm_dex_str(c->dex, uleb(&p));
        if (!strcmp(el, "name")) { jobj *v = value_object(c->dex, &p, "Ljava/lang/String;"); if (name) *name = v; if (v) *anonymous = false; }
        else if (!strcmp(el, "accessFlags")) { jvalue v = prim_value(&p, 'I'); if (flags) *flags = v.i; }
        else skip_value(&p);
    }
    return true;
}
NAT(An_Class_isAnonymousClass) { (void)a; bool anon = false; *ret = Z(inner_class(self_class(self), &anon, NULL, NULL) && anon); return true; }
NAT(An_Class_getInnerClassFlags) { int32_t f = a[0].i; bool anon; inner_class(self_class(self), &anon, NULL, &f); *ret = I(f); return true; }
NAT(An_Class_getInnerClassName) { (void)a; jobj *n = NULL; bool anon; inner_class(self_class(self), &anon, &n, NULL); *ret = L(n); return true; }
NAT(An_Class_getEnclosingClass)
{
    (void)a;
    dvm_class *c = self_class(self);
    const uint8_t *set = c && c->dex ? class_set(c) : NULL;
    jobj *v = set ? system_value(c->dex, set, "Ldalvik/annotation/EnclosingClass;", "value", "Ljava/lang/Class;") : NULL;
    if (!v && set) {
        jobj *m = system_value(c->dex, set, "Ldalvik/annotation/EnclosingMethod;", "value", NULL);
        dvm_method *em = m ? dvm_method_of_reflect(m) : NULL;
        if (em) v = em->cls->jc->mirror;
    }
    *ret = L(v);
    return !tl_jni_pending();
}
NAT(An_Class_getDeclaringClass)
{
    (void)a;
    dvm_class *c = self_class(self);
    bool anon = false;
    if (!inner_class(c, &anon, NULL, NULL) || anon) { *ret = L(NULL); return true; }
    *ret = L(system_value(c->dex, class_set(c), "Ldalvik/annotation/EnclosingClass;", "value", "Ljava/lang/Class;"));
    return !tl_jni_pending();
}
NAT(An_Class_getEnclosingMethodNative)
{
    (void)a;
    dvm_class *c = self_class(self);
    jobj *m = c && c->dex ? system_value(c->dex, class_set(c), "Ldalvik/annotation/EnclosingMethod;", "value", NULL) : NULL;
    dvm_method *em = m ? dvm_method_of_reflect(m) : NULL;
    *ret = L(em && strcmp(em->name, "<init>") ? m : NULL);
    return !tl_jni_pending();
}
NAT(An_Class_getEnclosingConstructorNative)
{
    (void)a;
    dvm_class *c = self_class(self);
    jobj *m = c && c->dex ? system_value(c->dex, class_set(c), "Ldalvik/annotation/EnclosingMethod;", "value", NULL) : NULL;
    dvm_method *em = m ? dvm_method_of_reflect(m) : NULL;
    *ret = L(em && !strcmp(em->name, "<init>") ? m : NULL);
    return !tl_jni_pending();
}
NAT(An_Class_getDeclaredClasses)
{
    (void)a;
    dvm_class *c = self_class(self);
    jobj *v = c && c->dex ? system_value(c->dex, class_set(c), "Ldalvik/annotation/MemberClasses;", "value", "[Ljava/lang/Class;") : NULL;
    *ret = L(v ? v : class_array(0));
    return !tl_jni_pending();
}

/* ---- Method / Constructor (Executable) */

static dvm_method *self_method(jobj *self) { return dvm_method_of_reflect(self); }
static const uint8_t *method_set(dvm_method *m) { return m && m->cls && m->cls->dex ? member_set(m->cls, 1, m->idx) : NULL; }

NAT(An_Exe_getDeclaredAnnotations) { (void)a; dvm_method *m = self_method(self); *ret = L(method_set(m) ? all_runtime(m->cls->dex, method_set(m)) : annotation_array(0)); return !tl_jni_pending(); }
NAT(An_Exe_getAnnotation) { dvm_method *m = self_method(self); *ret = L(method_set(m) ? one_runtime(m->cls->dex, method_set(m), a[0].l) : NULL); return !tl_jni_pending(); }
NAT(An_Exe_isAnnotationPresent) { dvm_method *m = self_method(self); *ret = Z(method_set(m) && has_runtime(m->cls->dex, method_set(m), a[0].l)); return true; }
NAT(An_Exe_getSignatureAnnotation)
{
    (void)a;
    dvm_method *m = self_method(self);
    *ret = L(method_set(m) ? system_value(m->cls->dex, method_set(m), "Ldalvik/annotation/Signature;", "value", "[Ljava/lang/String;") : NULL);
    return !tl_jni_pending();
}
NAT(An_Exe_getExceptionTypes)
{
    (void)a;
    dvm_method *m = self_method(self);
    jobj *v = method_set(m) ? system_value(m->cls->dex, method_set(m), "Ldalvik/annotation/Throws;", "value", "[Ljava/lang/Class;") : NULL;
    if (!v && m && m->proxy_throws) v = m->proxy_throws;
    *ret = L(v ? v : class_array(0));
    return !tl_jni_pending();
}
NAT(An_Exe_getParameterAnnotations)
{
    (void)a;
    dvm_method *m = self_method(self);
    int np = m ? m->nparams : 0;
    jobj *outer = tl_jni_new_obj_array(tl_jni_class("[Ljava/lang/annotation/Annotation;"), (uint32_t)np);
    outer->cls = tl_jni_class("[[Ljava/lang/annotation/Annotation;");
    outer->refs = 1u << 30;
    const uint8_t *list = m && m->cls && m->cls->dex ? member_set(m->cls, 2, m->idx) : NULL;
    uint32_t n = list ? rd32(list) : 0;
    for (int i = 0; i < np; i++) {
        uint32_t off = (uint32_t)i < n ? rd32(list + 4 + 4 * i) : 0;
        outer->oarr.v[i] = off ? all_runtime(m->cls->dex, m->cls->dex->b + off) : annotation_array(0);
    }
    *ret = L(outer);
    return !tl_jni_pending();
}
/* An annotation element's default, from its class's AnnotationDefault (an annotation of the class itself, holding the defaults) */
NAT(An_Method_getDefaultValue)
{
    (void)a;
    dvm_method *m = self_method(self);
    *ret = L(NULL);
    if (!m || !m->cls || !m->cls->dex) return true;
    const uint8_t *set = class_set(m->cls), *p;
    if (!system_element(m->cls->dex, set, "Ldalvik/annotation/AnnotationDefault;", "value", &p)) return true;
    uint8_t h = *p++;
    if ((h & 0x1f) != V_ANNOTATION) return true;
    uleb(&p);
    uint32_t n = uleb(&p);
    for (uint32_t i = 0; i < n; i++) {
        const char *name = dvm_dex_str(m->cls->dex, uleb(&p));
        if (!strcmp(name, m->name)) { *ret = L(value_object(m->cls->dex, &p, m->sig + 2)); return !tl_jni_pending(); }
        skip_value(&p);
    }
    return true;
}

/* ---- Field */

static dvm_field *self_field(jobj *self) { return dvm_field_of_reflect(self); }
static const uint8_t *field_set(dvm_field *f) { return f && f->cls && f->cls->dex ? member_set(f->cls, 0, f->idx) : NULL; }

NAT(An_Field_getDeclaredAnnotations) { (void)a; dvm_field *f = self_field(self); *ret = L(field_set(f) ? all_runtime(f->cls->dex, field_set(f)) : annotation_array(0)); return !tl_jni_pending(); }
NAT(An_Field_getAnnotation) { dvm_field *f = self_field(self); *ret = L(field_set(f) ? one_runtime(f->cls->dex, field_set(f), a[0].l) : NULL); return !tl_jni_pending(); }
NAT(An_Field_isAnnotationPresent) { dvm_field *f = self_field(self); *ret = Z(field_set(f) && has_runtime(f->cls->dex, field_set(f), a[0].l)); return true; }
NAT(An_Field_getSignatureAnnotation)
{
    (void)a;
    dvm_field *f = self_field(self);
    *ret = L(field_set(f) ? system_value(f->cls->dex, field_set(f), "Ldalvik/annotation/Signature;", "value", "[Ljava/lang/String;") : NULL);
    return !tl_jni_pending();
}

/* @CriticalNative (and @FastNative): build-time annotations ART reads to choose how it calls a native method */
bool dvm_method_annotated_build(dvm_method *m, const char *desc);
bool dvm_method_annotated_build(dvm_method *m, const char *desc)
{
    const uint8_t *set = method_set(m);
    return set && find_item(m->cls->dex, set, desc, 0) != NULL;
}

