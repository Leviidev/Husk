/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Android libraries as Mach-O dylibs, made ahead of time. See husk-tl-aot.h. */
#include "husk-tl-aot.h"
#include "husk-tl-codesign.h"
#include "husk-tl-internal.h"

#include <errno.h>
#include <mach-o/fixup-chains.h>
#include <mach-o/loader.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <TargetConditionals.h>
#include <unistd.h>
#include <zlib.h>

#define PAGE ((uint64_t)16384)
#define ROUND_UP(x) (((x) + PAGE - 1) & ~(PAGE - 1))
#define MANIFEST_AT (PAGE - 256)          /* the manifest's offset in the header page */

typedef struct { uint8_t *b; size_t n, cap; } buf;
static bool put(buf *B, const void *p, size_t n)
{
    if (B->n + n > B->cap) {
        size_t cap = (B->n + n) * 2 + 4096;
        uint8_t *nb = realloc(B->b, cap);
        if (!nb) return false;
        B->b = nb; B->cap = cap;
    }
    memcpy(B->b + B->n, p, n); B->n += n;
    return true;
}
static void pad(buf *B, size_t to) { static const uint8_t z[64]; while (B->n < to) put(B, z, to - B->n < 64 ? to - B->n : 64); }
static bool any_bytes(const uint8_t *p, size_t n) { for (size_t i = 0; i < n; i++) if (p[i]) return true; return false; }

bool tl_aot_make_dylib(const char *name, const uint8_t *file, size_t flen, const char *out_path, bool ios, tl_ld_aot_image *info,
                       char *err, size_t errlen)
{
    tl_ld_aot_image A;
    if (!tl_ld_aot_build(name, file, flen, &A)) { snprintf(err, errlen, "%s", A.err); return false; }
    const uint64_t S = A.size;               /* the image, the stub pages, the slot page and the thread block */
    const uint64_t HDR = PAGE;               /* the header page; the image's pages follow at their own vaddrs */
    const size_t NP = A.page_count;
    bool ok = false;
    buf L = {0}, C = {0};
    uint32_t *starts = NULL;
    FILE *f = NULL;
    char tmp[1100]; snprintf(tmp, sizeof(tmp), "%s.part", out_path);

    /* LiveContainer/ZSign sign ld-style Mach-Os: one r-x __TEXT, then rw- __DATA, then __LINKEDIT. AMFI's execSeg is that __TEXT
     * range; a hole of non-executable pages in it is `code signature invalid` on iOS 27, and a segment named __DATA that is not
     * writable is rejected by dyld. The image keeps its vaddrs (file offset = vaddr) so the loader can still use one pointer.
     * Data pages in the image are r-x like const data in __TEXT; their writable copies (page_class bit 2) live after it. The slot
     * and thread-block pages at the end of the image stay rw- __DATA, past execSeg. */
    const uint64_t text_file = HDR + (uint64_t)(A.npages + A.nstub) * PAGE; /* header + image + stubs; slots follow */
    if (text_file < HDR + PAGE || text_file + 2 * PAGE != HDR + S) {
        snprintf(err, errlen, "the image layout does not split into __TEXT then __DATA"); goto out;
    }
    /* A library built for 4 KiB pages has pages that hold code and writable data both: the writable copies of its data pages follow
     * the image, at the image's size plus the page's index, which is where the rewritten data accesses name them (mixed_page in
     * husk-tl-ld.c). Consecutive copies form one segment, and the copies at the end of a run that are all zeros (.bss) are left out
     * of the file: the kernel zero-fills them. The gaps between the copies are address space only, which is what keeps an address
     * into a copied page plus an offset -- a vtable header plus 0x10 -- on the right copied page. */
    struct { size_t first, last; uint64_t file, bytes; } copy[128]; int ncopy = 0;
    uint64_t copy_file = 0;
    for (size_t p = 0; p < NP && A.page_class && A.text; ) {
        if (!(A.page_class[p] & 2)) { p++; continue; }
        size_t q = p; while (q + 1 < NP && (A.page_class[q + 1] & 2)) q++;
        size_t end = q + 1;
        while (end > p && !any_bytes(A.text + (end - 1) * PAGE, PAGE)) end--;
        if (ncopy == 128) { snprintf(err, errlen, "the library's data pages alternate too often to lay them out"); goto out; }
        copy[ncopy].first = p; copy[ncopy].last = q; copy[ncopy].file = copy_file; copy[ncopy].bytes = (uint64_t)(end - p) * PAGE;
        copy_file += copy[ncopy].bytes; ncopy++;
        p = q + 1;
    }
    const uint64_t dup_vm = HDR + S;                        /* page p's copy: dup_vm + p * PAGE */
    const uint64_t link_vm = ncopy ? dup_vm + (uint64_t)(copy[ncopy - 1].last + 1) * PAGE : dup_vm;
    const uint64_t link_file = HDR + S + copy_file;
    const uint32_t nseg = 3 + (uint32_t)ncopy;     /* __TEXT, __DATA (slots), the copies, __LINKEDIT */
    if (ncopy + 3 > 200) { snprintf(err, errlen, "the library needs too many segments to lay out"); goto out; }

    /* __LINKEDIT: chained-fixups header with no fixups, an empty export trie, an empty string table */
    struct dyld_chained_fixups_header fh = {0};
    fh.starts_offset = sizeof(fh);
    starts = calloc(1u + nseg, sizeof(uint32_t));
    if (!starts) { snprintf(err, errlen, "out of memory"); goto out; }
    starts[0] = nseg;                                /* one entry per segment, all zero: no fixups anywhere */
    fh.imports_offset = fh.starts_offset + (uint32_t)(1 + nseg) * 4;
    const uint32_t imports_off = fh.imports_offset;
    fh.symbols_offset = fh.imports_offset;
    fh.imports_format = DYLD_CHAINED_IMPORT;
    put(&L, &fh, sizeof(fh)); put(&L, starts, (size_t)(1 + nseg) * 4); pad(&L, (L.n + 7) & ~7ul);
    const uint32_t fix_size = (uint32_t)L.n;
    const uint32_t trie_off = (uint32_t)L.n; const uint8_t trie[8] = { 0, 0 }; put(&L, trie, 8);
    const uint32_t str_off = (uint32_t)L.n; const uint8_t strs[8] = { ' ', 0 }; put(&L, strs, 8);
    (void)imports_off;

    /* load commands: one __TEXT covering the header, the image and the stubs (ld-style), then rw- __DATA */
    struct segment_command_64 text = { LC_SEGMENT_64, sizeof(struct segment_command_64) + sizeof(struct section_64), "__TEXT",
        0, text_file, 0, text_file, VM_PROT_READ | VM_PROT_EXECUTE, VM_PROT_READ | VM_PROT_EXECUTE, 1, 0 };
    struct section_64 man = { "__husk_manifest", "__TEXT", MANIFEST_AT, sizeof(tl_aot_manifest), (uint32_t)MANIFEST_AT, 3, 0, 0,
        S_REGULAR, 0, 0, 0 };
    put(&C, &text, sizeof(text)); put(&C, &man, sizeof(man));
    int ndata = 0;
    struct segment_command_64 slots = { LC_SEGMENT_64, sizeof(struct segment_command_64), "__DATA",
        text_file, 2 * PAGE, text_file, 2 * PAGE, VM_PROT_READ | VM_PROT_WRITE, VM_PROT_READ | VM_PROT_WRITE, 0, 0 };
    put(&C, &slots, sizeof(slots));
    ndata = 1;
    for (int c = 0; c < ncopy; c++) {
        struct segment_command_64 sg = { LC_SEGMENT_64, sizeof(struct segment_command_64), "",
            dup_vm + (uint64_t)copy[c].first * PAGE, (uint64_t)(copy[c].last - copy[c].first + 1) * PAGE,
            HDR + S + copy[c].file, copy[c].bytes, 0, 0, 0, 0 };
        sg.maxprot = sg.initprot = VM_PROT_READ | VM_PROT_WRITE;
        snprintf(sg.segname, sizeof(sg.segname), "__DATA_%d", ndata);
        ndata++;
        put(&C, &sg, sizeof(sg));
    }
    struct segment_command_64 link = { LC_SEGMENT_64, sizeof(struct segment_command_64), "__LINKEDIT",
        link_vm, ROUND_UP(L.n), link_file, L.n, VM_PROT_READ, VM_PROT_READ, 0, 0 };
    put(&C, &link, sizeof(link));

    const char *slash = strrchr(out_path, '/');
    char install[300]; snprintf(install, sizeof(install), "@rpath/%s", slash ? slash + 1 : out_path);
    size_t nl = strlen(install) + 1, idsize = (sizeof(struct dylib_command) + nl + 7) & ~7ul;
    struct dylib_command id = { LC_ID_DYLIB, (uint32_t)idsize, { { sizeof(struct dylib_command) }, 2, 0x10000, 0x10000 } };
    size_t at = C.n; put(&C, &id, sizeof(id)); put(&C, install, nl); pad(&C, at + idsize);

    /* dyld refuses a dylib that links nothing: libSystem, as every dylib has */
    static const char libsystem[] = "/usr/lib/libSystem.B.dylib";
    size_t lssize = (sizeof(struct dylib_command) + sizeof(libsystem) + 7) & ~7ul;
    struct dylib_command ls = { LC_LOAD_DYLIB, (uint32_t)lssize, { { sizeof(struct dylib_command) }, 2, 0x05470000, 0x00010000 } };
    at = C.n; put(&C, &ls, sizeof(ls)); put(&C, libsystem, sizeof(libsystem)); pad(&C, at + lssize);

    struct linkedit_data_command fix = { LC_DYLD_CHAINED_FIXUPS, sizeof(struct linkedit_data_command), (uint32_t)link_file, fix_size };
    put(&C, &fix, sizeof(fix));
    struct linkedit_data_command exp = { LC_DYLD_EXPORTS_TRIE, sizeof(struct linkedit_data_command), (uint32_t)link_file + trie_off, 8 };
    put(&C, &exp, sizeof(exp));
    struct symtab_command st = { LC_SYMTAB, sizeof(struct symtab_command), 0, 0, (uint32_t)link_file + str_off, 8 };
    put(&C, &st, sizeof(st));
    struct dysymtab_command dst = { LC_DYSYMTAB, sizeof(struct dysymtab_command) };
    put(&C, &dst, sizeof(dst));
    struct build_version_command bv = { LC_BUILD_VERSION, sizeof(struct build_version_command),
        ios ? PLATFORM_IOS : PLATFORM_MACOS, ios ? 0x000F0000u : 0x000B0000u, 0x001A0000u, (uint32_t)0 };
    put(&C, &bv, sizeof(bv));
    tl_aot_manifest M = { "", TL_AOT_VERSION, (uint32_t)PAGE, tl_aot_hash(file, flen), flen, A.base_vaddr, A.npages, A.nstub, A.size, A.n_mixed };
    memcpy(M.magic, TL_AOT_MAGIC, 8);
    struct uuid_command uu = { LC_UUID, sizeof(struct uuid_command) };
    uint64_t h = M.so_hash ^ ((uint64_t)TL_AOT_VERSION << 56);   /* the same library and loader always give the same UUID */
    memcpy(uu.uuid, &h, 8); h = h * 6364136223846793005ull + S; memcpy(uu.uuid + 8, &h, 8);
    put(&C, &uu, sizeof(uu));

    struct mach_header_64 mh = { MH_MAGIC_64, CPU_TYPE_ARM64, CPU_SUBTYPE_ARM64_ALL, MH_DYLIB, nseg + 8, (uint32_t)C.n,
        MH_NOUNDEFS | MH_DYLDLINK | MH_TWOLEVEL | MH_NO_REEXPORTED_DYLIBS, 0 };
    if (!C.b || !L.b || sizeof(mh) + C.n > MANIFEST_AT) { snprintf(err, errlen, "the load commands do not fit"); goto out; }

    /* the file, streamed: header page (with the manifest), then every page of the image as it was built (patched code, the .so's
     * data, zeros for the slot and thread-block pages), then the mixed pages' copies, then linkedit */
    buf H = {0};
    put(&H, &mh, sizeof(mh)); put(&H, C.b, C.n); pad(&H, MANIFEST_AT); put(&H, &M, sizeof(M)); pad(&H, HDR);
    if (!H.b || H.n != HDR || !A.text) { free(H.b); snprintf(err, errlen, "out of memory"); goto out; }
    if (!(f = fopen(tmp, "wb"))) { free(H.b); snprintf(err, errlen, "%s: %s", tmp, strerror(errno)); goto out; }
    bool w = fwrite(H.b, 1, H.n, f) == H.n
          && fwrite(A.text, 1, (size_t)S, f) == S;
    for (int c = 0; w && c < ncopy; c++)
        if (copy[c].bytes) w = fwrite(A.text + copy[c].first * PAGE, 1, (size_t)copy[c].bytes, f) == copy[c].bytes;
    w = w && fwrite(L.b, 1, L.n, f) == L.n;
    free(H.b);
    if (fclose(f) != 0) w = false;
    f = NULL;
    if (!w) { snprintf(err, errlen, "%s: writing failed (%s)", tmp, strerror(errno)); unlink(tmp); goto out; }
    if (rename(tmp, out_path) != 0) { snprintf(err, errlen, "%s: %s", out_path, strerror(errno)); unlink(tmp); goto out; }
    ok = true;
out:
    if (info) { *info = A; info->text = NULL; info->region = NULL; info->page_class = NULL; }
    tl_ld_aot_free(&A);
    free(starts); free(L.b); free(C.b);
    return ok;
}

/* ------------------------------------------------------------- preparing an app */

void tl_log_line(const char *fmt, ...);

static bool read_text(const char *path, char *buf, size_t n)
{
    FILE *f = fopen(path, "r"); if (!f) return false;
    size_t k = fread(buf, 1, n - 1, f); fclose(f); buf[k] = 0;
    return true;
}
static bool write_text(const char *path, const char *text)
{
    FILE *f = fopen(path, "w"); if (!f) return false;
    bool ok = fputs(text, f) >= 0; return fclose(f) == 0 && ok;
}

/* Some APKs ship a library as gzip data and unpack it at run time (pysolfc's libpybundle.so); convert what the game will load,
 * so the hash in the dylib's name is the hash of the file the game writes out. */
static uint8_t *gunzip(const uint8_t *src, size_t n, size_t *out_len)
{
    z_stream z = {0};
    if (inflateInit2(&z, 15 + 32) != Z_OK) return NULL;      /* gzip or zlib header */
    size_t cap = n * 3 + 65536, have = 0;
    uint8_t *b = malloc(cap);
    z.next_in = (Bytef *)src; z.avail_in = (uInt)n;
    while (b) {
        if (have == cap) {
            size_t nc = cap + cap / 2 + 65536;
            uint8_t *nb = realloc(b, nc);
            if (!nb) { free(b); b = NULL; break; }
            b = nb; cap = nc;
        }
        size_t room = cap - have; if (room > 0x40000000u) room = 0x40000000u;
        z.next_out = b + have; z.avail_out = (uInt)room;
        int r = inflate(&z, Z_NO_FLUSH);
        have += room - z.avail_out;
        if (r == Z_STREAM_END) break;
        if (r != Z_OK) { free(b); b = NULL; break; }
    }
    inflateEnd(&z);
    if (b) *out_len = have;
    return b;
}

static bool is_arm64_elf(const uint8_t *p, size_t n)
{
    return n >= 20 && p[0] == 0x7f && p[1] == 'E' && p[2] == 'L' && p[3] == 'F'
        && p[4] == 2 && p[5] == 1 && p[18] == 0xb7 && p[19] == 0x00;   /* 64-bit, little-endian, EM_AARCH64 */
}

/* Converts one library into the cache, or leaves a non-library alone. 1 = done (or already there), 0 = not a library,
 * -1 = failed (err has the reason). */
static int prepare_one(const char *name, const uint8_t *data, size_t len, const char *cache_dir, const uint8_t *p12,
                       size_t p12_len, const char *password, const char *cert_tag, const char *sign_ident, char *err, size_t errlen)
{
    if (!is_arm64_elf(data, len)) {
        tl_log_line("aot: %s is not an arm64 ELF library, left alone", name);
        return 0;
    }
    char path[1200], signer[1220], tag[256] = "";
    char dylib[160]; tl_aot_file_name(dylib, sizeof(dylib), tl_aot_hash(data, len), name);
    snprintf(path, sizeof(path), "%s/%s", cache_dir, dylib);
    snprintf(signer, sizeof(signer), "%s.signer", path);
    struct stat st;
    bool have = stat(path, &st) == 0, signed_now = have && read_text(signer, tag, sizeof(tag)) && cert_tag && !strcmp(tag, cert_tag);
    char e2[200] = "";
    bool ok = true;
    if (!signed_now) {
        unlink(path);
        unlink(signer);
        tl_log_line("aot: converting %s (%zu KiB)", name, len >> 10);
        ok = tl_aot_make_dylib(name, data, len, path, TARGET_OS_IPHONE, NULL, e2, sizeof(e2));   /* for the platform this runs on */
        if (ok) {
            ok = husk_codesign_dylib(path, p12, p12_len, password, sign_ident, e2, sizeof(e2));
            if (ok && cert_tag) write_text(signer, cert_tag);
            if (ok) tl_log_line("aot: %s signed", name);
        }
    }
    if (!ok) { snprintf(err, errlen, "%s: %s", name, e2); unlink(path); return -1; }
    return 1;
}

/* Python-for-android games ship their extension modules in a gzip'd tar that is named like a library (pysolfc's
 * libpybundle.so); the game writes the tar out and loads the modules, so convert the libraries inside it too. The dylib's name
 * only uses the file name, which is the same at run time. */
static size_t tar_octal(const uint8_t *p, size_t n)
{
    size_t v = 0, i = 0;
    while (i < n && (p[i] == ' ' || p[i] == 0)) i++;
    for (; i < n && p[i] >= '0' && p[i] <= '7'; i++) v = v * 8 + (size_t)(p[i] - '0');
    return v;
}
static int prepare_tar(const uint8_t *t, size_t n, const char *what, const char *cache_dir, const uint8_t *p12, size_t p12_len,
                       const char *password, const char *cert_tag, const char *sign_ident, char *err, size_t errlen)
{
    int done = 0; size_t off = 0; char longname[256] = "";
    while (off + 512 <= n) {
        const uint8_t *h = t + off;
        if (!h[0]) break;                                     /* the zero blocks that end the archive */
        char name[512] = "";
        if (longname[0]) { snprintf(name, sizeof(name), "%s", longname); longname[0] = 0; }
        else {
            size_t pfx = 0; while (pfx < 155 && h[345 + pfx]) pfx++;
            if (pfx) snprintf(name, sizeof(name), "%.*s/%.100s", (int)pfx, (const char *)h + 345, (const char *)h);
            else snprintf(name, sizeof(name), "%.100s", (const char *)h);
        }
        size_t size = tar_octal(h + 124, 12);
        char type = h[156] ? (char)h[156] : '0';
        off += 512;
        if (size > n - off) break;
        if (type == 'L') {                                    /* GNU long name, the next header's */
            snprintf(longname, sizeof(longname), "%.*s", (int)(size < sizeof(longname) - 1 ? size : sizeof(longname) - 1), t + off);
        } else if (type == '0' || type == '7') {
            const char *base = strrchr(name, '/'); base = base ? base + 1 : name;
            size_t bl = strlen(base);
            if (bl > 3 && !strcmp(base + bl - 3, ".so") && is_arm64_elf(t + off, size)) {
                int r = prepare_one(base, t + off, size, cache_dir, p12, p12_len, password, cert_tag, sign_ident, err, errlen);
                if (r < 0) { tl_log_line("aot: %s skipped: %s", base, err); if (errlen) err[0] = 0; }
                else done += r;
            }
        }
        off += (size + 511) & ~(size_t)511;
    }
    tl_log_line("aot: %s is a tar of Android libraries; %d converted from it", what, done);
    return done;
}

int husk_aot_prepare(const char *const *apks, int napks, const char *cache_dir, const uint8_t *p12, size_t p12_len,
                     const char *password, const char *cert_tag, const char *sign_ident, tl_aot_progress_fn progress, void *user,
                     char *err, size_t errlen)
{
    mkdir(cache_dir, 0755);
    tl_zip *z = calloc((size_t)(napks > 0 ? napks : 1), sizeof(tl_zip));
    if (!z) { snprintf(err, errlen, "out of memory"); return -1; }
    /* the libraries, by name, from the first APK that has each */
    typedef struct { int apk; const tl_zip_entry *e; const char *name; } lib;   /* e NULL: apks[apk] is the library file itself */
    lib *libs = NULL; int nlibs = 0, cap = 0, ready = -1;
    for (int a = 0; a < napks; a++) {
        size_t al = strlen(apks[a]);
        if (al > 3 && !strcmp(apks[a] + al - 3, ".so")) {   /* a library file (the Java runtime's, from the app) */
            const char *slash = strrchr(apks[a], '/'), *base = slash ? slash + 1 : apks[a];
            bool seen = false;
            for (int k = 0; k < nlibs && !seen; k++) seen = !strcmp(libs[k].name, base);
            if (seen) continue;
            if (nlibs == cap) { cap = cap ? cap * 2 : 16; lib *nl = realloc(libs, (size_t)cap * sizeof(lib)); if (!nl) { snprintf(err, errlen, "out of memory"); goto out; } libs = nl; }
            libs[nlibs++] = (lib){ a, NULL, base };
            continue;
        }
        char zerr[160] = "";
        if (!tl_zip_open(&z[a], apks[a], zerr, sizeof(zerr))) { tl_log_line("aot: %s: %s", apks[a], zerr); continue; }
        for (size_t i = 0; i < z[a].count; i++) {
            const char *n = z[a].entries[i].name;
            size_t l = strlen(n);
            static const char *const prefixes[] = { "lib/arm64-v8a/", "lib/arm64/" };
            const char *base = NULL;
            for (int p = 0; p < 2 && !base; p++) {
                size_t pl = strlen(prefixes[p]);
                if (l > pl + 3 && !strncmp(n, prefixes[p], pl) && !strcmp(n + l - 3, ".so") && !strchr(n + pl, '/'))
                    base = n + pl;
            }
            if (!base) continue;
            bool seen = false;
            for (int k = 0; k < nlibs && !seen; k++) seen = !strcmp(libs[k].name, base);
            if (seen) continue;
            if (nlibs == cap) { cap = cap ? cap * 2 : 16; lib *nl = realloc(libs, (size_t)cap * sizeof(lib)); if (!nl) { snprintf(err, errlen, "out of memory"); goto out; } libs = nl; }
            libs[nlibs++] = (lib){ a, &z[a].entries[i], base };
        }
    }
    ready = 0;
    for (int k = 0; k < nlibs; k++) {
        if (progress) progress(user, k, nlibs, libs[k].name);
        const uint8_t *data; size_t len; bool owned = false; char zerr[160] = "";
        if (!libs[k].e) {
            FILE *f = fopen(apks[libs[k].apk], "rb");
            uint8_t *b = NULL; long n = -1;
            if (f && fseek(f, 0, SEEK_END) == 0 && (n = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0 && (b = malloc((size_t)n))
                && fread(b, 1, (size_t)n, f) == (size_t)n) { data = b; len = (size_t)n; owned = true; }
            else { free(b); b = NULL; }
            if (f) fclose(f);
            if (!b) { snprintf(err, errlen, "%s: %s", libs[k].name, strerror(errno)); ready = -1; goto out; }
        } else if (!tl_zip_data(&z[libs[k].apk], libs[k].e, (size_t)1 << 31, &data, &len, &owned, zerr, sizeof(zerr))) {
            snprintf(err, errlen, "%s: %s", libs[k].name, zerr); ready = -1; goto out;
        }
        if (len > 2 && data[0] == 0x1f && data[1] == 0x8b) {
            size_t ulen = 0;
            uint8_t *u = gunzip(data, len, &ulen);
            if (!u) { if (owned) free((void *)data); snprintf(err, errlen, "%s: gzip data could not be unpacked", libs[k].name); ready = -1; goto out; }
            tl_log_line("aot: %s is gzip data (%zu KiB), %zu KiB unpacked", libs[k].name, len >> 10, ulen >> 10);
            if (owned) free((void *)data);
            data = u; len = ulen; owned = true;
        }
        int r;
        if (is_arm64_elf(data, len))
            r = prepare_one(libs[k].name, data, len, cache_dir, p12, p12_len, password, cert_tag, sign_ident, err, errlen);
        else if (len > 512 && !memcmp(data + 257, "ustar", 5))
            r = prepare_tar(data, len, libs[k].name, cache_dir, p12, p12_len, password, cert_tag, sign_ident, err, errlen);
        else {
            tl_log_line("aot: %s is not an arm64 ELF library, left alone", libs[k].name);
            r = 0;
        }
        if (owned) free((void *)data);
        if (r < 0) {
            /* one library that will not convert must not stop the rest: JIT-less still runs on those that did */
            tl_log_line("aot: %s skipped: %s", libs[k].name, err);
            if (errlen) err[0] = 0;
            continue;
        }
        ready += r;
    }
    if (progress) progress(user, nlibs, nlibs, NULL);
out:
    for (int a = 0; a < napks; a++) if (z[a].map) tl_zip_close(&z[a]);
    free(z); free(libs);
    return ready;
}
