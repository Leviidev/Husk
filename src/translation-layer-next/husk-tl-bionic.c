/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The core of the bionic shim: how names are looked up, how errno and signal
 * numbers cross over, system properties, the C runtime hooks, and the dynamic
 * linking calls.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-bionic.h"

#include <crt_externs.h>
#include <dlfcn.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "husk-tl-ld.h"
#include "husk-tl-va.h"

/* ------------------------------------------------------------------ errno */

static const struct { int darwin, guest; } k_errno[] = {
    { EPERM, 1 }, { ENOENT, 2 }, { ESRCH, 3 }, { EINTR, 4 }, { EIO, 5 }, { ENXIO, 6 }, { E2BIG, 7 },
    { ENOEXEC, 8 }, { EBADF, 9 }, { ECHILD, 10 }, { EAGAIN, 11 }, { ENOMEM, 12 }, { EACCES, 13 },
    { EFAULT, 14 }, { ENOTBLK, 15 }, { EBUSY, 16 }, { EEXIST, 17 }, { EXDEV, 18 }, { ENODEV, 19 },
    { ENOTDIR, 20 }, { EISDIR, 21 }, { EINVAL, 22 }, { ENFILE, 23 }, { EMFILE, 24 }, { ENOTTY, 25 },
    { ETXTBSY, 26 }, { EFBIG, 27 }, { ENOSPC, 28 }, { ESPIPE, 29 }, { EROFS, 30 }, { EMLINK, 31 },
    { EPIPE, 32 }, { EDOM, 33 }, { ERANGE, 34 }, { EDEADLK, 35 }, { ENAMETOOLONG, 36 }, { ENOLCK, 37 },
    { ENOSYS, 38 }, { ENOTEMPTY, 39 }, { ELOOP, 40 }, { ENOMSG, 42 }, { EIDRM, 43 }, { EOVERFLOW, 75 },
    { EILSEQ, 84 }, { ENOTSOCK, 88 }, { EDESTADDRREQ, 89 }, { EMSGSIZE, 90 }, { EPROTOTYPE, 91 },
    { ENOPROTOOPT, 92 }, { EPROTONOSUPPORT, 93 }, { ESOCKTNOSUPPORT, 94 }, { ENOTSUP, 95 },
    { EPFNOSUPPORT, 96 }, { EAFNOSUPPORT, 97 }, { EADDRINUSE, 98 }, { EADDRNOTAVAIL, 99 },
    { ENETDOWN, 100 }, { ENETUNREACH, 101 }, { ENETRESET, 102 }, { ECONNABORTED, 103 },
    { ECONNRESET, 104 }, { ENOBUFS, 105 }, { EISCONN, 106 }, { ENOTCONN, 107 }, { ESHUTDOWN, 108 },
    { ETOOMANYREFS, 109 }, { ETIMEDOUT, 110 }, { ECONNREFUSED, 111 }, { EHOSTDOWN, 112 },
    { EHOSTUNREACH, 113 }, { EALREADY, 114 }, { EINPROGRESS, 115 }, { ESTALE, 116 }, { EDQUOT, 122 },
    { ECANCELED, 125 }, { EOPNOTSUPP, 95 },
};

int tl_errno_to_guest(int e)
{
    for (size_t i = 0; i < sizeof(k_errno) / sizeof(k_errno[0]); i++) if (k_errno[i].darwin == e) return k_errno[i].guest;
    return e;
}

int tl_errno_from_guest(int e)
{
    for (size_t i = 0; i < sizeof(k_errno) / sizeof(k_errno[0]); i++) if (k_errno[i].guest == e) return k_errno[i].darwin;
    return e;
}

static __thread int t_guest_errno;
int *tl_guest_errno_ptr(void) { return &t_guest_errno; }
void tl_set_guest_errno(int e) { t_guest_errno = e; }
static int *bionic___errno(void) { return &t_guest_errno; }

/* ---------------------------------------------------------------- signals */

/* Linux arm64 signal numbers by index; the value is Darwin's. */
static const int k_sig_to_darwin[65] = {
    [0] = 0, [1] = SIGHUP, [2] = SIGINT, [3] = SIGQUIT, [4] = SIGILL, [5] = SIGTRAP, [6] = SIGABRT,
    [7] = SIGBUS, [8] = SIGFPE, [9] = SIGKILL, [10] = SIGUSR1, [11] = SIGSEGV, [12] = SIGUSR2,
    [13] = SIGPIPE, [14] = SIGALRM, [15] = SIGTERM, [16] = -1, [17] = SIGCHLD, [18] = SIGCONT,
    [19] = SIGSTOP, [20] = SIGTSTP, [21] = SIGTTIN, [22] = SIGTTOU, [23] = SIGURG, [24] = SIGXCPU,
    [25] = SIGXFSZ, [26] = SIGVTALRM, [27] = SIGPROF, [28] = SIGWINCH, [29] = SIGIO,
    [30] = SIGINFO /* SIGPWR: Boehm's stop-the-world signal */, [31] = SIGSYS,
};

int tl_signal_to_darwin(int s)
{
    if (s < 0 || s > 64) return -1;
    if (s >= 32) return -1;
    if (s == 0) return 0;
    return k_sig_to_darwin[s] ? k_sig_to_darwin[s] : -1;
}

int tl_signal_from_darwin(int d)
{
    for (int i = 1; i < 32; i++) if (k_sig_to_darwin[i] == d) return i;
    return d;
}

/* ------------------------------------------------------------------ notes */

void tl_note_once(const char *what)
{
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    static const char *seen[256];
    static int n;
    pthread_mutex_lock(&m);
    for (int i = 0; i < n; i++) if (!strcmp(seen[i], what)) { pthread_mutex_unlock(&m); return; }
    if (n < 256) seen[n++] = strdup(what);
    pthread_mutex_unlock(&m);
    tl_log_line("bionic: %s", what);
}

/* ------------------------------------------------------------ properties */

static const struct { const char *k, *v; } k_props[] = {
    { "ro.build.version.sdk", "34" }, { "ro.build.version.release", "14" },
    { "ro.build.version.release_or_codename", "14" }, { "ro.build.version.codename", "REL" },
    { "ro.build.version.incremental", "11000000" }, { "ro.build.version.security_patch", "2024-05-05" },
    { "ro.build.id", "UP1A.231105.001" }, { "ro.build.display.id", "UP1A.231105.001" },
    { "ro.build.type", "user" }, { "ro.build.tags", "release-keys" },
    { "ro.build.fingerprint", "google/shiba/shiba:14/UP1A.231105.001/11000000:user/release-keys" },
    { "ro.product.manufacturer", "Google" }, { "ro.product.brand", "google" },
    { "ro.product.model", "Pixel 8" }, { "ro.product.name", "shiba" }, { "ro.product.device", "shiba" },
    { "ro.product.board", "shiba" }, { "ro.product.cpu.abi", "arm64-v8a" },
    { "ro.product.cpu.abilist", "arm64-v8a" }, { "ro.hardware", "shiba" }, { "ro.board.platform", "gs301" },
    { "ro.soc.manufacturer", "Google" }, { "ro.soc.model", "Tensor G3" },
    { "ro.opengles.version", "196610" }, { "ro.kernel.qemu", "0" }, { "ro.debuggable", "0" },
    { "ro.secure", "1" }, { "ro.zygote", "zygote64" }, { "ro.config.low_ram", "false" },
    { "ro.sf.lcd_density", "420" }, { "ro.hwui.use_vulkan", "" }, { "persist.sys.locale", "en-US" },
    { "persist.sys.timezone", "UTC" }, { "gsm.operator.iso-country", "us" }, { "ro.carrier", "unknown" },
    { "ro.com.google.clientidbase", "android-google" }, { "debug.hwui.renderer", "" },
    { "ro.vendor.build.fingerprint", "google/shiba/shiba:14/UP1A.231105.001/11000000:user/release-keys" },
    { "ro.system.build.version.sdk", "34" }, { "ro.product.first_api_level", "34" },
};

const char *tl_sysprop(const char *name)
{
    for (size_t i = 0; i < sizeof(k_props) / sizeof(k_props[0]); i++) if (!strcmp(k_props[i].k, name)) return k_props[i].v;
    return NULL;
}

/* __system_property_find returns an opaque pointer that read() takes back. */
/* sysinfo(2): what the engines ask for to size their caches -- how much RAM there is, and how much is free. */
typedef struct { long uptime; unsigned long loads[3], totalram, freeram, sharedram, bufferram, totalswap, freeswap; unsigned short procs, pad; unsigned long totalhigh, freehigh; unsigned int mem_unit; char pad2[4]; } guest_sysinfo;
static int bionic_sysinfo(guest_sysinfo *si)
{
    memset(si, 0, sizeof(*si));
    uint64_t mem = 0; size_t len = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &len, NULL, 0);
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    si->uptime = ts.tv_sec;
    si->totalram = (unsigned long)mem;
    si->freeram = (unsigned long)(mem / 4);
    si->mem_unit = 1;
    si->procs = 1;
    return 0;
}

/* CPU_COUNT(): the number of CPUs in a set. */
static int bionic_sched_cpucount(size_t setsize, const unsigned char *set)
{
    int n = 0;
    for (size_t i = 0; i < setsize; i++) n += __builtin_popcount(set[i]);
    return n;
}

static const void *bionic___system_property_find(const char *name)
{
    for (size_t i = 0; i < sizeof(k_props) / sizeof(k_props[0]); i++) if (!strcmp(k_props[i].k, name)) return &k_props[i];
    return NULL;
}

static int bionic___system_property_read(const void *pi, char *name, char *value)
{
    if (!pi) return 0;
    const struct { const char *k, *v; } *p = pi;
    if (name) snprintf(name, 32, "%s", p->k);
    if (value) snprintf(value, 92, "%s", p->v);
    return (int)strlen(p->v);
}

int tl_dns_servers(char out[][64], int max);

static int bionic___system_property_get(const char *name, char *value)
{
    /* net.dns1, net.dns2: the phone's DNS servers (see tl_dns_servers) */
    if (name && value && !strncmp(name, "net.dns", 7) && name[7] >= '1' && name[7] <= '4' && !name[8]) {
        char servers[4][64];
        int n = tl_dns_servers(servers, 4), i = name[7] - '1';
        snprintf(value, 92, "%s", i < n ? servers[i] : "");
        return (int)strlen(value);
    }
    const char *v = name ? tl_sysprop(name) : NULL;
    if (!value) return 0;
    if (!v) { value[0] = 0; return 0; }
    snprintf(value, 92, "%s", v);
    return (int)strlen(value);
}

/* ---------------------------------------------------------------- logging */

static void log_emit(int prio, const char *tag, const char *text)
{
    static const char letters[] = "??VDIWEF";
    char p = (prio >= 0 && prio < 8) ? letters[prio] : '?';
    tl_log_line("%c/%s: %s", p, tag ? tag : "guest", text ? text : "");
}

static int bionic___android_log_write(int prio, const char *tag, const char *text)
{
    log_emit(prio, tag, text);
    return 1;
}

static int bionic___android_log_buf_write(int buf, int prio, const char *tag, const char *text)
{
    (void)buf;
    log_emit(prio, tag, text);
    return 1;
}

static int bionic___android_log_vprint(int prio, const char *tag, const char *fmt, tl_va_list *ap)
{
    char text[2048];
    tl_format(text, sizeof(text), fmt, ap);
    log_emit(prio, tag, text);
    return 1;
}

int tl_vai_android_log_print(tl_va_frame *f)
{
    tl_va_list ap; char text[2048];
    tl_va_start(f, 3, 0, &ap);
    tl_format(text, sizeof(text), (const char *)f->gp[2], &ap);
    log_emit((int)f->gp[0], (const char *)f->gp[1], text);
    return 1;
}
TL_VA_STUB(tl_va_android_log_print, tl_vai_android_log_print);
extern void tl_va_android_log_print(void);

int tl_vai_android_log_buf_print(tl_va_frame *f)
{
    tl_va_list ap; char text[2048];
    tl_va_start(f, 4, 0, &ap);
    tl_format(text, sizeof(text), (const char *)f->gp[3], &ap);
    log_emit((int)f->gp[1], (const char *)f->gp[2], text);
    return 1;
}
TL_VA_STUB(tl_va_android_log_buf_print, tl_vai_android_log_buf_print);
extern void tl_va_android_log_buf_print(void);

static void guest_abort(const char *why);

int tl_vai_android_log_assert(tl_va_frame *f)
{
    /* __android_log_assert(cond, tag, fmt, ...) */
    tl_va_list ap; char text[2048];
    const char *fmt = (const char *)f->gp[2];
    if (fmt) { tl_va_start(f, 3, 0, &ap); tl_format(text, sizeof(text), fmt, &ap); }
    else snprintf(text, sizeof(text), "assertion failed: %s", f->gp[0] ? (const char *)f->gp[0] : "?");
    log_emit(7, (const char *)f->gp[1], text);
    guest_abort("__android_log_assert");
    return 0;
}
TL_VA_STUB(tl_va_android_log_assert, tl_vai_android_log_assert);
extern void tl_va_android_log_assert(void);

int tl_vai_syslog(tl_va_frame *f)
{
    tl_va_list ap; char text[1024];
    tl_va_start(f, 2, 0, &ap);
    tl_format(text, sizeof(text), (const char *)f->gp[1], &ap);
    log_emit(4, "syslog", text);
    return 0;
}
TL_VA_STUB(tl_va_syslog, tl_vai_syslog);
extern void tl_va_syslog(void);

static void bionic_openlog(const char *ident, int option, int facility) { (void)ident; (void)option; (void)facility; }
static void bionic_closelog(void) {}

/* --------------------------------------------------- C runtime and aborts */

static void describe_caller(void *lr, char *out, size_t n)
{
    const char *ln = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(lr, &ln, &sa);
    if (ln) snprintf(out, n, "%s %s+%#lx", ln, sym ? sym : "?", sa ? (unsigned long)((const char *)lr - (const char *)sa) : 0ul);
    else snprintf(out, n, "%p", lr);
}

static void guest_abort(const char *why)
{
    char where[200];
    describe_caller(__builtin_return_address(0), where, sizeof(where));
    tl_log_line("bionic: guest abort (%s) at %s", why, where);
    abort();
}

static void bionic_abort(void)
{
    char where[200];
    describe_caller(__builtin_return_address(0), where, sizeof(where));
    tl_log_line("bionic: abort() called from %s", where);
    abort();
}

/* When set, a guest exit() ends the guest, not the app that is hosting it. The hook does not return. */
void (*tl_guest_exit_hook)(int status);

static void bionic_exit(int status)
{
    char where[200];
    describe_caller(__builtin_return_address(0), where, sizeof(where));
    tl_log_line("bionic: exit(%d) called from %s", status, where);
    if (tl_guest_exit_hook) tl_guest_exit_hook(status);
    exit(status);
}

static void bionic___stack_chk_fail(void) { guest_abort("stack smashing detected"); }

static void bionic___assert2(const char *file, int line, const char *func, const char *expr)
{
    char where[200];
    describe_caller(__builtin_return_address(0), where, sizeof(where));
    tl_log_line("bionic: assertion failed: %s:%d: %s: %s (from %s)", file, line, func ? func : "?", expr, where);
    abort();
}

static void bionic_android_set_abort_message(const char *msg) { tl_log_line("bionic: abort message: %s", msg ? msg : "(null)"); }

static void bionic___libc_init(void) {}

/* Fork handlers have nothing to run for: this process never forks guest code. */
static int bionic___register_atfork(void *prepare, void *parent, void *child, void *dso)
{
    (void)prepare; (void)parent; (void)child; (void)dso;
    return 0;
}
static int bionic_pthread_atfork(void *prepare, void *parent, void *child) { (void)prepare; (void)parent; (void)child; return 0; }

typedef struct { void (*fn)(void *); void *arg; void *dso; } atexit_ent;
static atexit_ent *g_atexit;       /* grows: a big C++ program registers a destructor per static object, thousands of them */
static int g_natexit, g_capatexit;
static pthread_mutex_t g_atexit_lock = PTHREAD_MUTEX_INITIALIZER;

static int bionic___cxa_atexit(void (*fn)(void *), void *arg, void *dso)
{
    pthread_mutex_lock(&g_atexit_lock);
    if (g_natexit == g_capatexit) {
        int cap = g_capatexit ? g_capatexit * 2 : 1024;
        atexit_ent *n = realloc(g_atexit, (size_t)cap * sizeof(atexit_ent));
        if (n) { g_atexit = n; g_capatexit = cap; }
    }
    int ok = g_natexit < g_capatexit;
    if (ok) g_atexit[g_natexit++] = (atexit_ent){ fn, arg, dso };
    pthread_mutex_unlock(&g_atexit_lock);
    return ok ? 0 : -1;
}

static void bionic___cxa_finalize(void *dso)
{
    for (;;) {
        atexit_ent e = {0};
        pthread_mutex_lock(&g_atexit_lock);
        for (int i = g_natexit - 1; i >= 0; i--) {
            if (g_atexit[i].fn && (!dso || g_atexit[i].dso == dso)) {
                e = g_atexit[i];
                g_atexit[i].fn = NULL;
                break;
            }
        }
        pthread_mutex_unlock(&g_atexit_lock);
        if (!e.fn) return;
        e.fn(e.arg);
    }
}

/* -------------------------------------------------------- system queries */

static unsigned long bionic_getauxval(unsigned long type)
{
    static uint8_t random16[16] = { 0x4a, 0x13, 0x9c, 0x71, 0xe2, 0x05, 0x88, 0x3d, 0xb6, 0x21, 0x5f, 0xc4, 0x90, 0x2e, 0x67, 0xd8 };
    switch (type) {
    case 6:  return 16384;                                   /* AT_PAGESZ */
    case 16: return 0xff;                                    /* AT_HWCAP: FP, ASIMD, EVTSTRM, AES, PMULL, SHA1, SHA2, CRC32 */
    case 26: return 0;                                       /* AT_HWCAP2 */
    case 25: return (unsigned long)random16;                 /* AT_RANDOM */
    case 17: return 100;                                     /* AT_CLKTCK */
    case 23: return 0;                                       /* AT_SECURE */
    default: errno = 0; tl_set_guest_errno(2); return 0;
    }
}

static int bionic_getpagesize(void) { return 16384; }

static long bionic_sysconf(int name)
{
    switch (name) {
    case 0:    return 131072;                                /* _SC_ARG_MAX */
    case 5:    return 1024;                                  /* _SC_CHILD_MAX */
    case 6:    return 100;                                   /* _SC_CLK_TCK */
    case 10:   return 65536;                                 /* _SC_NGROUPS_MAX */
    case 11:   return 32768;                                 /* _SC_OPEN_MAX */
    case 27:   return 20;                                    /* _SC_STREAM_MAX */
    case 28:   return 6;                                     /* _SC_TZNAME_MAX */
    case 37:   return 2147483647;                            /* _SC_ATEXIT_MAX */
    case 38:   return 1024;                                  /* _SC_IOV_MAX */
    case 39: case 40: return 16384;                          /* _SC_PAGESIZE / _SC_PAGE_SIZE */
    case 96: case 97: return getenv("TL_NCPU") ? atol(getenv("TL_NCPU")) : (long)sysconf(_SC_NPROCESSORS_ONLN);   /* _SC_NPROCESSORS_CONF / ONLN */
    case 98: { uint64_t m = 0; size_t l = sizeof(m); sysctlbyname("hw.memsize", &m, &l, NULL, 0); return (long)(m / 16384); }
    case 99: { uint64_t m = 0; size_t l = sizeof(m); sysctlbyname("hw.memsize", &m, &l, NULL, 0); return (long)(m / 16384 / 2); }
    case 100:  return 200809;                                /* _SC_MONOTONIC_CLOCK */
    case 0x4a: return 16384;                                 /* _SC_THREAD_STACK_MIN-ish */
    default:
        tl_set_guest_errno(22);
        return -1;
    }
}

static int bionic_uname(void *buf)
{
    /* bionic: six fields of 65 bytes. */
    char *b = buf;
    memset(b, 0, 65 * 6);
    snprintf(b + 65 * 0, 65, "Linux");
    snprintf(b + 65 * 1, 65, "localhost");
    snprintf(b + 65 * 2, 65, "5.15.119-android14-11-g1");
    snprintf(b + 65 * 3, 65, "#1 SMP PREEMPT Fri Nov 03 00:00:00 UTC 2023");
    snprintf(b + 65 * 4, 65, "aarch64");
    snprintf(b + 65 * 5, 65, "localdomain");
    return 0;
}

static int bionic_gethostname(char *name, size_t len) { snprintf(name, len, "localhost"); return 0; }

static int bionic_gettid(void)
{
    uint64_t tid = 0;
    pthread_threadid_np(NULL, &tid);
    return (int)tid;
}

static int bionic_getpriority(int which, int who) { (void)which; (void)who; return 0; }
static int bionic_setpriority(int which, int who, int prio) { (void)which; (void)who; (void)prio; return 0; }

static int bionic_sched_getaffinity(int pid, size_t size, void *mask)
{
    (void)pid;
    memset(mask, 0, size);
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    for (long i = 0; i < n && (size_t)(i / 8) < size; i++) ((uint8_t *)mask)[i / 8] |= (uint8_t)(1u << (i % 8));
    return 0;
}
static int bionic_sched_setaffinity(int pid, size_t size, const void *mask) { (void)pid; (void)size; (void)mask; return 0; }
static int bionic_sched_getparam(int pid, int *param) { (void)pid; if (param) *param = 0; return 0; }
static int bionic_sched_getscheduler(int pid) { (void)pid; return 0; }

static int bionic_prctl(int option, unsigned long a2, unsigned long a3, unsigned long a4, unsigned long a5)
{
    (void)a3; (void)a4; (void)a5;
    switch (option) {
    case 15: pthread_setname_np((const char *)a2); return 0;                 /* PR_SET_NAME */
    case 16: pthread_getname_np(pthread_self(), (char *)a2, 16); return 0;   /* PR_GET_NAME */
    case 0x53564d41: return 0;                                               /* PR_SET_VMA (naming anonymous memory) */
    case 38: case 4: case 36: return 0;                                      /* NO_NEW_PRIVS, DUMPABLE, THP: accepted, nothing to do */
    default:
        tl_set_guest_errno(22);
        return -1;
    }
}

long tl_linux_syscall(long a0, long a1, long a2, long a3, long a4, long a5, long nr);

static long bionic_syscall(long num, long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)a6;
    long r = tl_linux_syscall(a1, a2, a3, a4, a5, a6, num);
    if (r < 0 && r > -4096) { tl_set_guest_errno((int)-r); return -1; }
    return r;
}

static int bionic_fork(void) { tl_set_guest_errno(38); return -1; }
static int bionic_execv(const char *p, char *const a[]) { (void)p; (void)a; tl_set_guest_errno(38); return -1; }
static int bionic_execve(const char *p, char *const a[], char *const e[]) { (void)p; (void)a; (void)e; tl_set_guest_errno(38); return -1; }
static int bionic_waitpid(int pid, int *st, int opt) { (void)pid; (void)st; (void)opt; tl_set_guest_errno(10); return -1; }
static long bionic_ptrace(int req, int pid, void *addr, void *data) { (void)req; (void)pid; (void)addr; (void)data; tl_set_guest_errno(1); return -1; }

/* A passwd entry like Android's: an app user. */
static struct { char *pw_name; uint32_t pw_uid, pw_gid; char *pw_dir; char *pw_shell; } g_pwd = { "u0_a1", 10001, 10001, "/data", "/system/bin/sh" };
static void *bionic_getpwuid(uint32_t uid) { (void)uid; return &g_pwd; }
static int bionic_getpwuid_r(uint32_t uid, void *pwd, char *buf, size_t n, void **result)
{
    (void)uid; (void)buf; (void)n;
    memcpy(pwd, &g_pwd, sizeof(g_pwd));
    *result = pwd;
    return 0;
}

/* ------------------------------------------------------------------ dl*** */

typedef struct { int unused; } sysh;
static sysh g_sys_handle[16];
static char g_sys_names[16][48];
static int g_nsys;
static __thread char t_dlerr[200];
static __thread bool t_dlerr_set;

static void dl_fail(const char *fmt, const char *arg)
{
    snprintf(t_dlerr, sizeof(t_dlerr), fmt, arg);
    t_dlerr_set = true;
}

static void *bionic_dlopen(const char *path, int flags)
{
    if (!path) return &g_sys_handle[0];          /* the global namespace */
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    /* RTLD_NOLOAD (bionic: 4): only one already loaded. Geode finds the game this way. */
    if (flags & 4) {
        tl_lib *L = tl_ld_find_lib(base);
        if (getenv("TL_DL_TRACE")) tl_log_line("dl: dlopen(%s, NOLOAD) -> %s", path, L ? "loaded" : "not loaded");
        if (!L) dl_fail("dlopen failed: library \"%s\" is not loaded", path);
        return L;
    }
    if (!strcmp(base, "libvulkan.so") && !tl_vk_available()) { dl_fail("dlopen failed: library \"%s\" not found", path); return NULL; }
    if (tl_bionic_is_system_lib(base)) {
        for (int i = 0; i < g_nsys; i++) if (!strcmp(g_sys_names[i], base)) return &g_sys_handle[i + 1];
        if (g_nsys < 15) { snprintf(g_sys_names[g_nsys], 48, "%s", base); g_nsys++; return &g_sys_handle[g_nsys]; }
        return &g_sys_handle[0];
    }
    /* A path to a file of its own (a mod's library) loads from there; a bare name, or a path into the app, from the APK. */
    tl_lib *L = tl_ld_find_lib(base);
    if (!L) L = tl_ld_load(path[0] == '/' ? path : base);
    if (getenv("TL_DL_TRACE")) tl_log_line("dl: dlopen(%s) -> %s", path, L ? "ok" : "not found");
    if (!L) { dl_fail("dlopen failed: library \"%s\" not found", path); return NULL; }
    tl_ld_init(L);
    return L;
}

static void *bionic_dlsym(void *handle, const char *name)
{
    if (!name) return NULL;
    void *r = NULL;
    if (handle == NULL || handle == (void *)-1L) {
        r = tl_ld_sym(NULL, name);
        if (!r) r = tl_bionic_find(name);
    } else if ((sysh *)handle >= g_sys_handle && (sysh *)handle < g_sys_handle + 16) {
        r = tl_bionic_find(name);
    } else {
        r = tl_ld_sym((tl_lib *)handle, name);
    }
    if (getenv("TL_DL_TRACE")) tl_log_line("dl: dlsym(%s) -> %s", name, r ? "found" : "missing");
    if (!r) dl_fail("undefined symbol: %s", name);
    return r;
}

static int bionic_dlclose(void *handle) { (void)handle; return 0; }

static char *bionic_dlerror(void)
{
    if (!t_dlerr_set) return NULL;
    t_dlerr_set = false;
    return t_dlerr;
}

typedef struct { const char *dli_fname; void *dli_fbase; const char *dli_sname; void *dli_saddr; } guest_dl_info;
static int bionic_dladdr(const void *addr, guest_dl_info *info)
{
    const char *ln = NULL; const void *sa = NULL;
    const char *sym = tl_ld_symbol_at(addr, &ln, &sa);
    if (!ln) return 0;
    info->dli_fname = ln;
    /* The base of the library the address is in: Geode computes every hook address from the game's. */
    info->dli_fbase = tl_ld_lib_base(tl_ld_lib_of(addr));
    info->dli_sname = sym;
    info->dli_saddr = (void *)sa;
    return 1;
}

typedef struct { uint64_t addr; const char *name; const void *phdr; uint16_t phnum; uint64_t adds, subs; size_t tls_modid; void *tls_data; } guest_phdr_info;
typedef int (*guest_phdr_cb)(guest_phdr_info *, size_t, void *);
struct phdr_ctx { guest_phdr_cb cb; void *data; };
static int phdr_trampoline(uintptr_t bias, const char *name, const void *phdr, unsigned phnum, void *user)
{
    struct phdr_ctx *c = user;
    guest_phdr_info i = { bias, name, phdr, (uint16_t)phnum, 0, 0, 0, NULL };
    return c->cb(&i, sizeof(i), c->data);
}
static int bionic_dl_iterate_phdr(guest_phdr_cb cb, void *data)
{
    struct phdr_ctx c = { cb, data };
    return tl_ld_iterate(phdr_trampoline, &c);
}

/* ------------------------------------------------------------------ data */

static char **g_environ_slot;
static char **tl_environ_ptr(void) { return NULL; }

/* ------------------------------------------------------------- the table */

const tl_bionic_entry tl_tab_core[] = {
    TL_WRAP("__errno", bionic___errno),
    TL_WRAP("__system_property_find", bionic___system_property_find),
    TL_WRAP("__system_property_read", bionic___system_property_read),
    TL_WRAP("__system_property_get", bionic___system_property_get),
    TL_WRAP("__android_log_write", bionic___android_log_write),
    TL_WRAP("__android_log_buf_write", bionic___android_log_buf_write),
    TL_WRAP("__android_log_vprint", bionic___android_log_vprint),
    TL_WRAP("__android_log_print", tl_va_android_log_print),
    TL_WRAP("__android_log_buf_print", tl_va_android_log_buf_print),
    TL_WRAP("__android_log_assert", tl_va_android_log_assert),
    TL_WRAP("syslog", tl_va_syslog),
    TL_WRAP("openlog", bionic_openlog),
    TL_WRAP("closelog", bionic_closelog),
    TL_WRAP("abort", bionic_abort),
    TL_WRAP("exit", bionic_exit),
    TL_DIRECT(_exit), TL_DIRECT(_Exit),
    TL_WRAP("__stack_chk_fail", bionic___stack_chk_fail),
    TL_WRAP("__assert2", bionic___assert2),
    TL_WRAP("android_set_abort_message", bionic_android_set_abort_message),
    TL_WRAP("__libc_init", bionic___libc_init),
    TL_WRAP("__register_atfork", bionic___register_atfork),
    TL_WRAP("pthread_atfork", bionic_pthread_atfork),
    TL_WRAP("__cxa_atexit", bionic___cxa_atexit),
    TL_WRAP("__cxa_finalize", bionic___cxa_finalize),
    TL_WRAP("getauxval", bionic_getauxval),
    TL_WRAP("getpagesize", bionic_getpagesize),
    TL_WRAP("sysconf", bionic_sysconf), TL_WRAP("sysinfo", bionic_sysinfo),
    TL_WRAP("uname", bionic_uname),
    TL_WRAP("gethostname", bionic_gethostname),
    TL_WRAP("gettid", bionic_gettid),
    TL_DIRECT(getpid), TL_DIRECT(getppid), TL_DIRECT(getuid), TL_DIRECT(geteuid), TL_DIRECT(getegid),
    TL_WRAP("getpriority", bionic_getpriority),
    TL_WRAP("setpriority", bionic_setpriority),
    TL_WRAP("sched_getaffinity", bionic_sched_getaffinity), TL_WRAP("__sched_cpucount", bionic_sched_cpucount),
    TL_WRAP("sched_setaffinity", bionic_sched_setaffinity),
    TL_WRAP("sched_getparam", bionic_sched_getparam),
    TL_WRAP("sched_getscheduler", bionic_sched_getscheduler),
    TL_DIRECT(sched_yield),
    TL_WRAP("prctl", bionic_prctl),
    TL_WRAP("syscall", bionic_syscall),
    TL_WRAP("fork", bionic_fork),
    TL_WRAP("execv", bionic_execv),
    TL_WRAP("execve", bionic_execve),
    TL_WRAP("waitpid", bionic_waitpid),
    TL_WRAP("ptrace", bionic_ptrace),
    TL_WRAP("getpwuid", bionic_getpwuid),
    TL_WRAP("getpwuid_r", bionic_getpwuid_r),
    TL_WRAP("dlopen", bionic_dlopen),
    TL_WRAP("dlsym", bionic_dlsym),
    TL_WRAP("dlclose", bionic_dlclose),
    TL_WRAP("dlerror", bionic_dlerror),
    TL_WRAP("dladdr", bionic_dladdr),
    TL_WRAP("dl_iterate_phdr", bionic_dl_iterate_phdr),
    TL_END
};

/* ----------------------------------------------------------------- lookup */

static const tl_bionic_entry *const k_tables[] = { tl_tab_core, tl_tab_str, tl_tab_io, tl_tab_io2, tl_tab_str2, tl_tab_net, tl_tab_pthread, tl_tab_ndk, tl_tab_egl, tl_tab_cxx, tl_tab_opensles };

typedef struct { const char *name; void *addr; } slot;
static slot *g_slots;
static size_t g_nslots;
static pthread_once_t g_once = PTHREAD_ONCE_INIT;

static uint64_t fnv(const char *s) { uint64_t h = 1469598103934665603ull; while (*s) { h ^= (unsigned char)*s++; h *= 1099511628211ull; } return h; }

static void build(void)
{
    size_t total = 0;
    for (size_t t = 0; t < sizeof(k_tables) / sizeof(k_tables[0]); t++) for (const tl_bionic_entry *e = k_tables[t]; e->name; e++) total++;
    g_nslots = 1;
    while (g_nslots < total * 2) g_nslots <<= 1;
    g_slots = calloc(g_nslots, sizeof(slot));
    (void)g_environ_slot; (void)tl_environ_ptr;
    for (size_t t = 0; t < sizeof(k_tables) / sizeof(k_tables[0]); t++) {
        for (const tl_bionic_entry *e = k_tables[t]; e->name; e++) {
            size_t i = fnv(e->name) & (g_nslots - 1);
            while (g_slots[i].name) {
                if (!strcmp(g_slots[i].name, e->name)) { tl_log_line("bionic: duplicate table entry %s", e->name); break; }
                i = (i + 1) & (g_nslots - 1);
            }
            g_slots[i].name = e->name;
            g_slots[i].addr = e->addr;
        }
    }
}

void *tl_bionic_find(const char *name)
{
    pthread_once(&g_once, build);
    /* environ is a variable the guest reads whenever it walks the environment. The process may change it under the guest (setenv reallocates the array and frees the
     * old one), so the guest is given the address of the live variable, not of a copy made at start-up. */
    if (name[0] == 'e' && !strcmp(name, "environ")) return _NSGetEnviron();
    size_t i = fnv(name) & (g_nslots - 1);
    while (g_slots[i].name) {
        if (!strcmp(g_slots[i].name, name)) return g_slots[i].addr;
        i = (i + 1) & (g_nslots - 1);
    }
    /* OpenGL ES entry points are not in a table: they come from ANGLE, by name. */
    if (name[0] == 'g' && name[1] == 'l' && name[2] >= 'A' && name[2] <= 'Z') return tl_egl_resolve(name);
    if (!strncmp(name, "egl", 3)) return tl_egl_resolve(name);
    /* Vulkan is MoltenVK's, the same way. */
    if (name[0] == 'v' && name[1] == 'k' && name[2] >= 'A' && name[2] <= 'Z') return tl_vk_resolve(name);
    return NULL;
}

bool tl_bionic_is_system_lib(const char *soname)
{
    static const char *const sys[] = {
        "libc.so", "libm.so", "libdl.so", "liblog.so", "libandroid.so", "libEGL.so", "libGLESv1_CM.so",
        "libGLESv2.so", "libGLESv3.so", "libz.so", "libmediandk.so", "libOpenSLES.so", "libaaudio.so",
        "libvulkan.so", "libnativewindow.so", "libjnigraphics.so", "libcamera2ndk.so", "libstdc++.so", NULL };
    for (int i = 0; sys[i]; i++) if (!strcmp(sys[i], soname)) return true;
    return false;
}
