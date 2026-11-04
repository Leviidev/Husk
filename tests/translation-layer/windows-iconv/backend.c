/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "iconv.h"
#include <windows.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static iconv_t (*open_fn)(const char *, const char *);
static size_t (*convert_fn)(iconv_t, char **, size_t *, char **, size_t *);
static int (*close_fn)(iconv_t);
static int *(*dll_errno)(void);

void husk_test_iconv_init(void)
{
    const char *path = getenv("TL_TEST_ICONV_DLL");
    HMODULE lib = path ? LoadLibraryA(path) : NULL;
    /* Git's mingw64 libiconv-2.dll imports msvcrt.dll. Copy its thread-local
     * errno into this executable's CRT; the CRTs need not be identical. */
    HMODULE crt = LoadLibraryA("msvcrt.dll");
    if (lib) {
        open_fn = (void *)GetProcAddress(lib, "libiconv_open");
        convert_fn = (void *)GetProcAddress(lib, "libiconv");
        close_fn = (void *)GetProcAddress(lib, "libiconv_close");
    }
    dll_errno = crt ? (void *)GetProcAddress(crt, "_errno") : NULL;
    if (!open_fn || !convert_fn || !close_fn || !dll_errno) {
        fprintf(stderr, "Set TL_TEST_ICONV_DLL to Git for Windows mingw64/bin/libiconv-2.dll\n");
        exit(2);
    }
}
static void copy_error(void)
{
    /* msvcrt EILSEQ=42; the compiler's CRT may assign a different value. */
    int e = *dll_errno();
    errno = e == 42 ? EILSEQ : e;
}
iconv_t husk_test_iconv_open(const char *to, const char *from)
{
    iconv_t r = open_fn(to, from);
    if (r == (iconv_t)-1) copy_error();
    return r;
}
size_t husk_test_iconv(iconv_t cd, char **in, size_t *ni, char **out, size_t *no)
{
    size_t r = convert_fn(cd, in, ni, out, no);
    if (r == (size_t)-1) copy_error();
    return r;
}
int husk_test_iconv_close(iconv_t cd)
{
    int r = close_fn(cd);
    if (r == -1) copy_error();
    return r;
}
