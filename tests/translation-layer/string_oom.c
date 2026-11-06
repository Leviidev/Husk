/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Actual libcore StringFactory + Husk JNI/interpreter, with allocator failures. */
#include "husk-tl-dvm-internal.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void tl_log_line(const char *fmt, ...) { (void)fmt; }
bool dvm_string_init(const char *sig, const jvalue *p, jvalue *ret);
static void (*arm)(int, size_t, int);
static int (*disarm)(void);

static void fault(dvm_method *method, jvalue *args, int kind, size_t size, int skip) {
    jvalue ret = {0};
    arm(kind, size, skip);
    bool ok = dvm_call(method, NULL, args, &ret);
    int allocation_safe = disarm();
    assert(!ok && allocation_safe && ret.l == NULL);
    jobj *error = tl_jni_pending_object();
    assert(error && !strcmp(error->cls->name, "java/lang/OutOfMemoryError"));
    tl_jni_clear();
    // The runtime and input array remain usable after every failed stage.
    assert(dvm_call(method, NULL, args, &ret));
    assert(ret.l && !tl_jni_pending());
    assert(strlen(tl_jni_string(ret.l)) == 39);
}

int main(int argc, char **argv) {
    assert(argc == 3);
    arm = dlsym(RTLD_DEFAULT, "string_oom_arm");
    disarm = dlsym(RTLD_DEFAULT, "string_oom_disarm");
    assert(arm && disarm);
    const char *jars[] = { "com.android.art/javalib/core-oj.jar", "com.android.art/javalib/core-libart.jar" };
    char paths[2][1024]; const char *boot[2];
    for (int i = 0; i < 2; i++) { snprintf(paths[i], sizeof(paths[i]), "%s/%s", argv[1], jars[i]); boot[i] = paths[i]; }
    tl_jni_init();
    assert(tl_dvm_start(boot, 2));
    tl_jclass *factory = dvm_class_named("java/lang/StringFactory");
    assert(factory);
    dvm_class *fc = dvm_class_of(factory);
    assert(dvm_ensure_init(fc));
    dvm_method *method = dvm_find_method(fc, "newStringFromBytes", "([BIII)Ljava/lang/String;", true);
    assert(method);
    jobj *bytes = tl_jni_new_prim_array('B', 39);
    memset(bytes->arr.data, 's', 39);
    jvalue args[4] = { { .l = bytes }, { .i = 0 }, { .i = 0 }, { .i = 39 } };
    jvalue ret = {0};
    // Resolve everything before arming the allocator.
    assert(dvm_call(method, NULL, args, &ret));
    fault(method, args, 1, 118, 0);  // exact size and failure from the phone's crash
    fault(method, args, 1, 78, 0);   // temporary UTF-16 buffer
    fault(method, args, 1, 78, 1);   // persistent UTF-16 buffer
    fault(method, args, 2, sizeof(jobj), 0);
    fault(method, args, 3, 1, 0);    // JNI's initial empty string
    uint32_t slots = dvm_class_of(dvm_class_named("java/lang/String"))->nslots;
    fault(method, args, 2, (size_t)(slots ? slots : 1) * sizeof(jvalue), 0);

    const uint16_t unicode[] = { 'A', 0, 0x4e2d, 0xd83d, 0xde00 };
    jobj *text = tl_dvm_string_u16(unicode, 5);
    assert(text && !strcmp(tl_jni_string(text), "A\xc0\x80\xe4\xb8\xad\xf0\x9f\x98\x80"));
    int32_t count; const uint16_t *chars = tl_dvm_string_chars(text, &count);
    assert(count == 5 && !memcmp(chars, unicode, sizeof(unicode)));
    args[2].i = 1; args[3].i = 39;
    assert(!dvm_call(method, NULL, args, &ret));
    assert(!strcmp(tl_jni_pending_object()->cls->name, "java/lang/StringIndexOutOfBoundsException"));
    tl_jni_clear();
    args[2].i = 0; args[3].i = 0;
    assert(dvm_call(method, NULL, args, &ret));
    assert(!strcmp(tl_jni_string(ret.l), ""));
    assert(dvm_string_init("()V", NULL, &ret) && ret.l);
    assert(tl_dvm_add_apk(argv[2]));
    const char *signature = "([B)I";
    jvalue input = { .l = bytes };
    assert(tl_dvm_call_static("StringAllocation", "catchFailure", signature, &input, &ret) && ret.i == 39);
    arm(1, 118, 0);
    bool caught = tl_dvm_call_static("StringAllocation", "catchFailure", signature, &input, &ret);
    int safe = disarm();
    assert(caught && ret.i == -118 && safe && !tl_jni_pending());
    assert(tl_dvm_call_static("StringAllocation", "catchFailure", signature, &input, &ret) && ret.i == 39);
    puts("PASS: six StringFactory allocation failures, no allocations while reporting OOM, Java catch/recovery, Unicode, bounds and empty constructors");
}
