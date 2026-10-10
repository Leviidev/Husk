/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk's Dalvik runtime: an interpreter for DEX bytecode, so apps (and the Java side of engines) whose code is Java can run.
 *
 * Android's own runtime, ART, cannot run on Apple arm64: it keeps every object in the low 4 GB (32-bit references), and xnu never
 * lets a process map there. This runtime keeps ordinary 64-bit pointers instead. Its objects are the JNI world's jobj
 * (husk-tl-jni.c), so native code -- an engine's .so, libcore's own natives -- sees the same objects through JNI that the
 * interpreter works on, and the Android framework classes Husk implements in C (Activity, Context, Looper...) are reached the
 * same way the engines reach them.
 *
 * The boot class path is the real one: libcore (core-oj.jar, core-libart.jar) from the ART APEX, its bytecode interpreted, its
 * native methods called in libjavacore.so / libopenjdk.so through Husk's linker. What ART implements inside itself rather than in
 * those libraries -- Object, Class, String's natives, Thread, Unsafe, VMRuntime, reflection -- is in husk-tl-dvm-natives.c.
 */
#ifndef HUSK_TL_DVM_H
#define HUSK_TL_DVM_H

#include <stdbool.h>
#include <stdint.h>

#include "husk-tl-jni.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Start the runtime with these boot class path entries (.jar/.apk/.dex files, searched in order). Call after tl_jni_init. */
bool tl_dvm_start(const char *const *boot, int nboot);
/* Add an app's DEX files (every classesN.dex in the APK), after the boot class path. */
bool tl_dvm_add_apk(const char *apk);
bool tl_dvm_running(void);
/* libcore's own native libraries (libjavacore.so, libopenjdk.so and what they need), from this folder (the ART APEX's lib64). */
bool tl_dvm_load_natives(const char *libdir);

/* Call a static method by name: what a driver does to start the app's code. Returns false when it threw (the exception is pending). */
bool tl_dvm_call_static(const char *cls, const char *name, const char *sig, const jvalue *args, jvalue *ret);

/* Make a string from UTF-16 / read a string as UTF-16 (made and kept on first use). */
jobj *tl_dvm_string_u16(const uint16_t *s, int32_t n);
const uint16_t *tl_dvm_string_chars(jobj *s, int32_t *n);

/* The exception pending on this thread as text ("java.lang.Foo: message"), for logs; NULL when there is none. */
const char *tl_dvm_describe_pending(char *buf, size_t n);

void tl_dvm_set_trace(int level);       /* 1: classes and natives; 2: every invoke */

#ifdef __cplusplus
}
#endif

#endif
