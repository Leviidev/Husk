/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for Android's own Java runtime (ART) on the translation layer: loads libart.so and its
 * libraries with the guest linker, starts a Java VM with JNI_CreateJavaVM and runs Java code in it.
 *
 *   art-test <libs.apk> <art-root> [class method]
 *
 * libs.apk is a zip with lib/arm64-v8a/ holding the ART module's libraries (and ICU's); art-root is the
 * unpacked /apex directory, which holds com.android.art/javalib (the boot class path) and com.android.i18n.
 * With no class it prints a line through java.lang.System.out, which runs Java code from core-oj.
 *
 * Environment:
 *   TL_VERBOSE=2       log every unresolved import
 *   ART_OPTS="-X.. -X.."  extra runtime options
 */
#include <dlfcn.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "husk-tl-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-xmem.h"

void tl_log_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
}

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul);
    else {
        Dl_info di;      /* a host address: name the macOS or Translation Layer function */
        if (dladdr(addr, &di) && di.dli_sname) fprintf(stderr, "  %-6s %p  [host] %s+%#lx\n", label, addr, di.dli_sname, (unsigned long)((const char *)addr - (const char *)di.dli_saddr));
        else fprintf(stderr, "  %-6s %p\n", label, addr);
    }
}

static void on_crash(int sig, siginfo_t *info, void *uctx)
{
    ucontext_t *uc = uctx;
    _STRUCT_ARM_THREAD_STATE64 *ss = &uc->uc_mcontext->__ss;
    fprintf(stderr, "\n=== CRASH: signal %d, fault address %p ===\n", sig, info->si_addr);
    describe("pc", (void *)ss->__pc);
    describe("lr", (void *)ss->__lr);
    uint64_t *fp = (uint64_t *)ss->__fp;
    for (int i = 0; i < 16 && fp && ((uintptr_t)fp & 7) == 0 && (uintptr_t)fp > 0x100000000ull; i++) {
        describe("frame", (void *)fp[1]);
        fp = (uint64_t *)fp[0];
    }
    fprintf(stderr, "  x0=%#llx x1=%#llx x2=%#llx x3=%#llx x8=%#llx x19=%#llx x20=%#llx\n",
            ss->__x[0], ss->__x[1], ss->__x[2], ss->__x[3], ss->__x[8], ss->__x[19], ss->__x[20]);
    fflush(stderr);
    _exit(139);
}

static void install_crash_handler(void)
{
    static uint8_t altstack[65536];
    stack_t st = { .ss_sp = altstack, .ss_size = sizeof(altstack), .ss_flags = 0 };
    sigaltstack(&st, NULL);
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    int sigs[] = { SIGSEGV, SIGBUS, SIGILL, SIGTRAP, SIGABRT };
    for (size_t i = 0; i < sizeof(sigs) / sizeof(sigs[0]); i++) sigaction(sigs[i], &sa, NULL);
}

/* ---- the slice of the JNI invocation API used here, by table index (jni.h order) */
typedef struct { char *optionString; void *extraInfo; } JavaVMOption;
typedef struct { int32_t version; int32_t nOptions; JavaVMOption *options; uint8_t ignoreUnrecognized; } JavaVMInitArgs;
typedef void *const *JNIEnvT;      /* JNIEnv: a pointer to the function table */
typedef union { int64_t j; void *l; } jval;
#define JNI_VERSION_1_6 0x00010006
#define FN(env, i, T) ((T)((*(void *const *const *)(env))[i]))
enum { J_FindClass = 6, J_ExceptionDescribe = 16, J_ExceptionClear = 17, J_GetMethodID = 33, J_CallVoidMethodA = 63,
       J_GetStaticMethodID = 113, J_CallStaticVoidMethodA = 143, J_GetStaticFieldID = 144, J_GetStaticObjectField = 145,
       J_NewStringUTF = 167, J_NewObjectArray = 172, J_ExceptionCheck = 228 };

static bool check(void *env, const char *what)
{
    if (!FN(env, J_ExceptionCheck, uint8_t (*)(void *))(env)) return true;
    fprintf(stderr, "art-test: Java exception during %s\n", what);
    FN(env, J_ExceptionDescribe, void (*)(void *))(env);
    FN(env, J_ExceptionClear, void (*)(void *))(env);
    return false;
}

int main(int argc, char **argv)
{
    install_crash_handler();
    if (argc < 3) { fprintf(stderr, "usage: %s <libs.apk> <apex-root> [class method]\n", argv[0]); return 2; }
    const char *v = getenv("TL_VERBOSE");
    tl_ld_set_verbosity(v ? atoi(v) : 1);
    if (!tl_ld_add_apk(argv[1])) return 1;
    const char *root = argv[2];

    /* ART reads where its module, ICU and the time zone data live from the environment, as init sets it on a phone */
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s/com.android.art", root); setenv("ANDROID_ART_ROOT", buf, 1);
    snprintf(buf, sizeof(buf), "%s/com.android.i18n", root); setenv("ANDROID_I18N_ROOT", buf, 1);
    snprintf(buf, sizeof(buf), "%s/com.android.tzdata", root); setenv("ANDROID_TZDATA_ROOT", buf, 1);
    snprintf(buf, sizeof(buf), "%s/system", root); setenv("ANDROID_ROOT", buf, 1);
    snprintf(buf, sizeof(buf), "%s/data", root); setenv("ANDROID_DATA", buf, 1);

    /* On a phone libc carries the C++ unwinder; here it is a library of its own, loaded first so libc++ finds it. */
    if (!tl_ld_load("libunwind.so")) fprintf(stderr, "art-test: no libunwind.so: C++ exceptions will not work\n");
    tl_lib *art = tl_ld_load("libart.so");
    if (!art) { fprintf(stderr, "art-test: libart.so did not load\n"); return 1; }
    fprintf(stderr, "art-test: libart.so loaded, %zu unresolved imports in all\n", tl_ld_unresolved_count());
    if (!tl_ld_init(art)) { fprintf(stderr, "art-test: libart.so constructors failed\n"); return 1; }
    int32_t (*create)(void **, void **, JavaVMInitArgs *) = tl_ld_sym(art, "JNI_CreateJavaVM");
    if (!create) { fprintf(stderr, "art-test: no JNI_CreateJavaVM\n"); return 1; }

    /* The boot class path: the core library jars ART ships with, and ICU's. No boot image: ART loads the dex files themselves. */
    static const char *const jars[] = { "com.android.art/javalib/core-oj.jar", "com.android.art/javalib/core-libart.jar",
        "com.android.art/javalib/okhttp.jar", "com.android.art/javalib/bouncycastle.jar", "com.android.art/javalib/apache-xml.jar",
        "com.android.i18n/javalib/core-icu4j.jar", NULL };
    char bcp[4096] = "-Xbootclasspath:", loc[4096] = "-Xbootclasspath-locations:";
    for (int i = 0; jars[i]; i++) {
        snprintf(bcp + strlen(bcp), sizeof(bcp) - strlen(bcp), "%s%s/%s", i ? ":" : "", root, jars[i]);
        snprintf(loc + strlen(loc), sizeof(loc) - strlen(loc), "%s/apex/%s", i ? ":" : "", jars[i]);
    }
    JavaVMOption opts[32]; int n = 0;
    opts[n++].optionString = bcp;
    opts[n++].optionString = loc;
    opts[n++].optionString = "-Xint";                 /* the interpreter: no JIT yet */
    opts[n++].optionString = "-Xusejit:false";
    opts[n++].optionString = "-Xnoimage-dex2oat";
    opts[n++].optionString = "-Xverify:none";
    opts[n++].optionString = "-Xcheck:jni";
    char *extra = getenv("ART_OPTS") ? strdup(getenv("ART_OPTS")) : NULL;
    for (char *s = extra, *tok; s && (tok = strsep(&s, " ")) && n < 31;) if (*tok) opts[n++].optionString = tok;
    for (int i = 0; i < n; i++) opts[i].extraInfo = NULL;
    JavaVMInitArgs args = { JNI_VERSION_1_6, n, opts, 1 };

    void *vm = NULL, *env = NULL;
    fprintf(stderr, "art-test: JNI_CreateJavaVM with %d options\n", n);
    int32_t r = create(&vm, &env, &args);
    fprintf(stderr, "art-test: JNI_CreateJavaVM -> %d (vm %p, env %p)\n", r, vm, env);
    if (r != 0 || !env) return 1;

    if (argc >= 5) {
        void *cls = FN(env, J_FindClass, void *(*)(void *, const char *))(env, argv[3]);
        if (!check(env, "FindClass") || !cls) return 1;
        void *m = FN(env, J_GetStaticMethodID, void *(*)(void *, void *, const char *, const char *))(env, cls, argv[4], "([Ljava/lang/String;)V");
        if (!check(env, "GetStaticMethodID") || !m) return 1;
        void *strcls = FN(env, J_FindClass, void *(*)(void *, const char *))(env, "java/lang/String");
        jval a[1]; a[0].l = FN(env, J_NewObjectArray, void *(*)(void *, int32_t, void *, void *))(env, 0, strcls, NULL);
        FN(env, J_CallStaticVoidMethodA, void (*)(void *, void *, void *, jval *))(env, cls, m, a);
        check(env, argv[4]);
    } else {
        /* System.out.println: PrintStream, the charset encoder and the stream underneath are all Java, run by ART's interpreter */
        void *sys = FN(env, J_FindClass, void *(*)(void *, const char *))(env, "java/lang/System");
        if (!check(env, "FindClass(System)") || !sys) return 1;
        void *f = FN(env, J_GetStaticFieldID, void *(*)(void *, void *, const char *, const char *))(env, sys, "out", "Ljava/io/PrintStream;");
        void *out = FN(env, J_GetStaticObjectField, void *(*)(void *, void *, void *))(env, sys, f);
        if (!check(env, "System.out") || !out) return 1;
        void *ps = FN(env, J_FindClass, void *(*)(void *, const char *))(env, "java/io/PrintStream");
        void *println = FN(env, J_GetMethodID, void *(*)(void *, void *, const char *, const char *))(env, ps, "println", "(Ljava/lang/String;)V");
        jval a[1]; a[0].l = FN(env, J_NewStringUTF, void *(*)(void *, const char *))(env, "Hello from ART, running on the Husk translation layer");
        FN(env, J_CallVoidMethodA, void (*)(void *, void *, void *, jval *))(env, out, println, a);
        check(env, "println");
    }
    fprintf(stderr, "art-test: done\n");
    return 0;
}
