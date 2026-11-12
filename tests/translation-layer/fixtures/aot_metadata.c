/* SPDX-License-Identifier: GPL-2.0-or-later */
/* An ordinary read-only object and .eh_frame, also linked into a merged R-X
 * segment by test_aot_metadata.py. Full code pages must remain executable only. */
__attribute__((used, aligned(4096), visibility("default")))
const unsigned aot_metadata[2048] = {[0] = 17, [3] = 42, [2047] = 99};

__attribute__((visibility("default"), noinline, aligned(16384)))
unsigned read_metadata(unsigned index) { return aot_metadata[index & 2047]; }

__attribute__((visibility("default"), naked, aligned(16384)))
void full_code_pages(void)
{
    __asm__(".rept 8192\n nop\n .endr\n ret\n");
}
