/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif

static uintptr_t mapped_begin, mapped_end;
static size_t checked_reads;

/* The iOS failure was a kernel copyout into a private mapping of the file
 * being read. Host kernels may allow it, so check this I/O constraint as well
 * as the resulting bytes. Other implementations using independent storage
 * remain valid; only an overlapping read destination is forbidden. */
static size_t checked_fread(void *destination, size_t size, size_t count, FILE *file)
{
    assert(size == 0 || count <= SIZE_MAX / size);
    size_t bytes = size * count;
    uintptr_t begin = (uintptr_t)destination;
    assert(bytes <= UINTPTR_MAX - begin);
    if (bytes && mapped_end > mapped_begin && begin < mapped_end && begin + bytes > mapped_begin) {
        fputs("FAIL: filesystem read targets the private source-file mapping\n", stderr);
        exit(77);
    }
    checked_reads++;
    return fread(destination, size, count, file);
}

#define fread checked_fread
#include "husk-tl-aot-data.h"
#undef fread

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

    /* A real dylib's destination is a private mapping of the source file.
     * Use cold file-backed pages and a non-aligned multi-buffer copy. */
    original = tmpfile();
    assert(original);
    const size_t bytes = 32768, start = 8192, copied = 16387, restored = 20000;
    uint8_t *initial_large = malloc(bytes);
    assert(initial_large);
    for (size_t i = 0; i < bytes; i++) initial_large[i] = (uint8_t)(i * 17 + i / 251);
    assert(fwrite(initial_large, 1, bytes, original) == bytes);
    assert(fflush(original) == 0);
    for (int session = 0; session < 50; session++) {
#ifndef _WIN32
        uint8_t *mapped = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE, fileno(original), 0);
        assert(mapped != MAP_FAILED);
#else
        uint8_t *mapped = malloc(bytes);
        assert(mapped);
        memcpy(mapped, initial_large, bytes);
#endif
        mapped_begin = (uintptr_t)mapped;
        mapped_end = mapped_begin + bytes;
        assert(tl_aot_restore_data(mapped + start, restored, original, start, copied));
        mapped_begin = mapped_end = 0;
        assert(!memcmp(mapped, initial_large, start + copied));
        for (size_t i = start + copied; i < start + restored; i++) assert(mapped[i] == 0);
        assert(!memcmp(mapped + start + restored, initial_large + start + restored, bytes - start - restored));
#ifndef _WIN32
        assert(munmap(mapped, bytes) == 0);
#else
        free(mapped);
#endif
    }
    free(initial_large);
    fclose(original);
    assert(checked_reads > 0);
    puts("PASS: repeated file-backed images restore data/BSS and never pass their private mapping to filesystem reads");
    return 0;
}
