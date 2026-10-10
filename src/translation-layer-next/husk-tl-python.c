/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Python games and apps (Kivy, pygame, anything built with python-for-android): SDL2 with CPython inside libmain.
 *
 * python-for-android's PythonActivity does three things before SDL starts the interpreter:
 *   - unpacks the app (assets/private.tar, gzipped) into files/app;
 *   - unpacks Python's standard library and modules (lib/<abi>/libpybundle.so, a gzipped tar despite the name) there too;
 *   - sets the environment p4a's start.c reads: ANDROID_ARGUMENT, ANDROID_ENTRYPOINT, PYTHONHOME, PYTHONPATH and friends.
 * This does the same, so the SDL driver can run the game as it runs any other SDL2 game.
 */
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"

static void mkdirs(const char *path)
{
    char p[2048];
    snprintf(p, sizeof(p), "%s", path);
    for (char *s = p + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(p, 0755); *s = '/'; }
    mkdir(p, 0755);
}

static long octal(const char *s, size_t n)
{
    long v = 0;
    for (size_t i = 0; i < n && s[i]; i++) if (s[i] >= '0' && s[i] <= '7') v = v * 8 + (s[i] - '0');
    return v;
}

/* Extract a gzipped tar held in memory into `dir`. Returns the number of files written, or -1. */
static int untar_gz(const uint8_t *gz, size_t gzlen, const char *dir)
{
    z_stream z;
    memset(&z, 0, sizeof(z));
    if (inflateInit2(&z, 16 + MAX_WBITS) != Z_OK) return -1;
    size_t cap = gzlen * 4 + (1 << 20), len = 0;
    uint8_t *tar = malloc(cap);
    z.next_in = (Bytef *)gz; z.avail_in = (uInt)gzlen;
    int r;
    do {
        if (len == cap) { cap *= 2; tar = realloc(tar, cap); }
        z.next_out = tar + len; z.avail_out = (uInt)(cap - len);
        r = inflate(&z, Z_NO_FLUSH);
        len = cap - z.avail_out;
    } while (r == Z_OK);
    inflateEnd(&z);
    if (r != Z_STREAM_END) { free(tar); return -1; }

    int files = 0;
    char longname[1024] = "";
    for (size_t o = 0; o + 512 <= len; ) {
        const char *h = (const char *)tar + o;
        if (!h[0]) break;
        long size = octal(h + 124, 12);
        char type = h[156];
        char name[1100];
        if (longname[0]) { snprintf(name, sizeof(name), "%s", longname); longname[0] = 0; }
        else if (!memcmp(h + 257, "ustar", 5) && h[345]) snprintf(name, sizeof(name), "%.155s/%.100s", h + 345, h);
        else snprintf(name, sizeof(name), "%.100s", h);
        const uint8_t *data = tar + o + 512;
        o += 512 + (size_t)((size + 511) & ~511L);
        if (o > len + 512) break;
        if (type == 'L') { snprintf(longname, sizeof(longname), "%.*s", (int)(size < 1023 ? size : 1023), (const char *)data); continue; }
        if (type == 'x' || type == 'g') continue;                      /* pax headers: the plain names are enough here */
        if (strstr(name, "..")) continue;
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", dir, name);
        if (type == '5') { mkdirs(path); continue; }
        if (type != '0' && type != 0) continue;
        char *slash = strrchr(path, '/');
        if (slash) { *slash = 0; mkdirs(path); *slash = '/'; }
        FILE *f = fopen(path, "wb");
        if (!f) continue;
        fwrite(data, 1, (size_t)size, f);
        fclose(f);
        files++;
    }
    free(tar);
    return files;
}

static int extract_entry(tl_zip *z, const char *entry, const char *dir)
{
    const tl_zip_entry *e = tl_zip_find(z, entry);
    if (!e) return 0;
    const uint8_t *d; size_t len; bool owned; char err[160];
    if (!tl_zip_data(z, e, (size_t)1 << 30, &d, &len, &owned, err, sizeof(err))) { tl_log_line("python: cannot read %s: %s", entry, err); return -1; }
    int n = untar_gz(d, len, dir);
    if (owned) free((void *)d);
    tl_log_line("python: %s -> %d files", entry, n);
    return n;
}

bool tl_python_is_app(const char *apk)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
    bool yes = tl_zip_find(&z, "assets/private.tar") && (tl_zip_find(&z, "lib/arm64-v8a/libpybundle.so")
               || tl_zip_find(&z, "lib/arm64-v8a/libpython3.9.so") || tl_zip_find(&z, "lib/arm64-v8a/libpython3.10.so")
               || tl_zip_find(&z, "lib/arm64-v8a/libpython3.11.so"));
    tl_zip_close(&z);
    return yes;
}

/* Unpack the app and Python once per APK (a stamp of its size and time), and set the environment. */
bool tl_python_prepare(const char *apk, const char *data_dir)
{
    if (!tl_python_is_app(apk)) return false;
    char files[1024], app[1100], stamp_path[1200], stamp[64];
    snprintf(files, sizeof(files), "%s/files", data_dir);
    snprintf(app, sizeof(app), "%s/app", files);
    snprintf(stamp_path, sizeof(stamp_path), "%s/.husk-unpacked", app);
    struct stat st;
    stat(apk, &st);
    snprintf(stamp, sizeof(stamp), "%lld-%ld", (long long)st.st_size, (long)st.st_mtime);
    char have[64] = "";
    FILE *f = fopen(stamp_path, "r");
    if (f) { if (!fgets(have, sizeof(have), f)) have[0] = 0; fclose(f); }
    if (strcmp(have, stamp)) {
        tl_log_line("python: unpacking the app and Python into %s", app);
        mkdirs(app);
        tl_zip z; char err[160];
        if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
        extract_entry(&z, "assets/private.tar", app);
        extract_entry(&z, "lib/arm64-v8a/libpybundle.so", app);
        tl_zip_close(&z);
        f = fopen(stamp_path, "w");
        if (f) { fputs(stamp, f); fclose(f); }
    }
    char entry[1200];
    snprintf(entry, sizeof(entry), "%s/main.pyc", app);
    if (access(entry, F_OK) != 0) snprintf(entry, sizeof(entry), "%s/main.py", app);
    char path[2300];
    snprintf(path, sizeof(path), "%s:%s/lib", app, app);
    setenv("ANDROID_ENTRYPOINT", entry, 1);
    setenv("ANDROID_ARGUMENT", app, 1);
    setenv("ANDROID_APP_PATH", app, 1);
    setenv("ANDROID_PRIVATE", files, 1);
    setenv("ANDROID_UNPACK", app, 1);
    setenv("PYTHONHOME", app, 1);
    setenv("PYTHONPATH", path, 1);
    setenv("PYTHONOPTIMIZE", "2", 1);
    setenv("P4A_BOOTSTRAP", "SDL2", 1);
    tl_log_line("python: entry point %s", entry);
    return true;
}
