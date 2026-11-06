/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-xmem.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>

void tl_log_line(const char *fmt, ...) { (void)fmt; }
int main(void)
{
    assert(husk_jit_budget_region_bytes() == 0); /* Query never prewarms. */
    assert(!tl_xmem_alloc(TL_XMEM_PAGE, NULL, NULL));
    char err[256] = "";
    assert(tl_xmem_open(4 * TL_XMEM_PAGE, err, sizeof(err)));
    assert(husk_jit_budget_region_bytes() == 4 * TL_XMEM_PAGE);
    uint8_t *rx, *rw;
    assert(tl_xmem_alloc(TL_XMEM_PAGE, &rx, &rw));
    rw[0] = 42; assert(rx[0] == 42);
    assert(!tl_xmem_alloc(4 * TL_XMEM_PAGE, NULL, NULL));
    assert(tl_xmem_used() == TL_XMEM_PAGE && tl_xmem_size() == 4 * TL_XMEM_PAGE);
    assert(!tl_xmem_alloc(SIZE_MAX, NULL, NULL));
    puts("PASS: fixed code-region capacity, aliasing, overflow bounds");
}
