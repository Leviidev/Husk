/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Java's way of reading an APK's assets, implemented in C: AssetManager.openFd and open, the InputStream and ReadableByteChannel it returns, AssetFileDescriptor.
 *
 * Older SDL 2 (before it learned to ask the NDK for an AAssetManager) reads every file it opens with SDL_RWFromFile through these: openFd for an asset stored
 * uncompressed (it then reads the file descriptor itself, at the asset's offset in the APK), or failing that a stream wrapped in a channel. The NDK side of assets is
 * in husk-tl-bionic-ndk.c; this is the same data seen from Java.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"

#define AM "android/content/res/AssetManager"
#define AFD "android/content/res/AssetFileDescriptor"
#define CHANNEL "husk/AssetChannel"

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vj(int64_t j) { jvalue v; v.j = j; return v; }
static const char *Str(const jobj *o) { const char *s = tl_jni_string(o); return s ? s : ""; }

jobj *tl_hle_assets(void);

/* The layer's own InputStream over an asset (husk-tl-jni-hle.c's `stream`): the channel reads from the same state. */
typedef struct { const uint8_t *data; size_t len, pos; uint8_t *owned; } stream;

/* The asset's place in the APK, if it is stored there uncompressed (so a file descriptor can read it in place). */
static bool stored_asset(const char *name, int *fd, int64_t *off, int64_t *len)
{
    char path[600]; snprintf(path, sizeof(path), "assets/%s", name);
    for (int i = 0; ; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) return false;
        const tl_zip_entry *e = tl_zip_find(z, path);
        if (!e) continue;
        if (e->method != 0 || e->local_offset + 30 >= z->size) return false;
        const uint8_t *h = z->map + e->local_offset;
        uint16_t nlen, elen; memcpy(&nlen, h + 26, 2); memcpy(&elen, h + 28, 2);
        *off = (int64_t)(e->local_offset + 30 + nlen + elen); *len = (int64_t)e->usize;
        *fd = dup(z->fd);
        return *fd >= 0;
    }
}

/* AssetManager.openFd(name): a descriptor for a stored asset, FileNotFoundException for one that is compressed or absent (which is what Android says). */
static void AM_openFd(tl_jcall *c)
{
    const char *name = Str(c->args[0].l);
    int fd; int64_t off, len;
    if (!stored_asset(name, &fd, &off, &len)) { tl_jni_throw("java/io/FileNotFoundException", name); c->ret = vl(NULL); return; }
    jobj *a = tl_jni_new_object(tl_jni_class(AFD));
    tl_jni_set_field(a, "fd", "I", vi(fd)); tl_jni_set_field(a, "off", "J", vj(off)); tl_jni_set_field(a, "len", "J", vj(len));
    c->ret = vl(a);
}
static void AFD_getFileDescriptor(tl_jcall *c)
{
    jobj *f = tl_jni_new_object(tl_jni_class("java/io/FileDescriptor"));
    tl_jni_set_field(f, "descriptor", "I", tl_jni_get_field(c->self, "fd", "I"));
    c->ret = vl(f);
}
static void AFD_getStartOffset(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "off", "J"); }
static void AFD_getLength(tl_jcall *c) { c->ret = tl_jni_get_field(c->self, "len", "J"); }
static void AFD_close(tl_jcall *c) { int fd = tl_jni_get_field(c->self, "fd", "I").i; if (fd >= 0) close(fd); tl_jni_set_field(c->self, "fd", "I", vi(-1)); }

/* AssetManager.list(path): the names directly inside an asset directory (files and folders), from every APK of the app. */
jobj *tl_assetstream_list(const char *path)
{
    char dir[400]; snprintf(dir, sizeof(dir), "%s", path ? path : "");
    size_t dl = strlen(dir); while (dl && dir[dl - 1] == '/') dir[--dl] = 0;
    char prefix[420]; snprintf(prefix, sizeof(prefix), dl ? "assets/%s/" : "assets/", dir);
    size_t pl = strlen(prefix);
    enum { MAXN = 4096 }; char **names = calloc(MAXN, sizeof(char *)); int n = 0;
    for (int i = 0; ; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) break;
        for (size_t k = 0; k < z->count && n < MAXN; k++) {
            const char *name = z->entries[k].name;
            if (strncmp(name, prefix, pl) || !name[pl]) continue;
            size_t len = strcspn(name + pl, "/");
            if (!len) continue;
            bool seen = false;
            for (int j = 0; j < n && !seen; j++) seen = strlen(names[j]) == len && !strncmp(names[j], name + pl, len);
            if (!seen) names[n++] = strndup(name + pl, len);
        }
    }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    for (int j = 0; j < n; j++) { arr->oarr.v[j] = tl_jni_new_string(names[j]); free(names[j]); }
    free(names);
    return arr;
}
static void AM_list(tl_jcall *c) { c->ret = vl(tl_assetstream_list(Str(c->args[0].l))); }

/* Every asset as LOVE's file system wants the list: "d<dir>" for each directory (named with and without its closing slash) and "f<file>" for each file. */
jobj *tl_assetstream_file_tree(void)
{
    enum { MAXN = 20000 }; char **names = calloc(MAXN, sizeof(char *)); int n = 0;
    names[n++] = strdup("d");                                  /* the root */
    for (int i = 0; ; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) break;
        for (size_t k = 0; k < z->count && n < MAXN - 64; k++) {
            const char *name = z->entries[k].name;
            if (strncmp(name, "assets/", 7) || !name[7]) continue;
            const char *p = name + 7;
            size_t len = strlen(p);
            if (p[len - 1] == '/') continue;                    /* directory entries come from their files */
            for (const char *s = strchr(p, '/'); s && n < MAXN - 4; s = strchr(s + 1, '/')) {      /* each directory above the file, once */
                char d[600]; snprintf(d, sizeof(d), "d%.*s", (int)(s - p), p);
                bool seen = false;
                for (int j = 0; j < n && !seen; j++) seen = !strcmp(names[j], d);
                if (!seen) { names[n++] = strdup(d); char d2[605]; snprintf(d2, sizeof(d2), "%s/", d); names[n++] = strdup(d2); }
            }
            char f[610]; snprintf(f, sizeof(f), "f%s", p);
            names[n++] = strdup(f);
        }
    }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    for (int j = 0; j < n; j++) { arr->oarr.v[j] = tl_jni_new_string(names[j]); free(names[j]); }
    free(names);
    return arr;
}

static void SDL_noExpansion(tl_jcall *c) { c->ret = vl(NULL); }                    /* SDLActivity.openAPKExpansionInputStream: no expansion file */

/* Channels.newChannel(stream): the same asset, read through the channel's read(ByteBuffer). */
static void CH_newChannel(tl_jcall *c)
{
    jobj *s = c->args[0].l;
    if (!s || !s->native) { c->ret = vl(NULL); return; }
    jobj *ch = tl_jni_new_object(tl_jni_class(CHANNEL));
    ch->native = s->native;
    c->ret = vl(ch);
}
static void CH_read(tl_jcall *c)                                                    /* read(ByteBuffer): -1 at the end */
{
    stream *st = c->self ? c->self->native : NULL;
    jobj *bb = c->args[0].l;
    void *addr = bb ? tl_jni_get_field(bb, "address", "J").l : NULL;
    int64_t cap = bb ? tl_jni_get_field(bb, "capacity", "J").j : 0;
    if (!st || !addr || cap <= 0 || st->pos >= st->len) { c->ret = vi(-1); return; }
    size_t n = (size_t)cap < st->len - st->pos ? (size_t)cap : st->len - st->pos;
    memcpy(addr, st->data + st->pos, n); st->pos += n;
    c->ret = vi((int)n);
}
static void CH_close(tl_jcall *c) { (void)c; }

#define M_(c, n, s, f) { c, n, s, f }
static const tl_jhle k_hle[] = {
    M_(AM, "openFd", "(Ljava/lang/String;)L" AFD ";", AM_openFd),
    M_(AM, "list", "(Ljava/lang/String;)[Ljava/lang/String;", AM_list),
    M_(AFD, "getFileDescriptor", "()Ljava/io/FileDescriptor;", AFD_getFileDescriptor),
    M_(AFD, "getStartOffset", "()J", AFD_getStartOffset),
    M_(AFD, "getLength", "()J", AFD_getLength),
    M_(AFD, "getDeclaredLength", "()J", AFD_getLength),
    M_(AFD, "close", "()V", AFD_close),
    M_("org/libsdl/app/SDLActivity", "openAPKExpansionInputStream", "(Ljava/lang/String;)Ljava/io/InputStream;", SDL_noExpansion),
    M_("java/nio/channels/Channels", "newChannel", "(Ljava/io/InputStream;)Ljava/nio/channels/ReadableByteChannel;", CH_newChannel),
    M_(CHANNEL, "read", "(Ljava/nio/ByteBuffer;)I", CH_read),
    M_(CHANNEL, "close", "()V", CH_close),
    { NULL, NULL, NULL, NULL }
};

void tl_assetstream_install(void)
{
    tl_jni_declare(AFD, "java/lang/Object");
    tl_jni_declare("java/io/FileDescriptor", "java/lang/Object");
    tl_jni_declare("java/io/InputStream", "java/lang/Object");
    tl_jni_declare(CHANNEL, "java/lang/Object");
    tl_jni_declare("java/nio/channels/ReadableByteChannel", "java/lang/Object");
    tl_jni_register_hle(k_hle);
}
