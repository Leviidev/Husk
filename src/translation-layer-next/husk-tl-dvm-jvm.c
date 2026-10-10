/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * libopenjdkjvm.so's JVM_* functions, which libopenjdk.so calls for what only the VM knows (memory, the clock, loading a library).
 * On Android they are ART's; here they are the Dalvik runtime's. The library itself is never loaded: its name is one Husk answers
 * (tl_bionic_is_system_lib), so libopenjdk binds to these.
 */
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-jni.h"

extern bool tl_dvm_load_library(const char *name_or_path);
extern void tl_va_snprintf(void);

static int64_t J_CurrentTimeMillis(void *env, void *cls) { (void)env; (void)cls; struct timeval tv; gettimeofday(&tv, NULL); return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000; }
static int64_t J_GetNanoTimeAdjustment(void *env, void *cls, int64_t offset_secs)
{
    (void)env; (void)cls;
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
    int64_t secs = ts.tv_sec - offset_secs;
    if (secs > 0xFFFFFFFFll || secs < -0xFFFFFFFFll) return -1;
    return secs * 1000000000ll + ts.tv_nsec;
}
static void J_Exit(int code) { tl_log_line("dvm: JVM_Exit(%d)", code); _exit(code); }
static void *J_FindLibraryEntry(void *handle, const char *name) { (void)handle; (void)name; return NULL; }
static int64_t J_FreeMemory(void) { return 256ll << 20; }
static int64_t J_MaxMemory(void) { return 512ll << 20; }
static int64_t J_TotalMemory(void) { return 512ll << 20; }
static void J_GC(void) {}
static int J_GetLastErrorString(char *buf, int len) { if (len > 0) snprintf(buf, (size_t)len, "%s", strerror(errno)); return (int)strlen(buf); }
static int J_InitializeSocketLibrary(void) { return 0; }
static uint8_t J_IsNaN(double d) { return isnan(d); }
static void *J_NativeLoad(void *env, jobj *filename, void *loader)
{
    (void)env; (void)loader;
    const char *f = tl_jni_string(filename);
    if (f && tl_dvm_load_library(f)) return NULL;
    char msg[600]; snprintf(msg, sizeof(msg), "dlopen failed: library \"%s\" not found", f ? f : "?");
    jobj *s = tl_jni_new_string(msg);
    s->refs = 1u << 30;
    return s;
}
static void J_Sync(void) { sync(); }
static const char *J_LD_PATH(void) { return "/system/lib64"; }

const tl_bionic_entry tl_tab_jvm[] = {
    TL_WRAP("JVM_CurrentTimeMillis", J_CurrentTimeMillis), TL_WRAP("JVM_GetNanoTimeAdjustment", J_GetNanoTimeAdjustment),
    TL_WRAP("JVM_Exit", J_Exit), TL_WRAP("JVM_FindLibraryEntry", J_FindLibraryEntry), TL_WRAP("JVM_FreeMemory", J_FreeMemory),
    TL_WRAP("JVM_MaxMemory", J_MaxMemory), TL_WRAP("JVM_TotalMemory", J_TotalMemory), TL_WRAP("JVM_GC", J_GC),
    TL_WRAP("JVM_GetLastErrorString", J_GetLastErrorString), TL_WRAP("JVM_InitializeSocketLibrary", J_InitializeSocketLibrary),
    TL_WRAP("JVM_IsNaN", J_IsNaN), TL_WRAP("JVM_NativeLoad", J_NativeLoad), TL_WRAP("JVM_Sync", J_Sync),
    TL_WRAP("jio_snprintf", tl_va_snprintf), TL_WRAP("android_get_LD_LIBRARY_PATH", J_LD_PATH),
    TL_END
};
