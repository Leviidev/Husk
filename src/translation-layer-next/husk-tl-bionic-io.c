/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Files, directories, memory mapping, time, signals, polling, sockets.
 *
 * Everything here crosses a boundary where Linux and Darwin disagree about a number or
 * a layout: open flags, the shape of struct stat and struct dirent, mmap flags, clock
 * ids, signal numbers, errno values. Each wrapper translates in and out and publishes
 * errno in the guest's own numbering.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-bionic.h"
const char *tl_ld_main_apk(void);
#include "husk-tl-internal.h"
#include "husk-tl-codewrite.h"
#include "husk-tl-xmem.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <execinfo.h>
#include <TargetConditionals.h>
#include <stdbool.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <xlocale.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <mach/mach.h>
#include <time.h>
#include <search.h>
#include <dlfcn.h>
#include <libkern/OSCacheControl.h>
#include <unistd.h>
#include <utime.h>

/* ------------------------------------------------------------ path mapping */

/*
 * Android paths the guest expects, mapped to somewhere that exists. The data
 * directory is wherever the host set aside for this app; /proc files that apps read
 * to learn about the device are synthesised into unlinked temporary files.
 */
static char g_data_dir[512];
/* New files a game never finished, because Husk was ended while it wrote them (tl_atomic_open): the old files beside them are whole. */
static void drop_unfinished(const char *dir, int depth)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (e->d_name[0] == '.' && (!e->d_name[1] || (e->d_name[1] == '.' && !e->d_name[2]))) continue;
        char p[1100];
        snprintf(p, sizeof(p), "%s/%s", dir, e->d_name);
        size_t n = strlen(e->d_name);
        if (e->d_type == DT_DIR) { if (depth < 12) drop_unfinished(p, depth + 1); }
        else if (n > 9 && !strcmp(e->d_name + n - 9, ".husk-new") && unlink(p) == 0) tl_log_line("file: dropped %s, a save never finished", p);
    }
    closedir(d);
}
void tl_set_data_dir(const char *dir)
{
    snprintf(g_data_dir, sizeof(g_data_dir), "%s", dir ? dir : "");
    if (g_data_dir[0]) drop_unfinished(g_data_dir, 0);
}
const char *tl_data_dir(void) { return g_data_dir[0] ? g_data_dir : "/tmp"; }

/* A file holding `content`, already unlinked. Where it can be made depends on the host: iOS gives an app no /tmp. */
static int synth_file_n(const void *content, size_t n);
static int synth_file(const char *content) { return synth_file_n(content, strlen(content)); }
static int synth_file_n(const void *content, size_t n)
{
    const char *dirs[3] = { g_data_dir[0] ? g_data_dir : NULL, getenv("TMPDIR"), "/tmp" };
    char tmpl[1100];
    int fd = -1;
    for (int i = 0; i < 3 && fd < 0; i++) {
        if (!dirs[i]) continue;
        snprintf(tmpl, sizeof(tmpl), "%s/husk-synth-XXXXXX", dirs[i]);
        fd = mkstemp(tmpl);
    }
    if (fd < 0) { tl_log_line("bionic: no writable directory for a synthetic file (%s)", strerror(errno)); return -1; }
    unlink(tmpl);
    if (write(fd, content, n) != (ssize_t)n) { close(fd); return -1; }
    lseek(fd, 0, SEEK_SET);
    return fd;
}

static const char *synth_content(const char *path, char *buf, size_t n)
{
    if (!strcmp(path, "/proc/cpuinfo")) {
        buf[0] = 0;
        long ncpu = getenv("TL_NCPU") ? atol(getenv("TL_NCPU")) : sysconf(_SC_NPROCESSORS_ONLN);
        for (long i = 0; i < ncpu; i++) {
            size_t l = strlen(buf);
            snprintf(buf + l, n - l, "processor\t: %ld\nBogoMIPS\t: 48.00\nFeatures\t: fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm lrcpc dcpop asimddp\n"
                     "CPU implementer\t: 0x61\nCPU architecture: 8\nCPU variant\t: 0x0\nCPU part\t: 0x0\nCPU revision\t: 0\n\n", i);
        }
        return buf;
    }
    if (!strcmp(path, "/proc/meminfo")) {
        uint64_t mem_bytes = 0; size_t mem_len = sizeof(mem_bytes);
        if (sysctlbyname("hw.memsize", &mem_bytes, &mem_len, NULL, 0) != 0 || !mem_bytes)
            mem_bytes = (uint64_t)4 << 30;
        uint64_t total_kb = mem_bytes / 1024;
        char note[96]; snprintf(note, sizeof(note), "guest /proc/meminfo reports %llu kB of RAM", (unsigned long long)total_kb);
        tl_note_once(note);
        snprintf(buf, n, "MemTotal:       %llu kB\nMemFree:        %llu kB\nMemAvailable:   %llu kB\nBuffers:           10000 kB\nCached:          %llu kB\nSwapTotal:             0 kB\nSwapFree:              0 kB\n",
                 (unsigned long long)total_kb, (unsigned long long)(total_kb / 4),
                 (unsigned long long)(total_kb / 2), (unsigned long long)(total_kb / 6));
        return buf;
    }
    if (!strncmp(path, "/sys/devices/system/cpu/", 24) && (strstr(path, "/present") || strstr(path, "/possible") || strstr(path, "/online"))) {
        snprintf(buf, n, "0-%ld\n", (getenv("TL_NCPU") ? atol(getenv("TL_NCPU")) : sysconf(_SC_NPROCESSORS_ONLN)) - 1);
        return buf;
    }
    if (!strcmp(path, "/proc/self/maps") || (!strncmp(path, "/proc/", 6) && strstr(path, "/maps") && !strcmp(strstr(path, "/maps"), "/maps"))) {
        size_t tl_ld_maps(char *buf, size_t n, const char *libdir);
        char dir[1024]; snprintf(dir, sizeof(dir), "%s/lib", tl_data_dir());
        tl_ld_maps(buf, n, dir);
        return buf;
    }
    if (!strcmp(path, "/proc/self/status")) {
        snprintf(buf, n, "Name:\tapp_process64\nState:\tR (running)\nTgid:\t%d\nPid:\t%d\nPPid:\t1\nUid:\t10001\t10001\t10001\t10001\nThreads:\t8\nVmRSS:\t  300000 kB\n", getpid(), getpid());
        return buf;
    }
    return NULL;
}

/* A file for a path whose contents are made up (/proc/cpuinfo ...), or -1 when the path is real. */
int tl_synth_open(const char *path)
{
    /* the auxiliary vector, as the kernel gives it (Go's runtime in a library reads its page size here): the values getauxval answers */
    if (!strcmp(path, "/proc/self/auxv") || (!strncmp(path, "/proc/", 6) && strstr(path, "/auxv") && !strcmp(strstr(path, "/auxv"), "/auxv"))) {
        unsigned long tl_bionic_getauxval(unsigned long type);
        static uint8_t random16[16]; static int made; if (!made) { arc4random_buf(random16, 16); made = 1; }
        uint64_t v[] = { 6, 16384, 16, tl_bionic_getauxval(16), 26, tl_bionic_getauxval(26), 17, 100, 25, (uint64_t)(uintptr_t)random16,
                         23, 0, 51, 4096, 0, 0 };
        return synth_file_n(v, sizeof(v));
    }
    static __thread char content[65536];
    const char *s = synth_content(path, content, sizeof(content));
    return s ? synth_file(s) : -1;
}

static char g_shared_storage[1024];
void tl_set_shared_storage(const char *dir) { snprintf(g_shared_storage, sizeof(g_shared_storage), "%s", dir ? dir : ""); }

/* The trusted root certificates (one PEM file per certificate, named by subject hash), where Android keeps them: TLS libraries that
 * read them from there (Dart's, in Flutter apps) find the copy the app carries. */
static char g_cacerts[1024];
void tl_set_cacerts_dir(const char *dir) { snprintf(g_cacerts, sizeof(g_cacerts), "%s", dir ? dir : ""); }

/* The system's fonts (/system/fonts) and their list (/system/etc/fonts.xml, font_fallback.xml), which text engines read to find a
 * typeface (Skia, in Flutter apps): a copy the app carries, laid out as fonts/ and etc/ under one directory. */
static char g_sysfonts[1024];
void tl_set_system_fonts_dir(const char *dir) { snprintf(g_sysfonts, sizeof(g_sysfonts), "%s", dir ? dir : ""); }
/* Fonts the app downloaded later (the Chinese, Japanese and Korean ones the bundle leaves out, #95): a /system/fonts file the bundle
 * does not have is looked for here too, and fonts.xml already names them. */
static char g_extrafonts[1024];
void tl_set_extra_fonts_dir(const char *dir) { snprintf(g_extrafonts, sizeof(g_extrafonts), "%s", dir ? dir : ""); }

const char *tl_path_resolve(const char *path, char *buf, size_t n)
{
    if (!path) return path;
    if (g_sysfonts[0]) {
        if (!strncmp(path, "/system/fonts", 13) && (path[13] == 0 || path[13] == '/')) {
            snprintf(buf, n, "%s/fonts%s", g_sysfonts, path + 13);
            if (g_extrafonts[0] && path[13] == '/' && access(buf, F_OK) != 0) {
                char extra[1100];
                snprintf(extra, sizeof(extra), "%s%s", g_extrafonts, path + 13);
                if (access(extra, F_OK) == 0) snprintf(buf, n, "%s", extra);
            }
            return buf;
        }
        if (!strcmp(path, "/system/etc/fonts.xml") || !strcmp(path, "/system/etc/font_fallback.xml")) { snprintf(buf, n, "%s/etc%s", g_sysfonts, path + 11); return buf; }
    }
    if (g_cacerts[0]) {
        static const char *const roots[] = { "/system/etc/security/cacerts", "/apex/com.android.conscrypt/cacerts" };
        for (int i = 0; i < 2; i++) {
            size_t l = strlen(roots[i]);
            if (!strncmp(path, roots[i], l) && (path[l] == 0 || path[l] == '/')) { snprintf(buf, n, "%s%s", g_cacerts, path + l); return buf; }
        }
    }
    /* /system/lib64/libdl.so and the like: the libraries Husk provides itself are files there, as on a phone, so loaders that look
     * for a dependency on disk before loading it (Facebook's SoLoader, in React Native apps) find them. Each is a small ELF stub;
     * loading it by that path loads Husk's own. */
    {
        static const char *const libdirs[] = { "/system/lib64/", "/System/lib64/", "/vendor/lib64/", "/system/lib/" };
        for (int i = 0; i < 4; i++) {
            size_t l = strlen(libdirs[i]);
            bool tl_bionic_is_system_lib(const char *soname);
            if (!strncmp(path, libdirs[i], l) && !strchr(path + l, '/') && tl_bionic_is_system_lib(path + l)) {
                snprintf(buf, n, "%s/.husk-system-lib2.so", tl_data_dir());
                struct stat st;
                if (stat(buf, &st) != 0) {
                    /* a minimal arm64 shared object: a header, a load segment and a dynamic section (SONAME only), which ELF
                     * readers such as SoLoader's MinElf parse to list a library's dependencies (none) */
                    uint8_t elf[0x110]; memset(elf, 0, sizeof(elf));
                    #define W16(o, v) do { elf[o] = (uint8_t)(v); elf[(o) + 1] = (uint8_t)((v) >> 8); } while (0)
                    #define W32(o, v) do { W16(o, (v) & 0xffff); W16((o) + 2, ((uint32_t)(v)) >> 16); } while (0)
                    #define W64(o, v) do { W32(o, (uint32_t)(v)); W32((o) + 4, (uint32_t)((uint64_t)(v) >> 32)); } while (0)
                    static const uint8_t ident[16] = { 0x7f, 'E', 'L', 'F', 2, 1, 1, 0 };
                    memcpy(elf, ident, 16);
                    W16(16, 3); W16(18, 0xb7); W32(20, 1); W64(32, 64); W16(52, 64); W16(54, 56); W16(56, 2); W16(58, 64);
                    W32(64, 1); W32(68, 4); W64(72, 0); W64(80, 0); W64(88, 0); W64(96, sizeof(elf)); W64(104, sizeof(elf)); W64(112, 0x1000);
                    W32(120, 2); W32(124, 6); W64(128, 0xb0); W64(136, 0xb0); W64(144, 0xb0); W64(152, 64); W64(160, 64); W64(168, 8);
                    W64(0xb0, 5); W64(0xb8, 0xf0);                  /* DT_STRTAB */
                    W64(0xc0, 10); W64(0xc8, 0x20);                 /* DT_STRSZ */
                    W64(0xd0, 14); W64(0xd8, 1);                    /* DT_SONAME */
                    memcpy(elf + 0xf1, "libhusk-system.so", 17);
                    #undef W64
                    #undef W32
                    #undef W16
                    FILE *f = fopen(buf, "wb");
                    if (f) { fwrite(elf, 1, sizeof(elf), f); fclose(f); }
                }
                return buf;
            }
        }
    }
    /* /data/user/0/<package> and /data/data/<package>: the app's own folder, as Android names it -- short, where the real one on a
       phone is ~150 characters (p4a's launcher allows its script path 128) */
    {
        static const char *const roots[] = { "/data/user/0/", "/data/data/" };
        for (int i = 0; i < 2; i++) {
            size_t l = strlen(roots[i]);
            if (strncmp(path, roots[i], l) || !path[l] || path[l] == '/') continue;
            const char *rest = strchr(path + l, '/');
            snprintf(buf, n, "%s%s", tl_data_dir(), rest ? rest : "");
            return buf;
        }
    }
    /* /data/local/tmp: the world-writable scratch folder (JNA unpacks itself there): a folder of the app's own */
    if (!strncmp(path, "/data/local/tmp", 15) && (path[15] == 0 || path[15] == '/')) {
        snprintf(buf, n, "%s/cache/local-tmp", tl_data_dir());
        mkdir(buf, 0777);
        snprintf(buf, n, "%s/cache/local-tmp%s", tl_data_dir(), path + 15);
        return buf;
    }
    const char *pkg = "/data/data/";
    if (!strncmp(path, pkg, strlen(pkg))) {
        const char *rest = strchr(path + strlen(pkg), '/');
        snprintf(buf, n, "%s%s", tl_data_dir(), rest ? rest : "");
        return buf;
    }
    if (!strncmp(path, "/data/user/0/", 13)) {
        const char *rest = strchr(path + 13, '/');
        snprintf(buf, n, "%s%s", tl_data_dir(), rest ? rest : "");
        return buf;
    }
    if (!strncmp(path, "/sdcard", 7) || !strncmp(path, "/storage/emulated/0", 19)) {
        const char *rest = !strncmp(path, "/sdcard", 7) ? path + 7 : path + 19;
        /* Shared storage (anything but an app's own Android/data and Android/obb) is one folder for every game when the app gave one,
         * which the player can fill from Files: a game whose data lives in a folder of its own on /sdcard finds it there. What a game
         * already keeps in its private copy stays found. */
        if (g_shared_storage[0] && strncmp(rest, "/Android/data", 13) && strncmp(rest, "/Android/obb", 12)) {
            char own[1024]; struct stat st;
            snprintf(own, sizeof(own), "%s/sdcard%s", tl_data_dir(), rest);
            if (rest[0] && rest[1] && stat(own, &st) == 0) { snprintf(buf, n, "%s", own); return buf; }
            snprintf(buf, n, "%s%s", g_shared_storage, rest);
            return buf;
        }
        snprintf(buf, n, "%s/sdcard%s", tl_data_dir(), rest);
        /* An expansion file (Android/obb/<package>/main.<version>.<package>.obb) not in the app's data: the one Husk's store
         * downloaded with the app, kept beside its APKs in obb/ */
        if (!strncmp(rest, "/Android/obb/", 13)) {
            struct stat st;
            const char *apk = tl_ld_main_apk(), *file = strrchr(rest, '/');
            if (stat(buf, &st) != 0 && apk && file && file[1]) {
                char beside[1024];
                const char *slash = strrchr(apk, '/');
                snprintf(beside, sizeof(beside), "%.*s/obb%s", slash ? (int)(slash - apk) : 0, apk, file);
                if (stat(beside, &st) == 0) snprintf(buf, n, "%s", beside);
            }
        }
        return buf;
    }
    return path;
}


/* ------------------------------------------------------------ virtual files */

/*
 * A file that is really a stretch of another: Unreal Engine games keep their data in an OBB, which a repackaged APK may carry inside itself (ARK's is 2 GB
 * stored in the APK). It is shown to the game under its usual name, and reads go to the right place in the APK, so nothing is unpacked. Matching is by file name.
 */
typedef struct { char name[160]; char host[1024]; uint64_t off, size; } vfile;
typedef struct { bool on; uint64_t off, size, pos; } vfd;
static vfile g_vfiles[8];
static int g_nvfiles;
static vfd g_vfd[4096];

void tl_vfile_add(const char *guest_name, const char *host_path, uint64_t off, uint64_t size)
{
    if 