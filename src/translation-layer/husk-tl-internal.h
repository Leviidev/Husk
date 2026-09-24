/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Shared between the translation layer's own files. Not part of the bridge:
 * Swift sees husk-tl.h only.
 */
#ifndef HUSK_TL_INTERNAL_H
#define HUSK_TL_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------ JSON */

/*
 * Just enough of a JSON writer for the reports. Commas are placed by the
 * writer, not the caller, which is where hand-built JSON usually goes wrong.
 * Any allocation failure is sticky and turns the result into an error object.
 */
typedef struct tl_json {
    char  *buf;
    size_t len, cap;
    bool   failed;
    int    depth;
    bool   after_key;
    bool   need_comma[32];
} tl_json;

void  tl_json_init(tl_json *j);
void  tl_json_begin_object(tl_json *j);
void  tl_json_end_object(tl_json *j);
void  tl_json_begin_array(tl_json *j);
void  tl_json_end_array(tl_json *j);
void  tl_json_key(tl_json *j, const char *key);
void  tl_json_string(tl_json *j, const char *s);   /* NULL writes null */
void  tl_json_int(tl_json *j, long long v);
void  tl_json_bool(tl_json *j, bool v);
/* Hands over the buffer; NULL only if not even an error object fits in memory. */
char *tl_json_finish(tl_json *j);

/* ------------------------------------------------------------------- ZIP */

typedef struct tl_zip_entry {
    char    *name;
    uint16_t method;        /* 0 stored, 8 deflated */
    uint16_t flags;
    uint64_t csize;
    uint64_t usize;
    uint64_t local_offset;
} tl_zip_entry;

typedef struct tl_zip {
    int            fd;
    const uint8_t *map;
    size_t         size;
    tl_zip_entry  *entries;
    size_t         count;
} tl_zip;

/* Reads the central directory. The file stays mapped until tl_zip_close(). */
bool tl_zip_open(tl_zip *z, const char *path, char *err, size_t errlen);
void tl_zip_close(tl_zip *z);
const tl_zip_entry *tl_zip_find(const tl_zip *z, const char *name);

/*
 * An entry's bytes. A stored entry points straight into the mapping and
 * `*owned` is false; a deflated one is inflated into a buffer the caller must
 * free(), and `*owned` is true. Entries larger than `limit` are refused.
 */
bool tl_zip_data(const tl_zip *z, const tl_zip_entry *e, size_t limit,
                 const uint8_t **out, size_t *out_len, bool *owned,
                 char *err, size_t errlen);

/* ------------------------------------------------------------------- ELF */

/*
 * How one page of a library image has to be mapped. A page can hold parts of
 * several segments -- with 4 KiB alignment, a 16 KiB page often does -- so
 * these combine.
 */
enum {
    TL_PAGE_R = 1,          /* something is loaded here */
    TL_PAGE_X = 2,          /* code: must execute */
    TL_PAGE_W = 4,          /* data written after relocation: must be writable */
};

typedef struct tl_segment {
    uint64_t vaddr;
    uint64_t memsz;
    uint32_t flags;         /* PF_R / PF_W / PF_X */
} tl_segment;

/*
 * Lay the loadable segments out on pages of `page` bytes. Writable bytes that
 * PT_GNU_RELRO covers do not count as writable: they are only written while
 * relocating, which the loader does through its own writable alias.
 *
 * Returns the number of pages and fills `flags` (up to `max` entries) and
 * `*base`, the first page's address; 0 means the layout is unusable. With
 * `flags` NULL it only counts, so the caller can size the array.
 * This is the loader's plan as much as the scanner's -- the scanner reports it
 * so that an app's fate is known before anything is loaded.
 */
size_t tl_page_plan(const tl_segment *loads, size_t nloads,
                    const tl_segment *relro, uint64_t page,
                    uint8_t *flags, size_t max, uint64_t *base);

#define TL_MAX_NEEDED 48

typedef struct tl_elf_report {
    bool     ok;
    char     error[160];
    bool     is64;
    uint16_t machine;

    /* layout */
    uint64_t min_align, max_align;
    uint64_t span;              /* bytes, whole 16 KiB pages */
    uint32_t pages, pages_exec, pages_write, pages_conflict;
    char     layout[96];        /* run-length summary, e.g. "R1 X4 R1 W2" */

    /* code */
    uint32_t svc;               /* direct system calls */
    uint32_t tpidr_reads;       /* mrs Xn, tpidr_el0 */
    uint32_t tpidr_writes;      /* msr tpidr_el0, Xn */

    /* dynamic linking */
    bool     has_tls;
    bool     textrel;
    uint32_t relocs;
    uint32_t relocs_tls;
    uint32_t relocs_unsupported;
    uint32_t first_unsupported;
    const char *packing;        /* "none", "android" (APS2) or "relr" */
    uint32_t imports;
    char     soname[96];
    int      needed_count;
    char     needed[TL_MAX_NEEDED][64];
} tl_elf_report;

void tl_elf_analyze(const uint8_t *data, size_t size, tl_elf_report *r);

#endif /* HUSK_TL_INTERNAL_H */
