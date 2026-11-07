/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Variadic calls across the Android/Darwin boundary.
 *
 * Guest code is compiled for AAPCS64: a variadic call passes its first eight
 * integer-class arguments in x0-x7, its first eight floating-point ones in
 * v0-v7, and anything beyond on the stack. Code compiled for Darwin reads *all*
 * variadic arguments from the stack. A shim written as `int f(const char *, ...)`
 * and called by the guest would therefore read garbage.
 *
 * So a variadic shim is two pieces. The entry point, made by TL_VA_STUB, is a few
 * instructions that spill the argument registers into a frame laid out the way
 * AAPCS64's own va_start leaves them, and call a C function with that frame. The C
 * function walks it with tl_va_arg_*. Guest va_list values -- what vsnprintf and
 * friends receive -- have the same layout, so one reader serves both.
 */
#ifndef HUSK_TL_VA_H
#define HUSK_TL_VA_H

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AAPCS64's va_list, byte for byte. */
typedef struct tl_va_list {
    void *stack;       /* next stack argument */
    void *gr_top;      /* end of the general-register save area */
    void *vr_top;      /* end of the FP/SIMD register save area */
    int   gr_offs;     /* negative offset from gr_top of the next general argument */
    int   vr_offs;     /* negative offset from vr_top of the next FP argument */
} tl_va_list;

/* What a stub saves: x0-x7, v0-v7 (16 bytes each), and the caller's stack pointer. */
typedef struct tl_va_frame {
    uint64_t gp[8];
    unsigned __int128 fp[8];
    void *stack;
} tl_va_frame;

/* Begin reading after `named_gp` general and `named_fp` FP fixed arguments. */
static inline void tl_va_start(tl_va_frame *f, int named_gp, int named_fp, tl_va_list *out)
{
    out->stack = f->stack;
    out->gr_top = &f->gp[8];
    out->vr_top = &f->fp[8];
    out->gr_offs = -(8 - named_gp) * 8;
    out->vr_offs = -(8 - named_fp) * 16;
}

static inline uint64_t tl_va_arg_u64(tl_va_list *ap)
{
    if (ap->gr_offs < 0) {
        uint64_t v = *(uint64_t *)((char *)ap->gr_top + ap->gr_offs);
        ap->gr_offs += 8;
        return v;
    }
    uint64_t v = *(uint64_t *)ap->stack;
    ap->stack = (char *)ap->stack + 8;
    return v;
}

static inline double tl_va_arg_f64(tl_va_list *ap)
{
    double d;
    if (ap->vr_offs < 0) {
        d = *(double *)((char *)ap->vr_top + ap->vr_offs);
        ap->vr_offs += 16;
        return d;
    }
    d = *(double *)ap->stack;
    ap->stack = (char *)ap->stack + 8;
    return d;
}

/*
 * Define `sym` as a variadic entry point that calls `impl(tl_va_frame *)`. `impl`
 * must be a non-static function; whatever it returns (x0 or d0) is returned to the
 * guest unchanged. Declare the entry in C as `extern void sym(void);` and take
 * its address for the symbol table.
 */
#define TL_VA_STUB(sym, impl) \
    __asm__(".text\n.p2align 2\n.globl _" #sym "\n_" #sym ":\n" \
            "  adrp x16, _" #impl "@PAGE\n" \
            "  add  x16, x16, _" #impl "@PAGEOFF\n" \
            "  b    _tl_va_common\n")

void tl_va_common(void);

/*
 * printf, with guest arguments. Formats into `out` (at most `cap` bytes including the
 * terminating NUL, like vsnprintf) and returns the length the whole output has.
 * `out` may be NULL with cap 0 to measure.
 */
int tl_format(char *out, size_t cap, const char *fmt, tl_va_list *ap);

/* The same for a guest wide format string (swprintf). */
int tl_format_wide(wchar_t *out, size_t cap, const wchar_t *fmt, tl_va_list *ap);

/*
 * scanf, with guest pointer arguments: up to 96 conversions. `src` is a string.
 */
int tl_scan_string(const char *src, const char *fmt, tl_va_list *ap);

/*
 * Wide scanf (swscanf / vswscanf), reading only the non-suppressed pointer arguments
 * named by `fmt` from the guest AAPCS64 va_list.
 */
int tl_count_wide_scan_args(const wchar_t *fmt);
int tl_scan_wide(const wchar_t *src, const wchar_t *fmt, tl_va_list *ap);

#ifdef __cplusplus
}
#endif

#endif /* HUSK_TL_VA_H */
