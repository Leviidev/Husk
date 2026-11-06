/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Exercise actual guest-facing adapters; no APK, network service, or GUI. */
#include "husk-tl-bionic.h"
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

void tl_set_data_dir(const char *);
int tl_synth_open(const char *);
void tl_log_line(const char *format, ...)
{
    va_list ap; va_start(ap, format); vfprintf(stderr, format, ap); va_end(ap); fputc('\n', stderr);
}
static void *symbol(const char *name)
{
    void *result = tl_bionic_find(name); assert(result); return result;
}
int main(int argc, char **argv)
{
    assert(argc == 2 && argv[1][0] == '/');
    const char *directory = argv[1];
    assert(!setenv("TL_TMP", directory, 1));
    assert(!setenv("TMPDIR", directory, 1));
    tl_set_data_dir(directory);
    FILE *(*guest_tmpfile)(void) = symbol("tmpfile");
    int (*shared_memory)(const char *, size_t) = symbol("ASharedMemory_create");
    int (*guest_bind)(int, const void *, unsigned) = symbol("bind");
    int fd = tl_anonymous_temp_file(); struct stat st;
    assert(fd >= 0 && !fstat(fd, &st) && st.st_nlink == 0); close(fd);
    FILE *file = guest_tmpfile(); assert(file && !fstat(fileno(file), &st) && st.st_nlink == 0); fclose(file);
    fd = shared_memory("test", 4096); assert(fd >= 0 && !fstat(fd, &st) && st.st_size == 4096 && st.st_nlink == 0); close(fd);
    fd = tl_synth_open("/proc/cpuinfo"); assert(fd >= 0 && !fstat(fd, &st) && st.st_nlink == 0); close(fd);

    unsigned char address[80] = {1, 0, 0};
    int name_length = snprintf((char *)address + 3, sizeof(address) - 3, "husk-temp-test-%d", getpid());
    unsigned address_length = (unsigned)name_length + 3;
    fd = socket(AF_UNIX, SOCK_STREAM, 0); assert(fd >= 0);
    assert(!guest_bind(fd, address, address_length));
    struct sockaddr_un actual; socklen_t length = sizeof(actual);
    assert(!getsockname(fd, (struct sockaddr *)&actual, &length));
    assert(!strncmp(actual.sun_path, directory, strlen(directory)) && actual.sun_path[strlen(directory)] == '/');
    close(fd); assert(!unlink(actual.sun_path));

    /* An explicit unusable TL_TMP must never fall through to valid TMPDIR/data. */
    char missing[4096]; snprintf(missing, sizeof(missing), "%s/absent-parent/absent-child", directory);
    assert(!setenv("TL_TMP", missing, 1));
    assert(tl_anonymous_temp_file() == -1 && errno == ENOENT);
    assert(!guest_tmpfile() && *tl_guest_errno_ptr() == 2);
    assert(shared_memory("test", 4096) == -1 && *tl_guest_errno_ptr() == 2);
    assert(tl_synth_open("/proc/cpuinfo") == -1 && errno == ENOENT);
    assert(!setenv("TL_TMP", "", 1));
    assert(tl_anonymous_temp_file() == -1 && errno == ENOENT);

    assert(!setenv("TL_TMP", ".", 1));
    assert(tl_anonymous_temp_file() == -1 && errno == EINVAL);

    char too_long[4096]; memset(too_long, 'x', sizeof(too_long));
    memcpy(too_long, directory, strlen(directory)); too_long[strlen(directory)] = '/'; too_long[sizeof(too_long) - 1] = 0;
    assert(!setenv("TL_TMP", too_long, 1));
    fd = socket(AF_UNIX, SOCK_STREAM, 0); assert(fd >= 0);
    assert(guest_bind(fd, address, address_length) == -1 && *tl_guest_errno_ptr() == 36 /* ENAMETOOLONG */);
    close(fd);
    assert(tl_anonymous_temp_file() == -1 && errno == ENAMETOOLONG);

    /* With no override, the configured app directory remains a valid fallback. */
    assert(!unsetenv("TL_TMP") && !unsetenv("TMPDIR"));
    fd = tl_anonymous_temp_file(); assert(fd >= 0); close(fd);
    tl_set_data_dir(NULL);
    assert(tl_anonymous_temp_file() == -1 && errno == ENOENT);
    puts("PASS: anonymous, stdio, ashmem, synthetic and Unix socket temporaries respect configured directories; invalid overrides never fall back");
    return 0;
}
