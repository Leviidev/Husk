/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdio.h>
/* Darwin's ABI, deliberately distinct from Bionic's NUMERICSERV. */
enum { AI_PASSIVE = 1, AI_CANONNAME = 2, AI_NUMERICHOST = 4,
       AI_NUMERICSERV = 0x1000, AI_ALL = 0x100, AI_V4MAPPED_CFG = 0x200,
       AI_ADDRCONFIG = 0x400, AI_V4MAPPED = 0x800,
       NI_NOFQDN = 1, NI_NUMERICHOST = 2, NI_NAMEREQD = 4,
       NI_NUMERICSERV = 8, NI_DGRAM = 16 };
#include "net_flags_under_test.h"
int main(void)
{
    assert(ai_flags_to_darwin(0) == 0);
    assert(ai_flags_to_darwin(0x8) == AI_NUMERICSERV);
    assert(ai_flags_to_darwin(0x400) == AI_ADDRCONFIG);
    assert(ai_flags_to_darwin(0x800) == AI_V4MAPPED);
    assert(ai_flags_to_darwin(0x108) == (AI_ALL | AI_NUMERICSERV));
    assert(ai_flags_to_darwin(0xf0f) == 0x1f07);
    assert(ai_flags_to_darwin(0x10) == -1);
    assert(ai_flags_to_darwin(0x20) == -1);
    assert(ni_flags_to_darwin(2) == NI_NUMERICHOST);
    assert(ni_flags_to_darwin(8) == NI_NUMERICSERV);
    assert(ni_flags_to_darwin(10) == (NI_NUMERICHOST | NI_NUMERICSERV));
    assert(ni_flags_to_darwin(0x1f) == 0x1f);
    assert(ni_flags_to_darwin(0x20) == -1);
    puts("PASS: Android resolver flags preserve numeric addresses/services and IPv4/IPv6 address filtering");
    return 0;
}
