/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Host harness for the Dalvik runtime (husk-tl-dvm.c): runs a static method from a DEX/APK on the real libcore.
 *
 *   java-test <dex-or-apk> <class> <method> [signature]       (default signature ()Ljava/lang/String; then ()I then ()V)
 *
 * Environment: TL_ART (the ART APEX root, default /Volumes/GTAV/husk2/root/apex/com.android.art), TL_DVM_TRACE=1|2,
 * TL_JNI_TRACE, TL_VERBOSE.
 */
#include <mach/mach.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ucontext.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-dvm.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"

void tl_log_line(const char *fmt, ...)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&m);
    va_list ap; va_start(ap, fmt);
    vfprintf(stderr, fmt, ap); fputc('\n', stderr);
    va_end(ap);
    pthread_mutex_unlock(&m);
}

static void describe(const char *label, const void *addr)
{
    const char *lib = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &lib, &sa);
    if (lib) fprintf(stderr, "  %-6s %p  %s  %s+%#lx\n", label, addr, lib, sym ? sym : "?", sa ? (unsigned long)((const char *)addr - (const char *)sa) : 0ul);
    else fprintf(stderr, "  %-6s %p\n", label, addr);
}
static int safe_read(uintptr_t addr, void *out, size_t n)
{
    vm_size_t got = 0;
    return vm_read_overwrite(mach_task_self(), (vm_address_t)addr, n, (vm_address_t)out, &got) == KERN_SUCCESS && got == n;
}
static void on_crash(int sig, siginfo_t *info, void *uctx)
{
    ucontext_t *uc = uctx;
    _STRUCT_ARM_THREAD_STATE64 *ss = &uc->uc_mcontext->__ss;
    fprintf(stderr, "\n=== CRASH: signal %d, fault address %p ===\n", sig, info->si_addr);
    describe("pc", (void *)ss->__pc);
    describe("lr", (void *)ss->__lr);
    uintptr_t fp = ss->__fp;
    for (int i = 0; i < 24 && fp && (fp & 7) == 0; i++) {
        uint64_t fr[2];
        if (!safe_read(fp, fr, sizeof(fr))) break;
        describe("frame", (void *)fr[1]);
        fp = fr[0];
    }
    fflush(stderr);
    _exit(139);
}

typedef struct { int argc; char **argv; int rc; } job;

static void *run(void *p)
{
    job *j = p;
    char **argv = j->argv;
    const char *art = getenv("TL_ART") ? getenv("TL_ART") : "/Volumes/GTAV/husk2/root/apex/com.android.art";
    /* the boot class path, in BOOTCLASSPATH's order */
    static const char *const jars[] = { "core-oj", "core-libart", "okhttp", "bouncycastle", "apache-xml" };
    char paths[8][600], lib[600];
    const char *boot[8]; int nb = 0;
    for (int i = 0; i < 5; i++) { snprintf(paths[nb], sizeof(paths[nb]), "%s/javalib/%s.jar", art, jars[i]); boot[nb] = paths[nb]; nb++; }
    snprintf(paths[nb], sizeof(paths[nb]), "%s/../com.android.i18n/javalib/core-icu4j.jar", art); boot[nb] = paths[nb]; nb++;
    snprintf(paths[nb], sizeof(paths[nb]), "%s/../com.android.conscrypt/javalib/conscrypt.jar", art); boot[nb] = paths[nb]; nb++;
    if (getenv("TL_FRAMEWORK")) { snprintf(paths[nb], sizeof(paths[nb]), "%s", getenv("TL_FRAMEWORK")); boot[nb] = paths[nb]; nb++; }
    snprintf(lib, sizeof(lib), "%s/lib64", art);
    /* what ICU and libcore read to find their data: the APEX roots */
    { char v[700];
      snprintf(v, sizeof(v), "%s", art); setenv("ANDROID_ART_ROOT", v, 0);
      snprintf(v, sizeof(v), "%s/../com.android.i18n", art); setenv("ANDROID_I18N_ROOT", v, 0);
      snprintf(v, sizeof(v), "%s/../com.android.tzdata", art); setenv("ANDROID_TZDATA_ROOT", v, 0);
      snprintf(v, sizeof(v), "%s/../../system", art); setenv("ANDROID_ROOT", v, 0); }
    tl_jni_init();
    if (!tl_dvm_start(boot, nb)) { j->rc = 1; return NULL; }
    { char i18n[700]; snprintf(i18n, sizeof(i18n), "%s/../com.android.i18n/lib64", art); tl_ld_add_search_dir(i18n); }
    { char d[700]; snprintf(d, sizeof(d), "%s/../com.android.conscrypt/lib64", art); tl_ld_add_search_dir(d); }
    { void tl_set_cacerts_dir(const char *); static char d[800]; snprintf(d, sizeof(d), "%s/../com.android.conscrypt/cacerts", art); tl_set_cacerts_dir(d); }   /* the trusted roots, as the app ships them */
    if (!getenv("TL_NO_NATIVES") && !tl_dvm_load_natives(lib)) fprintf(stderr, "java-test: libcore's natives did not all load\n");
    if (!tl_dvm_add_apk(argv[1])) { j->rc = 1; return NULL; }
    const char *sigs[] = { argv[4], "()Ljava/lang/String;", "()I", "()V", "([Ljava/lang/String;)V" };
    for (int i = argv[4] ? 0 : 1; i < 5; i++) {
        jvalue ret, arg; arg.l = NULL;
        bool ok = tl_dvm_call_static(argv[2], argv[3], sigs[i], &arg, &ret);
        char buf[600];
        const char *exc = tl_dvm_describe_pending(buf, sizeof(buf));
        if (!ok && exc && strstr(exc, "NoSuchMethodError")) { tl_jni_clear(); continue; }
        if (!ok) { printf("THREW %s\n", exc ? exc : "?"); j->rc = 2; return NULL; }
        const char *r = sigs[i] + strlen(sigs[i]) - 1;
        if (*r == ';') printf("RESULT \"%s\"\n", tl_jni_string(ret.l) ? tl_jni_string(ret.l) : "(null)");
        else if (*r == 'I') printf("RESULT %d\n", ret.i);
        else printf("RESULT (void)\n");
        fflush(stdout);
        j->rc = 0;
        return NULL;
    }
    printf("no such method\n");
    j->rc = 3;
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc < 4) { fprintf(stderr, "usage: %s <dex-or-apk> <class> <method> [signature]\n", argv[0]); return 2; }
    static uint8_t altstack[1 << 16];
    stack_t st = { .ss_sp = altstack, .ss_size = sizeof(altstack) };
    sigaltstack(&st, NULL);
    struct sigaction sa; memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_crash; sa.sa_flags = SA_SIGINFO | SA_ONSTACK; sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL); sigaction(SIGBUS, &sa, NULL); sigaction(SIGILL, &sa, NULL); sigaction(SIGABRT, &sa, NULL);
    tl_ld_set_verbosity(getenv("TL_VERBOSE") ? atoi(getenv("TL_VERBOSE")) : 0);
    tl_jni_set_trace(getenv("TL_JNI_TRACE") ? atoi(getenv("TL_JNI_TRACE")) : 0);
    tl_dvm_set_trace(getenv("TL_DVM_TRACE") ? atoi(getenv("TL_DVM_TRACE")) : 0);
    /* Java recursion is C recursion here: a big stack */
    job j = { argc, argv, 1 };
    pthread_attr_t a; pthread_attr_init(&a); pthread_attr_setstacksize(&a, 256u << 20);
    pthread_t t; pthread_create(&t, &a, run, &j);
    pthread_join(t, NULL);
    return j.rc;
}
