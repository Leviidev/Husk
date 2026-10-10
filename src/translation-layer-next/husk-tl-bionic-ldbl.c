/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * long double for Android's arm64 code (numpy, CPython's math module), and the last of the POSIX calls CPython's posix module
 * links against.
 *
 * On Android arm64 `long double` is IEEE quad precision -- 128 bits, passed and returned in a whole q register. On Apple arm64 it
 * is just a double. So libm's *l functions cannot be passed straight through: each one here takes the 128 bits as a vector (which
 * the AAPCS64 passes in the same register), converts to double, computes, and converts back. That loses quad's extra precision,
 * which a Python game never notices; nextafterl, which numpy uses to measure the type, works on the quad bits themselves so the
 * limits it finds are quad's.
 */
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"

typedef uint64_t q128 __attribute__((vector_size(16)));       /* lo = [0], hi = [1]; passed like a long double, in v0..v7 */

static double q2d(q128 q)
{
    uint64_t lo = q[0], hi = q[1];
    uint64_t sign = hi >> 63;
    int64_t exp = (int64_t)((hi >> 48) & 0x7FFF);
    uint64_t mant_hi = hi & 0xFFFFFFFFFFFFull;                   /* 48 bits */
    uint64_t m52 = (mant_hi << 4) | (lo >> 60);                  /* the top 52 of quad's 112 */
    uint64_t bits;
    if (exp == 0x7FFF) bits = (sign << 63) | (0x7FFull << 52) | (m52 | ((mant_hi || lo) && !m52 ? 1 : 0));
    else if (exp == 0) bits = sign << 63;                        /* zero, and quad subnormals (far below double's range) */
    else {
        int64_t e = exp - 16383 + 1023;
        if (e >= 0x7FF) bits = (sign << 63) | (0x7FFull << 52);
        else if (e <= 0) bits = sign << 63;
        else {
            /* round to nearest on the 60 bits dropped */
            uint64_t rest = lo << 4;
            if (rest > 0x8000000000000000ull || (rest == 0x8000000000000000ull && (m52 & 1))) {
                if (++m52 == (1ull << 52)) { m52 = 0; e++; }
            }
            bits = e >= 0x7FF ? (sign << 63) | (0x7FFull << 52) : (sign << 63) | ((uint64_t)e << 52) | m52;
        }
    }
    double d; memcpy(&d, &bits, 8); return d;
}

static q128 d2q(double d)
{
    uint64_t bits; memcpy(&bits, &d, 8);
    uint64_t sign = bits >> 63, exp = (bits >> 52) & 0x7FF, m = bits & 0xFFFFFFFFFFFFFull;
    uint64_t qe;
    if (exp == 0x7FF) qe = 0x7FFF;
    else if (exp == 0) {
        if (!m) qe = 0;
        else {                                                    /* a double subnormal is a quad normal */
            int shift = __builtin_clzll(m) - 11;
            m = (m << shift) & 0xFFFFFFFFFFFFFull;
            qe = (uint64_t)(16383 - 1022 - shift);
        }
    } else qe = exp - 1023 + 16383;
    q128 q;
    q[1] = (sign << 63) | (qe << 48) | (m >> 4);
    q[0] = m << 60;
    return q;
}

#define L1(name, fn) static q128 b_##name(q128 x) { return d2q(fn(q2d(x))); }
#define L2(name, fn) static q128 b_##name(q128 x, q128 y) { return d2q(fn(q2d(x), q2d(y))); }
L1(acoshl, acosh) L1(acosl, acos) L1(asinhl, asinh) L1(asinl, asin) L1(atanhl, atanh) L1(atanl, atan) L1(cbrtl, cbrt)
L1(ceill, ceil) L1(coshl, cosh) L1(cosl, cos) L1(exp2l, exp2) L1(expl, exp) L1(expm1l, expm1) L1(floorl, floor)
L1(log10l, log10) L1(log1pl, log1p) L1(log2l, log2) L1(logl, log) L1(rintl, rint) L1(sinhl, sinh) L1(sinl, sin)
L1(sqrtl, sqrt) L1(tanhl, tanh) L1(tanl, tan) L1(truncl, trunc) L1(fabsl, fabs) L1(roundl, round) L1(nearbyintl, nearbyint)
L1(erfl, erf) L1(erfcl, erfc) L1(tgammal, tgamma) L1(lgammal, lgamma)
L2(atan2l, atan2) L2(fmodl, fmod) L2(hypotl, hypot) L2(powl, pow) L2(fmaxl, fmax) L2(fminl, fmin) L2(copysignl, copysign)
L2(remainderl, remainder) L2(fdiml, fdim)

static q128 b_ldexpl(q128 x, int e) { return d2q(ldexp(q2d(x), e)); }
static q128 b_scalbnl(q128 x, int e) { return d2q(scalbn(q2d(x), e)); }
static q128 b_frexpl(q128 x, int *e) { return d2q(frexp(q2d(x), e)); }
static q128 b_modfl(q128 x, q128 *ip) { double i; double f = modf(q2d(x), &i); if (ip) *ip = d2q(i); return d2q(f); }
static void b_sincosl(q128 x, q128 *s, q128 *c) { double d = q2d(x); if (s) *s = d2q(sin(d)); if (c) *c = d2q(cos(d)); }
static long b_lrintl(q128 x) { return lrint(q2d(x)); }
static long b_lroundl(q128 x) { return lround(q2d(x)); }
static int b___isinfl(q128 x) { return isinf(q2d(x)); }
static int b___isnanl(q128 x) { return isnan(q2d(x)); }

/* nextafterl on the quad bits: one unit in quad's last place, toward y. */
static int qcmp(q128 a, q128 b)                                  /* -1, 0, 1 as numbers; 2 when unordered */
{
    double da = q2d(a), db = q2d(b);
    if (isnan(da) || isnan(db)) return 2;
    if (da != db) return da < db ? -1 : 1;
    /* equal as doubles: order by the bits (same sign here) */
    bool neg = a[1] >> 63;
    unsigned __int128 ua = ((unsigned __int128)(a[1] & ~(1ull << 63)) << 64) | a[0], ub = ((unsigned __int128)(b[1] & ~(1ull << 63)) << 64) | b[0];
    if (ua == ub) return 0;
    return (ua < ub) != neg ? -1 : 1;
}
static q128 b_nextafterl(q128 x, q128 y)
{
    int c = qcmp(x, y);
    if (c == 2) { q128 n; n[1] = 0x7FFF800000000000ull; n[0] = 0; return n; }
    if (c == 0) return y;
    unsigned __int128 mag = ((unsigned __int128)(x[1] & ~(1ull << 63)) << 64) | x[0];
    bool neg = x[1] >> 63;
    if (mag == 0) {                                               /* from zero: the smallest subnormal, signed toward y */
        q128 r; r[0] = 1; r[1] = c < 0 ? 0 : (1ull << 63); return r;
    }
    bool up = c < 0;                                              /* x < y: toward +inf */
    if (up != neg) mag++; else mag--;
    q128 r; r[0] = (uint64_t)mag; r[1] = (uint64_t)(mag >> 64) | (neg ? 1ull << 63 : 0);
    return r;
}

/* ---- POSIX odds and ends */

static int b_posix_ok(void) { return 0; }
static int b_enosys(void) { tl_set_guest_errno(38); return -1; }
static int b_eperm(void) { tl_set_guest_errno(1); return -1; }
static long b_fpathconf(int fd, int name) { (void)fd; return name == 3 /* _PC_NAME_MAX */ ? 255 : 4096; }
static int b_dup3(int old, int nw, int flags)
{
    int r = dup2(old, nw);
    if (r >= 0 && (flags & 0x80000 /* O_CLOEXEC */)) fcntl(r, F_SETFD, FD_CLOEXEC);
    return r;
}
static int b_getresid(unsigned *r, unsigned *e, unsigned *s) { if (r) *r = 10100; if (e) *e = 10100; if (s) *s = 10100; return 0; }
static int b_sigpending(uint64_t *set) { if (set) *set = 0; return 0; }
static int b_ttyname_r(int fd, char *b, size_t n) { (void)fd; (void)b; (void)n; return 25; /* ENOTTY */ }
static const char *b_hstrerror(int e) { (void)e; return "Unknown resolver error"; }
static void *b_null_lookup(const void *a, const void *b) { (void)a; (void)b; return NULL; }
static int b_getpwnam_r(const char *name, void *pwd, char *buf, size_t n, void **result) { (void)name; (void)pwd; (void)buf; (void)n; if (result) *result = NULL; return 0; }

const tl_bionic_entry tl_tab_ldbl[] = {
    TL_WRAP("acoshl", b_acoshl), TL_WRAP("acosl", b_acosl), TL_WRAP("asinhl", b_asinhl), TL_WRAP("asinl", b_asinl),
    TL_WRAP("atanhl", b_atanhl), TL_WRAP("atanl", b_atanl), TL_WRAP("cbrtl", b_cbrtl), TL_WRAP("ceill", b_ceill),
    TL_WRAP("coshl", b_coshl), TL_WRAP("cosl", b_cosl), TL_WRAP("exp2l", b_exp2l), TL_WRAP("expl", b_expl),
    TL_WRAP("expm1l", b_expm1l), TL_WRAP("floorl", b_floorl), TL_WRAP("log10l", b_log10l), TL_WRAP("log1pl", b_log1pl),
    TL_WRAP("log2l", b_log2l), TL_WRAP("logl", b_logl), TL_WRAP("rintl", b_rintl), TL_WRAP("sinhl", b_sinhl),
    TL_WRAP("sinl", b_sinl), TL_WRAP("sqrtl", b_sqrtl), TL_WRAP("tanhl", b_tanhl), TL_WRAP("tanl", b_tanl),
    TL_WRAP("truncl", b_truncl), TL_WRAP("fabsl", b_fabsl), TL_WRAP("roundl", b_roundl), TL_WRAP("nearbyintl", b_nearbyintl),
    TL_WRAP("erfl", b_erfl), TL_WRAP("erfcl", b_erfcl), TL_WRAP("tgammal", b_tgammal), TL_WRAP("lgammal", b_lgammal),
    TL_WRAP("atan2l", b_atan2l), TL_WRAP("fmodl", b_fmodl), TL_WRAP("hypotl", b_hypotl), TL_WRAP("powl", b_powl),
    TL_WRAP("fmaxl", b_fmaxl), TL_WRAP("fminl", b_fminl), TL_WRAP("copysignl", b_copysignl), TL_WRAP("remainderl", b_remainderl),
    TL_WRAP("fdiml", b_fdiml), TL_WRAP("ldexpl", b_ldexpl), TL_WRAP("scalbnl", b_scalbnl), TL_WRAP("frexpl", b_frexpl),
    TL_WRAP("modfl", b_modfl), TL_WRAP("sincosl", b_sincosl), TL_WRAP("lrintl", b_lrintl), TL_WRAP("lroundl", b_lroundl),
    TL_WRAP("__isinfl", b___isinfl), TL_WRAP("__isnanl", b___isnanl), TL_WRAP("nextafterl", b_nextafterl),
    TL_WRAP("nexttowardl", b_nextafterl),
    TL_DIRECT(copysign), TL_DIRECT(erf), TL_DIRECT(erfc), TL_DIRECT(readv), TL_DIRECT(sync), TL_DIRECT(getsid),
    TL_DIRECT(tcgetpgrp), TL_DIRECT(tcsetpgrp), TL_DIRECT(getitimer), TL_DIRECT(setitimer), TL_DIRECT(wait4),
    TL_DIRECT(deflateCopy), TL_DIRECT(inflateCopy),
    TL_WRAP("fpathconf", b_fpathconf), TL_WRAP("fstatvfs", b_enosys), TL_WRAP("dup3", b_dup3),
    TL_WRAP("getresuid", b_getresid), TL_WRAP("getresgid", b_getresid), TL_WRAP("setresuid", b_eperm), TL_WRAP("setresgid", b_eperm),
    TL_WRAP("siginterrupt", b_posix_ok), TL_WRAP("sigpending", b_sigpending), TL_WRAP("sigwait", b_enosys),
    TL_WRAP("ttyname_r", b_ttyname_r), TL_WRAP("hstrerror", b_hstrerror), TL_WRAP("getprotobyname", b_null_lookup),
    TL_WRAP("getservbyport", b_null_lookup), TL_WRAP("getpwnam_r", b_getpwnam_r), TL_WRAP("mkfifo", b_eperm),
    TL_WRAP("mknod", b_eperm), TL_WRAP("mknodat", b_eperm), TL_WRAP("posix_fadvise", b_posix_ok), TL_WRAP("posix_fallocate", b_posix_ok),
    TL_WRAP("clock_settime", b_eperm),
    TL_END
};
