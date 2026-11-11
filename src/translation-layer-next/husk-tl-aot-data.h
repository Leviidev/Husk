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
    if (file_bytes && (fseeko(file, (off_t)file_offset, SEEK_SET) ||
                      fread(address, 1, file_bytes, file) != file_bytes)) return false;
    memset((uint8_t *)address + file_bytes, 0, memory_bytes - file_bytes);
    return true;
}
#endif
