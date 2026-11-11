/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-aot-data.h"
#include <assert.h>

int main(void)
{
    FILE *original = tmpfile();
    assert(original);
    const uint8_t initial[] = { 8, 9, 0, 4, 5, 6 };
    assert(fwrite(initial, 1, sizeof(initial), original) == sizeof(initial));
    uint8_t memory[32];
    for (int session = 0; session < 10000; session++) {
        memset(memory, 0xEF, sizeof(memory));
        assert(tl_aot_restore_data(memory + 4, 20, original, 2, 4));
        assert(!memcmp(memory + 4, initial + 2, 4));
        for (int i = 8; i < 24; i++) assert(memory[i] == 0);
        for (int i = 0; i < 4; i++) assert(memory[i] == 0xEF);
        for (int i = 24; i < 32; i++) assert(memory[i] == 0xEF);
    }
    assert(!tl_aot_restore_data(memory, 2, original, 0, 3));
    assert(!tl_aot_restore_data(memory, sizeof(memory), original, UINT64_MAX, 1));
    assert(!tl_aot_restore_data(memory, sizeof(memory), original, 5, 2));
    assert(tl_aot_restore_data(memory, sizeof(memory), original, 0, 0));
    for (size_t i = 0; i < sizeof(memory); i++) assert(memory[i] == 0);
    fclose(original);
    puts("PASS: repeated signed images restore initial data and BSS without touching adjacent memory");
    return 0;
}
