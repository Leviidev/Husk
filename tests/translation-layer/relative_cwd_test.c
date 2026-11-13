/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-path.h"
#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define chdir _chdir
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

#define TL_ERRNO_BEGIN() do {} while (0)
#define TL_ERRNO_END() do {} while (0)
static void ftrace(const char *op, const char *p, long result, int error)
{ (void)op; (void)p; (void)result; (void)error; }
static void tl_set_guest_errno(int e) { (void)e; }
#include "relative_directory_under_test.h"

static void make_dir(const char *path)
{ if (access(path, 0) != 0) assert(mkdir(path, 0700) == 0); }
static void marker(const char *path, char value)
{ FILE *f = fopen(path, "wb"); assert(f); assert(fputc(value, f) == value); assert(!fclose(f)); }
static bool directory_contains(const char *path, const char *name)
{
    guest_dir *d = b_opendir(path); assert(d);
    bool found = false;
#ifdef _WIN32
    /* MinGW's host dirent has no d_type. The target platforms exercise the
     * real guest-dirent conversion too; Windows still opens through the same
     * production resolver/opendir code that selected the wrong directory. */
    struct dirent *e;
    while ((e = readdir(d->dir))) if (!strcmp(e->d_name, name)) found = true;
#else
    guest_dirent *e;
    while ((e = b_readdir(d))) if (!strcmp(e->d_name, name)) found = true;
#endif
    assert(!b_closedir(d));
    return found;
}
static void assert_marker(const char *path, char value)
{
    char out[1024];
    FILE *f = fopen(tl_path_resolve(path, out, sizeof(out)), "rb");
    assert(f && fgetc(f) == value); assert(!fclose(f));
}

int main(int argc, char **argv)
{
    assert(argc == 2 && strlen(argv[1]) < sizeof(data_root));
    strcpy(data_root, argv[1]);
    strcpy(g_guest_data_dir, "/data/data/com.game3c.vhtgmobile");
    char game[600], scripts[700], life[800], path[900], out[1024];
    snprintf(game, sizeof(game), "%s/files", data_root); make_dir(game);
    snprintf(game, sizeof(game), "%s/files/gameclient", data_root); make_dir(game);
    snprintf(scripts, sizeof(scripts), "%s/script", game); make_dir(scripts);
    snprintf(life, sizeof(life), "%s/lifeskill", scripts); make_dir(life);
    snprintf(path, sizeof(path), "%s/loaded.lua", life); marker(path, 'L');
    snprintf(path, sizeof(path), "%s/shared.txt", scripts); marker(path, 'C');
    snprintf(path, sizeof(path), "%s/shared.txt", data_root); marker(path, 'R');
    snprintf(path, sizeof(path), "%s/root-only.txt", data_root); marker(path, 'F');

    /* Same sequence as KScriptGroup::_LoadScriptInDirectory: chdir into the
     * script tree, enumerate '.', recurse into a directory, enumerate again.
     * Both directory wrappers and the resolver are actual production code. */
    assert(!chdir(game));
    /* g_GetFullPath prefixes the game's root with this Android alias before
     * the original ARM64 walker calls chdir and then opendir("."). */
    assert(!chdir(tl_path_resolve("/data/data/com.game3c.vhtgmobile/files/gameclient/script", out, sizeof(out))));
    assert(directory_contains(".", "lifeskill"));
    assert(!directory_contains(".", "root-only.txt"));
    assert_marker("shared.txt", 'C'); /* cwd wins over an existing root shadow */
    assert_marker("root-only.txt", 'F'); /* absent cwd keeps compatibility fallback */
    assert_marker("lifeskill\\loaded.lua", 'L');
    assert_marker(".//lifeskill\\loaded.lua", 'L');
    assert(!chdir(tl_path_resolve("lifeskill", out, sizeof(out))));
    assert(directory_contains(".", "loaded.lua"));
    assert(directory_contains("..", "lifeskill"));
    assert_marker("loaded.lua", 'L');
    assert_marker("/data/data/com.game3c.vhtgmobile/files/gameclient/script/lifeskill/loaded.lua", 'L');

    assert(!chdir(game));
    assert(!chdir(tl_path_resolve("script", out, sizeof(out))));
    assert(directory_contains(".", "lifeskill"));

    assert(!chdir(scripts));
    char tiny[4]; memset(tiny, 0xA5, sizeof(tiny));
    const char *original = "lifeskill\\loaded.lua";
    assert(tl_path_resolve(original, tiny, sizeof(tiny)) == original);
    for (unsigned i = 0; i < sizeof(tiny); i++) assert((unsigned char)tiny[i] == 0xA5);
    puts("PASS: production relative directory lookup preserves VHTG script recursion, cwd file precedence, normalized paths, Android aliases and missing-cwd fallback without truncating short buffers");
    return 0;
}
