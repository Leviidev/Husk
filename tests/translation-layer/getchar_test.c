/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-bionic.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
static int guest_errno;
void tl_set_guest_errno(int e) { guest_errno = e; }
int tl_errno_to_guest(int e) { return e; }
/* The runner extracts the exact production wrapper into the build directory. */
#include "getchar_under_test.h"
int main(int argc, char **argv)
{
    assert(argc == 2 && freopen(argv[1], "rb", stdin));
    guest_errno = 123;
    assert(b_getchar() == 'A');
    assert(b_getchar() == 255);  /* unsigned byte, not a signed char or EOF */
    assert(b_getchar() == EOF);
    assert(guest_errno == 123);
    puts("PASS: production getchar reads stdin, unsigned bytes and EOF without clearing guest errno");
    return 0;
}
