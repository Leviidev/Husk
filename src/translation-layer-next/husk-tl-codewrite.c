/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Writing to code, for guests that patch it: Geode, which hooks Geometry Dash by overwriting the first instructions of its
 * functions and building trampolines in executable memory it maps for itself.
 *
 * On a phone, memory can be executable or writable, never both: every library's code is in the executable view of the JIT
 * region (husk-tl-xmem.c), and the same pages are writable only through the second view, a fixed distance away. A hooking
 * library does not know that; it asks mprotect for write access and then stores straight into the code. So:
 *
 *   - mprotect on the JIT region says yes and changes nothing (the real permissions must stay as they are);
 *   - mmap for executable memory hands out a piece of the JIT region;
 *   - a store into the executable view faults, and the fault handler performs it through the writable view -- the store
 *     instruction is decoded from the faulting PC, its value read from the saved registers -- flushes the instruction cache
 *     for the bytes it changed, and resumes after the instruction.
 *
 * Patching happens a few hundred times while a mod loader starts, so a fault per store costs nothing that shows. Off unless
 * a driver turns it on (tl_codewrite_enable), so no other game's crash ever passes through here.
 */
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "husk-tl-codewrite.h"

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/ucontext.h>

#include "husk-tl-bionic.h"
#include "husk-tl-xmem.h"

static volatile bool g_on;
static struct sigaction g_prev_bus, g_prev_segv;
static volatile long g_emulated;

bool tl_codewrite_enabled(void) { return g_on; }
long tl_codewrite_count(void) { return g_emulated; }

static int64_t sext(uint64_t v, int bits) { return (int64_t)(v << (64 - bits)) >> (64 - bits); }

/* A general register as a store sees it: 31 is the zero register for the value, the stack pointer for the base. */
static uint64_t xreg(const struct __darwin_arm_thread_state64 *ss, unsigned r, bool sp)
{
    if (r == 31) return sp ? ss->__sp : 0;
    if (r == 29) return ss->__fp;
    if (r == 30) return ss->__lr;
    return ss->__x[r];
}
static void set_xreg(struct __darwin_arm_thread_state64 *ss, unsigned r, uint64_t v)
{
    if (r == 31) ss->__sp = v;
    else if (r == 29) ss->__fp = v;
    else if (r == 30) ss->__lr = v;
    else ss->__x[r] = v;
}

/* Store `bytes` of a register's value at the executable address `addr`, through the writable view. */
static bool put(uint64_t addr, const void *value, unsigned bytes)
{
    if (!tl_xmem_is_rx((const void *)addr) || !tl_xmem_is_rx((const void *)(addr + bytes - 1))) return false;
    memcpy((void *)(addr + (uint64_t)tl_xmem_delta()), value, bytes);
    tl_xmem_flush((const void *)addr, bytes);
    return true;
}

static void reg_bytes(const mcontext_t mc, bool simd, unsigned r, unsigned bytes, uint8_t out[16])
{
    memset(out, 0, 16);
    if (simd) memcpy(out, &mc->__ns.__v[r], bytes);
    else { uint64_t v = xreg(&mc->__ss, r, false); memcpy(out, &v, bytes > 8 ? 8 : bytes); }
}

/* The store at the faulting PC, done through the writable view. False if it is not a store this understands. */
static bool emulate(mcontext_t mc)
{
    struct __darwin_arm_thread_state64 *ss = &mc->__ss;
    uint32_t insn = *(const uint32_t *)ss->__pc;
    uint8_t buf[16];

    /* DC ZVA: memset zeroes whole blocks this way. */
    if ((insn & 0xFFFFFFE0u) == 0xD50B7420u) {
        uint64_t dczid;
        __asm__ volatile("mrs %0, dczid_el0" : "=r"(dczid));
        unsigned block = 4u << (dczid & 0xF);
        uint64_t a = xreg(ss, insn & 31, false) & ~(uint64_t)(block - 1);
        static const uint8_t zero[256];
        for (unsigned done = 0; done < block; done += 16) if (!put(a + done, zero, 16)) return false;
        return true;
    }

    unsigned rt = insn & 31, rn = (insn >> 5) & 31;
    bool simd = (insn >> 26) & 1;

    /* Atomics, as compiled for the C++ runtime's std::atomic and Dart's BSS set-up. A fault means the address is in the executable
     * view, which only this thread is writing at that moment in practice (start-up initialisation), so the store is done plainly. */
    {
        unsigned bytes = 1u << (insn >> 30);
        uint64_t a = xreg(ss, rn, true);
        if ((insn & 0x3FE07C00u) == 0x08007C00u) {                 /* STXR / STLXR Ws, Rt, [Xn]: store, and report success in Ws */
            reg_bytes(mc, false, rt, bytes, buf);
            if (!put(a, buf, bytes)) return false;
            set_xreg(ss, (insn >> 16) & 31, 0);
            return true;
        }
        if ((insn & 0x3FFFFC00u) == 0x089FFC00u) {                 /* STLR Rt, [Xn] */
            reg_bytes(mc, false, rt, bytes, buf);
            return put(a, buf, bytes);
        }
        if ((insn & 0x3FA07C00u) == 0x08A07C00u) {                 /* CAS{A}{L} Rs, Rt, [Xn]: store Rt if [Xn] == Rs; Rs gets the old value */
            unsigned rs = (insn >> 16) & 31;
            uint64_t old = 0, want = xreg(ss, rs, false), mask = bytes == 8 ? ~0ull : ((1ull << (8 * bytes)) - 1);
            memcpy(&old, (const void *)a, bytes);
            if ((old & mask) == (want & mask)) {
                reg_bytes(mc, false, rt, bytes, buf);
                if (!put(a, buf, bytes)) return false;
            }
            set_xreg(ss, rs, old);
            return true;
        }
    }

    /* STP / STNP: a pair. */
    if ((insn & 0x3A000000u) == 0x28000000u) {
        if ((insn >> 22) & 1) return false;                         /* a load */
        unsigned opc = insn >> 30, type = (insn >> 23) & 3;
        unsigned scale = simd ? 2 + opc : (opc == 0 ? 2 : opc == 2 ? 3 : 99);
        if (scale > 4) return false;
        unsigned rt2 = (insn >> 10) & 31, bytes = 1u << scale;
        int64_t off = sext((insn >> 15) & 0x7F, 7) << scale;
        uint64_t base = xreg(ss, rn, true);
        uint64_t a = type == 1 ? base : base + (uint64_t)off;       /* post-index stores at the base */
        reg_bytes(mc, simd, rt, bytes, buf);
        if (!put(a, buf, bytes)) return false;
        reg_bytes(mc, simd, rt2, bytes, buf);
        if (!put(a + bytes, buf, bytes)) return false;
        if (type == 1 || type == 3) set_xreg(ss, rn, base + (uint64_t)off);
        return true;
    }

    /* STR / STRB / STRH and their SIMD forms. */
    unsigned size = insn >> 30, opc = (insn >> 22) & 3;
    bool is_store = opc == 0 || (simd && opc == 2 && size == 0);    /* SIMD opc 2 with size 0 is the 128-bit store */
    unsigned scale = (simd && opc == 2) ? 4 : size;
    unsigned bytes = 1u << scale;

    if ((insn & 0x3B000000u) == 0x39000000u) {                      /* unsigned immediate offset */
        if (!is_store) return false;
        uint64_t a = xreg(ss, rn, true) + ((uint64_t)((insn >> 10) & 0xFFF) << scale);
        reg_bytes(mc, simd, rt, bytes, buf);
        return put(a, buf, bytes);
    }
    if ((insn & 0x3B200000u) == 0x38000000u) {                      /* unscaled / pre-index / post-index / unprivileged */
        if (!is_store) return false;
        unsigned type = (insn >> 10) & 3;
        int64_t off = sext((insn >> 12) & 0x1FF, 9);
        uint64_t base = xreg(ss, rn, true);
        uint64_t a = type == 1 ? base : base + (uint64_t)off;
        reg_bytes(mc, simd, rt, bytes, buf);
        if (!put(a, buf, bytes)) return false;
        if (type == 1 || type == 3) set_xreg(ss, rn, base + (uint64_t)off);
        return true;
    }
    if ((insn & 0x3B200C00u) == 0x38200800u) {                      /* register offset */
        if (!is_store) return false;
        unsigned rm = (insn >> 16) & 31, option = (insn >> 13) & 7, s = (insn >> 12) & 1;
        uint64_t idx = xreg(ss, rm, false);
        if (option == 2) idx = (uint32_t)idx;                       /* UXTW */
        else if (option == 6) idx = (uint64_t)(int64_t)(int32_t)idx;   /* SXTW */
        idx <<= s ? scale : 0;
        uint64_t a = xreg(ss, rn, true) + idx;
        reg_bytes(mc, simd, rt, bytes, buf);
        return put(a, buf, bytes);
    }
    return false;
}

static void chain(struct sigaction *prev, int sig, siginfo_t *info, void *uc)
{
    if ((prev->sa_flags & SA_SIGINFO) && prev->sa_sigaction) { prev->sa_sigaction(sig, info, uc); return; }
    if (prev->sa_handler != SIG_DFL && prev->sa_handler != SIG_IGN && prev->sa_handler) { prev->sa_handler(sig); return; }
    signal(sig, SIG_DFL);                                           /* nothing else wants it: let it fault again and end */
}

static void on_fault(int sig, siginfo_t *info, void *uc)
{
    ucontext_t *u = uc;
    if (g_on && info && tl_xmem_is_rx(info->si_addr) && emulate(u->uc_mcontext)) {
        u->uc_mcontext->__ss.__pc += 4;
        g_emulated++;
        return;
    }
    chain(sig == SIGBUS ? &g_prev_bus : &g_prev_segv, sig, info, uc);
}

void tl_codewrite_enable(void)
{
    if (g_on) return;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = on_fault;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGBUS, &sa, &g_prev_bus);
    sigaction(SIGSEGV, &sa, &g_prev_segv);
    g_on = true;
    tl_log_line("codewrite: stores into code go through the writable view from now on");
}
