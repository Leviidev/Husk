/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-va.h"
#include "husk-tl-bionic.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* ---------------------------------------------------------------- the stub */

/*
 * The common half of every TL_VA_STUB. Entered with x16 = the C implementation and
 * every argument register as the guest left it. Builds, below the caller's frame:
 *
 *      [sp +   0] x0..x7        (64 bytes)
 *      [sp +  64] q0..q7        (128 bytes)
 *      [sp + 192] caller's sp   (8 bytes, 16 with padding)
 *
 * then x29/x30 above that, calls the implementation with x0 = sp, and returns its
 * result untouched in x0 / d0.
 */
/* How many times an implementation returned with a callee-saved register changed, and the last such case. */
volatile struct { uint64_t count, x21, impl, mask; } tl_va_clobber;

__asm__(
    ".text\n.p2align 2\n.globl _tl_va_common\n_tl_va_common:\n"
    "  stp x29, x30, [sp, #-16]!\n"
    "  mov x29, sp\n"
    "  sub sp, sp, #288\n"
    "  stp x0, x1, [sp, #0]\n"
    "  stp x2, x3, [sp, #16]\n"
    "  stp x4, x5, [sp, #32]\n"
    "  stp x6, x7, [sp, #48]\n"
    "  stp q0, q1, [sp, #64]\n"
    "  stp q2, q3, [sp, #96]\n"
    "  stp q4, q5, [sp, #128]\n"
    "  stp q6, q7, [sp, #160]\n"
    "  add x9, x29, #16\n"
    "  str x9, [sp, #192]\n"
    /* The callee-saved registers, kept aside: whatever the implementation does, the guest gets them back. */
    "  stp x19, x20, [sp, #208]\n"
    "  stp x21, x22, [sp, #224]\n"
    "  stp x23, x24, [sp, #240]\n"
    "  stp x25, x26, [sp, #256]\n"
    "  stp x27, x28, [sp, #272]\n"
    "  str x16, [sp, #200]\n"
    "  mov x0, sp\n"
    "  blr x16\n"
    "  mov x11, xzr\n"
    "  ldp x9, x10, [sp, #208]\n"
    "  eor x9, x9, x19\n  eor x10, x10, x20\n  orr x11, x11, x9\n  orr x11, x11, x10\n"
    "  ldp x9, x10, [sp, #224]\n"
    "  eor x9, x9, x21\n  eor x10, x10, x22\n  orr x11, x11, x9\n  orr x11, x11, x10\n"
    "  ldp x9, x10, [sp, #240]\n"
    "  eor x9, x9, x23\n  eor x10, x10, x24\n  orr x11, x11, x9\n  orr x11, x11, x10\n"
    "  ldp x9, x10, [sp, #256]\n"
    "  eor x9, x9, x25\n  eor x10, x10, x26\n  orr x11, x11, x9\n  orr x11, x11, x10\n"
    "  ldp x9, x10, [sp, #272]\n"
    "  eor x9, x9, x27\n  eor x10, x10, x28\n  orr x11, x11, x9\n  orr x11, x11, x10\n"
    "  cbz x11, 1f\n"
    "  adrp x12, _tl_va_clobber@PAGE\n"
    "  add x12, x12, _tl_va_clobber@PAGEOFF\n"
    "  ldr x13, [x12]\n  add x13, x13, #1\n  str x13, [x12]\n"
    "  str x21, [x12, #8]\n"
    "  ldr x14, [sp, #200]\n"
    "  str x14, [x12, #16]\n"
    "  str x11, [x12, #24]\n"
    "1:\n"
    "  ldp x19, x20, [sp, #208]\n"
    "  ldp x21, x22, [sp, #224]\n"
    "  ldp x23, x24, [sp, #240]\n"
    "  ldp x25, x26, [sp, #256]\n"
    "  ldp x27, x28, [sp, #272]\n"
    "  mov sp, x29\n"
    "  ldp x29, x30, [sp], #16\n"
    "  ret\n");

/* ------------------------------------------------------------------ printf */

typedef struct { char *p; size_t len, cap; } sbuf;

static void sb_put(sbuf *b, const char *s, size_t n)
{
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->len + n + 1) nc *= 2;
        b->p = realloc(b->p, nc);
        b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = 0;
}

/* UTF-32 -> UTF-8. */
static size_t utf8_encode(uint32_t c, char *o)
{
    if (c < 0x80) { o[0] = (char)c; return 1; }
    if (c < 0x800) { o[0] = (char)(0xC0 | (c >> 6)); o[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { o[0] = (char)(0xE0 | (c >> 12)); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F)); return 3; }
    o[0] = (char)(0xF0 | (c >> 18)); o[1] = (char)(0x80 | ((c >> 12) & 0x3F));
    o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F));
    return 4;
}

enum { LEN_NONE, LEN_HH, LEN_H, LEN_L, LEN_LL, LEN_J, LEN_Z, LEN_T, LEN_BIGL };

/* Core formatter: `fmt` is a narrow format string, output is bytes. */
static void format_into(sbuf *out, const char *fmt, tl_va_list *ap)
{
    for (const char *f = fmt; *f; f++) {
        if (*f != '%') {
            const char *e = strchr(f, '%');
            size_t n = e ? (size_t)(e - f) : strlen(f);
            sb_put(out, f, n);
            f += n - 1;
            continue;
        }
        f++;
        char flags[8]; int nflags = 0;
        while (*f && strchr("-+ #0'", *f) && nflags < 7) flags[nflags++] = *f++;
        flags[nflags] = 0;
        int width = -1, prec = -1;
        if (*f == '*') {
            width = (int)(int32_t)tl_va_arg_u64(ap); f++;
            if (width < 0) { if (nflags < 7) { flags[nflags++] = '-'; flags[nflags] = 0; } width = -width; }
        } else if (*f >= '0' && *f <= '9') { width = 0; while (*f >= '0' && *f <= '9') width = width * 10 + (*f++ - '0'); }
        if (*f == '.') {
            f++; prec = 0;
            if (*f == '*') { prec = (int)(int32_t)tl_va_arg_u64(ap); f++; if (prec < 0) prec = -1; }
            else while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0');
        }
        int len = LEN_NONE;
        for (;;) {
            if (*f == 'h') { len = (len == LEN_H) ? LEN_HH : LEN_H; f++; }
            else if (*f == 'l') { len = (len == LEN_L) ? LEN_LL : LEN_L; f++; }
            else if (*f == 'j') { len = LEN_J; f++; }
            else if (*f == 'z') { len = LEN_Z; f++; }
            else if (*f == 't') { len = LEN_T; f++; }
            else if (*f == 'L') { len = LEN_BIGL; f++; }
            else if (*f == 'q') { len = LEN_LL; f++; }
            else break;
        }
        char conv = *f;
        if (!conv) break;

        char spec[48]; char tmp[512];
        int sn = 0;
        spec[sn++] = '%';
        for (int i = 0; i < nflags; i++) spec[sn++] = flags[i];
        if (width >= 0) sn += snprintf(spec + sn, sizeof(spec) - (size_t)sn, "%d", width);
        if (prec >= 0) sn += snprintf(spec + sn, sizeof(spec) - (size_t)sn, ".%d", prec);

        switch (conv) {
        case 'd': case 'i': {
            uint64_t raw = tl_va_arg_u64(ap);
            long long v;
            switch (len) {
            case LEN_HH: v = (signed char)raw; break;
            case LEN_H:  v = (short)raw; break;
            case LEN_NONE: v = (int)(int32_t)raw; break;
            default: v = (long long)raw; break;
            }
            strcpy(spec + sn, "lld");
            int n = snprintf(tmp, sizeof(tmp), spec, v);
            sb_put(out, tmp, (size_t)(n < (int)sizeof(tmp) ? n : (int)sizeof(tmp) - 1));
            break;
        }
        case 'u': case 'x': case 'X': case 'o': {
            uint64_t raw = tl_va_arg_u64(ap);
            unsigned long long v;
            switch (len) {
            case LEN_HH: v = (unsigned char)raw; break;
            case LEN_H:  v = (unsigned short)raw; break;
            case LEN_NONE: v = (uint32_t)raw; break;
            default: v = raw; break;
            }
            spec[sn++] = 'l'; spec[sn++] = 'l'; spec[sn++] = conv; spec[sn] = 0;
            int n = snprintf(tmp, sizeof(tmp), spec, v);
            sb_put(out, tmp, (size_t)(n < (int)sizeof(tmp) ? n : (int)sizeof(tmp) - 1));
            break;
        }
        case 'c': {
            uint64_t raw = tl_va_arg_u64(ap);
            if (len == LEN_L) {
                char u[4]; size_t k = utf8_encode((uint32_t)raw, u);
                sb_put(out, u, k);
            } else {
                spec[sn++] = 'c'; spec[sn] = 0;
                int n = snprintf(tmp, sizeof(tmp), spec, (int)(unsigned char)raw);
                sb_put(out, tmp, (size_t)(n < (int)sizeof(tmp) ? n : (int)sizeof(tmp) - 1));
            }
            break;
        }
        case 's': {
            const char *s = (const char *)(uintptr_t)tl_va_arg_u64(ap);
            if (len == LEN_L) {
                const wchar_t *w = (const wchar_t *)s;
                sbuf t = {0};
                if (!w) sb_put(&t, "(null)", 6);
                else for (size_t i = 0; w[i] && (prec < 0 || t.len < (size_t)prec); i++) {
                    char u[4]; size_t k = utf8_encode((uint32_t)w[i], u); sb_put(&t, u, k);
                }
                int pad = width > (int)t.len ? width - (int)t.len : 0;
                bool left = strchr(flags, '-') != NULL;
                if (!left) for (int i = 0; i < pad; i++) sb_put(out, " ", 1);
                if (t.p) sb_put(out, t.p, t.len);
                if (left) for (int i = 0; i < pad; i++) sb_put(out, " ", 1);
                free(t.p);
            } else {
                if (!s) s = "(null)";
                spec[sn++] = 's'; spec[sn] = 0;
                size_t need = strlen(s) + (width > 0 ? (size_t)width : 0) + 8;
                char *big = need > sizeof(tmp) ? malloc(need) : tmp;
                int n = snprintf(big, need > sizeof(tmp) ? need : sizeof(tmp), spec, s);
                sb_put(out, big, (size_t)n);
                if (big != tmp) free(big);
            }
            break;
        }
        case 'p': {
            uint64_t v = tl_va_arg_u64(ap);
            int n = snprintf(tmp, sizeof(tmp), "0x%llx", (unsigned long long)v);
            sb_put(out, tmp, (size_t)n);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            double d = tl_va_arg_f64(ap);
            if (len == LEN_BIGL) { /* a guest long double (IEEE quad) is in a q register: take its double value */ }
            spec[sn++] = conv; spec[sn] = 0;
            int n = snprintf(tmp, sizeof(tmp), spec, d);
            if (n >= (int)sizeof(tmp)) {
                char *big = malloc((size_t)n + 1);
                snprintf(big, (size_t)n + 1, spec, d);
                sb_put(out, big, (size_t)n);
                free(big);
            } else sb_put(out, tmp, (size_t)n);
            break;
        }
        case 'n': {
            int *p = (int *)(uintptr_t)tl_va_arg_u64(ap);
            if (p) *p = (int)out->len;
            break;
        }
        case '%':
            sb_put(out, "%", 1);
            break;
        default: {
            char lit[2] = { '%', conv };
            sb_put(out, lit, 2);
            break;
        }
        }
    }
}

int tl_format(char *out, size_t cap, const char *fmt, tl_va_list *ap)
{
    sbuf b = {0};
    sb_put(&b, "", 0);
    format_into(&b, fmt ? fmt : "(null)", ap);
    if (out && cap) {
        size_t n = b.len < cap - 1 ? b.len : cap - 1;
        memcpy(out, b.p, n);
        out[n] = 0;
    }
    int r = (int)b.len;
    free(b.p);
    return r;
}

int tl_format_wide(wchar_t *out, size_t cap, const wchar_t *fmt, tl_va_list *ap)
{
    /* Narrow the format (ASCII conversions), format, widen the result. */
    size_t fl = fmt ? wcslen(fmt) : 0;
    /* a format char that is no code point fails as bionic's does (EILSEQ, nothing written): Office, built with 2-byte wchar_t,
       hands libc's swprintf L"%d" as UTF-16 (0x640025 read as one 4-byte char) and a 5-unit buffer the 4-byte output overran */
    for (size_t i = 0; i < fl; i++)
        if ((uint32_t)fmt[i] > 0x10FFFF || ((uint32_t)fmt[i] >= 0xD800 && (uint32_t)fmt[i] < 0xE000)) { errno = EILSEQ; return -1; }
    char *nf = malloc(fl * 4 + 1);
    size_t k = 0;
    for (size_t i = 0; i < fl; i++) k += utf8_encode((uint32_t)fmt[i], nf + k);
    nf[k] = 0;
    sbuf b = {0};
    sb_put(&b, "", 0);
    format_into(&b, nf, ap);
    free(nf);
    /* UTF-8 -> wchar_t */
    size_t wn = 0;
    for (size_t i = 0; i < b.len;) {
        unsigned char c = (unsigned char)b.p[i];
        uint32_t cp; size_t adv;
        if (c < 0x80) { cp = c; adv = 1; }
        else if ((c >> 5) == 6 && i + 1 < b.len) { cp = ((c & 0x1F) << 6) | (b.p[i+1] & 0x3F); adv = 2; }
        else if ((c >> 4) == 14 && i + 2 < b.len) { cp = ((c & 0x0F) << 12) | ((b.p[i+1] & 0x3F) << 6) | (b.p[i+2] & 0x3F); adv = 3; }
        else if ((c >> 3) == 30 && i + 3 < b.len) { cp = ((c & 7) << 18) | ((b.p[i+1] & 0x3F) << 12) | ((b.p[i+2] & 0x3F) << 6) | (b.p[i+3] & 0x3F); adv = 4; }
        else { cp = c; adv = 1; }
        if (out && cap && wn + 1 < cap) out[wn] = (wchar_t)cp;
        wn++; i += adv;
    }
    if (out && cap) out[wn < cap - 1 ? wn : cap - 1] = 0;
    free(b.p);
    return (int)wn;
}

int tl_scan_string(const char *src, const char *fmt, tl_va_list *ap)
{
    /* Every output is a pointer, so the arguments are read blind -- more than any call needs -- and the host's sscanf takes only as many as the format asks for. A game that
     * parses a wide table with one call needs the room: GTA's timecyc.dat has fifty-two fields to a line. */
    enum { N = 96 };
    void *a[N];
    for (int i = 0; i < N; i++) a[i] = (void *)(uintptr_t)tl_va_arg_u64(ap);
    { static int tr = -1; if (tr < 0) tr = getenv("TL_SCAN_TRACE") ? 1 : 0; if (tr) tl_log_line("scan: sscanf(\"%.80s\", \"%s\")", src, fmt); }
    return sscanf(src, fmt, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15],
                  a[16], a[17], a[18], a[19], a[20], a[21], a[22], a[23], a[24], a[25], a[26], a[27], a[28], a[29], a[30], a[31],
                  a[32], a[33], a[34], a[35], a[36], a[37], a[38], a[39], a[40], a[41], a[42], a[43], a[44], a[45], a[46], a[47],
                  a[48], a[49], a[50], a[51], a[52], a[53], a[54], a[55], a[56], a[57], a[58], a[59], a[60], a[61], a[62], a[63],
                  a[64], a[65], a[66], a[67], a[68], a[69], a[70], a[71], a[72], a[73], a[74], a[75], a[76], a[77], a[78], a[79],
                  a[80], a[81], a[82], a[83], a[84], a[85], a[86], a[87], a[88], a[89], a[90], a[91], a[92], a[93], a[94], a[95]);
}

int tl_count_wide_scan_args(const wchar_t *fmt)
{
    if (!fmt) return 0;
    int seq = 0, max_pos = 0;
    for (const wchar_t *f = fmt; *f;) {
        if (*f != L'%') { f++; continue; }
        f++;
        if (!*f) break;
        if (*f == L'%') { f++; continue; }
        int pos = 0;
        const wchar_t * p = f;
        while (*p >= L'0' && *p <= L'9') {
            if (pos <= 1000) pos = pos * 10 + (int)(*p - L'0');
            p++;
        }
        bool has_pos = (p > f && *p == L'$');
        if (has_pos) f = p + 1;
        bool suppress = (*f == L'*');
        if (suppress) f++;
        while (*f >= L'0' && *f <= L'9') f++;
        if (*f == L'm') f++;
        while (*f == L'h' || *f == L'l' || *f == L'j' || *f == L'z' || *f == L't' || *f == L'L' || *f == L'q') f++;
        if (!*f) break;
        if (*f == L'[') {
            f++;
            if (*f == L'^') f++;
            if (*f == L']') f++;
            while (*f && *f != L']') f++;
            if (*f == L']') f++;
        } else {
            f++;
        }
        if (!suppress) {
            if (has_pos) { if (pos > max_pos) max_pos = pos; }
            else seq++;
        }
    }
    return max_pos > seq ? max_pos : seq;
}

int tl_scan_wide(const wchar_t *src, const wchar_t *fmt, tl_va_list *ap)
{
    if (!src || !fmt || !ap) return EOF;
    for (size_t i = 0; fmt[i]; i++) {
        uint32_t cp = (uint32_t)fmt[i];
        if (cp > 0x10FFFFu || (cp >= 0xD800u && cp < 0xE000u)) { errno = EILSEQ; return EOF; }
    }
    enum { N = 64 };
    int want = tl_count_wide_scan_args(fmt);
    if (want > N) want = N;
    void *a[N] = {0};
    for (int i = 0; i < want; i++) a[i] = (void *)(uintptr_t)tl_va_arg_u64(ap);
    return swscanf(src, fmt, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15],
                   a[16], a[17], a[18], a[19], a[20], a[21], a[22], a[23], a[24], a[25], a[26], a[27], a[28], a[29], a[30], a[31],
                   a[32], a[33], a[34], a[35], a[36], a[37], a[38], a[39], a[40], a[41], a[42], a[43], a[44], a[45], a[46], a[47],
                   a[48], a[49], a[50], a[51], a[52], a[53], a[54], a[55], a[56], a[57], a[58], a[59], a[60], a[61], a[62], a[63]);
}
