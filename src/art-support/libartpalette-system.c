/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * ART's palette: the few things the runtime asks of the system it runs on (thread priorities, tracing, ashmem, crash
 * stacks). libartpalette.so, in the ART module, dlopens libartpalette-system.so and calls these. On a phone that library
 * talks to the platform; on Husk there is no platform behind it, so each answer is the one an ordinary app process gets.
 *
 * Built for the guest with the NDK (scripts/build_art_support.sh) and loaded by the translation layer like any app library.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

typedef int32_t palette_status_t;
enum { PALETTE_STATUS_OK = 0, PALETTE_STATUS_CHECK_ERRNO = 1, PALETTE_STATUS_FAILED_CHECK_LOG = 2,
       PALETTE_STATUS_NOT_SUPPORTED = 3, PALETTE_STATUS_INVALID_ARGUMENT = 4 };

#define EXPORT __attribute__((visibility("default")))

/* Java thread priorities 1..10 as Linux nice values, the table Android uses. */
static const int kNice[11] = { 0, 19, 16, 13, 10, 0, -2, -4, -5, -6, -8 };

EXPORT palette_status_t PaletteSchedSetPriority(int32_t tid, int32_t prio)
{
    if (prio < 1 || prio > 10) return PALETTE_STATUS_INVALID_ARGUMENT;
    setpriority(PRIO_PROCESS, (id_t)tid, kNice[prio]);       /* best effort: iOS may refuse raising it */
    return PALETTE_STATUS_OK;
}

EXPORT palette_status_t PaletteSchedGetPriority(int32_t tid, int32_t *prio)
{
    errno = 0;
    int nice = getpriority(PRIO_PROCESS, (id_t)tid);
    if (nice == -1 && errno) return PALETTE_STATUS_CHECK_ERRNO;
    int best = 5;
    for (int p = 1; p <= 10; p++) if (kNice[p] >= nice) best = p;
    *prio = best;
    return PALETTE_STATUS_OK;
}

EXPORT palette_status_t PaletteWriteCrashThreadStacks(const char *stacks, size_t len)
{
    fprintf(stderr, "art: thread stacks at crash:\n%.*s\n", (int)len, stacks);
    return PALETTE_STATUS_OK;
}

/* No systrace. */
EXPORT palette_status_t PaletteTraceEnabled(bool *enabled) { *enabled = false; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteTraceBegin(const char *name) { (void)name; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteTraceEnd(void) { return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteTraceIntegerValue(const char *name, int32_t value) { (void)name; (void)value; return PALETTE_STATUS_OK; }

/* Ashmem is a Linux driver iOS does not have: a region is an unlinked file of the size asked for, which maps the same way. */
EXPORT palette_status_t PaletteAshmemCreateRegion(const char *name, size_t size, int *fd)
{
    (void)name;
    const char *dir = getenv("TMPDIR");
    if (!dir || !*dir) dir = getenv("ANDROID_DATA");
    char path[512];
    snprintf(path, sizeof(path), "%s/ashmem-XXXXXX", dir ? dir : "/data/local/tmp");
    int f = mkstemp(path);
    if (f < 0) return PALETTE_STATUS_CHECK_ERRNO;
    unlink(path);
    if (ftruncate(f, (off_t)size) != 0) { int e = errno; close(f); errno = e; return PALETTE_STATUS_CHECK_ERRNO; }
    *fd = f;
    return PALETTE_STATUS_OK;
}

EXPORT palette_status_t PaletteAshmemSetProtRegion(int fd, int prot) { (void)fd; (void)prot; return PALETTE_STATUS_OK; }

/* There is no odrefresh or dex2oat here, and no one listening for these. */
EXPORT palette_status_t PaletteCreateOdrefreshStagingDirectory(const char **dir) { *dir = NULL; return PALETTE_STATUS_NOT_SUPPORTED; }
EXPORT palette_status_t PaletteShouldReportDex2oatCompilation(bool *v) { *v = false; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteNotifyStartDex2oatCompilation(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteNotifyEndDex2oatCompilation(int a, int b, int c, int d) { (void)a; (void)b; (void)c; (void)d; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteNotifyDexFileLoaded(const char *path) { (void)path; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteNotifyOatFileLoaded(const char *path) { (void)path; return PALETTE_STATUS_OK; }
EXPORT palette_status_t PaletteShouldReportJniInvocations(bool *v) { *v = false; return PALETTE_STATUS_OK; }
EXPORT void PaletteNotifyBeginJniInvocation(void *env) { (void)env; }
EXPORT void PaletteNotifyEndJniInvocation(void *env) { (void)env; }
EXPORT palette_status_t PaletteReportLockContention(void *env, int32_t wait_ms, const char *filename, int32_t line,
                                                    const char *method, const char *owner_filename, int32_t owner_line,
                                                    const char *owner_method, const char *proc_name, const char *thread_name)
{
    (void)env; (void)wait_ms; (void)filename; (void)line; (void)method; (void)owner_filename; (void)owner_line;
    (void)owner_method; (void)proc_name; (void)thread_name;
    return PALETTE_STATUS_OK;
}
EXPORT palette_status_t PaletteSetTaskProfiles(int32_t tid, const char *const profiles[], size_t n)
{
    (void)tid; (void)profiles; (void)n;
    return PALETTE_STATUS_OK;
}
EXPORT palette_status_t PaletteDebugStoreGetString(char *result, size_t max)
{
    if (max) result[0] = 0;
    return PALETTE_STATUS_OK;
}
EXPORT palette_status_t PaletteMapPriority(int32_t prio, int32_t *nice)
{
    if (prio < 1 || prio > 10) return PALETTE_STATUS_INVALID_ARGUMENT;
    *nice = kNice[prio];
    return PALETTE_STATUS_OK;
}
