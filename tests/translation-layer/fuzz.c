/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * libFuzzer harness for the parsers that read other people's files: the ELF
 * analyser directly, and the ZIP reader through a temporary file (it maps
 * files, so it is fed one). Not part of run.sh -- run it by hand:
 *
 *   clang -g -O1 -fsanitize=fuzzer,address,undefined -I../../src/translation-layer \
 *       fuzz.c ../../src/translation-layer/husk-tl-{json,zip,elf,scan}.c -lz -o fuzz
 *   ./fuzz -max_total_time=120 corpus/
 */
#include "husk-tl.h"
#include "husk-tl-internal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    tl_elf_report r;
    tl_elf_analyze(data, size, &r);

    char path[] = "/tmp/husk-tl-fuzz-XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        return 0;
    }
    if (write(fd, data, size) == (ssize_t)size) {
        const char *paths[] = { path };
        husk_tl_free(husk_tl_scan(paths, 1));
        size_t len;
        husk_tl_free(husk_tl_read_entry(path, "lib/arm64-v8a/libgood.so", 1 << 24, &len));
    }
    close(fd);
    unlink(path);
    return 0;
}
