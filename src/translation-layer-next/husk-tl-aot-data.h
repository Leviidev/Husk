/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_AOT_DATA_H
#define HUSK_TL_AOT_DATA_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* dyld can return the same signed image after a game closes. Its code stays
 * immutable, but each writable segment needs its original data and fresh BSS
 * before the ELF loader relocates it for a new session. */
static inline bool tl_aot_restore_data(void *address, size_t memory_bytes, FILE *file,
                                     uint64_t file_offset, size_t file_bytes)
{
    if (file_bytes > memory_bytes || file_offset > INT64_MAX) return false;
    if (file_bytes && fseeko(file, (off_t)file_offset, SEEK_SET)) return false;
    /* The destination is a private dyld mapping of this same file. Do not let
     * read(2)'s copyout fault those file-backed pages while the filesystem is
     * reading the vnode: on iOS this can block the loader in the kernel. Read
     * into ordinary stack memory, then fault/write the mapping in userspace. */
    uint8_t buffer[4096];
    for (size_t offset = 0; offset < file_bytes; ) {
        size_t count = file_bytes - offset;
        if (count > sizeof(buffer)) count = sizeof(buffer);
        if (fread(buffer, 1, count, file) != count) return false;
        memcpy((uint8_t *)address + offset, buffer, count);
        offset += count;
    }
    memset((uint8_t *)address + file_bytes, 0, memory_bytes - file_bytes);
    return true;
}
#endif
