/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * libc calls that Android's own platform libraries make and games never did: ART, libbase, libc++ and libunwindstack, as
 * they come in the ART module. Most map onto the Darwin call; the Linux-only ones (namespaces, pidfds, real-time signals)
 * answer the way a sandboxed app process sees them.
 */
#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <zlib.h>

const char *tl_path_resolve(const char *path, char *buf, size_t n);   /* husk-tl-bionic-io.c */
void tl_eventfd_register(int rd, int wr);                             /* husk-tl-bionic-io.c */

/* The process name, as Android reports it: an app runs as its package, under app_process64. */
static const char *b_getprogname(void) { return "app_process64"; }

static int b_android_get_device_api_level(void) { return 36; }        /* Android 16, the release the ART module comes from */
static _Atomic int g_target_sdk = 36;
static void b_android_set_application_target_sdk_version(int v) { g_target_sdk = v; }
static int b_android_get_application_target_sdk_version(void) { return g_target_sdk; }

/* ---------------------------------------------------------------- files */

static int b_pipe2(int fds[2], int flags)
{
    TL_ERRNO_BEGIN();
    int r = pipe(fds);
    TL_ERRNO_END();
    if (r) return r;
    for (int i = 0; i < 2; i++) {
        if (flags & 0x80000) fcntl(fds[i], F_SETFD, FD_CLOEXEC);                         /* O_CLOEXEC */
        if (flags & 0x800) fcntl(fds[i], F_SETFL, fcntl(fds[i], F_GETFL) | O_NONBLOCK);   /* O_NONBLOCK */
    }
    return 0;
}

static int b_creat(const char *path, unsigned mode)
{
    char b[1024];
    TL_ERRNO_BEGIN(); int r = open(tl_path_resolve(path, b, sizeof(b)), O_WRONLY | O_CREAT | O_TRUNC, (mode_t)mode); TL_ERRNO_END();
    return r;
}

/* mkstemp and friends: make the file where the guest path really is, then write the name chosen back into its template. */
static void copy_back(char *tmpl, const char *made, size_t suffix)
{
    size_t tl = strlen(tmpl), ml = strlen(made);
    if (tl >= 6 + suffix && ml >= 6 + suffix) memcpy(tmpl + tl - 6 - suffix, made + ml - 6 - suffix, 6);
}
static int b_mkostemps(char *tmpl, int suffix, int flags)
{
    char b[1024];
    const char *host = tl_path_resolve(tmpl, b, sizeof(b));
    if (host != b) { snprintf(b, sizeof(b), "%s", host); }
    int hf = (flags & 0x80000) ? O_CLOEXEC : 0;
    TL_ERRNO_BEGIN(); int r = mkostemps(b, suffix, hf); TL_ERRNO_END();
    if (r >= 0) copy_back(tmpl, b, (size_t)suffix);
    return r;
}
static int b_mkstemp(char *tmpl) { return b_mkostemps(tmpl, 0, 0); }
static int b_mkstemps(char *tmpl, int suffix) { return b_mkostemps(tmpl, suffix, 0); }
static int b_mkostemp(char *tmpl, int flags) { return b_mkostemps(tmpl, 0, flags); }
static char *b_mkdtemp(char *tmpl)
{
    char b[1024];
    const char *host = tl_path_resolve(tmpl, b, sizeof(b));
    if (host != b) { snprintf(b, sizeof(b), "%s", host); }
    TL_ERRNO_BEGIN(); char *r = mkdtemp(b); TL_ERRNO_END();
    if (!r) return NULL;
    copy_back(tmpl, b, 0);
    return tmpl;
}

/* fallocate: growing a file is all ART asks of it. */
static int b_fallocate(int fd, int mode, long off, long len)
{
    if (mode) { tl_set_guest_errno(95 /* EOPNOTSUPP */); return -1; }
    struct stat st;
    if (fstat(fd, &st)) { tl_set_guest_errno(tl_errno_to_guest(errno)); return -1; }
    if (st.st_size >= off + len) return 0;
    TL_ERRNO_BEGIN(); int r = ftruncate(fd, off + len); TL_ERRNO_END();
    return r;
}

/* Linux MS_ flags: ASYNC 1, INVALIDATE 2, SYNC 4. */
static int b_msync(void *addr, size_t len, int flags)
{
    int d = (flags & 1 ? MS_ASYNC : 0) | (flags & 2 ? MS_INVALIDATE : 0) | (flags & 4 ? MS_SYNC : 0);
    TL_ERRNO_BEGIN(); int r = msync(addr, len, d); TL_ERRNO_END();
    return r;
}
static int b_mincore(void *addr, size_t len, unsigned char *vec)
{
    TL_ERRNO_BEGIN(); int r = mincore(addr, len, (char *)vec); TL_ERRNO_END();
    if (!r) { size_t n = (len + 16383) / 16384; for (size_t i = 0; i < n; i++) vec[i] &= 1; }
    return r;
}

/* No directory walks yet: the callers (temporary-directory cleanup) treat failure as nothing to do. */
static int b_nftw(const char *dir, void *fn, int depth, int flags) { (void)dir; (void)fn; (void)depth; (void)flags; tl_set_guest_errno(38); return -1; }
static void *b_getpwnam(const char *name) { (void)name; return NULL; }

/* ---------------------------------------------------------------- strings, numbers */

static double b_strtod_l(const char *s, char **end, void *loc) { (void)loc; return strtod(s, end); }
static float b_strtof_l(const char *s, char **end, void *loc) { (void)loc; return strtof(s, end); }

/* The GNU strerror_r returns the message, rather than an int. Errors arrive as Linux numbers. */
static char *b___gnu_strerror_r(int e, char *buf, size_t n)
{
    const char *m = strerror(tl_errno_from_guest(e));
    if (buf && n) { snprintf(buf, n, "%s", m); return buf; }
    return (char *)m;
}

static void b___assert(const char *file, int line, const char *msg)
{
    tl_log_line("bionic: assertion failed at %s:%d: %s", file ? file : "?", line, msg ? msg : "");
    abort();
}

typedef size_t (*fread_fn)(void *, size_t, size_t, void *);
static size_t b___fread_chk(void *buf, size_t bufsize, size_t size, size_t count, void *fp)
{
    if (size && count > bufsize / size) { tl_log_line("bionic: fread overflows its buffer"); abort(); }
    fread_fn f = (fread_fn)tl_bionic_find("fread");
    return f ? f(buf, size, count, fp) : 0;
}

/* ---------------------------------------------------------------- threads, processes, signals */

static int b_pthread_getname_np(pthread_t t, char *buf, size_t n)
{
    int r = pthread_getname_np(t, buf, n);
    return r ? tl_errno_to_guest(r) : 0;
}

/* Linux's per-thread CPU clock for another thread has no Darwin clock id: report the calling thread's. */
static int b_pthread_getcpuclockid(pthread_t t, int *clk) { (void)t; *clk = 3 /* CLOCK_THREAD_CPUTIME_ID */; return 0; }

/* tgkill: a thread signalling itself (abort paths, stack dumps) works; another thread by Linux id is not reachable. */
static int b_tgkill(int tgid, int tid, int sig)
{
    (void)tgid;
    uint64_t self = 0;
    pthread_threadid_np(NULL, &self);
    if ((uint64_t)(uint32_t)tid != (uint32_t)self) { tl_set_guest_errno(3 /* ESRCH */); return -1; }
    int d = tl_signal_to_darwin(sig);
    if (d <= 0) { tl_set_guest_errno(22); return -1; }
    return pthread_kill(pthread_self(), d) ? -1 : 0;
}

static int b_waitid(int idtype, int id, void *info, int options) { (void)idtype; (void)id; (void)info; (void)options; tl_set_guest_errno(10 /* ECHILD */); return -1; }
static int b_setpgid(int pid, int pgid) { (void)pid; (void)pgid; return 0; }
static int b_unshare(int flags) { (void)flags; tl_set_guest_errno(1 /* EPERM */); return -1; }
static int b_pidfd_open(int pid, unsigned flags) { (void)pid; (void)flags; tl_set_guest_errno(38); return -1; }
static long b_process_vm_readv(int pid, const void *l, unsigned long ln, const void *r, unsigned long rn, unsigned long f)
{
    (void)pid; (void)l; (void)ln; (void)r; (void)rn; (void)f;
    tl_set_guest_errno(38);
    return -1;
}

/* Real-time signals: bionic numbers them from 32 and keeps the first few for itself. Darwin has none to deliver. */
static int b___libc_current_sigrtmin(void) { return 35; }
static int b___libc_current_sigrtmax(void) { return 64; }

/* sigwaitinfo: ART's signal catcher thread waits here for SIGQUIT and SIGUSR1, which nothing sends on iOS. */
static int b_sigwaitinfo(const uint64_t *set, void *info)
{
    (void)set; (void)info;
    for (;;) sleep(3600);
    return -1;
}

/* thread_local destructors (libc++'s __cxa_thread_atexit): a list per thread, run when the thread ends. */
typedef struct dtor { void (*fn)(void *); void *obj; struct dtor *next; } dtor;
static pthread_key_t g_dtor_key;
static pthread_once_t g_dtor_once = PTHREAD_ONCE_INIT;
static void run_dtors(void *head)
{
    for (dtor *d = head; d;) { dtor *n = d->next; d->fn(d->obj); free(d); d = n; }
}
static void dtor_key(void) { pthread_key_create(&g_dtor_key, run_dtors); }
static int b___cxa_thread_atexit_impl(void (*fn)(void *), void *obj, void *dso)
{
    (void)dso;
    pthread_once(&g_dtor_once, dtor_key);
    dtor *d = malloc(sizeof(*d));
    if (!d) return -1;
    d->fn = fn; d->obj = obj; d->next = pthread_getspecific(g_dtor_key);
    pthread_setspecific(g_dtor_key, d);
    return 0;
}

/* ---------------------------------------------------------------- timerfd */
/*
 * Linux's timerfd: a file descriptor that becomes readable when a timer expires, and reads as the number of expirations. Flutter's
 * message loop sleeps on one through its ALooper. Here it is the read end of a pipe; a thread of our own writes an expiration (an
 * 8-byte count of 1) when each timer is due. The clock is the host's monotonic or real-time clock, which is what the guest's
 * clock_gettime reads for the same id.
 */
typedef struct { int rd, wr, clock; bool armed; uint64_t due, interval; } tfd;     /* due/interval in ns; due on its clock */
static tfd *g_tfd[1024];
static pthread_mutex_t g_tfd_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_tfd_cv = PTHREAD_COND_INITIALIZER;
static pthread_t g_tfd_thread;
static bool g_tfd_started;

static uint64_t clock_ns(int linux_clock)
{
    struct timespec ts;
    clock_gettime(linux_clock == 0 || linux_clock == 5 ? CLOCK_REALTIME : CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void *tfd_thread(void *arg)
{
    (void)arg;
    pthread_setname_np("husk-timerfd");
    pthread_mutex_lock(&g_tfd_mu);
    for (;;) {
        uint64_t wait = UINT64_MAX;
        for (int i = 0; i < 1024; i++) {
            tfd *t = g_tfd[i];
            if (!t || !t->armed) continue;
            uint64_t now = clock_ns(t->clock);
            if (now >= t->due) {
                uint64_t one = 1;
                (void)!write(t->wr, &one, sizeof(one));
                if (t->interval) { while (t->due <= now) t->due += t->interval; } else t->armed = false;
            }
            if (t->armed && t->due - now < wait) wait = t->due - now;
        }
        if (wait == UINT64_MAX) pthread_cond_wait(&g_tfd_cv, &g_tfd_mu);
        else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            uint64_t at = (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec + wait;
            ts.tv_sec = (time_t)(at / 1000000000ull); ts.tv_nsec = (long)(at % 1000000000ull);
            pthread_cond_timedwait(&g_tfd_cv, &g_tfd_mu, &ts);
        }
    }
    return NULL;
}

static int b_timerfd_create(int clock, int flags)
{
    int p[2];
    if (pipe(p)) { tl_set_guest_errno(tl_errno_to_guest(errno)); return -1; }
    if (p[0] >= 1024) { close(p[0]); close(p[1]); tl_set_guest_errno(24 /* EMFILE */); return -1; }
    fcntl(p[1], F_SETFL, O_NONBLOCK);
    if (flags & 0x800) fcntl(p[0], F_SETFL, O_NONBLOCK);       /* TFD_NONBLOCK */
    if (flags & 0x80000) { fcntl(p[0], F_SETFD, FD_CLOEXEC); fcntl(p[1], F_SETFD, FD_CLOEXEC); }
    tfd *t = calloc(1, sizeof(*t));
    t->rd = p[0]; t->wr = p[1]; t->clock = clock;
    pthread_mutex_lock(&g_tfd_mu);
    free(g_tfd[p[0]]);
    g_tfd[p[0]] = t;
    if (!g_tfd_started) { g_tfd_started = true; pthread_create(&g_tfd_thread, NULL, tfd_thread, NULL); }
    pthread_mutex_unlock(&g_tfd_mu);
    return p[0];
}

typedef struct { int64_t is, ins, vs, vns; } guest_itimerspec;     /* it_interval, it_value: two timespecs */
static int b_timerfd_settime(int fd, int flags, const guest_itimerspec *nv, guest_itimerspec *old)
{
    if (fd < 0 || fd >= 1024 || !nv) { tl_set_guest_errno(22); return -1; }
    pthread_mutex_lock(&g_tfd_mu);
    tfd *t = g_tfd[fd];
    if (!t) { pthread_mutex_unlock(&g_tfd_mu); tl_set_guest_errno(9 /* EBADF */); return -1; }
    uint64_t now = clock_ns(t->clock);
    if (old) {
        uint64_t left = t->armed && t->due > now ? t->due - now : 0;
        old->is = (int64_t)(t->interval / 1000000000ull); old->ins = (int64_t)(t->interval % 1000000000ull);
        old->vs = (int64_t)(left / 1000000000ull); old->vns = (int64_t)(left % 1000000000ull);
    }
    uint64_t value = (uint64_t)nv->vs * 1000000000ull + (uint64_t)nv->vns;
    { static int tr = -1; if (tr < 0) tr = getenv("TL_TIMERFD_TRACE") ? 1 : 0;
      if (tr) { char nm[32] = ""; pthread_getname_np(pthread_self(), nm, sizeof(nm)); tl_log_line("timerfd[%s]: fd %d set %s %lld ms from now", nm, fd, (flags & 1) ? "abs" : "rel", value ? (long long)(((flags & 1) ? (int64_t)value - (int64_t)now : (int64_t)value) / 1000000) : -1ll); } }
    t->interval = (uint64_t)nv->is * 1000000000ull + (uint64_t)nv->ins;
    if (!value) t->armed = false;
    else { t->due = (flags & 1 /* TFD_TIMER_ABSTIME */) ? value : now + value; t->armed = true; }
    /* a re-arm drops expirations not yet read, as Linux does */
    uint64_t junk[16];
    int fl = fcntl(t->rd, F_GETFL);
    fcntl(t->rd, F_SETFL, fl | O_NONBLOCK);
    while (read(t->rd, junk, sizeof(junk)) > 0) {}
    fcntl(t->rd, F_SETFL, fl);
    pthread_cond_signal(&g_tfd_cv);
    pthread_mutex_unlock(&g_tfd_mu);
    return 0;
}

static int b_timerfd_gettime(int fd, guest_itimerspec *cur)
{
    if (fd < 0 || fd >= 1024 || !cur) { tl_set_guest_errno(22); return -1; }
    pthread_mutex_lock(&g_tfd_mu);
    tfd *t = g_tfd[fd];
    if (!t) { pthread_mutex_unlock(&g_tfd_mu); tl_set_guest_errno(9); return -1; }
    uint64_t now = clock_ns(t->clock), left = t->armed && t->due > now ? t->due - now : 0;
    cur->is = (int64_t)(t->interval / 1000000000ull); cur->ins = (int64_t)(t->interval % 1000000000ull);
    cur->vs = (int64_t)(left / 1000000000ull); cur->vns = (int64_t)(left % 1000000000ull);
    pthread_mutex_unlock(&g_tfd_mu);
    return 0;
}

/* ---------------------------------------------------------------- epoll, eventfd */
/*
 * epoll on kqueue. An epoll instance is a kqueue; each watched descriptor is a read and/or a write filter carrying the caller's
 * 64-bit data, and each kevent that fires comes back as one epoll_event. Dart's I/O event handler (Flutter apps) and other Linux
 * event loops wait this way.
 */
enum { EPOLLIN = 0x1, EPOLLPRI = 0x2, EPOLLOUT = 0x4, EPOLLERR = 0x8, EPOLLHUP = 0x10, EPOLLRDHUP = 0x2000, EPOLLONESHOT = 1u << 30, EPOLLET = 1u << 31 };
typedef struct { uint32_t events; uint32_t pad; uint64_t data; } guest_epoll_event;     /* arm64: not packed, 16 bytes */

static int b_epoll_create1(int flags)
{
    TL_ERRNO_BEGIN(); int kq = kqueue(); TL_ERRNO_END();
    if (kq >= 0 && (flags & 0x80000)) fcntl(kq, F_SETFD, FD_CLOEXEC);
    return kq;
}
static int b_epoll_create(int size) { if (size <= 0) { tl_set_guest_errno(22); return -1; } return b_epoll_create1(0); }

static int b_epoll_ctl(int epfd, int op, int fd, const guest_epoll_event *ev)
{
    struct kevent k[2];
    int n = 0;
    if (op == 2 || op == 3) {               /* DEL, or MOD: remove both filters first (one may not be there) */
        EV_SET(&k[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&k[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        for (int i = 0; i < 2; i++) kevent(epfd, &k[i], 1, NULL, 0, NULL);
        if (op == 2) return 0;
    }
    if (!ev) { tl_set_guest_errno(14 /* EFAULT */); return -1; }
    uint16_t flags = EV_ADD | EV_ENABLE | ((ev->events & EPOLLET) ? EV_CLEAR : 0) | ((ev->events & EPOLLONESHOT) ? EV_ONESHOT : 0);
    void *udata = (void *)(uintptr_t)ev->data;
    if (ev->events & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) EV_SET(&k[n++], fd, EVFILT_READ, flags, 0, 0, udata);
    if (ev->events & EPOLLOUT) EV_SET(&k[n++], fd, EVFILT_WRITE, flags, 0, 0, udata);
    if (!n) EV_SET(&k[n++], fd, EVFILT_READ, flags | EV_DISABLE, 0, 0, udata);    /* hang-ups and errors only */
    TL_ERRNO_BEGIN(); int r = kevent(epfd, k, n, NULL, 0, NULL); TL_ERRNO_END();
    return r < 0 ? -1 : 0;
}

static int b_epoll_wait(int epfd, guest_epoll_event *out, int max, int timeout_ms)
{
    if (max <= 0) { tl_set_guest_errno(22); return -1; }
    struct kevent k[64];
    if (max > 64) max = 64;
    struct timespec ts = { timeout_ms / 1000, (long)(timeout_ms % 1000) * 1000000L };
    int r;
    for (;;) {
        errno = 0;
        r = kevent(epfd, NULL, 0, k, max, timeout_ms < 0 ? NULL : &ts);
        if (r >= 0 || errno != EINTR) break;
    }
    if (r < 0) { tl_set_guest_errno(tl_errno_to_guest(errno)); return -1; }
    for (int i = 0; i < r; i++) {
        uint32_t e = 0;
        if (k[i].filter == EVFILT_READ) e |= EPOLLIN;
        if (k[i].filter == EVFILT_WRITE) e |= EPOLLOUT;
        if (k[i].flags & EV_EOF) e |= EPOLLHUP | EPOLLRDHUP;
        if (k[i].flags & EV_ERROR) e |= EPOLLERR;
        out[i].events = e;
        out[i].data = (uint64_t)(uintptr_t)k[i].udata;
    }
    return r;
}
static int b_epoll_pwait(int epfd, guest_epoll_event *out, int max, int timeout_ms, const void *mask) { (void)mask; return b_epoll_wait(epfd, out, max, timeout_ms); }

/* eventfd: a pipe. A write of a count makes it readable; a read takes one count. Waking someone up is all its users here want. */
static int b_eventfd(unsigned initval, int flags)
{
    int p[2];
    if (pipe(p)) { tl_set_guest_errno(tl_errno_to_guest(errno)); return -1; }
    fcntl(p[1], F_SETFL, O_NONBLOCK);
    if (flags & 0x800) fcntl(p[0], F_SETFL, O_NONBLOCK);
    if (flags & 0x80000) { fcntl(p[0], F_SETFD, FD_CLOEXEC); fcntl(p[1], F_SETFD, FD_CLOEXEC); }
    if (initval) { uint64_t v = initval; (void)!write(p[1], &v, sizeof(v)); }
    tl_eventfd_register(p[0], p[1]);
    return p[0];
}

const tl_bionic_entry tl_tab_sys[] = {
    TL_WRAP("getprogname", b_getprogname),
    TL_WRAP("android_get_device_api_level", b_android_get_device_api_level),
    TL_WRAP("android_set_application_target_sdk_version", b_android_set_application_target_sdk_version),
    TL_WRAP("android_get_application_target_sdk_version", b_android_get_application_target_sdk_version),
    TL_WRAP("pipe2", b_pipe2), TL_WRAP("creat", b_creat),
    TL_WRAP("mkstemp", b_mkstemp), TL_WRAP("mkstemps", b_mkstemps), TL_WRAP("mkostemp", b_mkostemp), TL_WRAP("mkostemps", b_mkostemps),
    TL_WRAP("mkstemp64", b_mkstemp), TL_WRAP("mkdtemp", b_mkdtemp),
    TL_WRAP("fallocate", b_fallocate), TL_WRAP("fallocate64", b_fallocate),
    TL_WRAP("msync", b_msync), TL_WRAP("mincore", b_mincore),
    TL_WRAP("nftw", b_nftw), TL_WRAP("nftw64", b_nftw), TL_WRAP("getpwnam", b_getpwnam),
    TL_WRAP("strtod_l", b_strtod_l), TL_WRAP("strtof_l", b_strtof_l),
    TL_WRAP("__gnu_strerror_r", b___gnu_strerror_r), TL_WRAP("__assert", b___assert), TL_WRAP("__fread_chk", b___fread_chk),
    TL_DIRECT(aligned_alloc), TL_DIRECT(arc4random), TL_DIRECT(rand_r),
    TL_WRAP("pthread_getname_np", b_pthread_getname_np), TL_WRAP("pthread_getcpuclockid", b_pthread_getcpuclockid),
    TL_WRAP("tgkill", b_tgkill), TL_WRAP("waitid", b_waitid), TL_WRAP("setpgid", b_setpgid), TL_WRAP("unshare", b_unshare),
    TL_WRAP("pidfd_open", b_pidfd_open), TL_WRAP("process_vm_readv", b_process_vm_readv),
    TL_WRAP("__libc_current_sigrtmin", b___libc_current_sigrtmin), TL_WRAP("__libc_current_sigrtmax", b___libc_current_sigrtmax),
    TL_WRAP("sigwaitinfo", b_sigwaitinfo), TL_WRAP("sigwaitinfo64", b_sigwaitinfo),
    TL_WRAP("__cxa_thread_atexit_impl", b___cxa_thread_atexit_impl),
    TL_DIRECT(adler32_combine),
    TL_WRAP("epoll_create", b_epoll_create), TL_WRAP("epoll_create1", b_epoll_create1), TL_WRAP("epoll_ctl", b_epoll_ctl),
    TL_WRAP("epoll_wait", b_epoll_wait), TL_WRAP("epoll_pwait", b_epoll_pwait), TL_WRAP("eventfd", b_eventfd),
    TL_WRAP("timerfd_create", b_timerfd_create), TL_WRAP("timerfd_settime", b_timerfd_settime), TL_WRAP("timerfd_gettime", b_timerfd_gettime),
    TL_END
};
