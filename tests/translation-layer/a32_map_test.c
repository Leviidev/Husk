/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host check: map a real armeabi-v7a .so and find JNI_OnLoad. */
#include "husk-tl-arm32.h"
#include "husk-tl-jni-internal.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tl_log_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}
void *tl_bionic_find(const char *name) { (void)name; return NULL; }
void *tl_jni_env(void) { return (void *)(uintptr_t)0x10; }
jobj *tl_jni_class_object(const char *n) { (void)n; return NULL; }
void tl_jni_clear(void) {}
bool tl_jni_pending(void) { return false; }
jobj *tl_jni_pending_object(void) { return NULL; }
const char *tl_jni_string(const jobj *s) { (void)s; return ""; }
jobj *tl_jni_new_string(const char *s) { (void)s; return NULL; }
tl_jmeth *tl_jni_method(tl_jclass *c, const char *n, const char *s, bool st)
{ (void)c; (void)n; (void)s; (void)st; return NULL; }
tl_jfield *tl_jni_field(tl_jclass *c, const char *n, const char *s, bool st)
{ (void)c; (void)n; (void)s; (void)st; return NULL; }
int32_t tl_jni_register_one(jobj *cls, const char *name, const char *sig, void *fn)
{ (void)cls; (void)name; (void)sig; (void)fn; return 0; }
jvalue tl_jni_invoke(jobj *self, tl_jmeth *m, bool nonvirtual, const jvalue *args)
{ (void)self; (void)m; (void)nonvirtual; (void)args; jvalue z; memset(&z, 0, sizeof(z)); return z; }
void tl_jni_throw(const char *cls, const char *msg) { (void)cls; (void)msg; }
jobj *tl_jni_new_object(tl_jclass *cls) { (void)cls; return NULL; }
jvalue *tl_jni_field_slot(jobj *o, tl_jfield *f) { (void)o; (void)f; return NULL; }
void *tl_egl_resolve(const char *name) { (void)name; return NULL; }
void *tl_nwindow_get(void) { return NULL; }

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s lib.so [...]\n", argv[0]); return 2; }
    int fails = 0;
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f) { perror(argv[i]); return 1; }
        fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
        uint8_t *buf = malloc((size_t)n);
        if (!buf || fread(buf, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "read %s\n", argv[i]); return 1; }
        fclose(f);
        if (!tl_a32_is_elf(buf, (size_t)n)) {
            printf("FAIL %s: not ELF32 ARM\n", argv[i]); fails++; free(buf); continue;
        }
        /* opaque key; mapper keeps its own table */
        tl_lib *L = (tl_lib *)(uintptr_t)(0x1000 + i);
        if (!tl_a32_map(L, argv[i], buf, (size_t)n)) {
            printf("FAIL %s: map\n", argv[i]); fails++; free(buf); continue;
        }
        void *on = tl_a32_sym(L, "JNI_OnLoad");
        printf("ok   %s  JNI_OnLoad=%s va=%#x\n", argv[i], on ? "yes" : "no", on ? tl_a32_va(on) : 0);
        if (!on) fails++;
        free(buf);
    }
    return fails ? 1 : 0;
}
