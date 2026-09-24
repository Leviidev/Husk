/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * An APK is a ZIP file, and this reads only as much of the format as an APK
 * uses: the central directory (ZIP64 included, which large games need),
 * stored entries, and deflated ones.
 *
 * The file is someone else's, so every offset and length is checked against
 * the mapping before it is used, with the arithmetic arranged so that the
 * check itself cannot overflow.
 */
#include "husk-tl-internal.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#define SIG_LOCAL       0x04034b50u
#define SIG_CENTRAL     0x02014b50u
#define SIG_END         0x06054b50u
#define SIG_END64       0x06064b50u
#define SIG_END64_LOC   0x07064b50u

/* An APK with more files than this is not an APK. */
#define MAX_ENTRIES     (1u << 20)

static void fail(char *err, size_t errlen, const char *fmt, ...)
{
    if (!err || errlen == 0) {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, errlen, fmt, ap);
    va_end(ap);
}

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint64_t rd64(const uint8_t *p) { return rd32(p) | ((uint64_t)rd32(p + 4) << 32); }

/* [off, off+len) lies inside the file. */
static bool in_file(const tl_zip *z, uint64_t off, uint64_t len)
{
    return off <= z->size && len <= z->size - off;
}

/* The end-of-central-directory record: the last thing in the file, followed
 * only by a comment of up to 64 KiB. Searched for from the end. */
static bool find_end(const tl_zip *z, uint64_t *at)
{
    if (z->size < 22) {
        return false;
    }
    uint64_t lowest = z->size > 22 + 0xFFFF ? z->size - 22 - 0xFFFF : 0;
    for (uint64_t i = z->size - 22 + 1; i-- > lowest;) {
        const uint8_t *p = z->map + i;
        if (rd32(p) == SIG_END && i + 22 + rd16(p + 20) <= z->size) {
            *at = i;
            return true;
        }
    }
    return false;
}

/* Sizes and offsets too large for 32 bits are 0xFFFFFFFF in the fixed record
 * and the real value is in the ZIP64 extra field, in a fixed order, present
 * only for the fields that overflowed. */
static bool apply_zip64_extra(const uint8_t *extra, uint16_t len, tl_zip_entry *e,
                              bool need_usize, bool need_csize, bool need_offset)
{
    uint16_t pos = 0;
    while ((uint32_t)pos + 4 <= len) {
        uint16_t id = rd16(extra + pos);
        uint16_t size = rd16(extra + pos + 2);
        if ((uint32_t)pos + 4 + size > len) {
            return false;
        }
        if (id == 0x0001) {
            const uint8_t *f = extra + pos + 4;
            uint16_t used = 0;
            if (need_usize) {
                if (used + 8 > size) return false;
                e->usize = rd64(f + used); used += 8;
            }
            if (need_csize) {
                if (used + 8 > size) return false;
                e->csize = rd64(f + used); used += 8;
            }
            if (need_offset) {
                if (used + 8 > size) return false;
                e->local_offset = rd64(f + used);
            }
            return true;
        }
        pos = (uint16_t)(pos + 4 + size);
    }
    return !(need_usize || need_csize || need_offset);
}

bool tl_zip_open(tl_zip *z, const char *path, char *err, size_t errlen)
{
    memset(z, 0, sizeof(*z));
    z->fd = -1;

    z->fd = open(path, O_RDONLY);
    if (z->fd < 0) {
        fail(err, errlen, "cannot open: %s", strerror(errno));
        return false;
    }
    struct stat st;
    if (fstat(z->fd, &st) != 0 || st.st_size <= 0) {
        fail(err, errlen, "empty or unreadable file");
        tl_zip_close(z);
        return false;
    }
    z->size = (size_t)st.st_size;
    void *map = mmap(NULL, z->size, PROT_READ, MAP_PRIVATE, z->fd, 0);
    if (map == MAP_FAILED) {
        fail(err, errlen, "cannot map: %s", strerror(errno));
        z->size = 0;
        tl_zip_close(z);
        return false;
    }
    z->map = map;

    uint64_t end;
    if (!find_end(z, &end)) {
        fail(err, errlen, "not a ZIP file (no end-of-directory record)");
        tl_zip_close(z);
        return false;
    }
    const uint8_t *e = z->map + end;
    uint64_t count = rd16(e + 10);
    uint64_t cd_size = rd32(e + 12);
    uint64_t cd_off = rd32(e + 16);

    if (count == 0xFFFF || cd_size == 0xFFFFFFFFu || cd_off == 0xFFFFFFFFu) {
        if (end < 20 || rd32(z->map + end - 20) != SIG_END64_LOC) {
            fail(err, errlen, "ZIP64 archive without a ZIP64 locator");
            tl_zip_close(z);
            return false;
        }
        uint64_t rec = rd64(z->map + end - 20 + 8);
        if (!in_file(z, rec, 56) || rd32(z->map + rec) != SIG_END64) {
            fail(err, errlen, "damaged ZIP64 end-of-directory record");
            tl_zip_close(z);
            return false;
        }
        count = rd64(z->map + rec + 32);
        cd_size = rd64(z->map + rec + 40);
        cd_off = rd64(z->map + rec + 48);
    }

    if (!in_file(z, cd_off, cd_size)) {
        fail(err, errlen, "central directory lies outside the file");
        tl_zip_close(z);
        return false;
    }
    if (count > MAX_ENTRIES || count > cd_size / 46) {
        fail(err, errlen, "implausible entry count (%llu)", (unsigned long long)count);
        tl_zip_close(z);
        return false;
    }

    z->entries = calloc(count ? count : 1, sizeof(*z->entries));
    if (!z->entries) {
        fail(err, errlen, "out of memory");
        tl_zip_close(z);
        return false;
    }

    uint64_t pos = cd_off;
    const uint64_t limit = cd_off + cd_size;
    for (uint64_t i = 0; i < count; i++) {
        if (pos > limit || limit - pos < 46 || rd32(z->map + pos) != SIG_CENTRAL) {
            fail(err, errlen, "damaged central directory at entry %llu",
                 (unsigned long long)i);
            tl_zip_close(z);
            return false;
        }
        const uint8_t *c = z->map + pos;
        uint16_t name_len = rd16(c + 28);
        uint16_t extra_len = rd16(c + 30);
        uint16_t comment_len = rd16(c + 32);
        uint64_t record = 46ull + name_len + extra_len + comment_len;
        if (limit - pos < record) {
            fail(err, errlen, "central directory entry runs past its end");
            tl_zip_close(z);
            return false;
        }

        tl_zip_entry *en = &z->entries[z->count];
        en->flags = rd16(c + 8);
        en->method = rd16(c + 10);
        en->csize = rd32(c + 20);
        en->usize = rd32(c + 24);
        en->local_offset = rd32(c + 42);
        if (!apply_zip64_extra(c + 46 + name_len, extra_len, en,
                               en->usize == 0xFFFFFFFFu, en->csize == 0xFFFFFFFFu,
                               en->local_offset == 0xFFFFFFFFu)) {
            fail(err, errlen, "damaged ZIP64 field in entry %llu",
                 (unsigned long long)i);
            tl_zip_close(z);
            return false;
        }
        en->name = malloc((size_t)name_len + 1);
        if (!en->name) {
            fail(err, errlen, "out of memory");
            tl_zip_close(z);
            return false;
        }
        memcpy(en->name, c + 46, name_len);
        en->name[name_len] = '\0';
        /* A NUL inside a name would make it match something it is not. */
        if (strlen(en->name) != name_len) {
            free(en->name);
            en->name = NULL;
        } else {
            z->count++;
        }
        pos += record;
    }
    return true;
}

void tl_zip_close(tl_zip *z)
{
    if (z->entries) {
        for (size_t i = 0; i < z->count; i++) {
            free(z->entries[i].name);
        }
        free(z->entries);
    }
    if (z->map) {
        munmap((void *)z->map, z->size);
    }
    if (z->fd >= 0) {
        close(z->fd);
    }
    memset(z, 0, sizeof(*z));
    z->fd = -1;
}

const tl_zip_entry *tl_zip_find(const tl_zip *z, const char *name)
{
    for (size_t i = 0; i < z->count; i++) {
        if (strcmp(z->entries[i].name, name) == 0) {
            return &z->entries[i];
        }
    }
    return NULL;
}

bool tl_zip_data(const tl_zip *z, const tl_zip_entry *e, size_t limit,
                 const uint8_t **out, size_t *out_len, bool *owned,
                 char *err, size_t errlen)
{
    *out = NULL;
    *out_len = 0;
    *owned = false;

    if (e->flags & 1) {
        fail(err, errlen, "%s is encrypted", e->name);
        return false;
    }
    if (e->usize > limit) {
        fail(err, errlen, "%s is too large (%llu bytes)", e->name,
             (unsigned long long)e->usize);
        return false;
    }
    /* The local header repeats the name and carries its own extra field,
     * whose length need not match the central directory's. */
    if (!in_file(z, e->local_offset, 30) || rd32(z->map + e->local_offset) != SIG_LOCAL) {
        fail(err, errlen, "%s: damaged local header", e->name);
        return false;
    }
    const uint8_t *l = z->map + e->local_offset;
    uint64_t data_off = e->local_offset + 30ull + rd16(l + 26) + rd16(l + 28);
    if (!in_file(z, data_off, e->csize)) {
        fail(err, errlen, "%s runs past the end of the file", e->name);
        return false;
    }
    const uint8_t *data = z->map + data_off;

    if (e->method == 0) {
        if (e->csize != e->usize) {
            fail(err, errlen, "%s: stored entry with mismatched sizes", e->name);
            return false;
        }
        *out = data;
        *out_len = (size_t)e->usize;
        return true;
    }
    if (e->method != 8) {
        fail(err, errlen, "%s uses compression method %u", e->name, e->method);
        return false;
    }

    uint8_t *buf = malloc(e->usize ? (size_t)e->usize : 1);
    if (!buf) {
        fail(err, errlen, "out of memory inflating %s", e->name);
        return false;
    }
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        free(buf);
        fail(err, errlen, "inflate failed to start");
        return false;
    }
    /* zlib counts in uInt; feed and drain in chunks so entries over 4 GiB
     * would work too, not that an APK should have one. */
    uint64_t in_left = e->csize, out_left = e->usize;
    const uint8_t *in = data;
    uint8_t *dst = buf;
    int rc = Z_OK;
    while (rc == Z_OK) {
        if (zs.avail_in == 0 && in_left > 0) {
            uInt n = in_left > 0x40000000u ? 0x40000000u : (uInt)in_left;
            zs.next_in = (Bytef *)in;
            zs.avail_in = n;
            in += n;
            in_left -= n;
        }
        if (zs.avail_out == 0 && out_left > 0) {
            uInt n = out_left > 0x40000000u ? 0x40000000u : (uInt)out_left;
            zs.next_out = dst;
            zs.avail_out = n;
            dst += n;
            out_left -= n;
        }
        if (zs.avail_in == 0 && zs.avail_out == 0) {
            break;
        }
        /* Z_BUF_ERROR ends the loop too: out of input before the end marker,
         * or more output than the directory promised. Both are damage. */
        rc = inflate(&zs, Z_NO_FLUSH);
    }
    uint64_t produced = zs.total_out;
    inflateEnd(&zs);
    if (rc != Z_STREAM_END || produced != e->usize) {
        free(buf);
        fail(err, errlen, "%s did not inflate cleanly", e->name);
        return false;
    }
    *out = buf;
    *out_len = (size_t)e->usize;
    *owned = true;
    return true;
}
