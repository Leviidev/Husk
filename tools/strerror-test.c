/* SPDX-License-Identifier: GPL-2.0-or-later */
/* macOS: tools/regress/build.sh tools/strerror-test.c build/strerror-test && build/strerror-test */
#include "husk-tl-bionic.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

void tl_log_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); fputc('\n', stderr); va_end(ap);
}

int main(void)
{
    int (*guest_strerror_r)(int, char *, size_t) = tl_bionic_find("strerror_r");
    assert(guest_strerror_r);
    char buf[256];
    /* OpenSSL's initialization walks the Linux error numbers, including ones Darwin does not know. */
    for (int e = 0; e <= 133; e++) {
        memset(buf, 0x5a, sizeof(buf));
        errno = EBUSY; tl_set_guest_errno(75);
        int rc = guest_strerror_r(e, buf, sizeof(buf));
        if (rc || !buf[0] || !memchr(buf, 0, sizeof(buf))) {
            fprintf(stderr, "strerror_r(%d) returned %d or no terminated message\n", e, rc);
            return 1;
        }
        assert(errno == EBUSY && *tl_guest_errno_ptr() == 75);
    }
    const int unknown[] = { -1, INT_MIN, INT_MAX };
    for (unsigned i = 0; i < sizeof(unknown) / sizeof(unknown[0]); i++) {
        char expected[64]; snprintf(expected, sizeof(expected), "Unknown error %d", unknown[i]);
        assert(guest_strerror_r(unknown[i], buf, sizeof(buf)) == 0);
        assert(!strcmp(buf, expected));
    }
    /* Truncation returns Linux ERANGE (34), leaves errno alone, terminates the buffer, and respects its bound. */
    const int errors[] = { 11, 117, INT_MAX };
    for (unsigned i = 0; i < sizeof(errors) / sizeof(errors[0]); i++) {
        memset(buf, 0x5a, sizeof(buf));
        errno = EBUSY; tl_set_guest_errno(75);
        assert(guest_strerror_r(errors[i], buf, 1) == 34);
        assert(buf[0] == 0 && buf[1] == 0x5a);
        assert(guest_strerror_r(errors[i], buf, 0) == 34);
        assert(buf[0] == 0 && buf[1] == 0x5a);
        assert(errno == EBUSY && *tl_guest_errno_ptr() == 75);
    }
    assert(guest_strerror_r(INT_MAX, NULL, 0) == 34);
    puts("PASS: Linux error-number sweep, unknown errors, truncation, zero-length buffers, preserved errno");
    return 0;
}
