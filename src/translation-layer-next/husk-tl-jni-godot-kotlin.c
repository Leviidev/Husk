/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Godot 4.6/4.7 keep DirectoryAccessHandler in Kotlin, and moving it changed the signatures behind four of its natives -- while
 * GodotLib still asks for them by the Java ones, in dir_access_jandroid.cpp. The listing calls lost their access type (the id
 * alone names the folder, as in DirectoryAccessHandler.kt's interface) and fileWrite answers whether it wrote. The engine gets a
 * NoSuchMethodError for each and carries on without directories, so it cannot make the shader cache folder under user:// and
 * never finds what it saved: "Startup history could not be saved: 12" (#140). These answer the Kotlin forms from the same file
 * ids as the Java ones, so both handlers reach one file.
 */
#define _DARWIN_C_SOURCE
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-jni.h"

const char *tl_path_resolve(const char *path, char *buf, size_t n);

#define DA "org/godotengine/godot/io/directory/DirectoryAccessHandler"
#define FA "org/godotengine/godot/io/file/FileAccessHandler"

static jvalue vzin(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static const char *S(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

static void host_path(const char *path, char *buf, size_t n)
{
    const char *r = tl_path_resolve(path, buf, n);
    if (r != buf) snprintf(buf, n, "%s", r ? r : "");
}

/* Every folder above `path`, made as it goes: iOS has no /tmp for a game to land in, so a tree the engine writes into has to be
 * there first (FileData's open makes its own parent folders the same way). */
static void mkparents(const char *path)
{
    char p[1100];
    snprintf(p, sizeof(p), "%s", path);
    char *s = strrchr(p, '/');
    if (!s || s == p) return;
    *s = 0;
    for (char *c = p + 1; *c; c++) if (*c == '/') { *c = 0; mkdir(p, 0755); *c = '/'; }
    mkdir(p, 0755);
}

/* ------------------------------------------------------------ folders */

/*
 * The engine calls dirOpen(nativeAccessType, path), keeps only the id it gets back, then walks the folder with dirNext(id): the
 * path has to travel with the id. Ids start at 4096, clear of FileAccessHandler's (which are kernel descriptor numbers) and
 * never reused, so several of the engine's threads opening and closing folders cannot get each other's -- Kotlin's ids would
 * be reused once the ones before them closed, as the engine has two of every folder open at once when it copies a save.
 */
typedef struct { int id; char host[1152]; bool used; struct listing *list; } dirlink;
typedef struct listing { int n, at; char **names; bool *dirs; } listing;
static dirlink g_dirs[64];
static int g_next_dir = 4096;
static pthread_mutex_t g_dir_lock = PTHREAD_MUTEX_INITIALIZER;

static dirlink *dir_find(int id)
{
    for (int i = 0; i < 64; i++) if (g_dirs[i].used && g_dirs[i].id == id) return &g_dirs[i];
    return NULL;
}

static void listing_free(listing *l)
{
    if (!l) return;
    for (int i = 0; i < l->n; i++) free(l->names[i]);
    free(l->names); free(l->dirs); free(l);
}

static int dir_open_impl(tl_jcall *c)
{
    const char *path = S(c->args[1].l);
    char host[1100];
    host_path(path, host, sizeof(host));
    struct stat st;
    if (stat(host, &st) != 0 || !S_ISDIR(st.st_mode)) return 0;
    DIR *d = opendir(host);
    if (!d) return 0;
    listing *l = calloc(1, sizeof(*l));
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char full[1400];
        snprintf(full, sizeof(full), "%s/%s", host, e->d_name);
        l->names = realloc(l->names, sizeof(char *) * (size_t)(l->n + 1));
        l->dirs = realloc(l->dirs, sizeof(bool) * (size_t)(l->n + 1));
        l->names[l->n] = strdup(e->d_name);
        l->dirs[l->n] = stat(full, &st) == 0 && S_ISDIR(st.st_mode);
        l->n++;
    }
    closedir(d);
    l->at = -1;
    pthread_mutex_lock(&g_dir_lock);
    dirlink *slot = NULL;
    for (int i = 0; i < 64 && !slot; i++) if (!g_dirs[i].used) slot = &g_dirs[i];
    if (slot) {
        slot->used = true; slot->id = g_next_dir++;
        snprintf(slot->host, sizeof(slot->host), "%s", host);
        slot->list = l;
    }
    int id = slot ? slot->id : 0;
    pthread_mutex_unlock(&g_dir_lock);
    if (!id) listing_free(l);
    return id;
}

/* The Kotlin handler's listing calls, which name only the id. */
static void kotlin_dirOpen(tl_jcall *c) { c->ret.j = 0; c->ret.i = dir_open_impl(c); }
static void kotlin_dirNext(tl_jcall *c)
{
    
    pthread_mutex_lock(&g_dir_lock);
    dirlink *d = dir_find(c->args[0].i);
    listing *l = d ? d->list : NULL;
    const char *name = "";
    if (l) {
        if (l->at + 1 < l->n) name = l->names[++l->at];
        else l->at = l->n;
    }
    pthread_mutex_unlock(&g_dir_lock);
    c->ret.j = 0;
    c->ret.l = tl_jni_new_string(name);
}
static void kotlin_dirClose(tl_jcall *c)
{
    pthread_mutex_lock(&g_dir_lock);
    dirlink *d = dir_find(c->args[0].i);
    if (d) { listing_free(d->list); d->used = false; d->list = NULL; }
    pthread_mutex_unlock(&g_dir_lock);
    c->ret.j = 0;
}
static void kotlin_dirIsDir(tl_jcall *c)
{
    pthread_mutex_lock(&g_dir_lock);
    dirlink *d = dir_find(c->args[0].i);
    bool r = d && d->list && d->list->at >= 0 && d->list->at < d->list->n && d->list->dirs[d->list->at];
    pthread_mutex_unlock(&g_dir_lock);
    c->ret = vzin(r);
}
static void kotlin_isCurrentHidden(tl_jcall *c)
{
    pthread_mutex_lock(&g_dir_lock);
    dirlink *d = dir_find(c->args[0].i);
    bool r = d && d->list && d->list->at >= 0 && d->list->at < d->list->n && d->list->names[d->list->at][0] == '.';
    pthread_mutex_unlock(&g_dir_lock);
    c->ret = vzin(r);
}

/* A folder the engine only asks about is made then, as FilesystemDirectoryAccess.makeDir does on the phone (it creates it and
 * answers whether it is there): Godot's DirAccess::make_dir stops at ERR_ALREADY_EXISTS when the folder lists as present, but
 * open_dynamic_library, and whatever the game wrote to user://<package> on the last run, ask before making. Only the engine's
 * absolute paths: res:// and assets:// are the APK's and stay read-only. */
static void dirExists_create(tl_jcall *c)
{
    struct stat st;
    char host[1100];
    const char *path = S(c->args[1].l);
    
    host_path(path, host, sizeof(host));
    if (stat(host, &st) == 0) { c->ret = vzin(S_ISDIR(st.st_mode)); return; }
    if (path[0] != '/') { c->ret = vzin(0); return; }
    mkparents(host);
    c->ret = vzin(mkdir(host, 0755) == 0);
}
static void makeDir_create(tl_jcall *c)
{
    struct stat st;
    char host[1100];
    
    host_path(S(c->args[1].l), host, sizeof(host));
    if (stat(host, &st) == 0) { c->ret = vzin(S_ISDIR(st.st_mode)); return; }
    mkparents(host);
    c->ret = vzin(mkdir(host, 0755) == 0);
}
static void remove_path(tl_jcall *c)
{
    char host[1100];
    host_path(S(c->args[1].l), host, sizeof(host));
    c->ret = vzin(unlink(host) == 0 || rmdir(host) == 0);
}
/* A save is written to a temporary and renamed over the old one, and iOS's rename will not replace an existing file: keep the
 * old one aside, move the new one in, drop the old. (The phone's FileChannelDataAccess copies over the old one, to the effect.) */
static void rename_path(tl_jcall *c)
{
    char a[1100], b[1100];
    
    host_path(S(c->args[1].l), a, sizeof(a));
    host_path(S(c->args[2].l), b, sizeof(b));
    if (rename(a, b) == 0) { c->ret = vzin(1); return; }
    char old[1300];
    snprintf(old, sizeof(old), "%s.husk-old", b);
    unlink(old);
    if (rename(b, old) == 0 && rename(a, b) == 0) { unlink(old); c->ret = vzin(1); return; }
    rename(old, b);
    c->ret = vzin(0);
}

/* ---------------------------------------------------------- the files */

/*
 * A file is a descriptor plus one, the id the Java side hands the engine already, so both handlers share one set of ids: a
 * game that reads a file through the new handler and writes it through the old one (the engine keeps whichever it was given)
 * finds the same file.
 */
#define MAX_FILES 256
static struct { int fd; bool eof; } F[MAX_FILES];
static pthread_mutex_t f_lock = PTHREAD_MUTEX_INITIALIZER;

static int file_add(int fd)
{
    pthread_mutex_lock(&f_lock);
    int id = 0;
    for (int i = 1; i < MAX_FILES; i++) if (!F[i].fd) { id = i; F[i].fd = fd + 1; F[i].eof = false; break; }
    pthread_mutex_unlock(&f_lock);
    return id;
}
static int fd_of(int id) { return id > 0 && id < MAX_FILES && F[id].fd ? F[id].fd - 1 : -1; }

/* Godot's FileAccess mode flags: READ 1, WRITE 2, READ_WRITE 3, WRITE_READ 7. */
static int flags_of(int mode)
{
    switch (mode) {
    case 2: return O_WRONLY | O_CREAT | O_TRUNC;
    case 3: return O_RDWR;
    case 7: return O_RDWR | O_CREAT | O_TRUNC;
    default: return O_RDONLY;
    }
}

static void kotlin_fileOpen(tl_jcall *c)
{
    
    char host[1100];
    host_path(S(c->args[0].l), host, sizeof(host));
    int flags = flags_of(c->args[1].i);
    if (flags & O_CREAT) mkparents(host);
    int fd = open(host, flags, 0644);
    c->ret.j = 0;
    if (fd < 0) { c->ret.i = -(errno == ENOENT ? 7 : 12); return; }      /* -ERR_FILE_NOT_FOUND, -ERR_CANT_OPEN */
    int id = file_add(fd);
    if (!id) { close(fd); c->ret.i = 0; }
    else c->ret.i = id;
}
static void kotlin_fileRead(tl_jcall *c)
{
    int id = c->args[0].i, fd = fd_of(id);
    jobj *buf = c->args[1].l;
    char *p = buf ? tl_jni_get_field(buf, "address", "J").l : NULL;
    int64_t n = buf ? tl_jni_get_field(buf, "capacity", "J").j : 0;
    c->ret.j = 0;
    if (fd < 0 || !p || n <= 0) { c->ret.i = 0; return; }
    int64_t got = 0;
    while (got < n) { ssize_t r = read(fd, p + got, (size_t)(n - got)); if (r <= 0) break; got += r; }
    if (got < n) F[id].eof = true;
    c->ret.i = (int32_t)got;
}
/* Kotlin's answers whether it wrote; the Java one (_4) does the same for a caller that reads it, this for one that does not. */
static int write_buffer(int id, jobj *buf)
{
    int fd = fd_of(id);
    const char *p = buf ? tl_jni_get_field(buf, "address", "J").l : NULL;
    int64_t n = buf ? tl_jni_get_field(buf, "capacity", "J").j : 0;
    if (fd < 0 || !p) return 0;
    int64_t put = 0;
    while (put < n) { ssize_t w = write(fd, p + put, (size_t)(n - put)); if (w <= 0) return 0; put += w; }
    return 1;
}
static void kotlin_fileWrite(tl_jcall *c) { c->ret = vzin(write_buffer(c->args[0].i, c->args[1].l)); }
/* The Java form answers nothing; a caller that does not read the return gets this one. */
static void void_fileWrite(tl_jcall *c) { int id = c->args[0].i; write_buffer(id, c->args[1].l); c->ret.j = 0; }
/* Kotlin's throws where the operation fails (FileData.resize's FileChannel), for the engine to log and carry on; the Java form
 * answers the engine's error value. */
static void kotlin_fileResize(tl_jcall *c)
{
    int fd = fd_of(c->args[0].i);
    c->ret.j = 0;
    if (fd < 0 || ftruncate(fd, (off_t)c->args[1].j) != 0) tl_jni_throw("java/io/IOException", "fileResize");
}
static void int_fileResize(tl_jcall *c)
{
    int fd = fd_of(c->args[0].i);
    c->ret.j = 0;
    c->ret.i = fd >= 0 && ftruncate(fd, (off_t)c->args[1].j) == 0 ? 0 : 1;    /* OK, FAILED */
}

/* ------------------------------------------- by name, and by id alone */

static void name_stat(const char *path, struct stat *st)
{
    char host[1100];
    host_path(path, host, sizeof(host));
    if (stat(host, st) != 0) memset(st, 0, sizeof(*st));
}
static void name_fileExists(tl_jcall *c) { struct stat st; name_stat(S(c->args[0].l), &st); c->ret = vzin(st.st_mode != 0); }
static void name_fileSize(tl_jcall *c) { struct stat st; name_stat(S(c->args[0].l), &st); c->ret.j = S_ISREG(st.st_mode) ? (int64_t)st.st_size : -1; }
static void name_fileLastModified(tl_jcall *c) { struct stat st; name_stat(S(c->args[0].l), &st); c->ret.j = (int64_t)st.st_mtime; }
static void name_fileLastAccessed(tl_jcall *c) { struct stat st; name_stat(S(c->args[0].l), &st); c->ret.j = (int64_t)st.st_atime; }
static void name_fileGetPosition(tl_jcall *c) { int fd = fd_of(c->args[0].i); c->ret.j = fd >= 0 ? (int64_t)lseek(fd, 0, SEEK_CUR) : 0; }
static void name_fileGetSize(tl_jcall *c) { struct stat st; int fd = fd_of(c->args[0].i); c->ret.j = fd >= 0 && fstat(fd, &st) == 0 ? (int64_t)st.st_size : 0; }
static void name_isFileEof(tl_jcall *c) { int id = c->args[0].i; c->ret = vzin(fd_of(id) >= 0 && F[id].eof); }
static void name_setFileEof(tl_jcall *c) { int id = c->args[0].i; if (fd_of(id) >= 0) F[id].eof = c->args[1].z; }
/* Unit: it answers nothing, and says nothing broke. */
static void unit_fileSeek(tl_jcall *c) { int id = c->args[0].i, fd = fd_of(id); c->ret.j = 0; if (fd >= 0) { lseek(fd, (off_t)c->args[1].j, SEEK_SET); F[id].eof = false; } }
static void unit_fileSeekFromEnd(tl_jcall *c) { int id = c->args[0].i, fd = fd_of(id); c->ret.j = 0; if (fd >= 0) { lseek(fd, (off_t)c->args[1].j, SEEK_END); F[id].eof = false; } }
static void unit_fileFlush(tl_jcall *c) { int fd = fd_of(c->args[0].i); c->ret.j = 0; if (fd >= 0) fsync(fd); }
static void unit_fileClose(tl_jcall *c)
{
    int id = c->args[0].i, fd = fd_of(id);
    c->ret.j = 0;
    pthread_mutex_lock(&f_lock);
    F[id].fd = 0; F[id].eof = false;
    pthread_mutex_unlock(&f_lock);
    if (fd >= 0) close(fd);
}

/* ------------------------------------------------------- what it installs */

/*
 * Each call arrives with its arguments in its own signature's order, so the access type the Kotlin forms dropped is simply not
 * there. dirExists, makeDir, remove, rename and fileExists kept theirs; fileLastModified answers in seconds there.
 */
/* The classes the engine may hold for these two handlers. Godot 4.7 asks directory_access_handler for dir* and file_access_handler
 * for file*, but the log of #140 shows the calls landing on GodotNetUtils and on DirectoryAccessHandler instead, so each object it
 * could be holding gets the answers too; the first line each call logs says which one its lookup found. */
static const char *const k_da_classes[] = {
    DA, "org/godotengine/godot/utils/GodotNetUtils", "org/godotengine/godot/Godot",
    "org/godotengine/godot/nativeapi/GodotNativeBridge", "android/content/res/AssetManager",
};
static const char *const k_fa_classes[] = {
    FA, DA, "org/godotengine/godot/utils/GodotNetUtils",
};

#define MAXK (48 + 6 * 6)
void tl_godot_kotlin_install(void)
{
    static tl_jhle k[MAXK];
    static tl_jhle kf[MAXK];
    static const struct { const char *c, *n, *s; tl_jhle_fn f; } base[] = {
        { FA, "fileOpen", "(Ljava/lang/String;I)I", kotlin_fileOpen },
        { FA, "fileRead", "(ILjava/nio/ByteBuffer;)I", kotlin_fileRead },
        { FA, "fileWrite", "(ILjava/nio/ByteBuffer;)Z", kotlin_fileWrite },
        { FA, "fileWrite", "(ILjava/nio/ByteBuffer;)V", void_fileWrite },
        { FA, "fileResize", "(IJ)V", kotlin_fileResize },
        { FA, "fileResize", "(IJ)I", int_fileResize },
        { FA, "fileSeek", "(IJ)V", unit_fileSeek },
        { FA, "fileSeekFromEnd", "(IJ)V", unit_fileSeekFromEnd },
        { FA, "fileFlush", "(I)V", unit_fileFlush },
        { FA, "fileClose", "(I)V", unit_fileClose },
        { FA, "fileGetPosition", "(I)J", name_fileGetPosition },
        { FA, "fileGetSize", "(I)J", name_fileGetSize },
        { FA, "isFileEof", "(I)Z", name_isFileEof },
        { FA, "setFileEof", "(IZ)V", name_setFileEof },
        { FA, "fileExists", "(Ljava/lang/String;)Z", name_fileExists },
        { FA, "fileSize", "(Ljava/lang/String;)J", name_fileSize },
        { FA, "fileLastModified", "(Ljava/lang/String;)J", name_fileLastModified },
        { FA, "fileLastAccessed", "(Ljava/lang/String;)J", name_fileLastAccessed },
        { DA, "dirOpen", "(I)I", kotlin_dirOpen },
        { DA, "dirOpen", "(ILjava/lang/String;)I", kotlin_dirOpen },
        { DA, "dirNext", "(I)Ljava/lang/String;", kotlin_dirNext },
        { DA, "dirClose", "(I)V", kotlin_dirClose },
        { DA, "dirIsDir", "(I)Z", kotlin_dirIsDir },
        { DA, "isCurrentHidden", "(I)Z", kotlin_isCurrentHidden },
        { DA, "dirExists", "(ILjava/lang/String;)Z", dirExists_create },
        { DA, "makeDir", "(ILjava/lang/String;)Z", makeDir_create },
        { DA, "remove", "(ILjava/lang/String;)Z", remove_path },
        { DA, "rename", "(ILjava/lang/String;Ljava/lang/String;)Z", rename_path },
    };
    size_t n = 0, nf = 0;
    for (size_t i = 0; i < sizeof(base) / sizeof(base[0]) && n + 1 < MAXK; i++) {
        for (size_t c = 0; c < sizeof(k_da_classes) / sizeof(k_da_classes[0]) && n + 1 < MAXK; c++) {
            if (strcmp(base[i].c, DA)) continue;
            k[n] = (tl_jhle){ k_da_classes[c], base[i].n, base[i].s, base[i].f };
            n++;
        }
        for (size_t c = 0; c < sizeof(k_fa_classes) / sizeof(k_fa_classes[0]) && nf + 1 < MAXK; c++) {
            if (strcmp(base[i].c, FA)) continue;
            kf[nf] = (tl_jhle){ k_fa_classes[c], base[i].n, base[i].s, base[i].f };
            nf++;
        }
    }
    k[n] = (tl_jhle){ NULL, NULL, NULL, NULL };
    kf[nf] = (tl_jhle){ NULL, NULL, NULL, NULL };
    tl_jni_register_hle(k);
    tl_jni_register_hle(kf);
}
