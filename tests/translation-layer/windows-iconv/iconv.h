/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Test host only: forward to Git for Windows' real GNU libiconv, without
 * requiring its development headers or an import library. */
#ifndef HUSK_TEST_ICONV_H
#define HUSK_TEST_ICONV_H
#include <stddef.h>
typedef void *iconv_t;
iconv_t husk_test_iconv_open(const char *, const char *);
size_t husk_test_iconv(iconv_t, char **, size_t *, char **, size_t *);
int husk_test_iconv_close(iconv_t);
void husk_test_iconv_init(void);
#define iconv_open husk_test_iconv_open
#define iconv husk_test_iconv
#define iconv_close husk_test_iconv_close
#endif
