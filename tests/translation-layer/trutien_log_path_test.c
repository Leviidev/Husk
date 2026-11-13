/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-path.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define mkdir(path, mode) _mkdir(path)
#else
#include <strings.h>
#include <unistd.h>
#endif

static char data_root[400], g_guest_data_dir[256];
static char g_sysfonts[1024], g_cacerts[1024], g_extrafonts[1024], g_shared_storage[1024];
static const char *tl_data_dir(void) { return data_root; }
static const char *tl_ld_main_apk(void) { return ""; }
bool tl_bionic_is_system_lib(const char *name) { (void)name; return false; }
#include "trutien_log_resolver_under_test.h"

static bool same_path(const char *a, const char *b)
{
#ifdef _WIN32
    return !strcasecmp(a, b); /* The host filesystem is case-insensitive. */
#else
    return !strcmp(a, b);
#endif
}

static void make_dir(const char *path) { if (access(path, 0) != 0) assert(mkdir(path, 0700) == 0); }
static void write_marker(const char *path)
{
    FILE *f = fopen(path, "wb"); assert(f); assert(fwrite("keep", 1, 4, f) == 4); assert(!fclose(f));
}
static void marker_unchanged(const char *path)
{
    char bytes[8] = {0}; FILE *f = fopen(path, "rb"); assert(f);
    assert(fread(bytes, 1, sizeof(bytes), f) == 4 && !memcmp(bytes, "keep", 4)); fclose(f);
}
static void helper_refuses(const char *path, size_t cap)
{
    char value[1100], before[1100]; memset(value, 0xA5, sizeof(value));
    assert(strlen(path) < sizeof(value)); strcpy(value, path); memcpy(before, value, sizeof(value));
    assert(!tl_path_case_resolve_parent(value, cap)); assert(!memcmp(value, before, sizeof(value)));
}
static void resolver_keeps(const char *path)
{
    char out[1024]; const char *p = tl_path_resolve(path, out, sizeof(out));
    assert(!strcmp(p, path));
}

int main(int argc, char **argv)
{
    assert(argc == 2 && strlen(argv[1]) < sizeof(data_root)); strcpy(data_root, argv[1]);
    strcpy(g_guest_data_dir, "/data/data/vn.trutien.client");
    char base[700], lower[800], probe[1024], out[1024], existing[900];
    snprintf(base, sizeof(base), "%s/sdcard/Android/data/vn.trutien.client/files/element_astc_v2", data_root);
    snprintf(lower, sizeof(lower), "%s/logs", base); make_dir(lower);
    snprintf(probe, sizeof(probe), "%s/Logs/EC.log", base);
    snprintf(existing, sizeof(existing), "%s/EC.log", lower);
    if (access(existing, 0) == 0) assert(remove(existing) == 0);
    assert(tl_path_case_resolve_parent(probe, sizeof(probe)));
    assert(same_path(probe, existing)); assert(access(existing, 0) != 0); /* pure resolution */
    const char *p = tl_path_resolve("Logs\\EC.log", out, sizeof(out));
    assert(same_path(p, existing));
    write_marker(p); marker_unchanged(existing);
    p = tl_path_resolve("Logs/EC.log", out, sizeof(out)); assert(same_path(p, existing)); marker_unchanged(existing);
    helper_refuses(probe, sizeof(probe)); /* existing leaf belongs to ordinary lookup */
    char case_alias[1024]; snprintf(case_alias, sizeof(case_alias), "%s/LOGS/ec.LOG", base);
    helper_refuses(case_alias, sizeof(case_alias));

    /* Reproduce startup cleanup, then open the missing log through the actual
     * resolver. Resolution creates no file and does not change existing data. */
    assert(remove(existing) == 0);
    p = tl_path_resolve("logs/EC.log", out, sizeof(out)); assert(same_path(p, existing));
    assert(access(existing, 0) != 0); write_marker(p); marker_unchanged(existing);

    /* An existing resource in an earlier search root must win over the new
     * leaf fallback, and looking up either existing file does not truncate. */
    char first_dir[512], first_file[600];
    snprintf(first_dir, sizeof(first_dir), "%s/Logs", data_root); make_dir(first_dir);
    snprintf(first_file, sizeof(first_file), "%s/EC.log", first_dir); write_marker(first_file);
    p = tl_path_resolve("Logs/EC.log", out, sizeof(out)); assert(same_path(p, first_file));
    marker_unchanged(first_file); marker_unchanged(existing); assert(remove(first_file) == 0);

    strcpy(g_guest_data_dir, "/data/data/another.game"); resolver_keeps("Logs/new.log");
    strcpy(g_guest_data_dir, "/data/data/vn.trutien.client.extra"); resolver_keeps("Logs/new.log");
    strcpy(g_guest_data_dir, "/data/data/vn.trutien.client");
    resolver_keeps("Logs/sub/new.log"); resolver_keeps("Logs/../new.log");
    resolver_keeps("../Logs/new.log"); resolver_keeps("Logs/new..log");
    resolver_keeps("Logs/new.txt"); resolver_keeps("Logs/.log"); resolver_keeps("/Logs/new.log");
    char missing[1024]; snprintf(missing, sizeof(missing), "%s/Missing/EC.log", base);
    helper_refuses(missing, sizeof(missing));
    snprintf(missing, sizeof(missing), "%s/logs/../EC.log", base); helper_refuses(missing, sizeof(missing));
    snprintf(missing, sizeof(missing), "%s/logs/./EC.log", base); helper_refuses(missing, sizeof(missing));
    snprintf(missing, sizeof(missing), "%s/logs/..\\EC.log", base); helper_refuses(missing, sizeof(missing));
    snprintf(missing, sizeof(missing), "%s/logs/", base); helper_refuses(missing, sizeof(missing));
    snprintf(missing, sizeof(missing), "%s/logs/new.log", base); helper_refuses(missing, strlen(missing));
    assert(!tl_path_case_resolve_parent(NULL, 1024));
    helper_refuses(missing, 0);
    char long_path[1100]; memset(long_path, 'x', sizeof(long_path));
    long_path[0] = '/'; long_path[sizeof(long_path) - 1] = 0;
    helper_refuses(long_path, sizeof(long_path));
    char tiny[8]; memset(tiny, 0xAA, sizeof(tiny));
    p = tl_path_resolve("Logs/new.log", tiny, sizeof(tiny));
    assert(p != tiny && !strcmp(p, "Logs/new.log"));
    for (unsigned i = 0; i < sizeof(tiny); i++) assert((unsigned char)tiny[i] == 0xAA);
    /* A regular file cannot act as the parent of another missing leaf. */
    snprintf(missing, sizeof(missing), "%s/EC.log/new.log", lower); helper_refuses(missing, sizeof(missing));
    marker_unchanged(existing);
    puts("PASS: actual Tru Tien log lookup recreates a deleted leaf under its case-folded existing parent, preserves existing-file precedence and data, and rejects other packages, traversal, missing parents and short buffers");
    return 0;
}
