/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Host contract stub only. Device builds use Apple's actual SHA-256 implementation. */
#ifndef QUEST_TEST_COMMON_DIGEST_H
#define QUEST_TEST_COMMON_DIGEST_H
#include <stdint.h>
typedef unsigned int CC_LONG;
unsigned char *CC_SHA256(const void *data, CC_LONG length, unsigned char *out);
#endif
