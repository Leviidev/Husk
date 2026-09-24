/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The questions only the device can answer.
 *
 * Running an Android library natively on iOS rests on a few facts about XNU
 * that no document states and no simulator reproduces. Each check here
 * measures one, and each is chosen because its answer changes the design:
 *
 *   tpidr    Android's arm64 code reads its stack-protector cookie from the
 *            thread register, TPIDR_EL0 + 0x28, in nearly every function. If
 *            iOS leaves that register to us and keeps it across context
 *            switches, Android code runs as it is. If not, every one of those
 *            reads has to be rewritten at load time.
 *   x18      Reserved on both platforms, but code built with shadow call
 *            stacks (and some older libraries) keeps live values in it.
 *   carve    A library's code must execute and its data must be writable,
 *            at a fixed distance from each other -- the code finds its data
 *            PC-relatively. So a library needs ordinary memory directly beside
 *            executable memory. Whether XNU allows that inside a MAP_JIT
 *            mapping, or allows a MAP_JIT page to be placed beside ordinary
 *            memory, decides how the loader lays images out.
 *
 * Every check that executes generated code does so under a signal guard and
 * only after asking the kernel whether the page is executable, the same
 * double guard Husk's MAP_JIT test uses: a check must not become the thing
 * that killed the app.
 */
#include "husk-tl.h"
#include "husk-tl-internal.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <dlfcn.h>
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#endif

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#ifndef MAP_ANON
#define MAP_ANON MAP_ANONYMOUS
#endif

static void result(tl_json *j, const char *id, const char *title, const char *status,
                   const char *fmt, ...) __attribute__((format(printf, 5, 6)));
static void result(tl_json *j, const char *id, const char *title, const char *status,
                   const char *fmt, ...)
{
    char detail[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(detail, sizeof(detail), fmt, ap);
    va_end(ap);
    fprintf(stderr, "[husk-tl] check %s: %s -- %s\n", id, status, detail);

    tl_json_begin_object(j);
    tl_json_key(j, "id");       tl_json_string(j, id);
    tl_json_key(j, "title");    tl_json_string(j, title);
    tl_json_key(j, "status");   tl_json_string(j, status);
    tl_json_key(j, "detail");   tl_json_string(j, detail);
    tl_json_end_object(j);
}

#if defined(__aarch64__)
/* Said before each check that could take the process down, so that if one
 * ever does, the console's last line names it. */
static void starting(const char *id)
{
    fprintf(stderr, "[husk-tl] check %s: starting\n", id);
}

/* Long enough, one time in three, to be taken off the CPU. */
static void pause_briefly(unsigned i)
{
    if (i % 3 == 0) {
        struct timespec ts = { 0, 200 * 1000 };
        nanosleep(&ts, NULL);
    } else {
        sched_yield();
    }
}
#endif

/* ---------------------------------------------------------------- tpidr */

#if defined(__aarch64__)

#define TPIDR_THREADS   4
#define TPIDR_ROUNDS    150

static inline uint64_t tpidr_get(void)
{
    uint64_t v;
    __asm__ volatile("mrs %0, tpidr_el0" : "=r"(v));
    return v;
}

static inline void tpidr_set(uint64_t v)
{
    __asm__ volatile("msr tpidr_el0, %0" : : "r"(v) : "memory");
}

typedef struct tpidr_run {
    uint64_t initial;
    uint64_t block[8];      /* what it points at while it is ours */
    bool     wrote;
    uint32_t rounds;
    uint32_t lost;
    uint64_t seen;          /* the first value that was not ours */
} tpidr_run;

/* Point the register at a block of our own -- never left dangling, in case
 * anything does dereference it -- and see whether it is still ours after the
 * thread has been switched out and back many times. A register someone else
 * already set is only watched, never written. */
static void *tpidr_worker(void *arg)
{
    tpidr_run *t = arg;
    t->initial = tpidr_get();
    uint64_t want = t->initial;
    if (t->initial == 0) {
        want = (uint64_t)(uintptr_t)t->block;
        tpidr_set(want);
        t->wrote = true;
    }
    for (unsigned i = 0; i < TPIDR_ROUNDS; i++) {
        pause_briefly(i);
        uint64_t v = tpidr_get();
        t->rounds++;
        if (v != want) {
            if (t->lost++ == 0) t->seen = v;
            if (t->wrote) tpidr_set(want);      /* so later rounds measure afresh */
        }
    }
    if (t->wrote) tpidr_set(t->initial);
    return NULL;
}

static volatile uint64_t g_sig_tpidr;
static volatile sig_atomic_t g_sig_seen;

static void tpidr_on_signal(int sig)
{
    (void)sig;
    g_sig_tpidr = tpidr_get();
    g_sig_seen = 1;
}

/* Signal handlers run Android code too -- crash reporters, and the runtimes
 * that stop threads with signals for garbage collection. Returns false if no
 * signal arrived to measure with. */
static bool tpidr_in_signal(uint64_t want, uint64_t *in_handler, uint64_t *after)
{
    struct sigaction sa, prev;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = tpidr_on_signal;
    sigemptyset(&sa.sa_mask);
    /* SIGWINCH: nothing on iOS has a terminal, so nothing else wants it. */
    sigaction(SIGWINCH, &sa, &prev);
    sigset_t set, old;
    sigemptyset(&set);
    sigaddset(&set, SIGWINCH);
    pthread_sigmask(SIG_UNBLOCK, &set, &old);

    uint64_t initial = tpidr_get();
    g_sig_seen = 0;
    tpidr_set(want);
    pthread_kill(pthread_self(), SIGWINCH);
    for (int i = 0; i < 100 && !g_sig_seen; i++) {
        struct timespec ts = { 0, 1000 * 1000 };
        nanosleep(&ts, NULL);
    }
    *after = tpidr_get();
    tpidr_set(initial);

    pthread_sigmask(SIG_SETMASK, &old, NULL);
    sigaction(SIGWINCH, &prev, NULL);
    *in_handler = g_sig_tpidr;
    return g_sig_seen != 0;
}

static void check_tpidr(tl_json *j)
{
    static const char *title = "Thread register (TPIDR_EL0)";
    tpidr_run runs[TPIDR_THREADS];
    pthread_t threads[TPIDR_THREADS];
    memset(runs, 0, sizeof(runs));

    starting("tpidr");
    uint64_t here = tpidr_get();
    int started = 0;
    for (int i = 0; i < TPIDR_THREADS; i++) {
        if (pthread_create(&threads[i], NULL, tpidr_worker, &runs[i]) == 0) {
            started++;
        } else {
            break;
        }
    }
    for (int i = 0; i < started; i++) {
        pthread_join(threads[i], NULL);
    }
    if (started == 0) {
        result(j, "tpidr", title, "skip", "Could not start a thread to measure with.");
        return;
    }

    uint32_t rounds = 0, lost = 0;
    uint64_t in_use = here, seen = 0;
    for (int i = 0; i < started; i++) {
        rounds += runs[i].rounds;
        if (runs[i].initial && !in_use) in_use = runs[i].initial;
        if (runs[i].lost && !lost) seen = runs[i].seen;
        lost += runs[i].lost;
    }

    if (in_use) {
        result(j, "tpidr", title, "fail",
               "Already in use: it holds 0x%llx before Husk touches it, so the system "
               "owns it. Android code reads its stack guard from it; the guard would "
               "have to live wherever it points, or every read be rewritten.",
               (unsigned long long)in_use);
        return;
    }
    if (lost) {
        result(j, "tpidr", title, "fail",
               "Not kept: set on %d threads, it read back as 0x%llx after being "
               "switched out (%u of %u reads lost). Every read of it in Android code "
               "would have to be rewritten at load time.",
               started, (unsigned long long)seen, lost, rounds);
        return;
    }

    uint64_t block[8] = { 0 };
    uint64_t want = (uint64_t)(uintptr_t)block, in_handler = 0, after = 0;
    if (!tpidr_in_signal(want, &in_handler, &after)) {
        result(j, "tpidr", title, "pass",
               "Free, and kept across %u context switches on %d threads. (No signal "
               "arrived to check it inside a handler.)", rounds, started);
    } else if (in_handler != want || after != want) {
        result(j, "tpidr", title, "warn",
               "Kept across %u context switches, but inside a signal handler it read "
               "0x%llx and afterwards 0x%llx. Android code would run, but its signal "
               "handlers would need the register set on entry.",
               rounds, (unsigned long long)in_handler, (unsigned long long)after);
    } else {
        result(j, "tpidr", title, "pass",
               "Free, and ours: kept across %u context switches on %d threads and "
               "into a signal handler. Android code can read its stack guard from it "
               "as it is.", rounds, started);
    }
}

#else

static void check_tpidr(tl_json *j)
{
    result(j, "tpidr", "Thread register (TPIDR_EL0)", "skip", "Only meaningful on arm64.");
}

#endif

/* ------------------------------------------------------------------ x18 */

#if defined(__APPLE__) && defined(__aarch64__)

/* The compiler never allocates x18 on Apple platforms, so between these asm
 * statements nothing but the kernel can change it. (On Linux it is an ordinary
 * scratch register, which is why this check is Apple-only.) */
static uint32_t x18_run(uint32_t rounds, uint64_t *seen)
{
    const uint64_t canary = 0x4855534B78313821ull;
    uint64_t saved, v;
    uint32_t lost = 0;
    __asm__ volatile("mov %0, x18" : "=r"(saved));
    __asm__ volatile("mov x18, %0" : : "r"(canary));
    for (uint32_t i = 0; i < rounds; i++) {
        pause_briefly(i);
        __asm__ volatile("mov %0, x18" : "=r"(v));
        if (v != canary) {
            if (lost++ == 0) *seen = v;
            __asm__ volatile("mov x18, %0" : : "r"(canary));
        }
    }
    __asm__ volatile("mov x18, %0" : : "r"(saved));
    return lost;
}

static void check_x18(tl_json *j)
{
    static const char *title = "Platform register (x18)";
    uint64_t seen = 0;
    const uint32_t rounds = 300;
    starting("x18");
    uint32_t lost = x18_run(rounds, &seen);
    if (lost == 0) {
        result(j, "x18", title, "pass",
               "Kept across %u context switches. Libraries that keep values in x18 -- "
               "shadow call stacks, some older builds -- can run.", rounds);
    } else {
        result(j, "x18", title, "fail",
               "Changed by the system: read 0x%llx after a switch (%u of %u). Code from "
               "current Android compilers never uses x18, but a library built with "
               "shadow call stacks, or an old one that treats x18 as scratch, would "
               "fail at random.", (unsigned long long)seen, lost, rounds);
    }
}

#else

static void check_x18(tl_json *j)
{
    result(j, "x18", "Platform register (x18)", "skip",
           "Only meaningful on Apple arm64, where the compiler leaves x18 alone.");
}

#endif

/* ------------------------------------------------------ executable memory */

#if defined(__aarch64__)

#if defined(__APPLE__)
/* pthread_jit_write_protect_np is marked unavailable in the iOS SDK, but
 * libsystem exports it; looked up at run time so nothing links against it. */
typedef void (*write_protect_fn)(int);
typedef int  (*write_protect_supported_fn)(void);
static write_protect_fn g_write_protect;

static void jit_prepare(void)
{
    write_protect_supported_fn supported =
        (write_protect_supported_fn)dlsym(RTLD_DEFAULT, "pthread_jit_write_protect_supported_np");
    if (supported && supported()) {
        g_write_protect = (write_protect_fn)dlsym(RTLD_DEFAULT, "pthread_jit_write_protect_np");
    }
}

static void jit_writable(bool on)
{
    if (g_write_protect) g_write_protect(on ? 0 : 1);
}

static void flush_icache(void *p, size_t n) { sys_icache_invalidate(p, n); }

static bool page_executable(void *p)
{
    vm_address_t addr = (vm_address_t)p;
    vm_size_t size = 0;
    natural_t depth = 0;
    vm_region_submap_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
    if (vm_region_recurse_64(mach_task_self(), &addr, &size, &depth,
                             (vm_region_recurse_info_t)&info, &count) != KERN_SUCCESS) {
        return false;
    }
    /* vm_region answers for the region at or after the address. */
    return addr <= (vm_address_t)p && (info.protection & VM_PROT_EXECUTE) != 0;
}

#define JIT_FLAGS (MAP_PRIVATE | MAP_ANON | MAP_JIT)
#else
static void jit_prepare(void) {}
static void jit_writable(bool on) { (void)on; }
static void flush_icache(void *p, size_t n) { __builtin___clear_cache((char *)p, (char *)p + n); }
static bool page_executable(void *p) { (void)p; return true; }
#define JIT_FLAGS (MAP_PRIVATE | MAP_ANON)
#endif

static sigjmp_buf g_guard_jump;
static volatile sig_atomic_t g_guard_armed;
static pthread_t g_guard_thread;
static struct sigaction g_prev_bus, g_prev_segv, g_prev_ill;

static void guard_handler(int sig)
{
    if (g_guard_armed && pthread_equal(pthread_self(), g_guard_thread)) {
        siglongjmp(g_guard_jump, 1);
    }
    /* Not ours: a fault on another thread inside the window. Put back whoever
     * handled it before and return; the instruction faults again and reaches
     * them, as it would have without us. */
    const struct sigaction *prev = sig == SIGBUS ? &g_prev_bus
                                 : sig == SIGSEGV ? &g_prev_segv : &g_prev_ill;
    sigaction(sig, prev, NULL);
}

static void guard_arm(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = guard_handler;
    sigemptyset(&sa.sa_mask);
    g_guard_thread = pthread_self();
    sigaction(SIGBUS, &sa, &g_prev_bus);
    sigaction(SIGSEGV, &sa, &g_prev_segv);
    sigaction(SIGILL, &sa, &g_prev_ill);
    g_guard_armed = 1;
}

static void guard_disarm(void)
{
    g_guard_armed = 0;
    sigaction(SIGBUS, &g_prev_bus, NULL);
    sigaction(SIGSEGV, &g_prev_segv, NULL);
    sigaction(SIGILL, &g_prev_ill, NULL);
}

/* adrp x1, target -- the way compiled code finds its own data. */
static uint32_t adrp_x1(const void *pc, const void *target)
{
    int64_t delta = (int64_t)(((uintptr_t)target & ~(uintptr_t)0xFFF)
                            - ((uintptr_t)pc & ~(uintptr_t)0xFFF)) >> 12;
    uint32_t imm = (uint32_t)delta & 0x1FFFFF;
    return 0x90000000u | ((imm & 3u) << 29) | ((imm >> 2) << 5) | 1u;
}

/* A function that increments the 64-bit counter at `data` and returns it,
 * addressing it exactly as a library addresses its globals. Assembled into
 * `code` for execution at `pc`. `data` must be page-aligned, so the low 12
 * bits are zero and no add is needed. */
static void counter_fn(uint32_t code[5], const void *pc, const void *data)
{
    code[0] = adrp_x1(pc, data);
    code[1] = 0xF9400020u;      /* ldr x0, [x1]      */
    code[2] = 0x91000400u;      /* add x0, x0, #1    */
    code[3] = 0xF9000020u;      /* str x0, [x1]      */
    code[4] = 0xD65F03C0u;      /* ret               */
}

static const uint32_t kReturn7[2] = { 0x528000E0u, 0xD65F03C0u };   /* mov w0, #7; ret */

/*
 * Variant A: take executable memory, then replace one page in the middle of
 * it with ordinary memory. The loader would carve a library's writable pages
 * out of its JIT region this way.
 */
static void check_carve(tl_json *j, size_t page)
{
    static const char *title = "Code beside data, carved from JIT memory";
    starting("carve");
    uint8_t *r = mmap(NULL, 3 * page, PROT_READ | PROT_WRITE | PROT_EXEC, JIT_FLAGS, -1, 0);
    if (r == MAP_FAILED) {
        result(j, "carve", title, "fail", "No executable mapping: %s.", strerror(errno));
        return;
    }
    uint8_t *data = r + page, *tail = r + 2 * page;
    const char *volatile status = "fail";
    char why[200] = "";
    guard_arm();
    if (sigsetjmp(g_guard_jump, 1) == 0) {
        uint32_t fn[5];
        counter_fn(fn, r, data);
        jit_writable(true);
        memcpy(r, fn, sizeof(fn));
        memcpy(tail, kReturn7, sizeof(kReturn7));
        jit_writable(false);
        flush_icache(r, sizeof(fn));
        flush_icache(tail, sizeof(kReturn7));

        void *d = mmap(data, page, PROT_READ | PROT_WRITE,
                       MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
        if (d != data) {
            snprintf(why, sizeof(why), "The kernel would not replace a page inside the "
                     "JIT mapping (%s).", d == MAP_FAILED ? strerror(errno) : "moved");
        } else if (!page_executable(r) || !page_executable(tail)) {
            snprintf(why, sizeof(why), "Replacing one page took execute permission away "
                     "from the pages either side of it.");
        } else {
            *(volatile uint64_t *)data = 41;
            uint64_t got = ((uint64_t (*)(void))(void *)r)();
            int seven = ((int (*)(void))(void *)tail)();
            if (got == 42 && *(volatile uint64_t *)data == 42 && seven == 7) {
                status = "pass";
            } else {
                snprintf(why, sizeof(why), "The code ran but got the wrong answer "
                         "(%llu, %d).", (unsigned long long)got, seven);
            }
        }
    } else {
        snprintf(why, sizeof(why), "Faulted writing or running the code.");
    }
    guard_disarm();
    munmap(r, 3 * page);

    if (!strcmp(status, "pass")) {
        result(j, "carve", title, "pass",
               "A page in the middle of a MAP_JIT mapping was replaced with ordinary "
               "memory. The code before it still ran and wrote to it PC-relatively, "
               "and the code after it still ran: a library's code and data can sit "
               "side by side.");
    } else {
        result(j, "carve", title, "fail", "%s", why);
    }
}

/*
 * Variant B: take ordinary memory, then put an executable page at the front
 * of it. The other way round -- if XNU refuses A, the loader can lay each
 * image out in plain memory and drop code pages into it.
 */
static void check_place(tl_json *j, size_t page)
{
    static const char *title = "Code placed in front of data";
    starting("place");
    uint8_t *r = mmap(NULL, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (r == MAP_FAILED) {
        result(j, "place", title, "fail", "Could not reserve memory: %s.", strerror(errno));
        return;
    }
    uint8_t *data = r + page;
    const char *volatile status = "fail";
    char why[200] = "";

    void *c = mmap(r, page, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_FIXED | JIT_FLAGS, -1, 0);
    if (c != r) {
        snprintf(why, sizeof(why), "The kernel would not map executable memory at a "
                 "chosen address (%s).", c == MAP_FAILED ? strerror(errno) : "moved");
    } else {
        guard_arm();
        if (sigsetjmp(g_guard_jump, 1) == 0) {
            uint32_t fn[5];
            counter_fn(fn, r, data);
            jit_writable(true);
            memcpy(r, fn, sizeof(fn));
            jit_writable(false);
            flush_icache(r, sizeof(fn));
            *(volatile uint64_t *)data = 99;
            if (!page_executable(r)) {
                snprintf(why, sizeof(why), "The page was mapped but is not executable.");
            } else {
                uint64_t got = ((uint64_t (*)(void))(void *)r)();
                if (got == 100 && *(volatile uint64_t *)data == 100) {
                    status = "pass";
                } else {
                    snprintf(why, sizeof(why), "The code ran but got %llu.",
                             (unsigned long long)got);
                }
            }
        } else {
            snprintf(why, sizeof(why), "Faulted writing or running the code.");
        }
        guard_disarm();
    }
    munmap(r, 2 * page);

    if (!strcmp(status, "pass")) {
        result(j, "place", title, "pass",
               "An executable page was mapped at a chosen address in front of "
               "ordinary memory, and its code wrote to that memory PC-relatively.");
    } else {
        result(j, "place", title, "fail", "%s", why);
    }
}

#endif /* __aarch64__ */

static void check_memory(tl_json *j, bool may_execute, size_t page)
{
#if defined(__aarch64__)
    if (may_execute) {
        jit_prepare();
        check_carve(j, page);
        check_place(j, page);
    } else {
        const char *why = "Skipped: this process cannot execute memory it writes right "
                          "now. Enable JIT and run the checks again.";
        result(j, "carve", "Code beside data, carved from JIT memory", "skip", "%s", why);
        result(j, "place", "Code placed in front of data", "skip", "%s", why);
    }
#else
    (void)may_execute;
    (void)page;
    result(j, "carve", "Code beside data, carved from JIT memory", "skip",
           "Only meaningful on arm64.");
    result(j, "place", "Code placed in front of data", "skip", "Only meaningful on arm64.");
#endif
    /* The other source of executable memory, the region a trap-servicing
     * debugger grants, belongs to the allocator inside the QEMU library. */
    result(j, "dualmap", "Code beside data, in debugger-granted memory", "skip",
           "Not measured yet. Where only a trap-servicing debugger grants executable "
           "memory, the region comes from the JIT allocator inside the QEMU library, "
           "and the translation layer cannot borrow it until that allocator is its "
           "own library.");
}

char *husk_tl_run_checks(bool may_execute)
{
    tl_json j;
    tl_json_init(&j);
    tl_json_begin_array(&j);

    long page = sysconf(_SC_PAGESIZE);
    result(&j, "page", "Page size", "info",
           "%ld bytes.%s", page,
           page > 4096 ? " Libraries built for 4 KiB pages are laid out afresh for it; "
                         "each app's report says whether that works." : "");

    check_tpidr(&j);
    check_x18(&j);
    check_memory(&j, may_execute, page > 0 ? (size_t)page : 16384);

    tl_json_end_array(&j);
    return tl_json_finish(&j);
}
