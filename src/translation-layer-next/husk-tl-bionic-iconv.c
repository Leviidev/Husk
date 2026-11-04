/* SPDX-License-Identifier: GPL-2.0-or-later */
/* iconv has a compatible fixed-argument LP64 ABI, but errno does not. Never
 * bind these imports directly to Darwin: EILSEQ is 92 there and 84 on Android.
 * The descriptor is opaque to the guest and belongs to the host until close;
 * passing it back unchanged preserves stateful encodings across calls. */
#include "husk-tl-bionic.h"

#include <errno.h>
#include <iconv.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Android arm64 wchar_t is UTF-32LE. Do not inherit the host's WCHAR_T
 * encoding (notably UTF-16 on Windows test hosts). Preserve GNU suffixes. */
static const char *guest_encoding(const char *name, char out[64])
{
    const char *suffix = strstr(name, "//");
    size_t n = suffix ? (size_t)(suffix - name) : strlen(name);
    char key[16]; size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (c == '-' || c == '_' || c == ' ') continue;
        if (k + 1 >= sizeof(key)) return name;
        key[k++] = (char)c;
    }
    key[k] = 0;
    if (strcasecmp(key, "WCHART")) return name;
    if (suffix && strlen(suffix) >= 64 - strlen("UTF-32LE")) return name;
    snprintf(out, 64, "UTF-32LE%s", suffix ? suffix : "");
    return out;
}

/* Bounded logging even when a game polls conversion errors. No text contents
 * are logged. Counts describe bytes, not characters. E2BIG and EINVAL may be
 * normal streaming/backpressure, and are not evidence of a startup failure. */
static void conversion_note(const char *op, int host_error, size_t in, size_t out)
{
    static atomic_uint notes;
    if (!host_error && !getenv("TL_ICONV_TRACE")) return;
    if (atomic_fetch_add_explicit(&notes, 1, memory_order_relaxed) < 32)
        tl_log_line("iconv: %s errno=%d input_left=%zu output_left=%zu",
                    op, host_error ? tl_errno_to_guest(host_error) : 0, in, out);
}

static iconv_t b_iconv_open(const char *to, const char *from)
{
    if (!to || !from) { tl_set_guest_errno(22); return (iconv_t)-1; }
    char to_buf[64], from_buf[64];
    TL_ERRNO_BEGIN();
    iconv_t cd = iconv_open(guest_encoding(to, to_buf), guest_encoding(from, from_buf));
    int e = cd == (iconv_t)-1 ? errno : 0;
    if (e) tl_set_guest_errno(tl_errno_to_guest(e));
    static atomic_uint opens;
    if (atomic_fetch_add_explicit(&opens, 1, memory_order_relaxed) < 16)
        tl_log_line("iconv: open %.80s -> %.80s %s errno=%d", from, to,
                    cd == (iconv_t)-1 ? "failed" : "ready", e ? tl_errno_to_guest(e) : 0);
    return cd;
}

static size_t b_iconv(iconv_t cd, char **in, size_t *in_left, char **out, size_t *out_left)
{
    if (!cd || cd == (iconv_t)-1) { tl_set_guest_errno(9); return (size_t)-1; }
    /* in == NULL or *in == NULL is iconv's reset/flush operation. Do not
     * require input counts in that case, nor output arguments for a reset. */
    if (in && *in && (!in_left || !out || !*out || !out_left)) {
        tl_set_guest_errno(22); return (size_t)-1;
    }
    if (out && *out && !out_left) { tl_set_guest_errno(22); return (size_t)-1; }
    TL_ERRNO_BEGIN();
    size_t r = iconv(cd, in, in_left, out, out_left);
    int e = r == (size_t)-1 ? errno : 0;
    if (e) tl_set_guest_errno(tl_errno_to_guest(e));
    conversion_note(in && *in ? "convert" : "reset", e,
                    in && *in && in_left ? *in_left : 0, out && *out && out_left ? *out_left : 0);
    return r;
}

static int b_iconv_close(iconv_t cd)
{
    if (!cd || cd == (iconv_t)-1) { tl_set_guest_errno(9); return -1; }
    TL_ERRNO_BEGIN();
    int r = iconv_close(cd);
    int e = r == -1 ? errno : 0;
    if (e) tl_set_guest_errno(tl_errno_to_guest(e));
    conversion_note("close", e, 0, 0);
    return r;
}

const tl_bionic_entry tl_tab_iconv[] = {
    TL_WRAP("iconv_open", b_iconv_open),
    TL_WRAP("iconv", b_iconv),
    TL_WRAP("iconv_close", b_iconv_close),
    TL_END
};
