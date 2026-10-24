/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The Java a Godot engine calls back into (org.godotengine.godot), in C.
 *
 *   - GodotIO: the device -- folders, locale, screen, the keyboard, opening links;
 *   - FileAccessHandler: files outside the APK (user://, the cache), by id: open, read and write through direct
 *     ByteBuffers, seek, size;
 *   - DirectoryAccessHandler: listing folders, of the APK's assets (res:// when the game is not one pack) or of the disk;
 *   - Godot, GodotNetUtils, GodotTTS: what the engine asks of the activity, answered as a phone with nothing special does.
 */
#define _DARWIN_C_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-godot.h"
#include "husk-tl-jni.h"
#include "husk-tl-ld.h"
#include "husk-tl-internal.h"

const char *tl_path_resolve(const char *path, char *buf, size_t n);

/* An Android path as a host one, always in `buf` (tl_path_resolve hands back the path itself when it needs no change). */
static void host_path(const char *path, char *buf, size_t n)
{
    const char *r = tl_path_resolve(path, buf, n);
    if (r != buf) snprintf(buf, n, "%s", r ? r : "");
}

static struct { char data[512], files[600], cache[600], pkg[128]; int w, h; } G;

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vj(int64_t j) { jvalue v; v.j = j; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }
static jvalue vd(double d) { jvalue v; v.d = d; return v; }
static void Noop(tl_jcall *c) { (void)c; }
static void RetFalse(tl_jcall *c) { c->ret = vz(0); }
static void RetTrue(tl_jcall *c) { c->ret = vz(1); }
static void RetZero(tl_jcall *c) { c->ret = vi(0); }
static void RetNull(tl_jcall *c) { c->ret = vl(NULL); }
static jobj *STR(const char *s) { return tl_jni_new_string(s); }
static const char *S(jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

static void mkdirs(const char *path)
{
    char p[1024];
    snprintf(p, sizeof(p), "%s", path);
    for (char *s = p + 1; *s; s++) if (*s == '/') { *s = 0; mkdir(p, 0755); *s = '/'; }
    mkdir(p, 0755);
}

/* --------------------------------------------------------------- GodotIO */

static void IO_dataDir(tl_jcall *c) { c->ret = vl(STR(G.files)); }
static void IO_cacheDir(tl_jcall *c) { c->ret = vl(STR(G.cache)); }
static void IO_locale(tl_jcall *c) { c->ret = vl(STR("en_US")); }
static void IO_model(tl_jcall *c) { c->ret = vl(STR("Pixel 8")); }
static void IO_dpi(tl_jcall *c) { c->ret = vi(460); }
static void IO_density(tl_jcall *c) { c->ret = vf(3.0f); }
static void IO_orientation(tl_jcall *c) { c->ret = vi(G.w > G.h ? 0 : 1); }    /* SCREEN_LANDSCAPE / SCREEN_PORTRAIT */
static void IO_refresh(tl_jcall *c) { c->ret = vd(60.0); }
static void IO_uniqueID(tl_jcall *c) { c->ret = vl(STR("husk0000000000000")); }
static void IO_systemDir(tl_jcall *c) { c->ret = vl(STR(G.files)); }
static void IO_openURI(tl_jcall *c) { tl_log_line("godot: the game asked to open %s", S(c->args[0].l)); c->ret = vi(0); }
/* The window's safe area, {x, y, width, height}: all of it (the app keeps the game clear of the notch). */
static void IO_safeArea(tl_jcall *c)
{
    jobj *a = tl_jni_new_prim_array('I', 4);
    int32_t *v = (int32_t *)a->arr.data;
    v[0] = 0; v[1] = 0; v[2] = G.w; v[3] = G.h;
    c->ret = vl(a);
}
static void IO_tempDir(tl_jcall *c) { c->ret = vl(STR(G.cache)); }
static void IO_cutouts(tl_jcall *c) { c->ret = vl(tl_jni_new_prim_array('I', 0)); }

/* -------------------------------------------------- FileAccessHandler (disk) */

#define MAX_FILES 256
static struct { int fd; bool eof; } F[MAX_FILES];
static pthread_mutex_t f_lock = PTHREAD_MUTEX_INITIALIZER;

/* Godot's FileAccess mode flags: READ 1, WRITE 2, READ_WRITE 3, WRITE_READ 7. */
static void FA_open(tl_jcall *c)
{
    const char *path = S(c->args[0].l);
    int mode = c->args[1].i, flags;
    switch (mode) {
    case 1: flags = O_RDONLY; break;
    case 2: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case 3: flags = O_RDWR; break;
    case 7: flags = O_RDWR | O_CREAT | O_TRUNC; break;
    default: flags = O_RDONLY;
    }
    if (flags & O_CREAT) { char dir[1024]; snprintf(dir, sizeof(dir), "%s", path); char *s = strrchr(dir, '/'); if (s) { *s = 0; mkdirs(dir); } }
    char host[1100];
    host_path(path, host, sizeof(host));
    int fd = open(host, flags, 0644);
    if (fd < 0) { c->ret = vi(errno == ENOENT ? -1 : 0); return; }        /* FILE_NOT_FOUND_ERROR_ID, INVALID_FILE_ID */
    pthread_mutex_lock(&f_lock);
    int id = 0;
    for (int i = 1; i < MAX_FILES; i++) if (!F[i].fd) { id = i; break; }
    if (id) { F[id].fd = fd + 1; F[id].eof = false; } else close(fd);
    pthread_mutex_unlock(&f_lock);
    c->ret = vi(id);
}
static int fd_of(int id) { return id > 0 && id < MAX_FILES && F[id].fd ? F[id].fd - 1 : -1; }
static void FA_close(tl_jcall *c) { int fd = fd_of(c->args[0].i); if (fd >= 0) { close(fd); F[c->args[0].i].fd = 0; } }
static void FA_size(tl_jcall *c) { struct stat st; int fd = fd_of(c->args[0].i); c->ret = vj(fd >= 0 && fstat(fd, &st) == 0 ? st.st_size : 0); }
static void FA_pos(tl_jcall *c) { int fd = fd_of(c->args[0].i); c->ret = vj(fd >= 0 ? lseek(fd, 0, SEEK_CUR) : 0); }
static void FA_seek(tl_jcall *c) { int id = c->args[0].i, fd = fd_of(id); if (fd >= 0) { lseek(fd, c->args[1].j, SEEK_SET); F[id].eof = false; } }
static void FA_seekEnd(tl_jcall *c) { int id = c->args[0].i, fd = fd_of(id); if (fd >= 0) { lseek(fd, c->args[1].j, SEEK_END); F[id].eof = false; } }
static void FA_eof(tl_jcall *c) { int id = c->args[0].i; c->ret = vz(fd_of(id) >= 0 && F[id].eof); }
static void FA_setEof(tl_jcall *c) { int id = c->args[0].i; if (fd_of(id) >= 0) F[id].eof = c->args[1].z; }
static void FA_flush(tl_jcall *c) { int fd = fd_of(c->args[0].i); if (fd >= 0) fsync(fd); }
static void FA_read(tl_jcall *c)
{
    int id = c->args[0].i, fd = fd_of(id);
    jobj *buf = c->args[1].l;
    char *p = buf ? tl_jni_get_field(buf, "address", "J").l : NULL;
    int64_t n = buf ? tl_jni_get_field(buf, "capacity", "J").j : 0;
    if (fd < 0 || !p || n <= 0) { c->ret = vi(0); return; }
    int64_t got = 0;
    while (got < n) { ssize_t r = read(fd, p + got, (size_t)(n - got)); if (r <= 0) break; got += r; }
    if (got < n) F[id].eof = true;
    c->ret = vi((int)got);
}
static void FA_write(tl_jcall *c)
{
    int fd = fd_of(c->args[0].i);
    jobj *buf = c->args[1].l;
    const char *p = buf ? tl_jni_get_field(buf, "address", "J").l : NULL;
    int64_t n = buf ? tl_jni_get_field(buf, "capacity", "J").j : 0;
    if (fd < 0 || !p) return;
    int64_t put = 0;
    while (put < n) { ssize_t w = write(fd, p + put, (size_t)(n - put)); if (w <= 0) break; put += w; }
}
static void FA_write4(tl_jcall *c) { FA_write(c); c->ret = vz(fd_of(c->args[0].i) >= 0); }
static void FA_resize(tl_jcall *c) { int fd = fd_of(c->args[0].i); c->ret = vi(fd >= 0 && ftruncate(fd, c->args[1].j) == 0 ? 0 : 1); }   /* OK, FAILED */
static void FA_sizeOf(tl_jcall *c) { char host[1100]; struct stat st; host_path(S(c->args[0].l), host, sizeof(host)); c->ret = vj(stat(host, &st) == 0 ? st.st_size : -1); }
static void FA_accessed(tl_jcall *c) { char host[1100]; struct stat st; host_path(S(c->args[0].l), host, sizeof(host)); c->ret = vj(stat(host, &st) == 0 ? st.st_atime : 0); }
static void FA_exists(tl_jcall *c) { char host[1100]; struct stat st; host_path(S(c->args[0].l), host, sizeof(host)); c->ret = vz(stat(host, &st) == 0 && S_ISREG(st.st_mode)); }
static void FA_modified(tl_jcall *c) { char host[1100]; struct stat st; host_path(S(c->args[0].l), host, sizeof(host)); c->ret = vj(stat(host, &st) == 0 ? st.st_mtime : 0); }

/* ------------------------------------------- DirectoryAccessHandler (assets, disk) */

/*
 * accessType: 0 the APK's assets (paths relative to assets/), 1 user data, 2 the filesystem. A listing is read in full when
 * opened; dirNext walks it and dirIsDir / isCurrentHidden ask about the entry dirNext last gave.
 */
#define MAX_DIRS 64
typedef struct { bool used; int n, at; char **names; bool *dirs; } listing;
static listing D[MAX_DIRS];
static pthread_mutex_t d_lock = PTHREAD_MUTEX_INITIALIZER;

static void add_name(listing *l, const char *name, bool dir)
{
    for (int i = 0; i < l->n; i++) if (!strcmp(l->names[i], name)) return;
    l->names = realloc(l->names, sizeof(char *) * (size_t)(l->n + 1));
    l->dirs = realloc(l->dirs, sizeof(bool) * (size_t)(l->n + 1));
    l->names[l->n] = strdup(name); l->dirs[l->n] = dir; l->n++;
}

/* "res://x" or "/x" or "x" -> "assets/x/" (the prefix entries under that asset folder start with). */
static void asset_prefix(const char *path, char *out, size_t n)
{
    if (!strncmp(path, "res://", 6)) path += 6;
    while (*path == '/') path++;
    snprintf(out, n, "assets/%s", path);
    size_t l = strlen(out);
    if (l && out[l - 1] != '/' && l + 1 < n) { out[l] = '/'; out[l + 1] = 0; }
}

static bool list_assets(const char *path, listing *l)
{
    char pre[1100];
    asset_prefix(path, pre, sizeof(pre));
    size_t pl = strlen(pre);
    bool any = false;
    for (int z = 0;; z++) {
        const tl_zip *zip = tl_ld_apk_at(z);
        if (!zip) break;
        for (size_t i = 0; i < zip->count; i++) {
            const char *name = zip->entries[i].name;
            if (strncmp(name, pre, pl) || !name[pl]) continue;
            any = true;
            const char *rest = name + pl, *slash = strchr(rest, '/');
            char item[512];
            if (slash) { snprintf(item, sizeof(item), "%.*s", (int)(slash - rest), rest); add_name(l, item, true); }
            else add_name(l, rest, false);
        }
    }
    return any || !strcmp(pre, "assets/");
}

static bool list_disk(const char *path, listing *l)
{
    char host[1100];
    host_path(path, host, sizeof(host));
    DIR *d = opendir(host);
    if (!d) return false;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1400]; struct stat st;
        snprintf(full, sizeof(full), "%s/%s", host, e->d_name);
        add_name(l, e->d_name, stat(full, &st) == 0 && S_ISDIR(st.st_mode));
    }
    closedir(d);
    return true;
}

/* The APK's assets for res:// and relative paths under access type 0; the disk for anything absolute, whatever the type says. */
static bool in_assets(int type, const char *path) { return type == 0 && path[0] != '/'; }

static void DA_open(tl_jcall *c)
{
    const char *path = S(c->args[1].l);
    int type = in_assets(c->args[0].i, path) ? 0 : 2;
    listing l = { 0 };
    bool ok = type == 0 ? list_assets(path, &l) : list_disk(path, &l);
    if (!ok) { c->ret = vi(-1); return; }
    pthread_mutex_lock(&d_lock);
    int id = -1;
    for (int i = 1; i < MAX_DIRS; i++) if (!D[i].used) { id = i; break; }
    if (id > 0) { D[id] = l; D[id].used = true; D[id].at = -1; }
    pthread_mutex_unlock(&d_lock);
    c->ret = vi(id);
}
static listing *dir_of(int id) { return id > 0 && id < MAX_DIRS && D[id].used ? &D[id] : NULL; }
static void DA_next(tl_jcall *c)
{
    listing *l = dir_of(c->args[1].i);
    if (!l || l->at + 1 >= l->n) { if (l) l->at = l->n; c->ret = vl(STR("")); return; }
    l->at++;
    c->ret = vl(STR(l->names[l->at]));
}
static void DA_isDir(tl_jcall *c) { listing *l = dir_of(c->args[1].i); c->ret = vz(l && l->at >= 0 && l->at < l->n && l->dirs[l->at]); }
static void DA_hidden(tl_jcall *c) { listing *l = dir_of(c->args[1].i); c->ret = vz(l && l->at >= 0 && l->at < l->n && l->names[l->at][0] == '.'); }
static void DA_close(tl_jcall *c)
{
    listing *l = dir_of(c->args[1].i);
    if (!l) return;
    for (int i = 0; i < l->n; i++) free(l->names[i]);
    free(l->names); free(l->dirs);
    memset(l, 0, sizeof(*l));
}
static bool asset_entry(const char *path, bool dir)
{
    char pre[1100];
    asset_prefix(path, pre, sizeof(pre));
    size_t pl = strlen(pre);
    if (!dir && pl) pre[--pl] = 0;                  /* a file: no trailing slash */
    for (int z = 0;; z++) {
        const tl_zip *zip = tl_ld_apk_at(z);
        if (!zip) break;
        if (!dir && tl_zip_find(zip, pre)) return true;
        if (dir) for (size_t i = 0; i < zip->count; i++) if (!strncmp(zip->entries[i].name, pre, pl)) return true;
    }
    return false;
}
static bool disk_is(const char *path, bool dir)
{
    char host[1100]; struct stat st;
    host_path(path, host, sizeof(host));
    return stat(host, &st) == 0 && (dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode));
}
static void DA_dirExists(tl_jcall *c)
{
    const char *p = S(c->args[1].l);
    c->ret = vz(in_assets(c->args[0].i, p) ? asset_entry(p, true) : disk_is(p, true));
    if (getenv("TL_GODOT_TRACE")) tl_log_line("godot: dirExists(%d, %s) -> %d", c->args[0].i, p, c->ret.z);
}
static void DA_fileExists(tl_jcall *c) { const char *p = S(c->args[1].l); c->ret = vz(in_assets(c->args[0].i, p) ? asset_entry(p, false) : disk_is(p, false)); }
static void DA_assetsFileExists(tl_jcall *c) { c->ret = vz(asset_entry(S(c->args[0].l), false)); }
static void DA_fsFileExists(tl_jcall *c) { c->ret = vz(disk_is(S(c->args[0].l), false)); }
static void DA_makeDir(tl_jcall *c)
{
    if (in_assets(c->args[0].i, S(c->args[1].l))) { c->ret = vz(0); return; }
    char host[1100]; host_path(S(c->args[1].l), host, sizeof(host));
    c->ret = vz(mkdir(host, 0755) == 0 || errno == EEXIST);
    if (getenv("TL_GODOT_TRACE")) tl_log_line("godot: makeDir(%d, %s) -> %d", c->args[0].i, host, c->ret.z);
}
static void DA_remove(tl_jcall *c)
{
    if (in_assets(c->args[0].i, S(c->args[1].l))) { c->ret = vz(0); return; }
    char host[1100]; host_path(S(c->args[1].l), host, sizeof(host));
    c->ret = vz(remove(host) == 0);
}
static void DA_rename(tl_jcall *c)
{
    if (in_assets(c->args[0].i, S(c->args[1].l))) { c->ret = vz(0); return; }
    char a[1100], b[1100];
    host_path(S(c->args[1].l), a, sizeof(a)); host_path(S(c->args[2].l), b, sizeof(b));
    c->ret = vz(rename(a, b) == 0);
}
/* Godot 4 drops the access type from the calls on an open listing: the id is the only argument. */
static void shift1(tl_jcall *c, void (*fn)(tl_jcall *))
{
    jvalue a[2] = { c->args[0], c->args[0] };
    tl_jcall c2 = *c;
    c2.args = a;
    fn(&c2);
    c->ret = c2.ret;
}
static void DA4_next(tl_jcall *c) { shift1(c, DA_next); }
static void DA4_close(tl_jcall *c) { shift1(c, DA_close); }
static void DA4_isDir(tl_jcall *c) { shift1(c, DA_isDir); }
static void DA4_hidden(tl_jcall *c) { shift1(c, DA_hidden); }
static void DA_spaceLeft(tl_jcall *c) { struct statvfs v; c->ret = vj(statvfs(G.data, &v) == 0 ? (int64_t)v.f_bavail * (int64_t)v.f_frsize : 0); }
static void DA_drive(tl_jcall *c) { c->ret = vl(STR("")); }

/* ------------------------------------------------------------------ Godot */

jobj *tl_hle_activity(void);
static void G_activity(tl_jcall *c) { c->ret = vl(tl_hle_activity()); }
static jobj *g_render_view, *g_input_handler;
static void G_renderView(tl_jcall *c)
{
    if (!g_render_view) g_render_view = tl_jni_new_object(tl_jni_class("org/godotengine/godot/GodotRenderView"));
    c->ret = vl(g_render_view);
}
static void RV_inputHandler(tl_jcall *c)
{
    if (!g_input_handler) g_input_handler = tl_jni_new_object(tl_jni_class("org/godotengine/godot/input/GodotInputHandler"));
    c->ret = vl(g_input_handler);
}
static void G_glesVersion(tl_jcall *c) { c->ret = vi(0x30000); }
static void G_strings(tl_jcall *c) { c->ret = vl(tl_jni_new_obj_array(tl_jni_class("java/lang/String"), 0)); }
static void G_emptyString(tl_jcall *c) { c->ret = vl(STR("")); }
static void G_finish(tl_jcall *c) { (void)c; tl_godot_quit(); c->ret = vz(1); }
static void G_alert(tl_jcall *c) { tl_log_line("godot: alert \"%s\": %s", S(c->args[1].l), S(c->args[0].l)); }

#define IO "org/godotengine/godot/GodotIO"
#define FA "org/godotengine/godot/io/file/FileAccessHandler"
#define DA "org/godotengine/godot/io/directory/DirectoryAccessHandler"
#define GD "org/godotengine/godot/Godot"
#define M(c, n, s, f) { c, n, s, f }
static const tl_jhle k_godot[] = {
    M(IO, "getDataDir", "()Ljava/lang/String;", IO_dataDir), M(IO, "getCacheDir", "()Ljava/lang/String;", IO_cacheDir),
    M(IO, "getLocale", "()Ljava/lang/String;", IO_locale), M(IO, "getModel", "()Ljava/lang/String;", IO_model),
    M(IO, "getScreenDPI", "()I", IO_dpi), M(IO, "getScaledDensity", "()F", IO_density),
    M(IO, "getScreenOrientation", "()I", IO_orientation), M(IO, "setScreenOrientation", "(I)V", Noop),
    M("android/content/res/AssetManager", "setScreenOrientation", "(I)V", Noop),
    M(IO, "getScreenRefreshRate", "(D)D", IO_refresh), M(IO, "getUniqueID", "()Ljava/lang/String;", IO_uniqueID),
    M(IO, "getSystemDir", "(IZ)Ljava/lang/String;", IO_systemDir), M(IO, "openURI", "(Ljava/lang/String;)I", IO_openURI),
    M(IO, "getWindowSafeArea", "()[I", IO_safeArea), M(IO, "getDisplaySafeArea", "()[I", IO_safeArea),
    M(IO, "getDisplayCutouts", "()[I", IO_cutouts),
    M(IO, "showKeyboard", "(Ljava/lang/String;IIII)V", Noop), M(IO, "showKeyboard", "(Ljava/lang/String;ZIII)V", Noop),
    M(IO, "hideKeyboard", "()V", Noop), M(IO, "hasHardwareKeyboard", "()Z", RetFalse),

    M(FA, "fileOpen", "(Ljava/lang/String;I)I", FA_open), M(FA, "fileClose", "(I)V", FA_close),
    M(FA, "fileGetSize", "(I)J", FA_size), M(FA, "fileGetPosition", "(I)J", FA_pos),
    M(FA, "fileSeek", "(IJ)V", FA_seek), M(FA, "fileSeekFromEnd", "(IJ)V", FA_seekEnd),
    M(FA, "isFileEof", "(I)Z", FA_eof), M(FA, "fileRead", "(ILjava/nio/ByteBuffer;)I", FA_read),
    M(FA, "fileWrite", "(ILjava/nio/ByteBuffer;)V", FA_write), M(FA, "fileFlush", "(I)V", FA_flush),
    M(FA, "fileExists", "(Ljava/lang/String;)Z", FA_exists), M(FA, "fileLastModified", "(Ljava/lang/String;)J", FA_modified),

    M(DA, "dirOpen", "(ILjava/lang/String;)I", DA_open), M(DA, "dirNext", "(II)Ljava/lang/String;", DA_next),
    M(DA, "dirClose", "(II)V", DA_close), M(DA, "dirIsDir", "(II)Z", DA_isDir), M(DA, "isCurrentHidden", "(II)Z", DA_hidden),
    M(DA, "dirExists", "(ILjava/lang/String;)Z", DA_dirExists), M(DA, "fileExists", "(ILjava/lang/String;)Z", DA_fileExists),
    M(DA, "assetsFileExists", "(Ljava/lang/String;)Z", DA_assetsFileExists), M(DA, "filesystemFileExists", "(Ljava/lang/String;)Z", DA_fsFileExists),
    M(DA, "makeDir", "(ILjava/lang/String;)Z", DA_makeDir), M(DA, "remove", "(ILjava/lang/String;)Z", DA_remove),
    M(DA, "rename", "(ILjava/lang/String;Ljava/lang/String;)Z", DA_rename), M(DA, "getSpaceLeft", "(I)J", DA_spaceLeft),
    M(DA, "getDriveCount", "(I)I", RetZero), M(DA, "getDrive", "(II)Ljava/lang/String;", DA_drive),

    M(GD, "getGLESVersionCode", "()I", G_glesVersion), M(GD, "getCommandLine", "()[Ljava/lang/String;", G_strings),
    M(GD, "getGrantedPermissions", "()[Ljava/lang/String;", G_strings), M(GD, "requestPermission", "(Ljava/lang/String;)Z", RetTrue),
    M(GD, "requestPermissions", "()Z", RetTrue), M(GD, "getCACertificates", "()Ljava/lang/String;", G_emptyString),
    M(GD, "getClipboard", "()Ljava/lang/String;", G_emptyString), M(GD, "setClipboard", "(Ljava/lang/String;)V", Noop),
    M(GD, "hasClipboard", "()Z", RetFalse), M(GD, "setKeepScreenOn", "(Z)V", Noop), M(GD, "vibrate", "(I)V", Noop),
    M(GD, "restart", "()V", Noop), M(GD, "forceQuit", "()V", G_finish), M(GD, "forceQuit", "(I)Z", G_finish),
    M(GD, "alert", "(Ljava/lang/String;Ljava/lang/String;)V", G_alert), M(GD, "initInputDevices", "()V", Noop),
    M(GD, "getInputFallbackMapping", "()Ljava/lang/String;", G_emptyString), M(GD, "onGodotSetupCompleted", "()V", Noop),
    M(GD, "onGodotMainLoopStarted", "()V", Noop), M(GD, "isProjectManagerHint", "()Z", RetFalse),
    M(GD, "onVideoInit", "()V", Noop), M(GD, "createOffscreenGL", "()Z", RetFalse), M(GD, "destroyOffscreenGL", "()V", Noop),
    M(GD, "setOffscreenGLCurrent", "(Z)V", Noop), M(GD, "getSurface", "()Landroid/view/Surface;", RetNull),
    M(GD, "isActivityResumed", "()Z", RetTrue), M(GD, "createNewGodotInstance", "([Ljava/lang/String;)I", RetZero),
    M(GD, "beginBenchmarkMeasure", "(Ljava/lang/String;)V", Noop), M(GD, "endBenchmarkMeasure", "(Ljava/lang/String;)V", Noop),
    M(GD, "dumpBenchmark", "(Ljava/lang/String;)V", Noop),
    M(FA, "setFileEof", "(IZ)V", FA_setEof),
    M("org/godotengine/godot/utils/GodotNetUtils", "multicastLockAcquire", "()V", Noop),
    M("org/godotengine/godot/utils/GodotNetUtils", "multicastLockRelease", "()V", Noop),
    M(DA, "dirNext", "(I)Ljava/lang/String;", DA4_next), M(DA, "dirClose", "(I)V", DA4_close),
    M(DA, "dirIsDir", "(I)Z", DA4_isDir), M(DA, "isCurrentHidden", "(I)Z", DA4_hidden),
    M(FA, "fileWrite", "(ILjava/nio/ByteBuffer;)Z", FA_write4), M(FA, "fileResize", "(IJ)I", FA_resize),
    M(FA, "fileSize", "(Ljava/lang/String;)J", FA_sizeOf), M(FA, "fileLastAccessed", "(Ljava/lang/String;)J", FA_accessed),
    M(IO, "getTempDir", "()Ljava/lang/String;", IO_tempDir), M(IO, "getDisplayRotation", "()I", RetZero),
    M(GD, "isDarkModeSupported", "()Z", RetFalse), M(GD, "isDarkMode", "()Z", RetFalse),
    M(GD, "getAccentColor", "()I", RetZero), M(GD, "getBaseColor", "()I", RetZero),
    M(GD, "showDialog", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)V", Noop),
    M(GD, "showInputDialog", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V", Noop),
    M(GD, "showFilePicker", "(Ljava/lang/String;Ljava/lang/String;I[Ljava/lang/String;)V", Noop),
    M(GD, "vibrate", "(II)V", Noop), M(GD, "onGodotTerminating", "()V", Noop),
    M(GD, "nativeBeginBenchmarkMeasure", "(Ljava/lang/String;Ljava/lang/String;)V", Noop),
    M(GD, "nativeEndBenchmarkMeasure", "(Ljava/lang/String;Ljava/lang/String;)V", Noop),
    M(GD, "nativeDumpBenchmark", "(Ljava/lang/String;)V", Noop),
    M(GD, "getGDExtensionConfigFiles", "()[Ljava/lang/String;", G_strings),
    M(GD, "checkInternalFeatureSupport", "(Ljava/lang/String;)Z", RetFalse),
    M(GD, "nativeEnableImmersiveMode", "(Z)V", Noop), M(GD, "isInImmersiveMode", "()Z", RetTrue),
    M(GD, "setWindowColor", "(Ljava/lang/String;)V", Noop), M(GD, "getActivity", "()Landroid/app/Activity;", G_activity),
    M(GD, "getRenderView", "()Lorg/godotengine/godot/GodotRenderView;", G_renderView),
    M("org/godotengine/godot/GodotRenderView", "getInputHandler", "()Lorg/godotengine/godot/input/GodotInputHandler;", RV_inputHandler),
    M("org/godotengine/godot/GodotRenderView", "requestPointerCapture", "()V", Noop),
    M("org/godotengine/godot/GodotRenderView", "releasePointerCapture", "()V", Noop),
    { NULL, NULL, NULL, NULL }
};

static const char *const k_classes[] = {
    "org/godotengine/godot/GodotRenderView", "org/godotengine/godot/input/GodotInputHandler", GD, IO, FA, DA, "org/godotengine/godot/GodotLib", "org/godotengine/godot/utils/GodotNetUtils", "org/godotengine/godot/tts/GodotTTS",
};

void tl_godot_kotlin_install(void);

void tl_godot_hle_install(const char *pkg, const char *apk, const char *data, int w, int h)
{
    (void)apk;
    snprintf(G.data, sizeof(G.data), "%s", data);
    snprintf(G.files, sizeof(G.files), "%s/files", data);
    snprintf(G.cache, sizeof(G.cache), "%s/cache", data);
    snprintf(G.pkg, sizeof(G.pkg), "%s", pkg);
    G.w = w; G.h = h;
    mkdirs(G.files); mkdirs(G.cache);
    for (size_t i = 0; i < sizeof(k_classes) / sizeof(k_classes[0]); i++) tl_jni_declare(k_classes[i], "java/lang/Object");
    tl_jni_register_hle(k_godot);
    /* Godot 4.7 calls these four on the AssetManager it was given, not on GodotIO: same answers there (#140). */
    static const tl_jhle k_assets[] = {
        M("android/content/res/AssetManager", "getScreenRefreshRate", "(D)D", IO_refresh),
        M("android/content/res/AssetManager", "getDisplaySafeArea", "()[I", IO_safeArea),
        M("android/content/res/AssetManager", "getLocale", "()Ljava/lang/String;", IO_locale),
        M("android/content/res/AssetManager", "getDataDir", "()Ljava/lang/String;", IO_dataDir),
        { NULL, NULL, NULL, NULL }
    };
    tl_jni_register_hle(k_assets);
    /* Godot 4.7 calls what Godot.java answered on org.godotengine.godot.nativeapi.GodotNativeBridge instead: the same answers
     * there (#140: getRenderView answered null and the engine dereferenced it on its GL thread) */
    static const char NB[] = "org/godotengine/godot/nativeapi/GodotNativeBridge";
    static tl_jhle bridge[sizeof(k_godot) / sizeof(k_godot[0])];
    size_t nb = 0;
    for (const tl_jhle *e = k_godot; e->cls; e++) if (!strcmp(e->cls, GD)) { bridge[nb] = *e; bridge[nb].cls = NB; nb++; }
    bridge[nb] = (tl_jhle){ NULL, NULL, NULL, NULL };
    tl_jni_declare(NB, "java/lang/Object");
    tl_jni_register_hle(bridge);
    tl_godot_kotlin_install();
}
