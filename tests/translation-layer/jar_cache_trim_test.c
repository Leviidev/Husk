/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>

static char g_data_dir[1024];
static const char *tl_data_dir(void) { return g_data_dir; }

/* Extracted from src/translation-layer-next/husk-tl-bionic-io.c by tests/run_portable.py. */
#include "jar_cache_trim_under_test.h"

static void write_cache_file(const char *name, size_t size, time_t mtime)
{
    char path[1400];
    assert(snprintf(path, sizeof(path), "%s/.jar-cache/%s", g_data_dir, name) < (int)sizeof(path));
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    for (size_t i = 0; i < size; i++) assert(fputc('A' + (int)(i % 26), f) != EOF);
    assert(fclose(f) == 0);
    struct utimbuf ut = { mtime, mtime };
    assert(utime(path, &ut) == 0);
}

static bool exists_cache_file(const char *name)
{
    char path[1400];
    snprintf(path, sizeof(path), "%s/.jar-cache/%s", g_data_dir, name);
    return access(path, F_OK) == 0;
}

int main(void)
{
    char tmpl[] = "/tmp/husk-jar-trim-XXXXXX";
    char *root = mkdtemp(tmpl);
    assert(root != NULL);
    snprintf(g_data_dir, sizeof(g_data_dir), "%s", root);

    char cache_dir[1200];
    snprintf(cache_dir, sizeof(cache_dir), "%s/.jar-cache", g_data_dir);
    assert(mkdir(cache_dir, 0755) == 0);

    write_cache_file("oldest.dex", 600, 1000000);
    write_cache_file("middle.dex", 600, 2000000);
    write_cache_file("newest.dex", 600, 3000000);

    /* Trimming 1800 -> 1200 evicts oldest.dex while keeping middle & newest. */
    tl_jar_cache_trim(1200);
    assert(!exists_cache_file("oldest.dex"));
    assert(exists_cache_file("middle.dex"));
    assert(exists_cache_file("newest.dex"));

    /* Trimming 1200 -> 700 evicts middle.dex and retains newest.dex. */
    tl_jar_cache_trim(700);
    assert(!exists_cache_file("middle.dex"));
    assert(exists_cache_file("newest.dex"));

    char rem[1400];
    snprintf(rem, sizeof(rem), "%s/newest.dex", cache_dir);
    unlink(rem);
    rmdir(cache_dir);
    rmdir(g_data_dir);

    puts("PASS: tl_jar_cache_trim evicts oldest entries to target budget and preserves newest");
    return 0;
}

