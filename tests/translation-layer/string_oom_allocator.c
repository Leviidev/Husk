/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Fail allocations only after the runtime boots and the calling test arms us. */
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static _Thread_local int kind, skip, failed, after_failure;
static _Thread_local size_t bytes;
void string_oom_arm(int k, size_t n, int before) {
    kind = k; bytes = n; skip = before; failed = after_failure = 0;
}
int string_oom_disarm(void) { kind = 0; return failed == 1 && after_failure == 0; }
static int refuse(int k, size_t n) {
    if (!kind) return 0;
    // After the requested failure, ALL allocations fail. Reporting the error
    // must use the boot-time reserve, without attempting another allocation.
    if (failed) { after_failure++; errno = ENOMEM; return 1; }
    if (kind != k || bytes != n) return 0;
    if (skip) { skip--; return 0; }
    failed++; errno = ENOMEM; return 1;
}
static void *failing_malloc(size_t n) { return refuse(1, n) ? NULL : malloc(n); }
static void *failing_calloc(size_t n, size_t s) { return refuse(2, n * s) ? NULL : calloc(n, s); }
static char *failing_strdup(const char *s) { return refuse(3, strlen(s) + 1) ? NULL : strdup(s); }
__attribute__((used,section("__DATA,__interpose"))) static const struct { const void *replacement, *original; } hooks[] = {
    { failing_malloc, malloc }, { failing_calloc, calloc }, { failing_strdup, strdup }
};
