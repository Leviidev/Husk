/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk -- iOS dual-mapped JIT memory for QEMU's TCG.  See husk-ios-jit.h.
 *
 * Derived from AetherPS4-iOS's src/core/ios/ios_jit_allocator.cpp
 * (shadPS4 Emulator Project, GPL-2.0-or-later), ported C++ -> C.
 */

#include "husk-ios-jit.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE

#include <errno.h>
#include <mach/mach.h>
#include <mach/vm_map.h>        /* vm_remap/vm_protect: mach_vm.h is absent from the iOS SDK */
#include <os/log.h>
#include <os/proc.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <libkern/OSCacheControl.h>
#include <sys/ucontext.h>   /* not <ucontext.h>: that one #errors without _XOPEN_SOURCE */
#include <unistd.h>

/* Maximum verbosity by default: this path is nearly impossible to debug after the
 * fact on device, and every line here is printed at most a handful of times per
 * session. Goes to os_log (visible in Console.app) and stderr both. */
static double husk_now_ms(void)
{
    static double base = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
    if (base == 0) { base = t; }
    return t - base;
}

#define HUSK_LOG(fmt, ...)                                                     \
    do {                                                                       \
        os_log(OS_LOG_DEFAULT, "[husk-jit] " fmt, ##__VA_ARGS__);              \
        fprintf(stderr, "[%9.2fms][husk-jit] " fmt "\n",                       \
                husk_now_ms(), ##__VA_ARGS__);                                 \
        fflush(stderr);                                                        \
    } while (0)

/* ------------------------------------------------------------------ symbols */
/*
 * The trap sequences live in husk-brk.S, compiled straight into this binary.
 * Husk deliberately does NOT link or dlopen Stossy11's BreakpointJIT.framework:
 * it has no stated license, and any embedded framework carrying entitlements is
 * rejected by AMFI at launch on sideloaded builds. See husk-brk.S for the
 * verification that our encoding matches theirs exactly.
 */
extern void    *husk_brk_get_jit_mapping(void *addr, size_t len);
extern void     husk_brk_jit_detach(void);
extern uint64_t husk_brk_probe(void);

/*
 * StikDebug answers brk #0x69 by writing a constant into x0. Two encodings are
 * seen in the wild:
 *
 *   0xE0000069  -- the value as written in the script
 *   0x690000E0  -- that value byte-reversed, which is what universal.js actually
 *                  produces: it sends the gdb-remote packet `P0=E0000069`, but P
 *                  takes the register in TARGET byte order, and ARM64 is little
 *                  endian. It also supplies only 4 bytes for an 8-byte register,
 *                  so the upper half keeps whatever poison was there (0xcccccccc
 *                  observed on iPhone18,1 / iOS 27).
 *
 * Matching on an exact value is therefore the wrong test. What actually
 * distinguishes "serviced" from "not serviced" is far simpler: when StikDebug is
 * absent, our own SIGTRAP handler steps over the brk and sets x0 to exactly 0.
 * So any non-zero answer means something serviced the trap.
 */
#define HUSK_PROBE_MAGIC    0xE0000069ull
#define HUSK_PROBE_MAGIC_LE 0x690000E0ull

static atomic_bool           g_jit_available;
static atomic_uint_least64_t g_alloc_counter;

/* ------------------------------------------------------- unserviced-trap guard */
/*
 * Set for exactly the duration of the BreakGetJITMapping call. If StikDebug is
 * not there to service the brk, the trap is a real SIGTRAP that would otherwise
 * kill the process outright -- there is no "call returned an error" to recover
 * from. The handler below turns that into a NULL return, which the allocation
 * path already handles.
 */
static _Thread_local bool g_expecting_jit_trap;

static struct sigaction g_prev_sigtrap;
static struct sigaction g_prev_sigbus;

static void husk_trap_handler(int sig, siginfo_t *info, void *ctx)
{
    (void)info;
    if (g_expecting_jit_trap && ctx != NULL) {
        ucontext_t *uc = (ucontext_t *)ctx;
        /* Step over the brk and report failure in x0, exactly as the
         * StikDebug-serviced path would have written a result there. */
        uc->uc_mcontext->__ss.__pc += 4;
        uc->uc_mcontext->__ss.__x[0] = 0;
        return;
    }

    /* Not ours: restore and re-raise so a genuine breakpoint or bus error is
     * not silently swallowed. */
    struct sigaction *prev = (sig == SIGTRAP) ? &g_prev_sigtrap : &g_prev_sigbus;
    sigaction(sig, prev, NULL);
    raise(sig);
}

void husk_ios_jit_install_trap_handler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = husk_trap_handler;
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGTRAP, &sa, &g_prev_sigtrap) != 0) {
        HUSK_LOG("sigaction(SIGTRAP) failed: %s", strerror(errno));
    }
    if (sigaction(SIGBUS, &sa, &g_prev_sigbus) != 0) {
        HUSK_LOG("sigaction(SIGBUS) failed: %s", strerror(errno));
    }
    HUSK_LOG("trap guard installed (SIGTRAP, SIGBUS)");
}

/* ---------------------------------------------------------------- footprint */
/*
 * phys_footprint is the figure jetsam judges us on, so it is the one worth
 * logging around every large allocation. An iOS app with the
 * increased-memory-limit entitlement still dies silently when this crosses the
 * device's cap, and a silent kill with no crash log is otherwise very hard to
 * tell apart from any other sudden death.
 */
void husk_ios_jit_log_footprint(const char *tag)
{
    task_vm_info_data_t info;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    if (task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&info, &count)
        != KERN_SUCCESS) {
        HUSK_LOG("footprint[%s]: task_info failed", tag ? tag : "");
        return;
    }
    /*
     * os_proc_available_memory() is the number that actually matters: how much
     * more this process may allocate before jetsam kills it. phys_footprint alone
     * says how much we have used but not how close to the edge that is, and the
     * limit varies by device and by whether the increased-memory-limit entitlement
     * is honoured. A jetsam kill is a SIGKILL -- no handler runs and the log simply
     * stops -- so the only way to see it coming is to watch this fall.
     */
    size_t avail = os_proc_available_memory();

    HUSK_LOG("footprint[%s]: phys=%.1f MiB  resident=%.1f MiB  "
             "available-before-jetsam=%.1f MiB",
             tag ? tag : "",
             info.phys_footprint / (1024.0 * 1024.0),
             info.resident_size  / (1024.0 * 1024.0),
             avail / (1024.0 * 1024.0));
}

size_t husk_ios_available_memory(void)
{
    return os_proc_available_memory();
}

/* ---------------------------------------------------------------- self-test */
/*
 * The decisive check. StikDebug can hand back an address, and vm_remap can
 * succeed, while the pages were never actually prepared -- `_M,rx` allocating
 * without prepare_memory_region walking each page looks identical from here.
 * In that case nothing goes wrong until TCG branches into generated code, and
 * the app dies somewhere deep in the emulator with no useful context.
 *
 * So: write a two-instruction function through the RW alias, then CALL it
 * through the RX alias. If JIT works at all, this returns 42.
 */
static bool husk_jit_selftest(const HuskDualMapping *m)
{
    /* movz w0, #42  ;  ret */
    static const uint32_t kCode[2] = { 0x52800540u, 0xD65F03C0u };

    if (m->rw_addr == NULL || m->rx_addr == NULL) {
        return false;
    }

    HUSK_LOG("selftest: writing %zu bytes through RW alias %p", sizeof(kCode),
             (void *)m->rw_addr);
    memcpy(m->rw_addr, kCode, sizeof(kCode));

    /* Flush the write through to the RX view before executing it. */
    sys_icache_invalidate(m->rx_addr, sizeof(kCode));
    HUSK_LOG("selftest: icache invalidated on RX alias %p", (void *)m->rx_addr);

    /* Read back through RX to confirm the two aliases really share pages. */
    uint32_t readback[2];
    memcpy(readback, m->rx_addr, sizeof(readback));
    if (readback[0] != kCode[0] || readback[1] != kCode[1]) {
        HUSK_LOG("selftest: FAIL -- RX alias does not reflect RW writes "
                 "(read 0x%08x 0x%08x, expected 0x%08x 0x%08x). The two mappings "
                 "are not backed by the same pages.",
                 readback[0], readback[1], kCode[0], kCode[1]);
        return false;
    }
    HUSK_LOG("selftest: RX alias reflects RW writes (0x%08x 0x%08x) -- aliasing OK",
             readback[0], readback[1]);

    HUSK_LOG("selftest: CALLING generated code at %p ...", (void *)m->rx_addr);
    int (*fn)(void) = (int (*)(void))(void *)m->rx_addr;
    int result = fn();

    if (result != 42) {
        HUSK_LOG("selftest: FAIL -- generated code ran but returned %d, expected 42",
                 result);
        return false;
    }
    HUSK_LOG("selftest: PASS -- executed generated code from the RX alias, got 42. "
             "JIT is genuinely live.");
    return true;
}

/* ------------------------------------------------------------- allocation */

/*
 * Husk: take the JIT region early, before anything slow happens.
 *
 * StikDebug does not stay attached indefinitely. A first run downloads about
 * 1.1 GB of guest image before QEMU starts, and by the time qemu_init() reached
 * alloc_code_gen_buffer the debugger had let go:
 *
 *   StikDebug is NOT servicing traps -- probe returned 0
 *   could not obtain 268435456 bytes of JIT memory
 *
 * Nothing can recover from that in-process: without a debugger there is no way
 * to get executable memory, and asking again later is exactly what does not
 * work. So the region is claimed at app launch, while the attachment is fresh,
 * and held until QEMU asks for it.
 */
static HuskDualMapping husk_ios_jit_allocate_real(size_t bytes);

static HuskDualMapping husk_prewarmed;
static bool husk_prewarm_done;

HUSK_EXPORT bool husk_ios_jit_prewarm(size_t bytes)
{
    if (husk_prewarm_done) {
        return husk_prewarmed.rw_addr != NULL;
    }
    husk_prewarm_done = true;
    husk_prewarmed = husk_ios_jit_allocate_real(bytes);
    fprintf(stderr, "[husk-jit] prewarm %s: %zu bytes\n",
            husk_prewarmed.rw_addr ? "OK" : "FAILED", bytes);
    return husk_prewarmed.rw_addr != NULL;
}

HuskDualMapping husk_ios_jit_allocate(size_t bytes)
{
    /*
     * Hand back the prewarmed region when it is big enough. QEMU asks for
     * exactly tb-size, which is what prewarm was given, so this is the normal
     * path -- the fallback below only runs if prewarm never happened.
     */
    if (husk_prewarmed.rw_addr && husk_prewarmed.size >= bytes) {
        fprintf(stderr, "[husk-jit] using the prewarmed region (%zu bytes)\n",
                husk_prewarmed.size);
        return husk_prewarmed;
    }
    return husk_ios_jit_allocate_real(bytes);
}

static HuskDualMapping husk_ios_jit_allocate_real(size_t bytes)
{
    HuskDualMapping region = { NULL, NULL, 0 };
    uint64_t n = atomic_fetch_add(&g_alloc_counter, 1) + 1;

    /* Cheap attach probe: brk #0x69 is answered with a constant when StikDebug
     * is live, and swallowed by our trap handler as 0 when it is not. Doing this
     * first turns "no debugger" into a clean diagnostic instead of three slow
     * retries of the real request. */
    g_expecting_jit_trap = true;
    uint64_t probe = husk_brk_probe();
    g_expecting_jit_trap = false;
    if (probe == 0) {
        HUSK_LOG("#%llu: StikDebug is NOT servicing traps -- probe returned 0, which "
                 "is our own SIGTRAP handler stepping over an unanswered brk. "
                 "Cannot allocate %zu bytes.",
                 (unsigned long long)n, bytes);
        return region;
    }

    uint32_t probe_lo = (uint32_t)probe;
    if (probe_lo == (uint32_t)HUSK_PROBE_MAGIC ||
        probe_lo == (uint32_t)HUSK_PROBE_MAGIC_LE) {
        HUSK_LOG("#%llu: StikDebug attach probe OK (0x%llx)",
                 (unsigned long long)n, (unsigned long long)probe);
    } else {
        /* Serviced, but by something that answers differently. Proceed -- the
         * allocation itself is the real test -- and record the value so an
         * unfamiliar StikDebug build is identifiable from the log alone. */
        HUSK_LOG("#%llu: trap was serviced but the answer is unrecognised (0x%llx). "
                 "Continuing anyway; the allocation below is the real test.",
                 (unsigned long long)n, (unsigned long long)probe);
    }

    /*
     * Ask for a FRESH region (x0 == 0) so StikDebug allocates it with
     * debugserver `_M<size>,rx` and then walks every 16 KiB page with
     * `M<addr>,1:69`. Requesting a fresh region is the only branch that gets
     * those pages prepared; handing in an address we allocated ourselves does not.
     *
     * StikDebug can be momentarily unresponsive (busy, or briefly suspended by
     * iOS) rather than permanently gone, and the two are indistinguishable from
     * here, so retry a few times before giving up.
     */
    enum { kMaxAttempts = 3 };
    void *rx = NULL;
    for (int attempt = 1; attempt <= kMaxAttempts; attempt++) {
        HUSK_LOG("#%llu: requesting execute-capable region, size=%zu (%.1f MiB), "
                 "attempt %d/%d",
                 (unsigned long long)n, bytes, (double)bytes / (1024.0 * 1024.0),
                 attempt, kMaxAttempts);

        g_expecting_jit_trap = true;
        rx = husk_brk_get_jit_mapping(NULL, bytes);
        g_expecting_jit_trap = false;

        HUSK_LOG("#%llu: BreakGetJITMapping returned %p", (unsigned long long)n, rx);
        if (rx != NULL) {
            break;
        }
        if (attempt < kMaxAttempts) {
            usleep(50 * 1000);
        }
    }

    if (rx == NULL) {
        HUSK_LOG("#%llu: FAILED after %d attempts. StikDebug must be attached with "
                 "the Universal JIT script BEFORE the guest is started.",
                 (unsigned long long)n, kMaxAttempts);
        return region;
    }

    /* Writable alias of the same physical pages. Purely local -- no debugger
     * involvement, and therefore still available after detach. */
    vm_address_t rw = 0;
    vm_prot_t cur_prot = VM_PROT_NONE, max_prot = VM_PROT_NONE;
    kern_return_t kr = vm_remap(mach_task_self(), &rw, (vm_size_t)bytes,
                                /*mask=*/0, VM_FLAGS_ANYWHERE,
                                mach_task_self(), (vm_address_t)rx,
                                /*copy=*/FALSE, &cur_prot, &max_prot,
                                VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        HUSK_LOG("#%llu: vm_remap failed for rx=%p size=%zu: %d (%s)",
                 (unsigned long long)n, rx, bytes, (int)kr, mach_error_string(kr));
        return region;
    }

    kr = vm_protect(mach_task_self(), rw, (vm_size_t)bytes, /*set_maximum=*/FALSE,
                    VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS) {
        HUSK_LOG("#%llu: vm_protect(RW) failed for rw=%p size=%zu: %d (%s)",
                 (unsigned long long)n, (void *)rw, bytes, (int)kr,
                 mach_error_string(kr));
        vm_deallocate(mach_task_self(), rw, (vm_size_t)bytes);
        return region;
    }

    region.rw_addr = (uint8_t *)rw;
    region.rx_addr = (uint8_t *)rx;
    region.size    = bytes;
    atomic_store(&g_jit_available, true);

    HUSK_LOG("#%llu: dual mapping established: rw=%p rx=%p size=%zu diff=%+lld",
             (unsigned long long)n, (void *)region.rw_addr, (void *)region.rx_addr,
             region.size, (long long)(region.rx_addr - region.rw_addr));
    husk_ios_jit_log_footprint("after-jit-alloc");

    if (!husk_jit_selftest(&region)) {
        HUSK_LOG("#%llu: SELFTEST FAILED -- the region is not usable as JIT memory. "
                 "Refusing to hand it to TCG; QEMU would crash on its first "
                 "generated block instead of failing here.",
                 (unsigned long long)n);
        husk_ios_jit_release(&region);
        return region;
    }

    return region;
}

void husk_ios_jit_release(HuskDualMapping *m)
{
    if (m == NULL) {
        return;
    }
    if (m->rw_addr != NULL) {
        vm_deallocate(mach_task_self(), (vm_address_t)m->rw_addr, (vm_size_t)m->size);
        m->rw_addr = NULL;
    }
    if (m->rx_addr != NULL) {
        vm_deallocate(mach_task_self(), (vm_address_t)m->rx_addr, (vm_size_t)m->size);
        m->rx_addr = NULL;
    }
    m->size = 0;
}

void husk_ios_jit_detach(void)
{
    HUSK_LOG("detaching debugger; RX mappings persist after this");
    g_expecting_jit_trap = true;
    husk_brk_jit_detach();
    g_expecting_jit_trap = false;
    HUSK_LOG("debugger detached");
}

bool husk_ios_jit_is_available(void)
{
    return atomic_load(&g_jit_available);
}

#else /* !iOS: keep the symbols so host builds and tests link */

void husk_ios_jit_install_trap_handler(void) {}
static HuskDualMapping husk_ios_jit_allocate_real(size_t bytes)
{
    (void)bytes;
    HuskDualMapping m = { NULL, NULL, 0 };
    return m;
}
HuskDualMapping husk_ios_jit_allocate(size_t bytes)
{
    return husk_ios_jit_allocate_real(bytes);
}
bool husk_ios_jit_prewarm(size_t bytes) { (void)bytes; return false; }
void husk_ios_jit_release(HuskDualMapping *m) { (void)m; }
void husk_ios_jit_detach(void) {}
bool husk_ios_jit_is_available(void) { return false; }
void husk_ios_jit_log_footprint(const char *tag) { (void)tag; }
size_t husk_ios_available_memory(void) { return 0; }

#endif
