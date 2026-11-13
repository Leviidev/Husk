/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-path.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    char norm[64];
    tl_path_normalize_slashes("./UserData//SystemSettings.ini", norm, sizeof(norm));
    assert(strcmp(norm, "./UserData/SystemSettings.ini") == 0);
    tl_path_cache_invalidate();

#ifndef _WIN32
    assert(argc >= 2);
    const char *root = argv[1];
    char prev_cwd[1024];
    assert(getcwd(prev_cwd, sizeof(prev_cwd)) != NULL);
    assert(chdir(root) == 0);

    /* Relative 1- and 2-component paths (start == 0) resolve against '.' instead of '/'. */
    char rel[512] = "userdata/systemsettings.ini";
    assert(tl_path_case_resolve(rel));
    assert(strcmp(rel, "UserData/SystemSettings.ini") == 0);

    /* Cached negative miss, then immediate discovery after tl_path_cache_invalidate(). */
    char late[512];
    snprintf(late, sizeof(late), "%s/userdata/newfile.txt", root);
    assert(!tl_path_case_resolve(late));

    char actual_new[512];
    snprintf(actual_new, sizeof(actual_new), "%s/UserData/NewFile.txt", root);
    FILE *f = fopen(actual_new, "w");
    assert(f != NULL);
    fclose(f);
    tl_path_cache_invalidate();

    snprintf(late, sizeof(late), "%s/userdata/newfile.txt", root);
    assert(tl_path_case_resolve(late));
    assert(strcmp(late, actual_new) == 0);

    assert(unlink(actual_new) == 0);
    tl_path_cache_invalidate();
    snprintf(late, sizeof(late), "%s/userdata/newfile.txt", root);
    assert(!tl_path_case_resolve(late));

    assert(chdir(prev_cwd) == 0);
#else
    (void)argc; (void)argv;
#endif
    puts("PASS: case-insensitive path resolution handles relative '.', caching, and invalidation");
    return 0;
}
