/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-bionic.h"
#include "husk-tl-path.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif

static char g_data_dir[1024], g_guest_data_dir[256];
static int guest_errno;
void tl_set_guest_errno(int e) { guest_errno = e; }
int tl_errno_to_guest(int e) { return e; } /* EINVAL/ERANGE share these values. */
void tl_log_line(const char *fmt, ...) { (void)fmt; }
/* Exact production getcwd wrapper and package configuration, extracted by runner. */
#include "getcwd_under_test.h"

int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *pkg = "example.game";
    const char *expected = "/data/data/example.game/files/quest";
    snprintf(g_data_dir, sizeof(g_data_dir), "%s", argv[1]);
    tl_set_guest_package(pkg);
    char host[1200], out[1024], mapped[1200];
    snprintf(host, sizeof(host), "%s/files/quest", argv[1]);
    assert(chdir(host) == 0);
    char *physical = tl_guest_getcwd(NULL, 0, NULL, NULL);
    assert(physical && strlen(physical) > 150);
    guest_errno = 123;
    assert(b_getcwd(out, 260) == out && !strcmp(out, expected));
    assert(guest_errno == 123);
    /* A filename within NAME_MAX can overflow the game's 260-byte buffer
     * with the physical prefix, while the same logical path fits on Android. */
    assert(strlen(physical) + 100 >= 260);
    assert(strlen(out) + 100 < 260);
    const char *resolved = tl_resolve_data_path(out, g_data_dir, mapped, sizeof(mapped));
    assert(resolved && !strcmp(resolved, host) && chdir(resolved) == 0);
    assert(tl_resolve_data_path("/data/user/0/example.game/files/quest", g_data_dir, mapped, sizeof(mapped)));
    assert(!strcmp(mapped, host));
    assert(tl_resolve_data_path("/data/data/example.game", g_data_dir, mapped, sizeof(mapped)));
    assert(!strcmp(mapped, g_data_dir));
    assert(!tl_resolve_data_path("/data/data/", g_data_dir, mapped, sizeof(mapped)));
    assert(!tl_resolve_data_path("/data/database/example.game", g_data_dir, mapped, sizeof(mapped)));
    snprintf(out, sizeof(out), "%s/keep.txt", expected);
    assert(tl_resolve_data_path(out, g_data_dir, mapped, sizeof(mapped)));
    FILE *f = fopen(mapped, "rb");
    assert(f && fgetc(f) == 'K' && fclose(f) == 0);
    puts("PASS: short Android cwd roundtrips to the original directory and saved file");

    size_t needed = strlen(expected) + 1;
    assert(b_getcwd(out, needed) == out && !strcmp(out, expected));
    memset(out, 'X', sizeof(out));
    assert(!b_getcwd(out, needed - 1) && guest_errno == ERANGE && out[0] == 'X');
    assert(!b_getcwd(out, 0) && guest_errno == EINVAL && out[0] == 'X');
    assert(!b_getcwd(NULL, needed - 1) && guest_errno == ERANGE);
    char *owned = b_getcwd(NULL, 0);
    assert(owned && !strcmp(owned, expected)); free(owned);
    owned = b_getcwd(NULL, 260);
    assert(owned && !strcmp(owned, expected)); free(owned);
    puts("PASS: exact buffer bounds, allocation, EINVAL, ERANGE and guest errno");

    tl_set_guest_package("bad/package");
    assert(b_getcwd(out, sizeof(out)) && !strcmp(out, physical));
    tl_set_guest_package(NULL);
    assert(b_getcwd(out, sizeof(out)) && !strcmp(out, physical));
    tl_set_guest_package(pkg);
    snprintf(host, sizeof(host), "%s-sibling", argv[1]);
    assert(chdir(host) == 0);
    owned = tl_guest_getcwd(NULL, 0, NULL, NULL);
    assert(owned && b_getcwd(out, sizeof(out)) && !strcmp(out, owned)); free(owned);
    assert(strlen(out) > strlen(expected));
    puts("PASS: common-prefix siblings and unconfigured sessions retain real cwd");

    assert(chdir(argv[1]) == 0);
    assert(b_getcwd(out, sizeof(out)) && !strcmp(out, "/data/data/example.game"));

    /* Android data ancestor path simulation: /data, /data/data, /data/user, /data/user/0 */
    assert(tl_android_data_ancestor_mode("/data") == 0771);
    assert(tl_android_data_ancestor_mode("/data/") == 0771);
    assert(tl_android_data_ancestor_mode("/data/data") == 0771);
    assert(tl_android_data_ancestor_mode("/data/data///") == 0771);
    assert(tl_android_data_ancestor_mode("/data/user") == 0711);
    assert(tl_android_data_ancestor_mode("/data/user/0") == 0771);
    assert(tl_android_data_ancestor_mode("/data/user/0/") == 0771);
    assert(tl_android_data_ancestor_mode("/data/data/example.game") == 0);
    assert(tl_android_data_ancestor_mode("/data/other") == 0);
    assert(tl_android_data_ancestor_mode(NULL) == 0);
    assert(tl_android_data_ancestor_mode("") == 0);

    assert(tl_android_data_ancestor_access("/data", 0) == 0);
    assert(tl_android_data_ancestor_access("/data", 1) == 0);
    assert(tl_android_data_ancestor_access("/data", 4) == EACCES);
    assert(tl_android_data_ancestor_access("/data", 2) == EACCES);
    assert(tl_android_data_ancestor_access("/data", 7) == EACCES);
    assert(tl_android_data_ancestor_access("/data/data", 0) == 0);
    assert(tl_android_data_ancestor_access("/data/user/0", 0) == 0);
    assert(tl_android_data_ancestor_access("/nonexistent", 0) == -1);
    puts("PASS: Android /data ancestor permission and access simulation");
#ifndef _WIN32
    snprintf(g_data_dir, sizeof(g_data_dir), "%s-alias/", argv[1]);
    assert(b_getcwd(out, sizeof(out)) && !strcmp(out, "/data/data/example.game"));
    assert(tl_resolve_data_path("/data/data/example.game/files/quest/keep.txt", g_data_dir, mapped, sizeof(mapped)));
    f = fopen(mapped, "rb"); assert(f && fgetc(f) == 'K' && fclose(f) == 0);
    puts("PASS: canonical/symlink data root and trailing slash retain the same files");
#endif
    free(physical);
    return 0;
}
