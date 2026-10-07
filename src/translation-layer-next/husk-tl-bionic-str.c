/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Strings, characters, wide characters, math, stdlib, stdio and the printf family.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-bionic.h"

#include <ctype.h>
#include <crt_externs.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <locale.h>
#include <net/if.h>
#include <regex.h>
#include <math.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>
#include <wctype.h>
#include <xlocale.h>

#include "husk-tl-va.h"

const char *tl_path_resolve(const char *path, char *buf, size_t n);   /* husk-tl-bionic-io.c */
int tl_synth_open(const char *path);
int tl_atomic_open(const char *real, int dflags, unsigned mode);
void tl_atomic_closed(int fd);
void tl_atomic_abandon(int fd);

/* --------------------------------------------------------------- the stdio */

/* bionic's stdin/stdout/stderr are &__sF[0..2]; sizeof(FILE) there is 152 on LP64. */
#define SF_SIZE 152
static uint8_t g_sF[3 * SF_SIZE] __attribute__((aligned(16)));

static FILE *map_stream(void *s)
{
    uint8_t *p = s;
    if (p >= g_sF && p < g_sF + sizeof(g_sF)) {
        switch ((p - g_sF) / SF_SIZE) { case 0: return stdin; case 1: return stdout; default: return stderr; }
    }
    return s;
}
static bool is_log_stream(FILE *f) { return f == stdout || f == stderr; }

/* Guest console output goes to the attempt's log, a line at a time. */
static void console_write(FILE *f, const char *p, size_t n)
{
    (void)f;
    static __thread char line[2048];
    static __thread size_t len;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '\n' || len == sizeof(line) - 1) {
            line[len] = 0;
            tl_log_line("%s", line);
            len = 0;
            if (p[i] == '\n') continue;
        }
        line[len++] = p[i];
    }
}

static size_t emit(FILE *f, const void *p, size_t n)
{
    if (is_log_stream(f)) { console_write(f, p, n); return n; }
    return fwrite(p, 1, n, f);
}

/* Format with guest arguments into a malloc'd string. */
static char *format_alloc(const char *fmt, tl_va_list *ap, int *len)
{
    tl_va_list copy = *ap;
    int n = tl_format(NULL, 0, fmt, &copy);
    char *buf = malloc((size_t)n + 1);
    tl_format(buf, (size_t)n + 1, fmt, ap);
    *len = n;
    return buf;
}

/* --- the printf family, as variadic stubs and va_list-taking functions --- */

int tl_vai_snprintf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 3, 0, &ap);
    return tl_format((char *)f->gp[0], (size_t)f->gp[1], (const char *)f->gp[2], &ap);
}
TL_VA_STUB(tl_va_snprintf, tl_vai_snprintf);
extern void tl_va_snprintf(void);

int tl_vai_sprintf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 2, 0, &ap);
    return tl_format((char *)f->gp[0], (size_t)0x7fffffff, (const char *)f->gp[1], &ap);
}
TL_VA_STUB(tl_va_sprintf, tl_vai_sprintf);
extern void tl_va_sprintf(void);

int tl_vai_printf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 1, 0, &ap);
    int n; char *s = format_alloc((const char *)f->gp[0], &ap, &n);
    emit(stdout, s, (size_t)n);
    free(s);
    return n;
}
TL_VA_STUB(tl_va_printf, tl_vai_printf);
extern void tl_va_printf(void);

int tl_vai_fprintf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 2, 0, &ap);
    int n; char *s = format_alloc((const char *)f->gp[1], &ap, &n);
    TL_ERRNO_BEGIN();
    emit(map_stream((void *)f->gp[0]), s, (size_t)n);
    TL_ERRNO_END();
    free(s);
    return n;
}
TL_VA_STUB(tl_va_fprintf, tl_vai_fprintf);
extern void tl_va_fprintf(void);

int tl_vai_swprintf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 3, 0, &ap);
    return tl_format_wide((wchar_t *)f->gp[0], (size_t)f->gp[1], (const wchar_t *)f->gp[2], &ap);
}
TL_VA_STUB(tl_va_swprintf, tl_vai_swprintf);
extern void tl_va_swprintf(void);

int tl_vai_sscanf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 2, 0, &ap);
    return tl_scan_string((const char *)f->gp[0], (const char *)f->gp[1], &ap);
}
TL_VA_STUB(tl_va_sscanf, tl_vai_sscanf);
extern void tl_va_sscanf(void);

int tl_vai_fscanf(tl_va_frame *f)
{
    tl_va_list ap; tl_va_start(f, 2, 0, &ap);
    void *a[16];
    for (int i = 0; i < 16; i++) a[i] = (void *)(uintptr_t)tl_va_arg_u64(&ap);
    TL_ERRNO_BEGIN();
    int r = fscanf(map_stream((void *)f->gp[0]), (const char *)f->gp[1], a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7],
                   a[8], a[9], a[10], a[11], a[12], a[13], a[14], a[15]);
    TL_ERRNO_END();
    return r;
}
TL_VA_STUB(tl_va_fscanf, tl_vai_fscanf);
extern void tl_va_fscanf(void);

static int b_vsnprintf(char *s, size_t n, const char *fmt, tl_va_list *ap) { return tl_format(s, n, fmt, ap); }
static int b___vsnprintf_chk(char *s, size_t n, int flag, size_t slen, const char *fmt, tl_va_list *ap)
{
    (void)flag;
    if (n > slen) { tl_log_line("bionic: __vsnprintf_chk: buffer %zu smaller than limit %zu", slen, n); abort(); }
    return tl_format(s, n, fmt, ap);
}
static int b___vsprintf_chk(char *s, int flag, size_t slen, const char *fmt, tl_va_list *ap)
{
    (void)flag;
    int n = tl_format(s, slen ? slen : (size_t)0x7fffffff, fmt, ap);
    if (slen && (size_t)n >= slen) { tl_log_line("bionic: __vsprintf_chk overflow"); abort(); }
    return n;
}
static int b_vprintf(const char *fmt, tl_va_list *ap)
{
    int n; char *s = format_alloc(fmt, ap, &n);
    emit(stdout, s, (size_t)n);
    free(s);
    return n;
}
static int b_vfprintf(void *f, const char *fmt, tl_va_list *ap)
{
    int n; char *s = format_alloc(fmt, ap, &n);
    TL_ERRNO_BEGIN();
    emit(map_stream(f), s, (size_t)n);
    TL_ERRNO_END();
    free(s);
    return n;
}
static int b_vasprintf(char **out, const char *fmt, tl_va_list *ap)
{
    int n; *out = format_alloc(fmt, ap, &n);
    return n;
}
static int b_vsscanf(const char *s, const char *fmt, tl_va_list *ap) { return tl_scan_string(s, fmt, ap); }

/* --- plain stdio --- */

static void *b_fopen(const char *path, const char *mode)
{
    char buf[1024];
    int sfd = tl_synth_open(path);
    if (sfd >= 0) return fdopen(sfd, mode[0] == 'r' ? "r" : "r");
    const char *real = tl_path_resolve(path, buf, sizeof(buf));
    TL_ERRNO_BEGIN();
    FILE *f = NULL;
    /* "w" and "w+" rewrite a file: done crash-safe, as open(O_TRUNC) is (tl_atomic_open). */
    if (mode[0] == 'w') {
        int fd = tl_atomic_open(real, (strchr(mode, '+') ? O_RDWR : O_WRONLY) | O_CREAT | O_TRUNC, 0666);
        if (fd >= 0 && !(f = fdopen(fd, mode))) { close(fd); tl_atomic_abandon(fd); }
    }
    if (!f) f = fopen(real, mode);
    TL_ERRNO_END();
    return f;
}
/* freopen: a game that sends its console output to a file (Wesnoth's log file) would otherwise redirect the runtime's own log; its standard streams are left where they are. */
static void *b_freopen(const char *path, const char *mode, void *stream)
{
    FILE *f = map_stream(stream);
    if (is_log_stream(f) || f == stdin) return stream;
    char buf[1024];
    TL_ERRNO_BEGIN(); FILE *r = freopen(tl_path_resolve(path, buf, sizeof(buf)), mode, f); TL_ERRNO_END();
    return r;
}
static void *b_fdopen(int fd, const char *mode) { TL_ERRNO_BEGIN(); FILE *f = fdopen(fd, mode); TL_ERRNO_END(); return f; }
static int b_fclose(void *f) { FILE *h = map_stream(f); int fd = fileno(h); TL_ERRNO_BEGIN(); int r = fclose(h); tl_atomic_closed(fd); TL_ERRNO_END(); return r; }
static char *b_fgets(char *s, int n, void *f) { TL_ERRNO_BEGIN(); char *r = fgets(s, n, map_stream(f)); TL_ERRNO_END(); return r; }
static size_t b_fread(void *p, size_t sz, size_t n, void *f) { TL_ERRNO_BEGIN(); size_t r = fread(p, sz, n, map_stream(f)); TL_ERRNO_END(); return r; }
static size_t b_fwrite(const void *p, size_t sz, size_t n, void *f)
{
    FILE *s = map_stream(f);
    if (is_log_stream(s)) { console_write(s, p, sz * n); return n; }
    TL_ERRNO_BEGIN(); size_t r = fwrite(p, sz, n, s); TL_ERRNO_END(); return r;
}
static int b_fseek(void *f, long off, int whence) { TL_ERRNO_BEGIN(); int r = fseek(map_stream(f), off, whence); TL_ERRNO_END(); return r; }
static int b_fseeko(void *f, long off, int whence) { TL_ERRNO_BEGIN(); int r = fseeko(map_stream(f), off, whence); TL_ERRNO_END(); return r; }
static long b_ftell(void *f) { TL_ERRNO_BEGIN(); long r = ftell(map_stream(f)); TL_ERRNO_END(); return r; }
static long b_ftello(void *f) { TL_ERRNO_BEGIN(); long r = ftello(map_stream(f)); TL_ERRNO_END(); return r; }
static int b_fflush(void *f)
{
    if (!f) { TL_ERRNO_BEGIN(); int r = fflush(NULL); TL_ERRNO_END(); return r; }
    FILE *s = map_stream(f);
    if (is_log_stream(s)) return 0;
    TL_ERRNO_BEGIN(); int r = fflush(s); TL_ERRNO_END(); return r;
}
static int b_fputc(int c, void *f)
{
    FILE *s = map_stream(f);
    if (is_log_stream(s)) { char ch = (char)c; console_write(s, &ch, 1); return c & 0xff; }
    TL_ERRNO_BEGIN(); int r = fputc(c, s); TL_ERRNO_END(); return r;
}
static int b_fputs(const char *str, void *f)
{
    FILE *s = map_stream(f);
    if (is_log_stream(s)) { console_write(s, str, strlen(str)); return 1; }
    TL_ERRNO_BEGIN(); int r = fputs(str, s); TL_ERRNO_END(); return r;
}
static int b_putchar(int c) { char ch = (char)c; console_write(stdout, &ch, 1); return c & 0xff; }
static int b_puts(const char *str) { console_write(stdout, str, strlen(str)); console_write(stdout, "\n", 1); return 1; }
static size_t b_wcsftime(wchar_t *buf, size_t n, const wchar_t *fmt, const struct tm *tm) { return wcsftime(buf, n, fmt, tm); }
static int b_putc(int c, void *f) { return b_fputc(c, f); }
static int b_getc(void *f) { TL_ERRNO_BEGIN(); int r = fgetc(map_stream(f)); TL_ERRNO_END(); return r; }
static int b_ungetc(int c, void *f) { return ungetc(c, map_stream(f)); }
static wint_t b_getwc(void *f) { return fgetwc(map_stream(f)); }
static wint_t b_putwc(wchar_t c, void *f) { return fputwc(c, map_stream(f)); }
static wint_t b_ungetwc(wint_t c, void *f) { return ungetwc(c, map_stream(f)); }
static int b_vsprintf(char *s, const char *fmt, tl_va_list *ap) { return tl_format(s, (size_t)0x7fffffff, fmt, ap); }
static int b_feof(void *f) { return feof(map_stream(f)); }
static int b_ferror(void *f) { return ferror(map_stream(f)); }
static void b_clearerr(void *f) { clearerr(map_stream(f)); }
static int b_fileno(void *f)
{
    FILE *s = map_stream(f);
    return s == stdin ? 0 : s == stdout ? 1 : s == stderr ? 2 : fileno(s);
}
static void b_setbuf(void *f, char *buf) { setbuf(map_stream(f), buf); }
static int b_setvbuf(void *f, char *buf, int mode, size_t size)
{
    /* bionic: _IOFBF 0, _IOLBF 1, _IONBF 2; Darwin: _IOFBF 0, _IOLBF 1, _IONBF 2 -- same. */
    return setvbuf(map_stream(f), buf, mode, size);
}
static int b_remove(const char *p) { char buf[1024]; TL_ERRNO_BEGIN(); int r = remove(tl_path_resolve(p, buf, sizeof(buf))); TL_ERRNO_END(); return r; }
static int b_rename(const char *a, const char *b)
{
    char x[1024], y[1024];
    TL_ERRNO_BEGIN(); int r = rename(tl_path_resolve(a, x, sizeof(x)), tl_path_resolve(b, y, sizeof(y))); TL_ERRNO_END(); return r;
}

/* ----------------------------------------------------------- _chk variants */

static void chk_fail(const char *what) { tl_log_line("bionic: %s: buffer overflow detected", what); abort(); }
static void *b___memcpy_chk(void *d, const void *s, size_t n, size_t dl) { if (n > dl) chk_fail("__memcpy_chk"); return memcpy(d, s, n); }
static void *b___memmove_chk(void *d, const void *s, size_t n, size_t dl) { if (n > dl) chk_fail("__memmove_chk"); return memmove(d, s, n); }
static void *b___memset_chk(void *d, int c, size_t n, size_t dl) { if (n > dl) chk_fail("__memset_chk"); return memset(d, c, n); }
static size_t b___strlen_chk(const char *s, size_t dl) { size_t n = strlen(s); if (n >= dl) chk_fail("__strlen_chk"); return n; }
static char *b___strchr_chk(const char *s, int c, size_t dl) { char *r = strchr(s, c); if (r && (size_t)(r - s) >= dl) chk_fail("__strchr_chk"); return r; }
static char *b___strncat_chk(char *d, const char *s, size_t n, size_t dl)
{
    if (strlen(d) + (strnlen(s, n)) + 1 > dl) chk_fail("__strncat_chk");
    return strncat(d, s, n);
}
static char *b___strncpy_chk2(char *d, const char *s, size_t n, size_t dl, size_t sl)
{
    (void)sl;
    if (n > dl) chk_fail("__strncpy_chk2");
    return strncpy(d, s, n);
}

/* ------------------------------------------------------------------ ctype */

static char g_ctype[257];
static char **g_environ_var;

__attribute__((constructor)) static void init_tables(void)
{
    enum { U = 1, L = 2, N = 4, S = 8, P = 0x10, C = 0x20, X = 0x40, B = 0x80 };
    memset(g_ctype, 0, sizeof(g_ctype));
    for (int c = 0; c < 128; c++) {
        int f = 0;
        if (c >= 'A' && c <= 'Z') f |= U;
        if (c >= 'a' && c <= 'z') f |= L;
        if (c >= '0' && c <= '9') f |= N;
        if (c == ' ' || (c >= 9 && c <= 13)) f |= S;
        if (c < 32 || c == 127) f |= C;
        if (c > 32 && c < 127 && !isalnum(c)) f |= P;
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) f |= X;
        if (c == ' ') f |= B;
        g_ctype[c + 1] = (char)f;
    }
    g_environ_var = *_NSGetEnviron();
}
static int b___ctype_get_mb_cur_max(void) { return 4; }

/* -------------------------------------------------------------- wide chars */

static size_t b_mbrtowc(wchar_t *pwc, const char *s, size_t n, void *ps)
{
    (void)ps;
    if (!s) return 0;
    if (n == 0) return (size_t)-2;
    unsigned char c = (unsigned char)s[0];
    uint32_t cp; size_t len;
    if (c < 0x80) { cp = c; len = 1; }
    else if ((c >> 5) == 6) { cp = c & 0x1f; len = 2; }
    else if ((c >> 4) == 14) { cp = c & 0x0f; len = 3; }
    else if ((c >> 3) == 30) { cp = c & 0x07; len = 4; }
    else { tl_set_guest_errno(84); return (size_t)-1; }
    if (n < len) return (size_t)-2;
    for (size_t i = 1; i < len; i++) {
        if (((unsigned char)s[i] >> 6) != 2) { tl_set_guest_errno(84); return (size_t)-1; }
        cp = (cp << 6) | ((unsigned char)s[i] & 0x3f);
    }
    if (pwc) *pwc = (wchar_t)cp;
    return cp == 0 ? 0 : len;
}
static size_t b_mbrlen(const char *s, size_t n, void *ps) { return b_mbrtowc(NULL, s, n, ps); }
static int b_mbtowc(wchar_t *pwc, const char *s, size_t n)
{
    if (!s) return 0;
    size_t r = b_mbrtowc(pwc, s, n, NULL);
    return (r == (size_t)-1 || r == (size_t)-2) ? -1 : (int)r;
}
static size_t b_wcrtomb(char *s, wchar_t wc, void *ps)
{
    (void)ps;
    if (!s) return 1;
    uint32_t c = (uint32_t)wc;
    if (c < 0x80) { s[0] = (char)c; return 1; }
    if (c < 0x800) { s[0] = (char)(0xC0 | (c >> 6)); s[1] = (char)(0x80 | (c & 0x3F)); return 2; }
    if (c < 0x10000) { s[0] = (char)(0xE0 | (c >> 12)); s[1] = (char)(0x80 | ((c >> 6) & 0x3F)); s[2] = (char)(0x80 | (c & 0x3F)); return 3; }
    if (c < 0x110000) {
        s[0] = (char)(0xF0 | (c >> 18)); s[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        s[2] = (char)(0x80 | ((c >> 6) & 0x3F)); s[3] = (char)(0x80 | (c & 0x3F));
        return 4;
    }
    tl_set_guest_errno(84);
    return (size_t)-1;
}
static size_t b_mbsnrtowcs(wchar_t *dst, const char **src, size_t nms, size_t len, void *ps)
{
    const char *s = *src; size_t out = 0;
    while (nms && (!dst || out < len)) {
        wchar_t w;
        size_t r = b_mbrtowc(&w, s, nms, ps);
        if (r == (size_t)-1) { *src = s; return r; }
        if (r == (size_t)-2) break;
        if (w == 0) { if (dst) { dst[out] = 0; *src = NULL; } return out; }
        if (dst) dst[out] = w;
        out++; s += r; nms -= r;
    }
    if (dst) *src = s;
    return out;
}
static size_t b_mbsrtowcs(wchar_t *dst, const char **src, size_t len, void *ps) { return b_mbsnrtowcs(dst, src, (size_t)-1, len, ps); }
static size_t b_wcsnrtombs(char *dst, const wchar_t **src, size_t nwc, size_t len, void *ps)
{
    const wchar_t *s = *src; size_t out = 0;
    while (nwc--) {
        char tmp[4];
        size_t r = b_wcrtomb(tmp, *s, ps);
        if (r == (size_t)-1) { *src = s; return r; }
        if (*s == 0) { if (dst && out < len) dst[out] = 0; if (dst) *src = NULL; return out; }
        if (dst) { if (out + r > len) break; memcpy(dst + out, tmp, r); }
        out += r; s++;
    }
    if (dst) *src = s;
    return out;
}
static wint_t b_btowc(int c) { return (c >= 0 && c < 0x80) ? (wint_t)c : WEOF; }
static int b_wctob(wint_t c) { return c < 0x80 ? (int)c : EOF; }

/* ------------------------------------------------------------ strtold: quad */

typedef unsigned long long v2u64 __attribute__((ext_vector_type(2)));

/* A guest `long double` is an IEEE binary128 returned in q0; Darwin's is a double. */
static v2u64 to_quad(double d)
{
    uint64_t b; memcpy(&b, &d, 8);
    uint64_t sign = b >> 63; int e = (int)((b >> 52) & 0x7ff); uint64_t m = b & ((1ull << 52) - 1);
    unsigned __int128 q = 0;
    if (e == 0x7ff) {
        q = ((unsigned __int128)sign << 127) | ((unsigned __int128)0x7fff << 112) | ((unsigned __int128)m << 60);
    } else if (e == 0 && m == 0) {
        q = (unsigned __int128)sign << 127;
    } else if (e == 0) {
        int p = 63 - __builtin_clzll(m);
        unsigned __int128 frac = (unsigned __int128)(m ^ (1ull << p)) << (112 - p);
        q = ((unsigned __int128)sign << 127) | ((unsigned __int128)(p - 1074 + 16383) << 112) | frac;
    } else {
        q = ((unsigned __int128)sign << 127) | ((unsigned __int128)(e - 1023 + 16383) << 112) | ((unsigned __int128)m << 60);
    }
    v2u64 r = { (uint64_t)q, (uint64_t)(q >> 64) };
    return r;
}
static v2u64 b_strtold(const char *s, char **end) { TL_ERRNO_BEGIN(); double d = strtod(s, end); TL_ERRNO_END(); return to_quad(d); }
static v2u64 b_strtold_l(const char *s, char **end, locale_t l) { TL_ERRNO_BEGIN(); double d = strtod_l(s, end, l); TL_ERRNO_END(); return to_quad(d); }
static v2u64 b_wcstold(const wchar_t *s, wchar_t **end) { TL_ERRNO_BEGIN(); double d = wcstod(s, end); TL_ERRNO_END(); return to_quad(d); }

/* -------------------------------------------------------- strtol & friends */

static long b_strtol(const char *s, char **e, int b) { TL_ERRNO_BEGIN(); long r = strtol(s, e, b); TL_ERRNO_END(); return r; }
static long long b_strtoll(const char *s, char **e, int b) { TL_ERRNO_BEGIN(); long long r = strtoll(s, e, b); TL_ERRNO_END(); return r; }
static unsigned long b_strtoul(const char *s, char **e, int b) { TL_ERRNO_BEGIN(); unsigned long r = strtoul(s, e, b); TL_ERRNO_END(); return r; }
static unsigned long long b_strtoull(const char *s, char **e, int b) { TL_ERRNO_BEGIN(); unsigned long long r = strtoull(s, e, b); TL_ERRNO_END(); return r; }
static long long b_strtoll_l(const char *s, char **e, int b, locale_t l) { TL_ERRNO_BEGIN(); long long r = strtoll_l(s, e, b, l); TL_ERRNO_END(); return r; }
static unsigned long long b_strtoull_l(const char *s, char **e, int b, locale_t l) { TL_ERRNO_BEGIN(); unsigned long long r = strtoull_l(s, e, b, l); TL_ERRNO_END(); return r; }
static double b_strtod(const char *s, char **e) { TL_ERRNO_BEGIN(); double r = strtod(s, e); TL_ERRNO_END(); return r; }
static float b_strtof(const char *s, char **e) { TL_ERRNO_BEGIN(); float r = strtof(s, e); TL_ERRNO_END(); return r; }

/* ------------------------------------------------------------------ misc */

static void *b_memalign(size_t align, size_t size)
{
    void *p = NULL;
    if (align < sizeof(void *)) align = sizeof(void *);
    return posix_memalign(&p, align, size) ? NULL : p;
}

/* The floating-point environment: bionic's fenv_t is two words of its own layout, and nothing here turns on exceptions or rounding, so these report success. */
static int b_fe_env(void *env) { if (env) memset(env, 0, 8); return 0; }
static int b_fe_ok(void) { return 0; }

static void *b_memrchr(const void *s, int c, size_t n)
{
    const unsigned char *p = (const unsigned char *)s + n;
    while (n--) if (*--p == (unsigned char)c) return (void *)p;
    return NULL;
}

static char *b_basename(const char *path)
{
    static __thread char buf[1024];
    if (!path || !*path) return ".";
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') n--;
    size_t start = n;
    while (start > 0 && path[start - 1] != '/') start--;
    if (n == 1 && path[0] == '/') return "/";
    size_t len = n - start;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, path + start, len);
    buf[len] = 0;
    return buf;
}

static void b_sincos(double x, double *s, double *c) { *s = sin(x); *c = cos(x); }
static int b___isnanf(float x) { return isnan(x); }
/* bionic's FP_* are 0 nan, 1 infinite, 2 zero, 3 subnormal, 4 normal */
static int b___fpclassifyd(double x) { switch (fpclassify(x)) { case FP_NAN: return 0; case FP_INFINITE: return 1; case FP_ZERO: return 2; case FP_SUBNORMAL: return 3; default: return 4; } }
static void b_sincosf(float x, float *s, float *c) { *s = sinf(x); *c = cosf(x); }

static const char *b_strerror(int e) { return strerror(tl_errno_from_guest(e)); }
static int b_strerror_r(int e, char *buf, size_t n)
{
    int r = strerror_r(tl_errno_from_guest(e), buf, n);
    return r ? tl_errno_to_guest(r) : 0;
}
static const char *b_strsignal(int sig)
{
    static __thread char buf[32];
    int d = tl_signal_to_darwin(sig);
    if (d > 0 && d < NSIG) return strsignal(d);
    snprintf(buf, sizeof(buf), "Unknown signal %d", sig);
    return buf;
}

/* LC_* categories and masks differ. */
static int cat_to_darwin(int c)
{
    switch (c) { case 0: return LC_CTYPE; case 1: return LC_NUMERIC; case 2: return LC_TIME; case 3: return LC_COLLATE;
                 case 4: return LC_MONETARY; case 5: return LC_MESSAGES; default: return LC_ALL; }
}
static char *b_setlocale(int cat, const char *name) { return setlocale(cat_to_darwin(cat), name); }
static locale_t b_newlocale(int mask, const char *name, locale_t base)
{
    int dm = 0;
    if (mask & 1)  dm |= LC_CTYPE_MASK;
    if (mask & 2)  dm |= LC_NUMERIC_MASK;
    if (mask & 4)  dm |= LC_TIME_MASK;
    if (mask & 8)  dm |= LC_COLLATE_MASK;
    if (mask & 16) dm |= LC_MONETARY_MASK;
    if (mask & 32) dm |= LC_MESSAGES_MASK;
    TL_ERRNO_BEGIN(); locale_t l = newlocale(dm, name, base); TL_ERRNO_END();
    return l;
}

/* time zone variables bionic exports */
static int g_daylight;
static long g_timezone;
static char *g_tzname[2] = { "UTC", "UTC" };

/* sys_signame: Linux names by number. */
static const char *g_sys_signame[32] = { "Unknown signal", "HUP", "INT", "QUIT", "ILL", "TRAP", "ABRT", "BUS", "FPE", "KILL",
    "USR1", "SEGV", "USR2", "PIPE", "ALRM", "TERM", "STKFLT", "CHLD", "CONT", "STOP", "TSTP", "TTIN", "TTOU", "URG", "XCPU",
    "XFSZ", "VTALRM", "PROF", "WINCH", "IO", "PWR", "SYS" };

/* ---------------------------------------------------------------- tables */

const tl_bionic_entry tl_tab_str[] = {
    /* string.h */
    TL_DIRECT(memchr), TL_DIRECT(memcmp), TL_DIRECT(memcpy), TL_DIRECT(memmove), TL_DIRECT(memset),
    TL_WRAP("memrchr", b_memrchr), TL_DIRECT(strcasecmp), TL_DIRECT(strcasestr), TL_DIRECT(strcat),
    TL_DIRECT(strchr), TL_DIRECT(strcmp), TL_DIRECT(strcoll), TL_DIRECT(strcoll_l), TL_DIRECT(strcpy),
    TL_DIRECT(strcspn), TL_DIRECT(strdup), TL_DIRECT(strlcpy), TL_DIRECT(strlcat), TL_DIRECT(strlen), TL_DIRECT(strncmp),
    TL_DIRECT(strncpy), TL_DIRECT(strnlen), TL_DIRECT(strpbrk), TL_DIRECT(strrchr), TL_DIRECT(strspn),
    TL_DIRECT(strstr), TL_DIRECT(strtok_r), TL_DIRECT(strxfrm), TL_DIRECT(strxfrm_l),
    TL_WRAP("strerror", b_strerror), TL_WRAP("strerror_r", b_strerror_r), TL_WRAP("strsignal", b_strsignal),
    TL_WRAP("basename", b_basename),
    TL_WRAP("__memcpy_chk", b___memcpy_chk), TL_WRAP("__memmove_chk", b___memmove_chk), TL_WRAP("__memset_chk", b___memset_chk),
    TL_WRAP("__strlen_chk", b___strlen_chk), TL_WRAP("__strchr_chk", b___strchr_chk), TL_WRAP("__strncat_chk", b___strncat_chk),
    TL_WRAP("__strncpy_chk2", b___strncpy_chk2),
    /* ctype.h, wctype.h */
    TL_DIRECT(isalnum), TL_DIRECT(isalpha), TL_DIRECT(islower), TL_DIRECT(isupper), TL_DIRECT(isxdigit),
    TL_DIRECT(tolower), TL_DIRECT(toupper), TL_DIRECT(isdigit_l), TL_DIRECT(islower_l), TL_DIRECT(isupper_l),
    TL_DIRECT(isxdigit_l), TL_DIRECT(tolower_l), TL_DIRECT(toupper_l),
    TL_DIRECT(isspace), TL_DIRECT(isprint), TL_DIRECT(isgraph), TL_DIRECT(iscntrl), TL_DIRECT(ispunct), TL_DIRECT(isblank),
    TL_DIRECT(iswctype), TL_DIRECT(wctype), TL_DIRECT(strncasecmp), TL_DIRECT(strtok), TL_DIRECT(atof),
    TL_DIRECT(iswalpha), TL_DIRECT(iswalnum), TL_DIRECT(iswblank), TL_DIRECT(iswcntrl), TL_DIRECT(iswdigit), TL_DIRECT(iswlower),
    TL_DIRECT(iswprint), TL_DIRECT(iswpunct), TL_DIRECT(iswspace), TL_DIRECT(iswupper), TL_DIRECT(iswxdigit),
    TL_DIRECT(iswalpha_l), TL_DIRECT(iswblank_l), TL_DIRECT(iswcntrl_l), TL_DIRECT(iswdigit_l), TL_DIRECT(iswlower_l),
    TL_DIRECT(iswprint_l), TL_DIRECT(iswpunct_l), TL_DIRECT(iswspace_l), TL_DIRECT(iswupper_l), TL_DIRECT(iswxdigit_l),
    TL_DIRECT(towlower), TL_DIRECT(towupper), TL_DIRECT(towlower_l), TL_DIRECT(towupper_l),
    TL_DATA("_ctype_", g_ctype), TL_WRAP("__ctype_get_mb_cur_max", b___ctype_get_mb_cur_max),
    /* wchar.h */
    TL_WRAP("mbrtowc", b_mbrtowc), TL_WRAP("mbrlen", b_mbrlen), TL_WRAP("mbtowc", b_mbtowc), TL_WRAP("wcrtomb", b_wcrtomb),
    TL_WRAP("mbsnrtowcs", b_mbsnrtowcs), TL_WRAP("mbsrtowcs", b_mbsrtowcs), TL_WRAP("wcsnrtombs", b_wcsnrtombs),
    TL_WRAP("btowc", b_btowc), TL_WRAP("wctob", b_wctob),
    TL_DIRECT(wcschr), TL_DIRECT(wcsrchr), TL_DIRECT(wcscpy), TL_DIRECT(wcsncpy), TL_DIRECT(wcscat), TL_DIRECT(wcsncat), TL_DIRECT(wcscmp), TL_DIRECT(wcsncmp),
    TL_DIRECT(wcsstr), TL_DIRECT(wcsspn), TL_DIRECT(wcscspn), TL_DIRECT(wcspbrk), TL_DIRECT(wcsdup), TL_DIRECT(wcsnlen),
    TL_DIRECT(wcslen), TL_DIRECT(wmemchr), TL_DIRECT(wmemcmp), TL_DIRECT(wmemcpy), TL_DIRECT(wmemmove), TL_DIRECT(wmemset),
    TL_DIRECT(wcscoll), TL_DIRECT(wcscoll_l), TL_DIRECT(wcsxfrm), TL_DIRECT(wcsxfrm_l),
    TL_DIRECT(wcstod), TL_DIRECT(wcstof), TL_DIRECT(wcstol), TL_DIRECT(wcstoll), TL_DIRECT(wcstoul), TL_DIRECT(wcstoull),
    TL_WRAP("wcstold", b_wcstold),
    TL_WRAP("swprintf", tl_va_swprintf),
    /* stdlib.h */
    TL_DIRECT(malloc), TL_DIRECT(calloc), TL_DIRECT(realloc), TL_DIRECT(free), TL_DIRECT(posix_memalign),
    TL_WRAP("memalign", b_memalign),
    TL_DIRECT(atoi), TL_DIRECT(atol), TL_DIRECT(atoll), TL_DIRECT(bsearch), TL_DIRECT(qsort), TL_DIRECT(div), TL_DIRECT(lldiv),
    TL_DIRECT(rand), TL_DIRECT(srand), TL_DIRECT(srand48), TL_DIRECT(lrand48),
    TL_WRAP("strtol", b_strtol), TL_WRAP("strtoll", b_strtoll), TL_WRAP("strtoul", b_strtoul), TL_WRAP("strtoull", b_strtoull),
    TL_WRAP("strtoll_l", b_strtoll_l), TL_WRAP("strtoull_l", b_strtoull_l), TL_WRAP("strtod", b_strtod), TL_WRAP("strtof", b_strtof),
    TL_WRAP("strtold", b_strtold), TL_WRAP("strtold_l", b_strtold_l),
    TL_DIRECT(getenv), TL_DIRECT(setenv), TL_DIRECT(unsetenv), TL_DATA("environ", &g_environ_var),
    TL_DIRECT(setjmp), TL_DIRECT(longjmp), TL_DIRECT(fnmatch), TL_DIRECT(getopt_long),
    TL_DATA("optarg", &optarg), TL_DATA("optind", &optind),
    /* math.h */
    TL_DIRECT(acos), TL_DIRECT(acosf), TL_DIRECT(asin), TL_DIRECT(asinf), TL_DIRECT(atan), TL_DIRECT(atan2), TL_DIRECT(atan2f),
    TL_DIRECT(atanf), TL_DIRECT(cbrtf), TL_DIRECT(cos), TL_DIRECT(cosf), TL_DIRECT(exp), TL_DIRECT(exp2f), TL_DIRECT(expf),
    TL_DIRECT(fmod), TL_DIRECT(fmodf), TL_DIRECT(hypot), TL_DIRECT(ldexp), TL_DIRECT(ldexpf), TL_DIRECT(log), TL_DIRECT(log10),
    TL_DIRECT(log10f), TL_DIRECT(log2), TL_DIRECT(log2f), TL_DIRECT(logb), TL_DIRECT(logf), TL_DIRECT(modf), TL_DIRECT(modff),
    TL_DIRECT(pow), TL_DIRECT(powf), TL_DIRECT(scalbn), TL_DIRECT(sin), TL_DIRECT(sinf), TL_DIRECT(sqrtf), TL_DIRECT(tan),
    TL_DIRECT(tanf), TL_WRAP("sincosf", b_sincosf), TL_WRAP("sincos", b_sincos),
    TL_DIRECT(sqrt), TL_DIRECT(fmin), TL_DIRECT(fmax), TL_DIRECT(frexp), TL_DIRECT(asinh), TL_DIRECT(tanh),
    TL_WRAP("__isnanf", b___isnanf), TL_WRAP("__fpclassifyd", b___fpclassifyd),
    /* locale.h */
    TL_WRAP("setlocale", b_setlocale), TL_WRAP("newlocale", b_newlocale), TL_DIRECT(freelocale), TL_DIRECT(uselocale),
    TL_DIRECT(localeconv),
    /* time zone data */
    TL_DATA("daylight", &g_daylight), TL_DATA("timezone", &g_timezone), TL_DATA("tzname", g_tzname),
    TL_DATA("sys_signame", g_sys_signame),
    /* stdio.h */
    TL_DATA("__sF", g_sF),
    TL_WRAP("fopen", b_fopen), TL_WRAP("freopen", b_freopen), TL_WRAP("freopen64", b_freopen), TL_WRAP("fopen64", b_fopen), TL_WRAP("fseeko64", b_fseeko), TL_WRAP("ftello64", b_ftello), TL_DIRECT(funopen), TL_DIRECT(wcwidth), TL_WRAP("fdopen", b_fdopen), TL_WRAP("fclose", b_fclose), TL_WRAP("fgets", b_fgets),
    TL_WRAP("fread", b_fread), TL_WRAP("fwrite", b_fwrite), TL_WRAP("fseek", b_fseek), TL_WRAP("fseeko", b_fseeko),
    TL_WRAP("ftell", b_ftell), TL_WRAP("ftello", b_ftello), TL_WRAP("fflush", b_fflush), TL_WRAP("fputc", b_fputc),
    TL_WRAP("putc", b_putc), TL_WRAP("getc", b_getc), TL_WRAP("fgetc", b_getc), TL_WRAP("ungetc", b_ungetc), TL_WRAP("getwc", b_getwc),
    TL_WRAP("putwc", b_putwc), TL_WRAP("ungetwc", b_ungetwc), TL_WRAP("vsprintf", b_vsprintf), TL_WRAP("wcsftime", b_wcsftime),
    TL_WRAP("fputs", b_fputs), TL_WRAP("puts", b_puts), TL_WRAP("feof", b_feof), TL_WRAP("ferror", b_ferror),
    TL_WRAP("clearerr", b_clearerr), TL_WRAP("fileno", b_fileno), TL_WRAP("setbuf", b_setbuf), TL_WRAP("setvbuf", b_setvbuf),
    TL_WRAP("remove", b_remove), TL_WRAP("rename", b_rename),
    TL_WRAP("snprintf", tl_va_snprintf), TL_WRAP("sprintf", tl_va_sprintf), TL_WRAP("printf", tl_va_printf),
    TL_WRAP("fprintf", tl_va_fprintf), TL_WRAP("sscanf", tl_va_sscanf), TL_WRAP("fscanf", tl_va_fscanf),
    TL_WRAP("vsnprintf", b_vsnprintf), TL_WRAP("__vsnprintf_chk", b___vsnprintf_chk), TL_WRAP("__vsprintf_chk", b___vsprintf_chk),
    TL_WRAP("vprintf", b_vprintf), TL_WRAP("vfprintf", b_vfprintf), TL_WRAP("vasprintf", b_vasprintf), TL_WRAP("vsscanf", b_vsscanf),
    TL_END
};

/* -------------------------------------------- fortified string functions, stdio odds, more math */

static char *b___strcpy_chk(char *d, const char *s, size_t dl) { size_t n = strlen(s); if (n >= dl) chk_fail("__strcpy_chk"); memcpy(d, s, n + 1); return d; }
static char *b___strcat_chk(char *d, const char *s, size_t dl) { size_t a = strlen(d), n = strlen(s); if (a + n >= dl) chk_fail("__strcat_chk"); memcpy(d + a, s, n + 1); return d; }
static char *b___strncpy_chk(char *d, const char *s, size_t n, size_t dl) { if (n > dl) chk_fail("__strncpy_chk"); return strncpy(d, s, n); }
static size_t b___strlcat_chk(char *d, const char *s, size_t n, size_t dl) { if (n > dl) chk_fail("__strlcat_chk"); return strlcat(d, s, n); }
static char *b___stpcpy_chk(char *d, const char *s, size_t dl) { size_t n = strlen(s); if (n + 1 > dl) chk_fail("__stpcpy_chk"); return stpcpy(d, s); }
static size_t b___strlcpy_chk(char *d, const char *s, size_t n, size_t dl) { if (n > dl) chk_fail("__strlcpy_chk"); return strlcpy(d, s, n); }
static char *b___strrchr_chk(const char *s, int c, size_t len) { (void)len; return strrchr(s, c); }
static char *b___fgets_chk(char *buf, int size, size_t bufsize, void *f) { if ((size_t)size > bufsize) chk_fail("__fgets_chk"); return b_fgets(buf, size, f); }
/* Linux's fd_set is an array of longs, a bit per descriptor */
static void b___FD_CLR_chk(int fd, uint64_t *set, size_t size) { if (fd < 0 || (size_t)fd >= size * 8) chk_fail("__FD_CLR_chk"); set[fd / 64] &= ~(1ull << (fd % 64)); }

static FILE *g_stdin_var = (FILE *)&g_sF[0], *g_stdout_var = (FILE *)&g_sF[SF_SIZE], *g_stderr_var = (FILE *)&g_sF[2 * SF_SIZE];
static void b_perror(const char *msg) { tl_log_line("perror: %s: %s", msg ? msg : "", strerror(errno)); }
static void b_rewind(void *f) { rewind(map_stream(f)); }
static wint_t b_fputwc(wchar_t c, void *f) { return fputwc(c, map_stream(f)); }
static void *b_popen(const char *cmd, const char *mode) { (void)cmd; (void)mode; tl_set_guest_errno(38); return NULL; }
static int b_pclose(void *f) { (void)f; tl_set_guest_errno(10); return -1; }
static FILE *b_tmpfile(void) { TL_ERRNO_BEGIN(); FILE *f = tmpfile(); TL_ERRNO_END(); return f; }
/* bionic's mbstate_t is 8 bytes, Darwin's is not; the conversion state is dropped */
static size_t b_wcsrtombs(char *dst, const wchar_t **src, size_t len, void *ps) { (void)ps; return wcsrtombs(dst, src, len, NULL); }

const tl_bionic_entry tl_tab_str2[] = {
    TL_WRAP("__strcpy_chk", b___strcpy_chk), TL_WRAP("__strcat_chk", b___strcat_chk), TL_WRAP("__strncpy_chk", b___strncpy_chk),
    TL_WRAP("__stpcpy_chk", b___stpcpy_chk), TL_WRAP("putchar", b_putchar), TL_WRAP("__strlcpy_chk", b___strlcpy_chk), TL_WRAP("__strlcat_chk", b___strlcat_chk), TL_WRAP("__strrchr_chk", b___strrchr_chk), TL_WRAP("__fgets_chk", b___fgets_chk),
    TL_WRAP("__FD_CLR_chk", b___FD_CLR_chk),
    TL_DATA("stdin", &g_stdin_var), TL_DATA("stdout", &g_stdout_var), TL_DATA("stderr", &g_stderr_var),
    TL_WRAP("perror", b_perror), TL_WRAP("rewind", b_rewind), TL_WRAP("fputwc", b_fputwc), TL_WRAP("popen", b_popen), TL_WRAP("pclose", b_pclose),
    TL_WRAP("tmpfile", b_tmpfile), TL_WRAP("wcsrtombs", b_wcsrtombs),
    TL_DIRECT(strncat), TL_DIRECT(strptime), TL_DIRECT(ldiv), TL_DIRECT(sleep), TL_DIRECT(pause), TL_DIRECT(arc4random_buf), TL_DIRECT(nan), TL_DIRECT(nanf),
    TL_DIRECT(ceil), TL_DIRECT(floor), TL_DIRECT(fabs), TL_DIRECT(trunc), TL_DIRECT(cbrt), TL_DIRECT(acosh), TL_DIRECT(atanh), TL_DIRECT(cosh),
    TL_WRAP("feholdexcept", b_fe_env), TL_WRAP("fegetenv", b_fe_env), TL_WRAP("fesetenv", b_fe_env), TL_WRAP("feupdateenv", b_fe_env), TL_WRAP("fegetround", b_fe_ok), TL_WRAP("fesetround", b_fe_ok), TL_WRAP("feclearexcept", b_fe_ok), TL_WRAP("feraiseexcept", b_fe_ok), TL_WRAP("fetestexcept", b_fe_ok),
    TL_DIRECT(regcomp), TL_DIRECT(regexec), TL_DIRECT(regfree), TL_DIRECT(regerror), TL_DIRECT(sinh), TL_DIRECT(sinhf), TL_DIRECT(scalbnf), TL_DIRECT(mbstowcs), TL_DIRECT(wcstombs), TL_DIRECT(exp2), TL_DIRECT(expm1), TL_DIRECT(log1p), TL_DIRECT(hypotf), TL_DIRECT(ilogbf), TL_DIRECT(nextafter),
    TL_DIRECT(nextafterf), TL_DIRECT(frexpf), TL_DIRECT(if_indextoname),
    TL_END
};
