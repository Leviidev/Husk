/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk -- the BreakpointJIT trap sequences, implemented natively.
 *
 * These three routines are the entire request channel to StikDebug. They are
 * reproduced here from the documented wire protocol (brk immediate + x16
 * command number) rather than linked from Stossy11's BreakpointJIT.framework,
 * for three reasons:
 *
 *   1. That framework ships as a bare binary with no stated license. Husk is
 *      GPL-2.0-or-later and cannot take on an unlicensed dynamic dependency.
 *   2. Embedding ANY framework that carries entitlements into a sideloaded app
 *      trips AMFI at launch ("has entitlements but is not a main binary"), which
 *      is why consumers of it must dlopen() lazily instead of linking. With the
 *      sequences compiled directly into the main binary, that whole class of
 *      problem disappears.
 *   3. Writing them by hand guarantees the exact encoding. The C versions rely
 *      on __attribute__((naked, optnone)) to stop the compiler touching them.
 *
 * Verified against BreakpointJIT.framework (com.stossy11.BreakpointJIT 1.0) via
 * `otool -tV`, which disassembles in full to:
 *
 *   _BreakGetJITMapping:  mov x16, #0x1 ; brk #0xf00d ; ret
 *   _BreakJITDetach:      mov x16, #0x0 ; brk #0xf00d ; ret
 *   _BreakMarkJITMapping: brk #0x69     ; ret
 *
 * Protocol (see StikDebug/Scripts/universal.js):
 *   brk #0xf00d dispatches on x16 -- 0 = detach, 1 = prepare region.
 *   Arguments arrive in x0/x1 and the result is written back into x0 before
 *   the debugger resumes us, so the trailing `ret` returns it.
 *   brk #0x69 is serviced by writing the constant 0xE0000069 into x0, which
 *   makes it a cheap "is StikDebug actually attached?" probe.
 *
 * If StikDebug is NOT attached, these brk instructions raise a real SIGTRAP.
 * husk_ios_jit_install_trap_handler() must be installed first so that becomes a
 * NULL return rather than process death.
 */

#if defined(__aarch64__) || defined(__arm64__)

.section __TEXT,__text,regular,pure_instructions

/* void *husk_brk_get_jit_mapping(void *addr, size_t len);
 * x0 = addr (pass NULL to request a fresh region), x1 = len.
 * Returns the executable region address in x0, or 0 on failure. */
.globl _husk_brk_get_jit_mapping
.p2align 2
_husk_brk_get_jit_mapping:
    mov     x16, #1              /* CMD_PREPARE_REGION */
    brk     #0xf00d
    ret

/* void husk_brk_jit_detach(void); */
.globl _husk_brk_jit_detach
.p2align 2
_husk_brk_jit_detach:
    mov     x16, #0              /* CMD_DETACH */
    brk     #0xf00d
    ret

/* uint64_t husk_brk_probe(void);
 * Returns 0xE0000069 when StikDebug is attached and servicing traps. */
.globl _husk_brk_probe
.p2align 2
_husk_brk_probe:
    brk     #0x69
    ret

#endif /* aarch64 */
