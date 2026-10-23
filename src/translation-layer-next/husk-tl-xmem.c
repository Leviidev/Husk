/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-xmem.h"

#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#if TARGET_OS_OSX
#include <mach/mach_vm.h>
#endif
#endif

#include "husk-tl-internal.h"

void tl_log_line(const char *fmt, ...);

static struct {
    uint8_t *rx, *rw;
    size_t size, used;
    bool open, shared;
    pthread_mutex_t lock;
} g_x = { .lock = PTHREAD_MUTEX_INITIALIZER };

#if defined(__APPLE__)
/*
 * Executable memory a process makes for itself, on a device that does not need a debugger to hand it out: one where the kernel lets a process that is marked as debugged
 * (CS_DEBUGGED) run pages it wrote. That is what jailbreak environments provide, what TrollStore's enable-jit does
 * (its root helper attaches with ptrace and lets go, leaving the process in the allow-invalid/debugged state), and what a debugger attach does on iOS versions where that state is sufficient for JIT:
 * an anonymous mapping allocated with RWX max protection, a read+write alias made with vm_remap, and the executable mapping demoted to read+execute via vm_protect
 * to work around W^X restrictions.
 *
 * The two views sit side by side, the alias right after the executable one, because the loader reaches one from the other with adrp (+-4 GiB). Nothing is trusted until a
 * function written through the alias has been called through the executable view; a guard turns a refusal into a failure rather than a crash.
 */
extern int csops(pid_t pid, unsigned int ops, void *useraddr, size_t usersize);
#define TL_CS_OPS_STATUS 0
#define TL_CS_DEBUGGED   0x10000000u

static sigjmp_buf g_self_jump;
static volatile sig_atomic_t g_self_running;
static void self_fault(int sig)
{
    if (g_self_running) siglongjmp(g_self_jump, 1);
    signal(sig, SIG_DFL);
    raise(sig);
}

/* movz w0, #0x5a5a ; ret */
static bool self_selftest(uint8_t *rx, uint8_t *rw)
{
    static const uint32_t code[2] = { 0x528B4B40u, 0xD65F03C0u };
    vm_address_t addr = (vm_address_t)rx; vm_size_t sz = 0; natural_t depth = 0;
    vm_region_submap_info_data_64_t info; mach_msg_type_number_t cnt = VM_REGION_SUBMAP_INFO_COUNT_64;
    if (vm_region_recurse_64(mach_task_self(), &addr, &sz, &depth, (vm_region_recurse_info_t)&info, &cnt) != KERN_SUCCESS || !(info.protection & VM_PROT_EXECUTE)) return false;
    struct sigaction sa, ob, os, oi;
    memset(&sa, 0, sizeof(sa)); sa.sa_handler = self_fault; sigemptyset(&sa.sa_mask);
    sigaction(SIGBUS, &sa, &ob); sigaction(SIGSEGV, &sa, &os); sigaction(SIGILL, &sa, &oi);
    bool ok = false;
    g_self_running = 1;
    if (sigsetjmp(g_self_jump, 1) == 0) {
        memcpy(rw, code, sizeof(code));
        sys_icache_invalidate(rx, sizeof(code));
        ok = ((int (*)(void))(void *)rx)() == 0x5a5a;
    }
    g_self_running = 0;
    sigaction(SIGBUS, &ob, NULL); sigaction(SIGSEGV, &os, NULL); sigaction(SIGILL, &oi, NULL);
    return ok;
}

static bool grant_self(size_t host_bytes, char *err, size_t errlen)
{
    uint32_t flags = 0;
    bool debugged = csops(getpid(), TL_CS_OPS_STATUS, &flags, sizeof(flags)) == 0 && (flags & TL_CS_DEBUGGED);
    static const int how[2][2] = { { PROT_READ | PROT_WRITE | PROT_EXEC, 0 }, { PROT_READ | PROT_WRITE | PROT_EXEC, MAP_JIT } };
    int last_errno = 0;
    for (size_t size = (host_bytes + TL_XMEM_PAGE - 1) & ~(size_t)(TL_XMEM_PAGE - 1); size >= (64u << 20); size /= 2) {
        uint8_t *base = mmap(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (base == MAP_FAILED) { last_errno = errno; continue; }
        for (int h = 0; h < 2; h++) {
            uint8_t *rx = mmap(base, size, how[h][0], MAP_FIXED | MAP_PRIVATE | MAP_ANON | how[h][1], -1, 0);
            if (rx == MAP_FAILED) { last_errno = errno; continue; }
            vm_address_t alias = (vm_address_t)(uintptr_t)(base + size);
            vm_prot_t cur, max;
            kern_return_t kr = vm_remap(mach_task_self(), &alias, size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(), (vm_address_t)(uintptr_t)rx, FALSE, &cur, &max, VM_INHERIT_NONE);
            if (kr != KERN_SUCCESS ||
                vm_protect(mach_task_self(), alias, size, FALSE, VM_PROT_READ | VM_PROT_WRITE) != KERN_SUCCESS ||
                vm_protect(mach_task_self(), (vm_address_t)(uintptr_t)rx, size, FALSE, VM_PROT_READ | VM_PROT_EXECUTE) != KERN_SUCCESS) {
                last_errno = EPERM;
                mmap(base, size * 2, PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
                continue;
            }
            if (self_selftest(rx, (uint8_t *)(uintptr_t)alias)) {
                g_x.rx = rx; g_x.rw = (uint8_t *)(uintptr_t)alias; g_x.size = size;
                tl_log_line("xmem: made its own %zu MiB executable region (%s mapping, rx %p, rw %p, process %s debugged)", size >> 20, h ? "MAP_JIT" : "plain", (void *)rx, (void *)alias, debugged ? "is" : "is not");
                return true;
            }
            last_errno = EPERM;
            munmap(base, size * 2);
            base = mmap(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
            if (base == MAP_FAILED) break;
        }
        if (base != MAP_FAILED) munmap(base, size * 2);
    }
    snprintf(err, errlen, "this process cannot make executable memory for itself (%s%s). It needs JIT: a jailbreak with JIT allowed for apps, TrollStore's enable-jit, or a debugger",
             last_errno ? strerror(last_errno) : "refused", debugged ? "" : "; it is not marked as debugged");
    return false;
}
#endif

#if defined(__APPLE__) && !TARGET_OS_OSX
/* The loader retargets code at the writable view with adrp (+-4 GiB). A region whose writable alias the kernel placed anywhere
 * (VM_FLAGS_ANYWHERE: hundreds of GiB away on iOS 27) gets a second alias of the same pages next to the executable view: a local
 * vm_remap, which needs no debugger. Free addresses after the region, then before it, in 16 MiB steps out to 2 GiB. */
static bool near(uintptr_t a, uintptr_t rx) { intptr_t d = (intptr_t)(a - rx); return d < ((intptr_t)2 << 30) && d > -((intptr_t)2 << 30); }
static bool adopt_alias(vm_address_t a, uintptr_t rx, size_t size, const char *how, ptrdiff_t was)
{
    if (!near(a, rx)) { vm_deallocate(mach_task_self(), a, size); return false; }
    kern_return_t kr = vm_protect(mach_task_self(), a, size, FALSE, VM_PROT_READ | VM_PROT_WRITE);
    if (kr != KERN_SUCCESS) { tl_log_line("xmem: %s alias at %p: vm_protect rw failed (%d)", how, (void *)a, kr); vm_deallocate(mach_task_self(), a, size); return false; }
    tl_log_line("xmem: the writable view was %td MiB from the executable one; a new one (%s) at %p is %td MiB from it", was >> 20, how, (void *)a, (ptrdiff_t)(a - rx) >> 20);
    g_x.rw = (uint8_t *)a;
    return true;
}
static void near_alias(void)
{
    ptrdiff_t d = g_x.rw - g_x.rx;
    if (d < ((ptrdiff_t)2 << 30) && d > -((ptrdiff_t)2 << 30)) return;
    const uintptr_t step = 16u << 20, reach = (uintptr_t)2 << 30, rx = (uintptr_t)g_x.rx, size = g_x.size;
    task_t me = mach_task_self();
    vm_prot_t cur, max;
    /* what is around the region, for the log */
    {
        vm_address_t a = rx; vm_size_t sz = 0; natural_t depth = 0; vm_region_submap_info_data_64_t info; mach_msg_type_number_t cnt = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(me, &a, &sz, &depth, (vm_region_recurse_info_t)&info, &cnt) == KERN_SUCCESS)
            tl_log_line("xmem: region rx %p size %zu MiB; the map entry there is %p..%p prot %d/%d tag %u", (void *)rx, size >> 20, (void *)a, (void *)(a + sz), info.protection, info.max_protection, info.user_tag);
        a = rx + size; sz = 0; depth = 0; cnt = VM_REGION_SUBMAP_INFO_COUNT_64;
        if (vm_region_recurse_64(me, &a, &sz, &depth, (vm_region_recurse_info_t)&info, &cnt) == KERN_SUCCESS)
            tl_log_line("xmem: next entry after the region: %p..%p prot %d/%d tag %u", (void *)a, (void *)(a + sz), info.protection, info.max_protection, info.user_tag);
    }
    /* 1. the kernel's own search, upward from just after the region and from 2 GiB below it */
    uintptr_t hints[2] = { rx + size, rx > reach ? rx - reach : 0 };
    for (int h = 0; h < 2; h++) {
        vm_address_t a = hints[h];
        kern_return_t kr = vm_remap(me, &a, size, 0, VM_FLAGS_ANYWHERE, me, rx, FALSE, &cur, &max, VM_INHERIT_NONE);
        if (kr != KERN_SUCCESS) { tl_log_line("xmem: remap searching from %p failed (%d)", (void *)hints[h], kr); continue; }
        if (adopt_alias(a, rx, size, h ? "searched from below" : "searched from after", d)) return;
        tl_log_line("xmem: remap searching from %p landed at %p, too far", (void *)hints[h], (void *)a);
    }
    /* 2. fixed addresses after, then before, in 16 MiB steps out to 2 GiB; and when no gap near the region holds all of it (a
     *    region placed low in the address space, between other mappings), the largest part of it that fits: the region is used
     *    from its start, so a shorter alias of its first part is a shorter region. */
    kern_return_t first = KERN_SUCCESS;
    for (uintptr_t want_size = size; want_size >= (64u << 20); want_size -= (32u << 20)) {
        for (uintptr_t off = 0; off <= reach; off += step) {
            for (int side = 0; side < 2; side++) {
                if (side == 1 && rx < want_size + off + step) continue;
                vm_address_t want = side == 0 ? rx + size + off : rx - want_size - off;
                kern_return_t kr = vm_remap(me, &want, want_size, 0, VM_FLAGS_FIXED, me, rx, FALSE, &cur, &max, VM_INHERIT_NONE);
                if (kr != KERN_SUCCESS) { if (first == KERN_SUCCESS) first = kr; continue; }
                if (adopt_alias(want, rx, want_size, want_size == size ? "fixed" : "fixed, shorter", d)) {
                    if (want_size < size) { tl_log_line("xmem: the region is used as %zu MiB of its %zu", (size_t)(want_size >> 20), (size_t)(size >> 20)); g_x.size = want_size; }
                    return;
                }
            }
        }
    }
    tl_log_line("xmem: fixed remaps near the region failed (first error %d)", first);
    /* 3. a reservation first (plain memory, a hint near the region), then the alias over it */
    for (int h = 0; h < 2; h++) {
        void *res = mmap((void *)hints[h], size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (res == MAP_FAILED) { tl_log_line("xmem: reservation near %p failed (%d)", (void *)hints[h], errno); continue; }
        if (!near((uintptr_t)res, rx)) { tl_log_line("xmem: reservation near %p landed at %p, too far", (void *)hints[h], res); munmap(res, size); continue; }
        vm_address_t a = (vm_address_t)res;
        kern_return_t kr = vm_remap(me, &a, size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, me, rx, FALSE, &cur, &max, VM_INHERIT_NONE);
        if (kr != KERN_SUCCESS) { tl_log_line("xmem: alias over the reservation at %p failed (%d)", res, kr); munmap(res, size); continue; }
        if (adopt_alias(a, rx, size, "over a reservation", d)) return;
    }
    tl_log_line("xmem: the writable view is %td MiB from the executable one and no address near it is free", d >> 20);
}

/* The phone: StikDebug's region if a debugger granted one, otherwise one the process makes for itself where the device allows that (before TXM). */
static bool open_platform(size_t host_bytes, char *err, size_t errlen)
{
    tl_dual_mapping *m = tl_find_stikdebug_prewarmed();
    if (m && m->rw_addr && m->rx_addr) {
        g_x.rx = m->rx_addr;
        g_x.rw = m->rw_addr;
        g_x.size = m->size;
        g_x.shared = true;
        near_alias();
        return true;
    }
    /* From iOS 26 the Trusted Execution Monitor decides what may execute, and an attempt to run unblessed memory can end the process rather than fail. Only a debugger
     * servicing traps can grant memory there, so nothing is tried. */
    if (__builtin_available(iOS 26.0, *)) {
        snprintf(err, errlen, "no StikDebug JIT region: this iOS enforces TXM, where only a debugger (StikDebug or Built-in StikJIT) can grant executable memory");
        return false;
    }
    return grant_self(host_bytes, err, errlen);
}
#else
/* A Mac (or a plain host): two views of ordinary memory. */
static bool open_platform(size_t host_bytes, char *err, size_t errlen)
{
    /* TL_XMEM_MIB: size the region as a phone's would be, to see whether a game fits there */
    if (getenv("TL_XMEM_MIB")) host_bytes = (size_t)atoi(getenv("TL_XMEM_MIB")) << 20;
#if defined(__APPLE__)
    /* TL_XMEM_SELF=1: make the region the way a jailbroken or TrollStore phone does, to test that path here */
    if (getenv("TL_XMEM_SELF")) return grant_self(host_bytes, err, errlen);
#endif
    size_t size = (host_bytes + TL_XMEM_PAGE - 1) & ~(size_t)(TL_XMEM_PAGE - 1);
    /* Reserve twice the span and put the two views side by side. adrp reaches +-4 GiB, and
     * the loader retargets code at the writable view with it, so the views must stay close:
     * left to place the alias anywhere, the kernel can put it more than 3 GiB away. */
    uint8_t *base = mmap(NULL, size * 2, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (base == MAP_FAILED) {
        snprintf(err, errlen, "mmap of %zu MiB failed: %s", (size * 2) >> 20, strerror(errno));
        return false;
    }
    /* the executable view first and the writable one after it, as on a phone: code that decodes its own adrp to find a slot
     * (Mono's PLT entries, tramp-arm64.c) takes the displacement as positive */
    uint8_t *rw = base + size;
    if (mprotect(rw, size, PROT_READ | PROT_WRITE) != 0) {
        snprintf(err, errlen, "mprotect(RW) failed: %s", strerror(errno));
        munmap(base, size * 2);
        return false;
    }
#if defined(__APPLE__)
    mach_vm_address_t alias = (mach_vm_address_t)(uintptr_t)base;
    vm_prot_t cur, max;
    kern_return_t kr = mach_vm_remap(mach_task_self(), &alias, size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                                     mach_task_self(), (mach_vm_address_t)(uintptr_t)rw, FALSE,
                                     &cur, &max, VM_INHERIT_NONE);
    if (kr != KERN_SUCCESS) {
        snprintf(err, errlen, "mach_vm_remap failed (%d)", kr);
        munmap(base, size * 2);
        return false;
    }
    if (mprotect((void *)(uintptr_t)alias, size, PROT_READ | PROT_EXEC) != 0) {
        snprintf(err, errlen, "mprotect(R|X) on the alias failed: %s", strerror(errno));
        munmap(base, size * 2);
        return false;
    }
    g_x.rx = (uint8_t *)(uintptr_t)alias;
#else
    snprintf(err, errlen, "no dual mapping on this host");
    munmap(base, size * 2);
    return false;
#endif
    g_x.rw = rw;
    g_x.size = size;
    return true;
}
#endif

bool tl_xmem_open(size_t host_bytes, char *err, size_t errlen)
{
    bool ok = true;
    pthread_mutex_lock(&g_x.lock);
    if (!g_x.open) {
        char local[160] = "";
        ok = open_platform(host_bytes, local, sizeof(local));
        if (ok) {
            g_x.open = true;
        } else if (err && errlen) {
            snprintf(err, errlen, "%s", local);
        }
    }
    pthread_mutex_unlock(&g_x.lock);
    return ok;
}

static bool (*g_reserve)(size_t);
static void find_reserve(void) { g_reserve = (bool (*)(size_t))dlsym(RTLD_DEFAULT, "husk_ios_jit_reserve_bottom"); }

/* Takes the region up to `end` from the bottom, unless QEMU holds that part of it (husk-ios-jit.c). No QEMU, no limit. */
bool tl_jit_reserve_bottom(size_t end)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, find_reserve);
    return !g_reserve || g_reserve(end);
}

bool tl_xmem_alloc(size_t bytes, uint8_t **rx, uint8_t **rw)
{
    size_t n = (bytes + TL_XMEM_PAGE - 1) & ~(size_t)(TL_XMEM_PAGE - 1);
    bool ok = false;
    pthread_mutex_lock(&g_x.lock);
    /* StikDebug's region is QEMU's too: Android, when it runs, holds the top of it (husk-ios-jit.c) */
    if (g_x.open && n <= g_x.size - g_x.used && (!g_x.shared || tl_jit_reserve_bottom(g_x.used + n))) {
        if (rx) *rx = g_x.rx + g_x.used;
        if (rw) *rw = g_x.rw + g_x.used;
        g_x.used += n;
        ok = true;
    }
    pthread_mutex_unlock(&g_x.lock);
    return ok;
}

ptrdiff_t tl_xmem_delta(void) { return g_x.rw - g_x.rx; }
bool tl_xmem_near(const void *p, size_t dist)
{
    if (!g_x.open || !p) return false;
    uintptr_t a = (uintptr_t)p, lo = (uintptr_t)g_x.rx, hi = lo + g_x.size;
    return a + dist >= lo && a <= hi + dist;
}

bool tl_xmem_contains(const void *p)
{
    const uint8_t *b = p;
    return g_x.open && ((b >= g_x.rx && b < g_x.rx + g_x.size)
                     || (b >= g_x.rw && b < g_x.rw + g_x.size));
}

bool tl_xmem_is_rx(const void *p)
{
    const uint8_t *b = p;
    return g_x.open && b >= g_x.rx && b < g_x.rx + g_x.size;
}

void tl_xmem_flush(const void *rx, size_t bytes)
{
#if defined(__APPLE__)
    sys_icache_invalidate((void *)rx, bytes);
#else
    __builtin___clear_cache((char *)rx, (char *)rx + bytes);
#endif
}

size_t tl_xmem_used(void) { return g_x.used; }
size_t tl_xmem_size(void)  { return g_x.size; }
