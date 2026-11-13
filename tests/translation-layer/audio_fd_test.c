/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
static ssize_t pread(int fd, void *buf, size_t n, int64_t off)
{
    if (_lseeki64(fd, off, SEEK_SET) < 0) return -1;
    int r = _read(fd, buf, (unsigned int)n);
    return (ssize_t)r;
}
#else
#include <unistd.h>
#endif

static int g_logs;
static void tl_log_line(const char *format, ...) { (void)format; g_logs++; }

/* Extracted directly from src/translation-layer-next/husk-tl-dvm-audio.c by tests/run_portable.py. */
#include "audio_fd_under_test.h"

static int make_temp_file(const char *payload, size_t len)
{
    FILE *f = tmpfile();
    assert(f != NULL);
    assert(fwrite(payload, 1, len, f) == len);
    fflush(f);
    int fd = dup(fileno(f));
    assert(fd >= 0);
    fclose(f);
    return fd;
}

int main(void)
{
    const char payload[] = "0123456789ABCDEF";
    int fd = make_temp_file(payload, sizeof(payload) - 1);

    /* 1. UNKNOWN_LENGTH (-1) and 0 read from offset to EOF of regular file. */
    size_t out = 0;
    uint8_t *buf = read_fd(fd, 0, -1, &out);
    assert(buf != NULL && out == 16 && memcmp(buf, payload, 16) == 0);
    free(buf);

    /* 2. Offset + INT64_MAX (e.g. AssetFileDescriptor length for streamed/unbounded FDs) clamps to remaining regular file size. */
    out = 0;
    buf = read_fd(fd, 6, INT64_MAX, &out);
    assert(buf != NULL && out == 10 && memcmp(buf, "6789ABCDEF", 10) == 0);
    free(buf);

    /* 3. Exact subrange read from offset. */
    out = 0;
    buf = read_fd(fd, 4, 4, &out);
    assert(buf != NULL && out == 4 && memcmp(buf, "4567", 4) == 0);
    free(buf);

    /* 4. Offset past EOF, negative fd/offset, NULL out all fail cleanly without OOM/UB. */
    assert(read_fd(fd, 32, -1, &out) == NULL);
    assert(read_fd(fd, 32, 4, &out) == NULL);
    assert(read_fd(fd, -1, 4, &out) == NULL);
    assert(read_fd(-1, 0, 4, &out) == NULL);
    assert(read_fd(fd, 0, 4, NULL) == NULL);

    /* 5. Closed fd returns NULL instead of undefined read. */
    close(fd);
    int before = g_logs;
    assert(read_fd(fd, 0, -1, &out) == NULL);
    assert(read_fd(fd, 0, 4, &out) == NULL);
    assert(g_logs >= before + 2);

    printf("PASS: read_fd clamps regular-file bounds, handles offsets, and rejects invalid/closed FDs\n");
    return 0;
}
