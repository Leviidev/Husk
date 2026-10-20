/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * The translation layer's loader: an attempt at actually running an app.
 *
 * What happens here, in order:
 *
 *   1. the APK's arm64 libraries are pulled out of the APK (husk-tl-zip);
 *   2. the primary one is parsed and laid out on 16 KiB pages exactly as
 *      the scanner planned it (tl_page_plan) -- the plan was written to be
 *      the loader's, and this is the moment that claim is cashed;
 *   3. executable pages come out of one MAP_JIT region and writable pages
 *      are carved out of it as ordinary memory (the layout husk-tl-probe
 *      measures on the device), so code finds its data PC-relatively
 *      while the data stays writable during execution;
 *   4. it is relocated against the shim in husk-tl-shim.c and against
 *      libraries loaded before it: RELATIVE, ABS64, GLOB_DAT and
 *      JUMP_SLOT, with Android packed relocations (APS2) and RELR handled
 *      by the same shapes husk-tl-elf.c counts with;
 *   5. its initialisers run, and a NativeActivity lifecycle is driven on
 *      a thread -- onCreate, a software ANativeWindow, touch events, and
 *      a shutdown after a fixed budget.
 *
 * This is an attempt, not a runtime. Everything it cannot do -- TLS,
 * ifuncs, TEXTREL, unresolvable imports -- it refuses loudly and logs,
 * because the point of the attempt is to find where the wall is.
 */
#define _GNU_SOURCE
#include "husk-tl-sound.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#endif

#include "husk-tl-internal.h"
#include "husk-tl.h"
#include "husk-tl-dex.h"
#include "husk-tl-framework.h"

/* The shim's surface, in husk-tl-shim.c. */
void *tl_shim_find(const char *name);
bool  tl_shim_supplies(const char *soname);
void *tl_shim_vm(void);
void *tl_shim_env(void);
void *tl_shim_activity_class(void);
void *tl_shim_new_input_queue(void);
void *tl_shim_new_asset_manager(void);
void  tl_shim_free(void *p);
void  tl_shim_bind_run(void *activity, void *window);
typedef struct tl_window tl_window;
tl_window *tl_window_create(int width, int height);
void tl_window_acquire(tl_window *w);
void tl_window_release(tl_window *w);

/* ------------------------------------------------------------- constants */

#define TL_PAGE 16384u

/* AArch64 relocation types -- the same table husk-tl-elf.c counts with. */
#define R_AARCH64_NONE          0
#define R_AARCH64_ABS64         257
#define R_AARCH64_GLOB_DAT      1025
#define R_AARCH64_JUMP_SLOT     1026
#define R_AARCH64_RELATIVE      1027
#define R_AARCH64_IRELATIVE     1032
#define R_AARCH64_TLS_DTPMOD64  1028
#define R_AARCH64_TLS_TPREL64   1030
#define R_AARCH64_TLSDESC       1031

#define DT_NULL         0
#define DT_NEEDED       1
#define DT_STRTAB       5
#define DT_SYMTAB       6
#define DT_RELA         7
#define DT_RELASZ       8
#define DT_HASH         4
#define DT_STRSZ        10
#define DT_SYMENT       11
#define DT_INIT         12
#define DT_FINI         13
#define DT_SONAME       14
#define DT_INIT_ARRAY   25
#define DT_FINI_ARRAY   26
#define DT_INIT_ARRAYSZ 27
#define DT_FINI_ARRAYSZ 28
#define DT_FLAGS        30
#define DT_JMPREL       23
#define DT_PLTRELSZ     2
#define DT_GNU_HASH     0x6ffffef5
#define DT_VERSYM       0x6ffffff0
#define DT_ANDROID_RELA 0x60000011
#define DT_ANDROID_RELASZ 0x60000012
#define DT_RELR         36
#define DT_RELRSZ       35
#define DT_ANDROID_RELR 0x6fffe000
#define DT_ANDROID_RELRSZ 0x6fffe001
#define DF_TEXTREL      4u

#define PT_LOAD     1
#define PT_DYNAMIC  2
#define PT_TLS      7
#define PT_GNU_RELRO 0x6474e552
#define PF_X 1
#define PF_W 2
#define PF_R 4

#define STB_WEAK   2
#define SHN_UNDEF  0

#define EM_AARCH64  183

#define ELF64_R_SYM(i)  ((i) >> 32)
#define ELF64_R_TYPE(i) ((i) & 0xffffffffu)

/* ELF64 fixed-size pieces, laid out explicitly: the loader reads files
 * written by other toolchains and cannot lean on the host's <elf.h>. */
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} tl_ehdr;
_Static_assert(sizeof(tl_ehdr) == 64, "tl_ehdr must be 64 bytes");

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} tl_phdr;
_Static_assert(sizeof(tl_phdr) == 56, "tl_phdr must be 56 bytes");

typedef struct {
    int64_t  d_tag;
    uint64_t d_val;
} tl_dyn;
_Static_assert(sizeof(tl_dyn) == 16, "tl_dyn must be 16 bytes");

typedef struct {
    uint32_t st_name;
    uint8_t  st_info;
    uint8_t  st_other;
    uint16_t st_shndx;
    uint64_t st_value;
    uint64_t st_size;
} tl_sym;
_Static_assert(sizeof(tl_sym) == 24, "tl_sym must be 24 bytes");

typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} tl_rela;
_Static_assert(sizeof(tl_rela) == 24, "tl_rela must be 24 bytes");

/* The NDK types the guest receives. Field order is the NDK's ABI; unused
 * fields are void* placeholders of the right size. */
typedef struct tl_window tl_window;
typedef struct tl_input_queue tl_input_queue;
typedef struct tl_asset_manager tl_asset_manager;
typedef struct tl_activity tl_activity;
typedef struct { int32_t left, top, right, bottom; } tl_rect;

typedef struct tl_activity_callbacks {
    void (*onStart)(tl_activity *);
    void (*onResume)(tl_activity *);
    void *(*onSaveInstanceState)(tl_activity *, size_t *);
    void (*onPause)(tl_activity *);
    void (*onStop)(tl_activity *);
    void (*onDestroy)(tl_activity *);
    void (*onWindowFocusChanged)(tl_activity *, int hasFocus);
    void (*onNativeWindowCreated)(tl_activity *, tl_window *);
    void (*onNativeWindowResized)(tl_activity *, tl_window *);
    void (*onInputQueueCreated)(tl_activity *, tl_input_queue *);
    void (*onInputQueueDestroyed)(tl_activity *, tl_input_queue *);
    void (*onContentRectChanged)(tl_activity *, const tl_rect *);
    void (*onSurfaceChanged)(tl_activity *, tl_window *, int format);
    void (*onSurfaceRedrawNeeded)(tl_activity *, tl_window *);
    void (*onSurfaceDestroyed)(tl_activity *);
    void (*onNativeWindowDestroyed)(tl_activity *, tl_window *);
} tl_activity_callbacks;

struct tl_activity {
    tl_activity_callbacks *callbacks;
    void   *vm;
    void   *env;
    void   *clazz;
    const char *internalDataPath;
    const char *externalDataPath;
    int32_t sdkVersion;
    void   *instance;
    tl_asset_manager *assetManager;
    const char *obbPath;
};

/* ------------------------------------------------------------ log buffer */

/* One run's log, grown as needed. Appended from the runner thread (and
 * from guest code, through the shim) and read from Swift, so a lock
 * stands over every append and snapshot. */
#define TL_LOG_CHUNK (1u << 16)

typedef struct {
    char  *buf;
    size_t len, cap;
    bool   failed;
} tl_logbuf;

static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static void tl_log_put(tl_logbuf *l, const char *s, size_t n)
{
    if (l->failed || n == 0) {
        return;
    }
    if (l->len + n + 1 > l->cap) {
        size_t cap = l->cap ? l->cap : TL_LOG_CHUNK;
        while (cap < l->len + n + 1) {
            cap *= 2;
        }
        char *grown = realloc(l->buf, cap);
        if (!grown) {
            l->failed = true;
            return;
        }
        l->buf = grown;
        l->cap = cap;
    }
    memcpy(l->buf + l->len, s, n);
    l->len += n;
    l->buf[l->len] = '\0';
}

/* The append funnel. Defined after g_run below, declared here for the
 * shim: guest code logs through it too. */
void tl_log_line(const char *fmt, ...);

/* --------------------------------------------------------------- the run */

typedef struct {
    char      name[96];
    uint8_t  *file;          /* the whole .so, kept for the file-relative reads */
    size_t    file_len;
    uint8_t  *base;          /* where it was mapped (executable view) */
    uint8_t  *base_rw;       /* writable mirror for dual mapping, or base if MAP_JIT */
    bool      is_stikdebug;
    size_t    npages;
    uint8_t  *page_flags;
    uint64_t  base_vaddr;
    const tl_sym *symtab; uint64_t syment;
    const uint8_t *strtab; uint64_t strsz;
    uint64_t gnu_hash;
    uint64_t init, fini, init_array, init_arraysz;
} tl_lib;

typedef struct {
    bool      active;
    bool      finished;
    int       exit_code;       /* 0 refused, 1 loaded, 2 drew */
    pthread_t thread;
    tl_logbuf log;

    tl_lib    libs[8];
    int       nlibs;

    /* what the guest was given */
    tl_window *window;
    void *queue;
    void *assets;
    tl_activity activity;
    tl_activity_callbacks callbacks;

    /* frames the guest has posted. The pixels themselves live in the slots below,
     * which outlive a run: a frame the UI is still showing must not vanish because
     * the next attempt began. */
    pthread_mutex_t frame_mutex;
    uint64_t frames;

    volatile bool stop_requested;
    /* ANativeActivity_onCreate, found after loading; the lifecycle thread
     * calls it, because that thread owns the executable-mode toggle. */
    void (*on_create)(void *, void *, size_t);

    /* Dalvik / DEX execution (Milestone 2) */
    tl_dex_context *dex_ctx;
} tl_run;

static tl_run g_run;

int tl_log_sink_fd = -1;

void tl_log_line(const char *fmt, ...)
{
    /* Room for the newline and the terminator after the longest line. The limit used to be one too many:
     * a line of 511 characters or more wrote its terminator one byte past the buffer, into the saved registers
     * above it -- harmless until Unity logged its 3 KB OpenGL extension list and the function returned to garbage. */
    char line[2048];
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(line, sizeof(line) - 2, fmt, ap);
    va_end(ap);
    if (w <= 0) {
        return;
    }
    if ((size_t)w > sizeof(line) - 3) {
        w = (int)sizeof(line) - 3;
    }
    line[w] = '\n';
    line[w + 1] = '\0';
    fputs(line, stderr);
    /* A file written line by line with write(2): what a process killed without warning still leaves behind. */
    if (tl_log_sink_fd >= 0) { ssize_t ignored = write(tl_log_sink_fd, line, strlen(line)); (void)ignored; }
    pthread_mutex_lock(&g_log_mutex);
    tl_log_put(&g_run.log, line, (size_t)w + 1);
    pthread_mutex_unlock(&g_log_mutex);
}

/*
 * Finished frames, handed to the UI without a copy.
 *
 * The guest draws into the window's own buffer, which is half-finished for most
 * of every frame, so the UI can never look at that. When a frame completes it is
 * copied into one of these slots and the slot is published as the newest. The UI
 * pins the newest slot to read it and releases it when done.
 *
 * At any moment one slot is the newest, the UI may hold two (the frame on the
 * screen and the one just taken to replace it), and one is being written -- four
 * in all. There are six, because Core Animation can keep an older image alive
 * while the GPU is still uploading it, and a producer that finds no free slot
 * has to drop the frame. The producer only ever writes a slot that is none of
 * those, so it takes no lock while it copies two megabytes, and a reader can
 * keep a frame as long as it likes without the guest ever waiting on it.
 *
 * Static rather than part of the run: the run struct is wiped and rebuilt
 * between attempts, and a frame that is still on screen -- or still being
 * uploaded by the display -- must not be freed under it.
 */
#define TL_FRAME_SLOTS 6

typedef struct {
    uint8_t *px;
    size_t   cap;
    int      pins;         /* readers holding it, or 1 while it is being written */
} tl_frame_slot;

static pthread_mutex_t g_slot_lock = PTHREAD_MUTEX_INITIALIZER;
static tl_frame_slot   g_slots[TL_FRAME_SLOTS];
static int             g_front = -1;        /* newest finished slot, or none */
static uint64_t        g_generation;
static int             g_front_w, g_front_h, g_front_stride;
static uint64_t        g_dropped;           /* frames with no free slot: should stay zero */

/* A new attempt starts with nothing on offer. Slots in use stay allocated. */
static void slots_invalidate(void)
{
    pthread_mutex_lock(&g_slot_lock);
    g_front = -1;
    pthread_mutex_unlock(&g_slot_lock);
}

/* The window, between lock/unlock, is what the guest is drawing into; this is
 * the moment it is finished, so it is copied out and published. */
void tl_loader_frame_posted(void)
{
    tl_window *w = g_run.window;
    if (!w) {
        return;
    }
    const size_t bytes = (size_t)w->width * w->height * 4;

    /* Reserve a slot nobody is using and that is not the newest. */
    int target = -1;
    pthread_mutex_lock(&g_slot_lock);
    for (int i = 0; i < TL_FRAME_SLOTS; i++) {
        if (i != g_front && g_slots[i].pins == 0) {
            target = i;
            g_slots[i].pins = 1;
            break;
        }
    }
    pthread_mutex_unlock(&g_slot_lock);
    if (target < 0) {
        g_dropped++;
        return;
    }

    /* The copy happens outside the lock: this slot is ours alone. */
    tl_frame_slot *slot = &g_slots[target];
    if (slot->cap < bytes) {
        uint8_t *grown = realloc(slot->px, bytes);
        if (!grown) {
            pthread_mutex_lock(&g_slot_lock);
            slot->pins = 0;
            pthread_mutex_unlock(&g_slot_lock);
            return;
        }
        slot->px = grown;
        slot->cap = bytes;
    }
    memcpy(slot->px, w->bits, bytes);

    pthread_mutex_lock(&g_slot_lock);
    slot->pins = 0;
    g_front = target;
    g_front_w = w->width;
    g_front_h = w->height;
    g_front_stride = w->stridePixels;
    g_generation++;
    pthread_mutex_unlock(&g_slot_lock);

    pthread_mutex_lock(&g_run.frame_mutex);
    g_run.frames++;
    pthread_mutex_unlock(&g_run.frame_mutex);
}

uint64_t husk_tl_frame_acquire(const uint8_t **pixels, int *width, int *height,
                               int *stride_pixels, int *token)
{
    uint64_t gen = 0;
    pthread_mutex_lock(&g_slot_lock);
    if (g_front >= 0 && g_slots[g_front].px) {
        g_slots[g_front].pins++;
        if (pixels) *pixels = g_slots[g_front].px;
        if (width)  *width = g_front_w;
        if (height) *height = g_front_h;
        if (stride_pixels) *stride_pixels = g_front_stride;
        if (token)  *token = g_front;
        gen = g_generation;
    }
    pthread_mutex_unlock(&g_slot_lock);
    return gen;
}

void husk_tl_frame_release(int token)
{
    if (token < 0 || token >= TL_FRAME_SLOTS) return;
    pthread_mutex_lock(&g_slot_lock);
    if (g_slots[token].pins > 0) g_slots[token].pins--;
    pthread_mutex_unlock(&g_slot_lock);
}

/* The pump publishes its counters here once a second; Swift reads them. */
static husk_tl_perf g_perf;

/* iOS publishes these from the main-thread CADisplayLink; Unity reads them
 * from the guest thread while answering Android Display queries. */
static atomic_uint g_display_refresh_millihz;
static atomic_uint g_max_display_refresh_millihz;

static unsigned refresh_millihz(float hz)
{
    if (!(hz > 0.0f) || hz > 1000.0f) return 0;
    return (unsigned)(hz * 1000.0f + 0.5f);
}

void husk_tl_set_display_refresh_rate(float hz)
{
    atomic_store_explicit(&g_display_refresh_millihz, refresh_millihz(hz), memory_order_relaxed);
}

void husk_tl_set_maximum_display_refresh_rate(float hz)
{
    atomic_store_explicit(&g_max_display_refresh_millihz, refresh_millihz(hz), memory_order_relaxed);
}

float husk_tl_display_refresh_rate(void)
{
    unsigned current = atomic_load_explicit(&g_display_refresh_millihz, memory_order_relaxed);
    if (!current) current = atomic_load_explicit(&g_max_display_refresh_millihz, memory_order_relaxed);
    return (float)current / 1000.0f;
}

float husk_tl_maximum_display_refresh_rate(void)
{
    return (float)atomic_load_explicit(&g_max_display_refresh_millihz, memory_order_relaxed) / 1000.0f;
}

void husk_tl_perf_snapshot(husk_tl_perf *out)
{
    if (!out) return;
    pthread_mutex_lock(&g_slot_lock);
    *out = g_perf;
    pthread_mutex_unlock(&g_slot_lock);
}

void tl_loader_request_stop(void)
{
    g_run.stop_requested = true;
}

/* ------------------------------------------------------------ JIT memory */

#if defined(__aarch64__) && defined(__APPLE__)
typedef void (*wp_fn)(int);
static wp_fn g_wp;
static void jit_init(void)
{
    int (*supported)(void) = (int (*)(void))dlsym(RTLD_DEFAULT,
        "pthread_jit_write_protect_supported_np");
    if (supported && supported()) {
        g_wp = (wp_fn)dlsym(RTLD_DEFAULT, "pthread_jit_write_protect_np");
    }
}
/* on == true: pages writable, execution unsafe. on == false: executable. */
static void jit_writable(bool on) { if (g_wp) g_wp(on ? 0 : 1); }
static void jit_icache(void *p, size_t n) { sys_icache_invalidate(p, n); }
#else
static void jit_init(void) {}
static void jit_writable(bool on) { (void)on; }
static void jit_icache(void *p, size_t n)
{
    __builtin___clear_cache((char *)p, (char *)p + (ptrdiff_t)n);
}
#endif

static size_t g_prewarmed_used = 0;

static bool is_valid_dual_mapping(const tl_dual_mapping *m)
{
    if (!m || !m->rw_addr || !m->rx_addr) return false;
    /* Must be 16 KiB page-aligned on iOS. */
    if (((uintptr_t)m->rw_addr & 0x3FFFull) != 0) return false;
    if (((uintptr_t)m->rx_addr & 0x3FFFull) != 0) return false;
    if (m->size < 1024 * 1024 || (m->size & 0x3FFFull) != 0) return false;
    return true;
}

tl_dual_mapping *tl_find_stikdebug_prewarmed(void)
{
    tl_dual_mapping *(*get_fn)(void) = (tl_dual_mapping *(*)(void))dlsym(RTLD_DEFAULT, "husk_ios_jit_get_mapping");
    if (get_fn) {
        tl_dual_mapping *m = get_fn();
        if (is_valid_dual_mapping(m)) return m;
    }

    /* Ensure prewarm has been called in case this attempt ran before QEMU. */
    bool (*prewarm_fn)(size_t) = (bool (*)(size_t))dlsym(RTLD_DEFAULT, "husk_ios_jit_prewarm");
    if (prewarm_fn) {
        prewarm_fn(256 * 1024 * 1024);
    }

    void *fn = dlsym(RTLD_DEFAULT, "husk_ios_jit_prewarm");
    if (!fn) return NULL;

    /* 1. Try static offset in libqemu-aarch64-softmmu.dylib (_husk_prewarmed is at 0x1ce6920, prewarm at 0x35d788) */
    tl_dual_mapping *m = (tl_dual_mapping *)((uintptr_t)fn + 0x1989198);
    if (is_valid_dual_mapping(m)) {
        return m;
    }

    /* 2. Decode the specific adrp+ldr right before epilogue (instruction 36) */
    const uint32_t *p = (const uint32_t *)fn;
    for (int i = 30; i < 50; i++) {
        uint32_t insn = p[i];
        if ((insn & 0x9F000000u) == 0x90000000u) { /* adrp */
            uint32_t next = p[i + 1];
            if ((next & 0xFFC00000u) == 0xF9400000u) { /* ldr Xt, [Xn, #imm] */
                uint32_t rd = insn & 0x1Fu;
                uint32_t rn = (next >> 5) & 0x1Fu;
                if (rd == rn) {
                    uint64_t immlo = (insn >> 29) & 3u;
                    uint64_t immhi = (insn >> 5) & 0x7FFFFu;
                    int64_t imm = (int64_t)((immhi << 2) | immlo);
                    if (imm & 0x100000) imm -= 0x200000;
                    uintptr_t pc = (uintptr_t)&p[i];
                    uintptr_t page = (pc & ~0xFFFull) + (imm << 12);
                    uint64_t pimm = ((next >> 10) & 0xFFFu) << 3;
                    tl_dual_mapping *cand = (tl_dual_mapping *)(page + pimm);
                    if (is_valid_dual_mapping(cand)) {
                        return cand;
                    }
                }
            }
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ ELF  */

static uint32_t ld32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}
static uint64_t ld64(const uint8_t *p) { return ld32(p) | ((uint64_t)ld32(p + 4) << 32); }

typedef struct {
    const uint8_t *d;
    size_t n;
    uint64_t lv[16], lo[16], lf[16]; int nloads;
    uint64_t dyn_vaddr, dyn_filesz;
} lex;

static bool lex_seg(lex *e, uint64_t v, uint64_t len, uint64_t *off)
{
    for (int i = 0; i < e->nloads; i++) {
        if (v >= e->lv[i] && v - e->lv[i] <= e->lf[i] && len <= e->lf[i] - (v - e->lv[i])) {
            uint64_t o = e->lo[i] + (v - e->lv[i]);
            if (o > e->n || len > e->n - o) {
                return false;
            }
            *off = o;
            return true;
        }
    }
    return false;
}

/* The dynamic table, flattened: it is a walk, so collect what is needed. */
typedef struct {
    uint64_t strtab, strsz, symtab, syment;
    uint64_t gnu_hash;
    uint64_t rela, relasz, jmprel, pltrelsz;
    uint64_t packed_rela, packed_relasz, relr, relrsz;
    uint64_t init, fini, init_array, init_arraysz;
    uint64_t needed[24]; int nneeded;
    bool textrel;
} ldyn;

static bool lex_dynamic(lex *e, ldyn *d)
{
    memset(d, 0, sizeof(*d));
    uint64_t off;
    if (e->dyn_filesz < sizeof(tl_dyn) || e->dyn_filesz % sizeof(tl_dyn) != 0
        || !lex_seg(e, e->dyn_vaddr, e->dyn_filesz, &off)) {
        return false;
    }
    const uint8_t *p = e->d + off;
    size_t n = (size_t)(e->dyn_filesz / sizeof(tl_dyn));
    for (size_t i = 0; i < n; i++) {
        int64_t tag = (int64_t)ld64(p + i * sizeof(tl_dyn));
        uint64_t val = ld64(p + i * sizeof(tl_dyn) + 8);
        switch (tag) {
        case DT_NULL: return true;
        case DT_NEEDED: if (d->nneeded < 24) d->needed[d->nneeded++] = val; break;
        case DT_STRTAB: d->strtab = val; break;
        case DT_STRSZ: d->strsz = val; break;
        case DT_SYMTAB: d->symtab = val; break;
        case DT_SYMENT: d->syment = val; break;
        case DT_GNU_HASH: d->gnu_hash = val; break;
        case DT_RELA: d->rela = val; break;
        case DT_RELASZ: d->relasz = val; break;
        case DT_JMPREL: d->jmprel = val; break;
        case DT_PLTRELSZ: d->pltrelsz = val; break;
        case DT_INIT: d->init = val; break;
        case DT_FINI: d->fini = val; break;
        case DT_INIT_ARRAY: d->init_array = val; break;
        case DT_INIT_ARRAYSZ: d->init_arraysz = val; break;
        case DT_FLAGS: d->textrel = (val & DF_TEXTREL) != 0; break;
        case DT_ANDROID_RELA: d->packed_rela = val; break;
        case DT_ANDROID_RELASZ: d->packed_relasz = val; break;
        case DT_RELR: d->relr = val; break;
        case DT_RELRSZ: d->relrsz = val; break;
        default: break;
        }
    }
    return true;
}

static const char *ldyn_str(lex *e, const ldyn *d, uint64_t vaddr_off)
{
    /* Returns a pointer into the FILE mapping; the file outlives the run. */
    uint64_t o;
    if (!lex_seg(e, d->strtab, d->strsz, &o) || vaddr_off >= d->strsz) {
        return "";
    }
    const char *base = (const char *)e->d + o;
    /* NUL-termination is the format's own guarantee; stay bounded anyway. */
    size_t i = 0;
    while (i < d->strsz - vaddr_off && base[vaddr_off + i] != '\0') {
        i++;
    }
    return base + vaddr_off;
}

/* ------------------------------------------------------------ mapping  */

static void unmap_lib(tl_lib *L)
{
    if (!L->base) {
        return;
    }
    for (size_t i = 0; i < L->npages; i++) {
        if (L->page_flags[i] & TL_PAGE_CARVED) {
            vm_deallocate(mach_task_self(), (vm_address_t)(L->base + i * TL_PAGE), TL_PAGE);
        }
    }
    if (!L->is_stikdebug) {
        munmap(L->base, L->npages * TL_PAGE);
    }
    free(L->page_flags);
}

/* ------------------------------------------------------------ symbols  */

static uint32_t gnu_hash_name(const char *s)
{
    uint32_t h = 5381;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        h = h * 33 + *p;
    }
    return h;
}

/* One loaded library's GNU hash table: find a defined symbol. */
static void *lib_lookup(const tl_lib *L, const char *name)
{
    if (!L->symtab || !L->strtab) {
        return NULL;
    }
    if (!L->gnu_hash) {
        for (size_t i = 1; i < 8192; i++) {
            const tl_sym *s = &L->symtab[i];
            uint32_t name_off = ld32((const uint8_t *)&s->st_name);
            if (name_off == 0 || name_off >= L->strsz) break;
            const char *sn = (const char *)L->strtab + name_off;
            if (!strcmp(sn, name) && s->st_value != 0) {
                uint64_t target_off = s->st_value - L->base_vaddr;
                size_t p = (size_t)(target_off / TL_PAGE);
                if (L->is_stikdebug && p < L->npages && (L->page_flags[p] & TL_PAGE_W)) {
                    return L->base_rw + target_off;
                }
                return L->base + target_off;
            }
        }
        return NULL;
    }
    const uint8_t *g = (const uint8_t *)L->gnu_hash;
    uint32_t nbuckets = ld32(g), symoffset = ld32(g + 4);
    uint32_t bloom_size = ld32(g + 8), bloom_shift = ld32(g + 12);
    const uint64_t *bloom = (const uint64_t *)(g + 16);
    const uint32_t *buckets = (const uint32_t *)(g + 16 + (size_t)bloom_size * 8);
    const uint32_t *chains = buckets + nbuckets;
    if (!nbuckets) {
        return NULL;
    }

    uint32_t h = gnu_hash_name(name);
    if (bloom_size > 0) {
        uint64_t word = bloom[(h / 64) % bloom_size];
        if ((word >> (h % 64)) == 0 || (word >> ((h >> bloom_shift) % 64)) == 0) {
            return NULL;
        }
    }
    uint32_t bucket = buckets[h % nbuckets];
    if (bucket < symoffset) {
        return NULL;
    }
    for (uint32_t i = bucket;; i++) {
        uint32_t chain = chains[i - symoffset];
        if ((h | 1) == (chain | 1)) {
            const tl_sym *s = &L->symtab[i];
            const char *sn = (const char *)L->strtab + ld32((const uint8_t *)&s->st_name);
            if (sn && !strcmp(sn, name) && s->st_value != 0) {
                uint64_t target_off = s->st_value - L->base_vaddr;
                size_t p = (size_t)(target_off / TL_PAGE);
                if (L->is_stikdebug && p < L->npages && (L->page_flags[p] & TL_PAGE_W)) {
                    return L->base_rw + target_off;
                }
                return L->base + target_off;
            }
        }
        if (chain & 1) {
            return NULL;
        }
    }
}

/* Resolve one import: libraries loaded earlier first (so a library sees
 * its own dependencies), then the shim. */
void *tl_shim_find(const char *name);

static void *resolve(const char *name, bool *weak)
{
    for (int i = 0; i < g_run.nlibs; i++) {
        void *a = lib_lookup(&g_run.libs[i], name);
        if (a) {
            return a;
        }
    }
    void *shim = tl_shim_find(name);
    if (shim) {
        return shim;
    }
    (void)weak;
    return NULL;
}

/* ------------------------------------------------------- relocation  */

typedef struct { const uint8_t *p; const uint8_t *end; bool bad; } sleb_r;
static int64_t rd_sleb(sleb_r *s)
{
    uint64_t v = 0;
    unsigned shift = 0;
    uint8_t b;
    do {
        if (s->p >= s->end || shift >= 64) {
            s->bad = true;
            return 0;
        }
        b = *s->p++;
        v |= (uint64_t)(b & 0x7f) << shift;
        shift += 7;
    } while (b & 0x80);
    if (shift < 64 && (b & 0x40)) {
        v |= ~0ull << shift;
    }
    return (int64_t)v;
}

/* All relocation targets are in RELRO or data -- never in code -- and the
 * carved layout makes every such page ordinary writable memory, so no
 * JIT window is needed here. */
static bool reloc_one(uint64_t off_in_image, uint32_t type, uint32_t symidx,
                      int64_t addend, tl_lib *L)
{
    uint64_t offset = off_in_image - L->base_vaddr;
    size_t page_idx = (size_t)(offset / TL_PAGE);
    uint8_t *place = (page_idx < L->npages && (L->page_flags[page_idx] & TL_PAGE_CARVED))
                   ? (L->base + offset)
                   : (L->base_rw ? L->base_rw + offset : L->base + offset);

    switch (type) {
    case R_AARCH64_NONE:
        return true;
    case R_AARCH64_RELATIVE: {
        uint64_t target_off = (uint64_t)addend - L->base_vaddr;
        size_t target_p = (size_t)(target_off / TL_PAGE);
        if (L->is_stikdebug && target_p < L->npages && (L->page_flags[target_p] & TL_PAGE_W)) {
            *(uint64_t *)place = (uint64_t)L->base_rw + target_off;
        } else {
            *(uint64_t *)place = (uint64_t)L->base + target_off;
        }
        return true;
    }
    case R_AARCH64_ABS64:
    case R_AARCH64_GLOB_DAT:
    case R_AARCH64_JUMP_SLOT: {
        if (symidx == 0) {
            uint64_t target_off = (uint64_t)addend - L->base_vaddr;
            size_t target_p = (size_t)(target_off / TL_PAGE);
            if (L->is_stikdebug && target_p < L->npages && (L->page_flags[target_p] & TL_PAGE_W)) {
                *(uint64_t *)place = (uint64_t)L->base_rw + target_off;
            } else {
                *(uint64_t *)place = (uint64_t)L->base + target_off;
            }
            return true;
        }
        const tl_sym *s = &L->symtab[symidx];
        uint32_t bind = s->st_info >> 4;
        bool undef = s->st_shndx == SHN_UNDEF;
        if (undef) {
            const char *sname = (const char *)L->strtab
                              + ld32((const uint8_t *)&s->st_name);
            bool weak = bind == STB_WEAK;
            void *addr = resolve(sname, &weak);
            if (!addr) {
                if (!weak) {
                    tl_log_line("reloc: unresolved import %s in %s -- refusing",
                                sname, L->name);
                    return false;
                }
                *(uint64_t *)place = 0;      /* a weak undefined stays NULL */
                return true;
            }
            *(uint64_t *)place = (uint64_t)addr;
            return true;
        }
        uint64_t target_off = s->st_value + (uint64_t)addend - L->base_vaddr;
        size_t target_p = (size_t)(target_off / TL_PAGE);
        if (L->is_stikdebug && target_p < L->npages && (L->page_flags[target_p] & TL_PAGE_W)) {
            *(uint64_t *)place = (uint64_t)L->base_rw + target_off;
        } else {
            *(uint64_t *)place = (uint64_t)L->base + target_off;
        }
        return true;
    }
    case R_AARCH64_IRELATIVE:
        tl_log_line("reloc: IRELATIVE (ifunc) in %s -- the attempt does not "
                    "run resolvers, refusing", L->name);
        return false;
    case R_AARCH64_TLS_DTPMOD64:
    case R_AARCH64_TLS_TPREL64:
    case R_AARCH64_TLSDESC:
        tl_log_line("reloc: TLS relocation in %s -- no TLS here, refusing", L->name);
        return false;
    default:
        tl_log_line("reloc: unsupported type %u in %s", type, L->name);
        return false;
    }
}

static bool apply_relocations(tl_lib *L, lex *e, const ldyn *d)
{
    size_t done = 0;

    /* Plain RELA. */
    if (d->rela && d->relasz) {
        uint64_t off;
        if (d->relasz % sizeof(tl_rela) != 0 || !lex_seg(e, d->rela, d->relasz, &off)) {
            return false;
        }
        size_t n = (size_t)(d->relasz / sizeof(tl_rela));
        for (size_t i = 0; i < n; i++) {
            uint64_t r_offset = ld64(e->d + off + i * sizeof(tl_rela));
            uint64_t r_info   = ld64(e->d + off + i * sizeof(tl_rela) + 8);
            int64_t  r_addend = (int64_t)ld64(e->d + off + i * sizeof(tl_rela) + 16);
            if (!reloc_one(r_offset, ELF64_R_TYPE(r_info), (uint32_t)ELF64_R_SYM(r_info),
                           r_addend, L)) {
                return false;
            }
            done++;
        }
    }
    /* PLT (JUMP_SLOT) tables. */
    if (d->jmprel && d->pltrelsz) {
        uint64_t off;
        if (d->pltrelsz % sizeof(tl_rela) != 0 || !lex_seg(e, d->jmprel, d->pltrelsz, &off)) {
            return false;
        }
        for (size_t i = 0; i < d->pltrelsz / sizeof(tl_rela); i++) {
            uint64_t r_offset = ld64(e->d + off + i * sizeof(tl_rela));
            uint64_t r_info   = ld64(e->d + off + i * sizeof(tl_rela) + 8);
            int64_t  r_addend = (int64_t)ld64(e->d + off + i * sizeof(tl_rela) + 16);
            if (!reloc_one(r_offset, ELF64_R_TYPE(r_info), (uint32_t)ELF64_R_SYM(r_info),
                           r_addend, L)) {
                return false;
            }
            done++;
        }
    }
    /* Android packed (APS2). */
    if (d->packed_rela && d->packed_relasz) {
        uint64_t off;
        if (!lex_seg(e, d->packed_rela, d->packed_relasz, &off)) {
            return false;
        }
        if (memcmp(e->d + off, "APS2", 4) != 0) {
            tl_log_line("reloc: packed table lacks the APS2 magic");
            return false;
        }
        sleb_r s = { e->d + off + 4, e->d + off + d->packed_relasz, false };
        int64_t count = rd_sleb(&s);
        int64_t relof = rd_sleb(&s);
        if (s.bad || count < 0) {
            return false;
        }
        int64_t done2 = 0;
        uint64_t info = 0;
        while (done2 < count) {
            int64_t group = rd_sleb(&s), gflags = rd_sleb(&s);
            if (s.bad || group <= 0 || done2 + group > count) {
                return false;
            }
            int64_t delta = (gflags & 2) ? rd_sleb(&s) : 0;
            if (gflags & 1) {
                info = (uint64_t)rd_sleb(&s);
            }
            int64_t addend = 0;
            bool has_addend = (gflags & 8) != 0;
            bool shared_addend = has_addend && (gflags & 4);
            if (shared_addend) {
                addend = rd_sleb(&s);
            }
            for (int64_t i = 0; i < group; i++) {
                if (gflags & 2) {
                    if (i > 0) {
                        relof += delta;
                    }
                } else {
                    relof = rd_sleb(&s);
                }
                if (has_addend && !shared_addend) {
                    addend = rd_sleb(&s);
                }
                if (s.bad) {
                    return false;
                }
                if (!reloc_one((uint64_t)relof, (uint32_t)(info & 0xffffffffu),
                               (uint32_t)(info >> 32), addend, L)) {
                    return false;
                }
                done++;
            }
            done2 += group;
        }
    }
    /* RELR: compact RELATIVE sets. A word with the low bit clear is one
     * address whose slot gets base + addr; a bitmap word (low bit set)
     * covers the 63 addresses after the running cursor, bit i meaning the
     * slot at cursor + i - 1. */
    if (d->relr && d->relrsz) {
        uint64_t off;
        if (d->relrsz % 8 != 0 || !lex_seg(e, d->relr, d->relrsz, &off)) {
            return false;
        }
        const uint8_t *w = e->d + off;
        size_t n = (size_t)(d->relrsz / 8);
        uint64_t addr = 0;
        for (size_t i = 0; i < n; i++) {
            uint64_t word = ld64(w + i * 8);
            if (!(word & 1)) {
                uint64_t offset = word - L->base_vaddr;
                size_t p = (size_t)(offset / TL_PAGE);
                uint8_t *target = (p < L->npages && (L->page_flags[p] & TL_PAGE_CARVED))
                                ? (L->base + offset)
                                : (L->base_rw ? L->base_rw + offset : L->base + offset);
                *(uint64_t *)target = (uint64_t)L->base + word;
                addr = word + 8;
                done++;
            } else {
                for (uint64_t bit = 1; bit != 0 && bit < (1ull << 63); bit <<= 1) {
                    if (word & bit) {
                        uint64_t a = addr + (bit >> 1);
                        uint64_t offset = a - L->base_vaddr;
                        size_t p = (size_t)(offset / TL_PAGE);
                        uint8_t *target = (p < L->npages && (L->page_flags[p] & TL_PAGE_CARVED))
                                        ? (L->base + offset)
                                        : (L->base_rw ? L->base_rw + offset : L->base + offset);
                        *(uint64_t *)target = (uint64_t)L->base + a;
                        done++;
                    }
                }
                addr += 8 * 63;
            }
        }
    }
    tl_log_line("reloc: %s, %zu relocations applied", L->name, done);
    return true;
}

/* ------------------------------------------------------- APK extraction */

static bool list_arm64_libs(const char *const *apks, int count,
                            char names[][96], char apk_of[][512], int *n,
                            char *err, size_t errlen)
{
    *n = 0;
    for (int a = 0; a < count; a++) {
        tl_zip z;
        char zerr[128] = {0};
        if (!tl_zip_open(&z, apks[a], zerr, sizeof(zerr))) {
            snprintf(err, errlen, "could not open %s: %s", apks[a], zerr);
            return false;
        }
        for (size_t i = 0; i < z.count; i++) {
            const char *f = z.entries[i].name;
            if (strncmp(f, "lib/arm64-v8a/", 14) != 0) {
                continue;
            }
            const char *base = f + 14;
            size_t bl = strlen(base);
            if (bl < 4 || bl >= 96 || strcmp(base + bl - 3, ".so") != 0) {
                continue;
            }
            bool dup = false;
            for (int j = 0; j < *n; j++) {
                if (!strcmp(names[j], base)) {
                    dup = true;
                    break;
                }
            }
            if (dup || *n >= 24) {
                continue;
            }
            snprintf(names[*n], 96, "%s", base);
            snprintf(apk_of[*n], 512, "%s", apks[a]);
            (*n)++;
        }
        tl_zip_close(&z);
    }
    return true;
}

static bool extract_lib(const char *apk, const char *libname,
                        uint8_t **out, size_t *outlen, char *err, size_t errlen)
{
    char entry[128];
    snprintf(entry, sizeof(entry), "lib/arm64-v8a/%s", libname);
    tl_zip z;
    char zerr[128] = {0};
    if (!tl_zip_open(&z, apk, zerr, sizeof(zerr))) {
        snprintf(err, errlen, "%s", zerr);
        return false;
    }
    const tl_zip_entry *en = tl_zip_find(&z, entry);
    if (!en) {
        tl_zip_close(&z);
        snprintf(err, errlen, "%s not in the APK", entry);
        return false;
    }
    const uint8_t *bytes;
    size_t len;
    bool owned;
    if (!tl_zip_data(&z, en, 1u << 30, &bytes, &len, &owned, err, errlen)) {
        tl_zip_close(&z);
        return false;
    }
    uint8_t *copy = malloc(len ? len : 1);
    if (!copy) {
        if (owned) free((void *)bytes);
        tl_zip_close(&z);
        snprintf(err, errlen, "out of memory");
        return false;
    }
    memcpy(copy, bytes, len);
    if (owned) free((void *)bytes);
    tl_zip_close(&z);
    *out = copy;
    *outlen = len;
    return true;
}

#if defined(__aarch64__)
static uint64_t g_bionic_tcb[512] __attribute__((aligned(4096)));

static uint32_t encode_adrp(uint32_t rt, const void *pc, const void *target)
{
    int64_t delta = (int64_t)(((uintptr_t)target & ~(uintptr_t)0xFFF)
                            - ((uintptr_t)pc & ~(uintptr_t)0xFFF)) >> 12;
    uint32_t imm = (uint32_t)delta & 0x1FFFFF;
    return 0x90000000u | ((imm & 3u) << 29) | ((imm >> 2) << 5) | (rt & 0x1Fu);
}

static void patch_guest_code(uint8_t *code_rw, const uint8_t *code_rx, size_t len,
                             const uint8_t *page_flags, size_t npages,
                             bool is_stikdebug, ptrdiff_t delta_rw_rx)
{
    g_bionic_tcb[0] = (uint64_t)(uintptr_t)g_bionic_tcb;
    g_bionic_tcb[1] = 1000;
    g_bionic_tcb[2] = 1000;
    g_bionic_tcb[5] = 0xdeadbeefcafebabeull; /* offset 0x28: stack canary guard */

    size_t count_tpidr = 0;
    size_t count_adrp = 0;
    for (size_t off = 0; off + 4 <= len; off += 4) {
        size_t p = off / TL_PAGE;
        if (p < npages && !(page_flags[p] & TL_PAGE_X)) {
            continue;
        }
        uint32_t insn = *(uint32_t *)(code_rw + off);
        if ((insn & 0xFFFFFFE0u) == 0xD53BD040u) {
            uint32_t rt = insn & 0x1Fu;
            uint32_t new_insn = encode_adrp(rt, code_rx + off, g_bionic_tcb);
            *(uint32_t *)(code_rw + off) = new_insn;
            count_tpidr++;
            continue;
        }

        if (is_stikdebug && (insn & 0x9F000000u) == 0x90000000u) {
            uint32_t immlo = (insn >> 29) & 3u;
            uint32_t immhi = (insn >> 5) & 0x7FFFFu;
            int64_t imm = (int64_t)((immhi << 2) | immlo);
            if (imm & 0x100000) imm -= 0x200000;

            const uint8_t *pc = code_rx + off;
            uintptr_t target_page = ((uintptr_t)pc & ~0xFFFull) + (imm << 12);
            if (target_page >= (uintptr_t)code_rx &&
                target_page < (uintptr_t)code_rx + npages * TL_PAGE) {
                size_t target_off = (size_t)(target_page - (uintptr_t)code_rx);
                size_t target_p = target_off / TL_PAGE;
                if (target_p < npages && (page_flags[target_p] & TL_PAGE_W)) {
                    uint32_t rt = insn & 0x1Fu;
                    const void *rw_target = (const void *)(target_page + delta_rw_rx);
                    uint32_t new_insn = encode_adrp(rt, pc, rw_target);
                    *(uint32_t *)(code_rw + off) = new_insn;
                    count_adrp++;
                }
            }
        }
    }
    if (count_tpidr > 0) {
        tl_log_line("patch: rewritten %zu 'mrs Xt, tpidr_el0' -> adrp stack guard", count_tpidr);
    }
    if (count_adrp > 0) {
        tl_log_line("patch: remapped %zu 'adrp' data references -> StikDebug RW alias", count_adrp);
    }
}
#else
static void patch_guest_code(uint8_t *code_rw, const uint8_t *code_rx, size_t len,
                             const uint8_t *page_flags, size_t npages,
                             bool is_stikdebug, ptrdiff_t delta_rw_rx)
{
    (void)code_rw; (void)code_rx; (void)len; (void)page_flags; (void)npages;
    (void)is_stikdebug; (void)delta_rw_rx;
}
#endif

/* ---------------------------------------------------------- loading  */

static bool load_library(const char *name, const uint8_t *file, size_t flen)
{
    if (g_run.nlibs >= (int)(sizeof(g_run.libs) / sizeof(g_run.libs[0]))) {
        tl_log_line("load: too many libraries, refusing %s", name);
        return false;
    }
    if (flen < sizeof(tl_ehdr) || memcmp(file, "\x7f""ELF", 4) != 0) {
        tl_log_line("load: %s is not an ELF file", name);
        return false;
    }
    const tl_ehdr *eh = (const tl_ehdr *)file;
    if (eh->e_ident[4] != 2 || eh->e_ident[5] != 1) {
        tl_log_line("load: %s is not a 64-bit little-endian image", name);
        return false;
    }
    if (eh->e_machine != EM_AARCH64) {
        tl_log_line("load: %s is not arm64 (machine %u)", name, eh->e_machine);
        return false;
    }
    if (eh->e_phentsize < sizeof(tl_phdr)
        || (size_t)eh->e_phoff + (size_t)eh->e_phnum * eh->e_phentsize > flen) {
        tl_log_line("load: %s has a program header table off the end", name);
        return false;
    }

    lex e;
    memset(&e, 0, sizeof(e));
    e.d = file; e.n = flen;
    const tl_phdr *loads[16]; int nloads = 0;
    bool has_relro = false, has_tls = false;
    tl_segment relro = {0};
    for (int i = 0; i < eh->e_phnum; i++) {
        const uint8_t *ph = file + eh->e_phoff + (size_t)i * eh->e_phentsize;
        uint32_t type = ld32(ph);
        if (type == PT_LOAD && nloads < 16) {
            loads[nloads++] = (const tl_phdr *)ph;
            e.lv[e.nloads] = ld64(ph + 16);              /* p_vaddr */
            e.lo[e.nloads] = ld64(ph + 8);               /* p_offset */
            e.lf[e.nloads] = ld64(ph + 32);              /* p_filesz */
            e.nloads++;
        } else if (type == PT_GNU_RELRO) {
            has_relro = true;
            relro.vaddr = ld64(ph + 16);
            relro.memsz = ld64(ph + 40);
        } else if (type == PT_DYNAMIC) {
            e.dyn_vaddr = ld64(ph + 16);
            e.dyn_filesz = ld64(ph + 32);
        } else if (type == PT_TLS) {
            has_tls = true;
        }
    }
    if (!nloads || !e.dyn_filesz) {
        tl_log_line("load: %s has no loadable segments", name);
        return false;
    }
    if (has_tls) {
        tl_log_line("load: %s uses TLS, which the attempt does not implement "
                    "-- refusing", name);
        return false;
    }

    /* The scanner's plan is the loader's layout. */
    tl_segment segs[16];
    for (int i = 0; i < nloads; i++) {
        const uint8_t *ph = (const uint8_t *)loads[i];
        segs[i].vaddr = ld64(ph + 16);
        segs[i].memsz = ld64(ph + 40);
        segs[i].flags = ld32(ph + 4);
    }
    uint64_t base_vaddr = 0;
    size_t npages = tl_page_plan(segs, (size_t)nloads,
                                 has_relro ? &relro : NULL, TL_PAGE, NULL, 0,
                                 &base_vaddr);
    if (npages == 0 || npages > 16384) {   /* 256 MiB is past an attempt */
        tl_log_line("load: %s does not fit the attempt's page budget", name);
        return false;
    }
    uint8_t *flags = malloc(npages);
    if (!flags) {
        tl_log_line("load: out of memory for the page plan");
        return false;
    }
    npages = tl_page_plan(segs, (size_t)nloads, has_relro ? &relro : NULL,
                          TL_PAGE, flags, npages, &base_vaddr);
    if (npages == 0) {
        tl_log_line("load: %s has no usable page plan", name);
        free(flags);
        return false;
    }

    /* Allocate executable memory: try StikDebug prewarmed dual mapping first
     * (required on TXM devices), falling back to plain MAP_JIT. */
    tl_dual_mapping *stik = tl_find_stikdebug_prewarmed();
    uint8_t *base = NULL;
    uint8_t *base_rw = NULL;
    bool is_stikdebug = false;

    if (stik && stik->rw_addr && stik->rx_addr) {
        size_t need = npages * TL_PAGE;
        if (g_prewarmed_used + need <= stik->size && tl_jit_reserve_bottom(g_prewarmed_used + need)) {
            base = stik->rx_addr + g_prewarmed_used;
            base_rw = stik->rw_addr + g_prewarmed_used;
            g_prewarmed_used += need;
            is_stikdebug = true;
            tl_log_line("jit: using StikDebug dual mapping (rx=%p rw=%p, %zu KiB)",
                        base, base_rw, need / 1024);
        }
    }

    if (!base) {
        jit_init();
        base = mmap(NULL, npages * TL_PAGE,
                    PROT_READ | PROT_WRITE | PROT_EXEC,
                    MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
        if (base == MAP_FAILED) {
            tl_log_line("load: JIT allocation failed (MAP_JIT %s, StikDebug %s); "
                        "executable memory is the one thing this cannot run without",
                        strerror(errno), stik ? "exhausted" : "not answering");
            free(flags);
            return false;
        }
        base_rw = base;
        jit_writable(true);
    }

    size_t carved = 0;
    if (!is_stikdebug) {
        for (size_t i = 0; i < npages; i++) {
            if (flags[i] & TL_PAGE_W) {
                void *target = base + i * TL_PAGE;
                void *r = mmap(target, TL_PAGE, PROT_READ | PROT_WRITE,
                               MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
                if (r != MAP_FAILED) {
                    flags[i] |= TL_PAGE_CARVED;
                    carved++;
                } else {
                    tl_log_line("load: carving page %zu failed (%s)", i, strerror(errno));
                    munmap(base, npages * TL_PAGE);
                    free(flags);
                    return false;
                }
            }
        }
        if (carved > 0) {
            tl_log_line("load: carved %zu writable page(s) beside executable memory", carved);
        }
    } else {
        size_t nwrite = 0;
        for (size_t i = 0; i < npages; i++) {
            if (flags[i] & TL_PAGE_W) nwrite++;
        }
        tl_log_line("load: using StikDebug dual-mapped RW alias for %zu writable page(s) (no carving needed)", nwrite);
    }

    /* Copy the segments in. Executable and dual-mapped segments copy through base_rw;
     * carved writable segments copy through base. */
    bool ok = true;
    jit_writable(true);
    for (int i = 0; i < nloads && ok; i++) {
        const uint8_t *ph = (const uint8_t *)loads[i];
        uint64_t v = ld64(ph + 16), fo = ld64(ph + 8);
        uint64_t fs = ld64(ph + 32), ms = ld64(ph + 40);
        if (fo > flen || fs > flen - fo) {
            tl_log_line("load: %s segment %d hangs off the file", name, i);
            ok = false;
            break;
        }
        uint64_t seg_off = v - base_vaddr;
        size_t start_page = (size_t)(seg_off / TL_PAGE);
        uint8_t *dst = (flags[start_page] & TL_PAGE_CARVED)
                     ? (base + seg_off)
                     : (base_rw + seg_off);
        memcpy(dst, file + fo, fs);
        if (ms > fs) {
            memset(dst + fs, 0, (size_t)(ms - fs));
        }
    }
    if (!ok) {
        unmap_lib(&(tl_lib){ .base = base, .npages = npages, .page_flags = flags, .is_stikdebug = is_stikdebug });
        return false;
    }
    patch_guest_code(base_rw, base, npages * TL_PAGE, flags, npages,
                     is_stikdebug, (ptrdiff_t)(base_rw - base));
    jit_icache(base, npages * TL_PAGE);

    ldyn d;
    if (!lex_dynamic(&e, &d)) {
        tl_log_line("load: %s has an unreadable dynamic table", name);
        unmap_lib(&(tl_lib){ .base = base, .npages = npages, .page_flags = flags, .is_stikdebug = is_stikdebug });
        return false;
    }
    if (d.textrel) {
        tl_log_line("load: %s needs TEXTREL (code patched in place), which "
                    "W^X pages forbid -- refusing", name);
        unmap_lib(&(tl_lib){ .base = base, .npages = npages, .page_flags = flags, .is_stikdebug = is_stikdebug });
        return false;
    }

    /* Symbol and string tables, read FILE-relative (their bytes are not
     * in the mapped image unless the library is fully file-backed, which
     * the carve layout does not guarantee). */
    uint64_t sym_off = 0, str_off = 0;
    if (!d.symtab || !d.strtab || !lex_seg(&e, d.strtab, d.strsz, &str_off)) {
        tl_log_line("load: %s has an unreadable string table", name);
        unmap_lib(&(tl_lib){ .base = base, .npages = npages, .page_flags = flags, .is_stikdebug = is_stikdebug });
        return false;
    }
    if (!lex_seg(&e, d.symtab, sizeof(tl_sym), &sym_off)) {
        sym_off = 0;
    }

    tl_lib *L = &g_run.libs[g_run.nlibs];
    memset(L, 0, sizeof(*L));
    snprintf(L->name, sizeof(L->name), "%s", name);
    L->file = NULL;
    L->base = base;
    L->base_rw = base_rw;
    L->is_stikdebug = is_stikdebug;
    L->npages = npages;
    L->page_flags = flags;
    L->base_vaddr = base_vaddr;
    L->symtab = (const tl_sym *)(file + sym_off);
    L->syment = d.syment ? d.syment : sizeof(tl_sym);
    L->strtab = file + str_off;
    L->strsz = d.strsz;
    L->gnu_hash = d.gnu_hash ? 0 : 0;      /* fixed up below, file-relative */
    L->init = d.init;
    L->fini = d.fini;
    L->init_array = d.init_array;
    L->init_arraysz = d.init_arraysz;

    /* gnu_hash is read through the mapped image at run time, but it lives
     * in read-only data, which is file-backed in the image. Map lookups
     * read it through L->base, so store the IMAGE address of the table. */
    {
        uint64_t gho = 0;
        if (d.gnu_hash && lex_seg(&e, d.gnu_hash, 64, &gho)) {
            /* The table's size is not in the file; it is bounded by the
             * strtab address. Store the image pointer only if the bytes
             * are readable in the image: read-only pages are mapped
             * file-backed, so compare against the image layout. */
            L->gnu_hash = (uint64_t)(base + (d.gnu_hash - base_vaddr));
        } else {
            L->gnu_hash = 0;
        }
    }

    g_run.nlibs++;
    tl_log_line("load: %s at %p, %zu pages (%zu carved as data), bias 0x%llx",
                name, base, npages, carved, (unsigned long long)base_vaddr);

    if (!apply_relocations(L, &e, &d)) {
        tl_log_line("load: %s failed relocation", name);
        g_run.nlibs--;
        unmap_lib(L);
        memset(L, 0, sizeof(*L));
        return false;
    }

    /* Dependencies: log the gaps. The attempt's helper set is the APK's
     * own libraries; anything else is a named gap in the report. */
    for (int i = 0; i < d.nneeded; i++) {
        const char *need = ldyn_str(&e, &d, d.needed[i]);
        bool have = false;
        for (int j = 0; j < g_run.nlibs; j++) {
            if (!strcmp(g_run.libs[j].name, need)) {
                have = true;
                break;
            }
        }
        if (!have && !tl_shim_supplies(need)) {
            tl_log_line("dep: %s needs %s, which is not in the attempt's set "
                        "-- its imports will resolve only if the shim covers them",
                        name, need);
        }
    }

    /* Initialisers: these EXECUTE guest code, so the toggle flips to
     * executable for them, on this thread, and back after. Constructors
     * write data -- and sometimes code, for layout transitions -- which
     * is why the executable window is per-thread and temporary. */
    jit_writable(false);
    if (L->init) {
        void (*fn)(void) = (void (*)(void))(base + (L->init - base_vaddr));
        tl_log_line("init: %s calling DT_INIT %p", name, (void *)fn);
        fn();
    }
    if (L->init_array && L->init_arraysz) {
        size_t n = (size_t)(L->init_arraysz / 8);
        for (size_t i = 0; i < n; i++) {
            uint64_t slot = L->init_array + i * 8;
            if (slot >= base_vaddr && slot - base_vaddr < npages * TL_PAGE) {
                uint64_t offset = slot - base_vaddr;
                size_t p = (size_t)(offset / TL_PAGE);
                uint8_t *src = (p < L->npages && (L->page_flags[p] & TL_PAGE_CARVED))
                             ? (base + offset)
                             : (base_rw ? base_rw + offset : base + offset);
                uint64_t fn_addr = *(uint64_t *)src;
                if (fn_addr) {
                    void (*fn)(void) = (void (*)(void))fn_addr;
                    tl_log_line("init: %s calling init_array[%zu] %p", name, i, (void *)fn);
                    fn();
                }
            }
        }
    }
    jit_icache(base, npages * TL_PAGE);
    jit_writable(true);
    tl_log_line("init: %s constructors ran", name);
    return true;
}

/* -------------------------------------------------------- the attempt  */

static int pick_primary(char names[][96], int n)
{
    for (int i = 0; i < n; i++) {
        if (!strcmp(names[i], "libmain.so") || !strcmp(names[i], "libgame.so")
            || strstr(names[i], "activity") || strstr(names[i], "native")) {
            return i;
        }
    }
    return n ? 0 : -1;
}

static void attempt_stop(int code)
{
    pthread_mutex_lock(&g_log_mutex);
    g_run.exit_code = code;
    g_run.finished = true;
    pthread_mutex_unlock(&g_log_mutex);
}

/* The lifecycle driver. Budget-limited, stop-flag aware, and every guest
 * callback is reached through the activity struct it was handed. Input
 * reaches the guest through the AInputQueue it was given, not through
 * the activity callbacks, which carry no input hook in the NDK's ABI. */
static void *attempt_thread(void *arg)
{
    int seconds = (int)(intptr_t)arg;

    /* The W^X toggle is per thread: this is the thread that runs guest
     * code, so this is the thread that switches the region to
     * executable. It stays executable for the whole run; nothing here
     * writes into the images while the guest is live. */
    jit_init();
    jit_writable(false);

    if (seconds > 0) tl_log_line("=== lifecycle begins, %d second budget ===", seconds);
    else tl_log_line("=== lifecycle begins, no time limit ===");

    /* The entry point, on the thread that owns the toggle:
     * void ANativeActivity_onCreate(ANativeActivity*, void* savedState,
     *                               size_t savedStateSize). */
    if (g_run.on_create) {
        g_run.on_create(&g_run.activity, NULL, 0);
        if (!g_run.activity.callbacks->onNativeWindowCreated
            && !g_run.activity.callbacks->onSurfaceChanged) {
            tl_log_line("attempt: onCreate ran but registered no window "
                        "callbacks -- nothing will draw through them");
        }
    }

    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC, &start);
    int budget_ms = seconds * 1000;

    if (g_run.dex_ctx) {
        /*
         * The frame pump: sixty frames a second, on a deadline.
         *
         * It used to draw a frame and then sleep a full sixteen milliseconds,
         * which puts the sleep on top of the work: a frame that cost five
         * milliseconds took twenty-one, and the game's clock still advanced by a
         * fixed sixteen per frame, so the game ran at three quarters of its
         * speed and looked slow. Here each frame has a deadline -- the previous
         * one plus a sixtieth of a second -- and the pump sleeps only for what is
         * left of it, so the work is absorbed instead of added.
         *
         * The clock the app sees is real time, so physics that integrate over a
         * frame's duration are right at any frame rate. Real time is capped at
         * fifty milliseconds a frame: after a stall -- the app was backgrounded,
         * or the device hitched -- the game's clock steps rather than leaps, and
         * a bird does not cross the screen in one tick.
         *
         * A pump that has fallen far behind resynchronises instead of running
         * frames back to back to catch up: sprinting to recover only makes the
         * next stall longer.
         */
        tl_log_line("dex: starting 60 FPS frame pump loop (deadline paced, real time)");
        tl_dex_context *dx = g_run.dex_ctx;

        const uint64_t frame_ns = 16666667ull;
        const uint64_t max_step_ns = 50ull * 1000000ull;

        tl_pacer pacer;
        tl_pacer_start(&pacer, frame_ns);

        uint64_t game_ns = 1000000000ull;           /* where the app's clock starts */
        uint64_t last = tl_dex_now_ns();

        /* The last window's worth of counters, for the once-in-two-seconds line. */
        uint64_t win_start = last;
        int win_frames = 0, win_late = 0;
        struct tl_dex_perf p0 = dx->perf;

        while (!g_run.stop_requested) {
            struct timespec now_ts;
            clock_gettime(CLOCK_MONOTONIC, &now_ts);
            long elapsed = (now_ts.tv_sec - start.tv_sec) * 1000
                         + (now_ts.tv_nsec - start.tv_nsec) / 1000000;
            if (seconds > 0 && elapsed > budget_ms) {
                tl_log_line("run: %d second budget reached after %llu frame(s)",
                            seconds, (unsigned long long)g_run.frames);
                break;
            }

            uint64_t now = tl_dex_now_ns();
            uint64_t step = now - last;
            last = now;
            if (step > max_step_ns) step = max_step_ns;
            game_ns += step;

            tl_dex_tick_frame(dx, game_ns);
            tl_loader_frame_posted();
            win_frames++;

            /* Sleep for what is left of this frame's time. */
            if (tl_pacer_wait(&pacer)) win_late++;
            uint64_t after = tl_dex_now_ns();

            if (after - win_start >= 2000000000ull && win_frames > 0) {
                double secs = (double)(after - win_start) / 1e9;
                double n = (double)win_frames;
                husk_tl_perf pf;
                pf.fps       = n / secs;
                pf.logic_ms  = (double)(dx->perf.ns_logic  - p0.ns_logic)  / n / 1e6;
                pf.render_ms = (double)(dx->perf.ns_render - p0.ns_render) / n / 1e6;
                pf.draw_ms   = (double)(dx->perf.ns_draw   - p0.ns_draw)   / n / 1e6;
                pf.draws     = (double)(dx->perf.draws     - p0.draws)     / n;
                pf.late_pct  = 100.0 * win_late / n;
                pthread_mutex_lock(&g_slot_lock);
                g_perf = pf;
                pthread_mutex_unlock(&g_slot_lock);
                tl_log_line("perf: %.1f fps | logic %.3f ms, render %.3f ms (draw %.3f ms, %.0f calls) | %.0f%% late%s",
                            pf.fps, pf.logic_ms, pf.render_ms, pf.draw_ms, pf.draws, pf.late_pct,
                            g_dropped ? " | frames dropped: no free slot" : "");
                win_start = after;
                win_frames = win_late = 0;
                p0 = dx->perf;
            }
        }
        int code = g_run.frames > 0 ? 2 : 1;
        tl_log_line("=== attempt ended: %s (%llu frames) ===",
                    code == 2 ? "the guest drew" : "loaded but drew nothing",
                    (unsigned long long)g_run.frames);
        attempt_stop(code);
        return NULL;
    }

    /* Lifecycle, as the NDK's native_app_glue would drive it: */
    if (g_run.activity.callbacks->onStart) {
        g_run.activity.callbacks->onStart(&g_run.activity);
    }
    if (g_run.activity.callbacks->onResume) {
        g_run.activity.callbacks->onResume(&g_run.activity);
    }
    if (g_run.activity.callbacks->onNativeWindowCreated) {
        g_run.activity.callbacks->onNativeWindowCreated(&g_run.activity, g_run.window);
    }
    if (g_run.activity.callbacks->onWindowFocusChanged) {
        g_run.activity.callbacks->onWindowFocusChanged(&g_run.activity, 1);
    }
    if (g_run.activity.callbacks->onInputQueueCreated) {
        g_run.activity.callbacks->onInputQueueCreated(&g_run.activity, g_run.queue);
    }
    if (g_run.activity.callbacks->onSurfaceChanged) {
        g_run.activity.callbacks->onSurfaceChanged(&g_run.activity, g_run.window, 1);
    }

    /* The run loop. It watches the budget and the stop flag; a guest that
     * renders does so from its own threads, into the window. */
    while (!g_run.stop_requested) {
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000
                     + (now.tv_nsec - start.tv_nsec) / 1000000;
        if (seconds > 0 && elapsed > budget_ms) {
            tl_log_line("run: %d second budget reached after %llu frame(s)",
                        seconds, (unsigned long long)g_run.frames);
            break;
        }
        struct timespec tick = { 0, 100 * 1000000 };
        nanosleep(&tick, NULL);
    }

    if (g_run.activity.callbacks->onSurfaceDestroyed) {
        g_run.activity.callbacks->onSurfaceDestroyed(&g_run.activity);
    }
    if (g_run.activity.callbacks->onNativeWindowDestroyed) {
        g_run.activity.callbacks->onNativeWindowDestroyed(&g_run.activity, g_run.window);
    }
    if (g_run.activity.callbacks->onPause) {
        g_run.activity.callbacks->onPause(&g_run.activity);
    }
    if (g_run.activity.callbacks->onStop) {
        g_run.activity.callbacks->onStop(&g_run.activity);
    }
    if (g_run.activity.callbacks->onDestroy) {
        g_run.activity.callbacks->onDestroy(&g_run.activity);
    }

    int code = g_run.frames > 0 ? 2 : 1;
    tl_log_line("=== attempt ended: %s ===",
                code == 2 ? "the guest drew" : "loaded but drew nothing");
    attempt_stop(code);
    return NULL;
}

int husk_tl_attempt_start(const char *const *apks, int count, int seconds)
{
    pthread_mutex_lock(&g_log_mutex);
    if (g_run.active && !g_run.finished) {
        pthread_mutex_unlock(&g_log_mutex);
        return -1;                    /* one attempt at a time */
    }
    pthread_mutex_unlock(&g_log_mutex);

    /* Tear the previous run down completely, then start clean. */
    husk_tl_attempt_reset();

    pthread_mutex_lock(&g_log_mutex);
    g_run.active = true;
    pthread_mutex_unlock(&g_log_mutex);

    /* Everything below runs on this thread: loading is synchronous, the
     * lifecycle gets its own thread so the runner returns to Swift. */
    tl_log_line("=== attempt begins: %d APK(s) ===", count);

    char names[24][96], apk_of[24][512];
    char err[192] = {0};
    int n = 0;
    if (!list_arm64_libs(apks, count, names, apk_of, &n, err, sizeof(err))) {
        tl_log_line("attempt: %s", err);
        attempt_stop(0);
        return -1;
    }
    if (n == 0) {
        tl_log_line("attempt: no arm64 libraries found -- entering Dalvik / DEX execution (Milestone 2)");
        goto start_dex;
    }
    for (int i = 0; i < n; i++) {
        tl_log_line("  lib: %s", names[i]);
    }

    int primary = pick_primary(names, n);
    if (primary < 0) {
        attempt_stop(0);
        return 0;
    }
    tl_log_line("attempt: primary library is %s", names[primary]);

    /* Load order: the primary first, then its helpers, so the primary's
     * relocations see the helpers' exports. */
    uint8_t *bytes = NULL; size_t blen = 0;
    if (!extract_lib(apk_of[primary], names[primary], &bytes, &blen,
                     err, sizeof(err))) {
        tl_log_line("attempt: %s", err);
        attempt_stop(0);
        return -1;
    }
    if (!load_library(names[primary], bytes, blen)) {
        free(bytes);
        tl_log_line("attempt: the primary library could not be loaded");
        attempt_stop(0);
        return 0;
    }
    g_run.libs[g_run.nlibs - 1].file = bytes;
    g_run.libs[g_run.nlibs - 1].file_len = blen;

    for (int i = 0; i < n && g_run.nlibs < 8; i++) {
        if (i == primary) {
            continue;
        }
        uint8_t *b = NULL; size_t l = 0;
        if (extract_lib(apk_of[i], names[i], &b, &l, err, sizeof(err))) {
            if (load_library(names[i], b, l)) {
                g_run.libs[g_run.nlibs - 1].file = b;
                g_run.libs[g_run.nlibs - 1].file_len = l;
            } else {
                free(b);
                tl_log_line("attempt: helper %s did not load (continuing)",
                            names[i]);
            }
        }
    }

    /* Run JNI_OnLoad for any loaded library that exports it. */
    for (int i = 0; i < g_run.nlibs; i++) {
        int (*on_load)(void *, void *) = (int (*)(void *, void *))lib_lookup(&g_run.libs[i], "JNI_OnLoad");
        if (on_load) {
            tl_log_line("jni: %s exports JNI_OnLoad at %p -- calling it", g_run.libs[i].name, on_load);
            jit_writable(false);
            int version = on_load(tl_shim_vm(), NULL);
            jit_writable(true);
            tl_log_line("jni: %s JNI_OnLoad returned 0x%x", g_run.libs[i].name, version);
        }
    }

    /* The entry point. */
    bool weak = false;
    void *onCreate = resolve("ANativeActivity_onCreate", &weak);
    if (!onCreate) {
        tl_log_line("attempt: no ANativeActivity_onCreate in %s -- entering Dalvik / DEX execution (Milestone 2)",
                    g_run.libs[0].name);
        goto start_dex;
    }
    tl_log_line("attempt: ANativeActivity_onCreate at %p", onCreate);
    g_run.on_create = (void (*)(void *, void *, size_t))onCreate;
    g_run.window = tl_window_create(540, 960);
    if (!g_run.window) {
        attempt_stop(0);
        return -1;
    }
    pthread_mutex_init(&g_run.frame_mutex, NULL);
    g_run.queue = tl_shim_new_input_queue();
    g_run.assets = tl_shim_new_asset_manager();
    g_run.stop_requested = false;
    memset(&g_run.callbacks, 0, sizeof(g_run.callbacks));
    char data_path[1024];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(data_path, sizeof(data_path), "%s/Documents", home);
    } else {
        snprintf(data_path, sizeof(data_path), "/tmp");
    }
    g_run.activity = (tl_activity){
        .callbacks = &g_run.callbacks,
        .vm = tl_shim_vm(),
        .env = tl_shim_env(),
        .clazz = tl_shim_activity_class(),
        .internalDataPath = data_path,
        .externalDataPath = data_path,
        .sdkVersion = 34,
        .instance = NULL,
        .assetManager = g_run.assets,
        .obbPath = NULL,
    };
    tl_shim_bind_run(&g_run.activity, g_run.window);

    if (pthread_create(&g_run.thread, NULL, attempt_thread,
                       (void *)(intptr_t)seconds) != 0) {
        tl_log_line("attempt: could not start the lifecycle thread");
        attempt_stop(1);
        return 0;
    }
    pthread_detach(g_run.thread);
    return 0;

start_dex:
    tl_log_line("dex: initializing Dalvik runtime for %s", apks[0]);
    g_run.window = tl_window_create(540, 960);
    if (!g_run.window) {
        attempt_stop(0);
        return -1;
    }
    pthread_mutex_init(&g_run.frame_mutex, NULL);
    g_run.stop_requested = false;

    g_run.dex_ctx = tl_dex_context_create(apks[0], (uint32_t *)g_run.window->bits, 540, 960);
    if (!g_run.dex_ctx) {
        tl_log_line("dex: failed to create DEX context");
        attempt_stop(0);
        return 0;
    }

    /* The app's settings and best score live next to its APK, so they are still
     * there the next time it is opened. */
    {
        char prefs_path[1100];
        snprintf(prefs_path, sizeof(prefs_path), "%s.prefs", apks[0]);
        tl_framework_attach_prefs(g_run.dex_ctx, prefs_path);
    }

    if (!tl_dex_load_apk(g_run.dex_ctx, apks[0])) {
        tl_log_line("dex: failed to load DEX files from APK");
        attempt_stop(0);
        return 0;
    }
    tl_log_line("dex: loaded %d classes from APK", g_run.dex_ctx->num_classes);

    /* Look for Flappy Bird game view class 'c' */
    tl_dex_class *c_class = tl_dex_find_class(g_run.dex_ctx, "Lcom/flappybird/recreation/c;");
    if (c_class) {
        tl_log_line("dex: found game view class %s", c_class->descriptor);
        tl_dex_object *view_obj = tl_dex_alloc_object(c_class);
        g_run.dex_ctx->current_view = view_obj;

        tl_dex_method *init_m = tl_dex_find_method(c_class, "<init>", "VL");
        if (init_m) {
            tl_log_line("dex: calling c.<init>(Context)...");
            tl_dex_val args[2];
            args[0].l = view_obj;
            args[1].l = g_run.dex_ctx->current_activity;
            tl_dex_invoke(g_run.dex_ctx, init_m, args, 2, NULL);
        }

        tl_dex_field *fi = tl_dex_find_field(c_class, "i", "I");
        tl_dex_field *fj = tl_dex_find_field(c_class, "j", "I");
        if (fi && view_obj->fields) view_obj->fields[fi->slot].i = 0;
        if (fj && view_obj->fields) view_obj->fields[fj->slot].i = 0;

        tl_dex_method *size_m = tl_dex_find_method(c_class, "onSizeChanged", "VIIII");
        if (size_m) {
            tl_log_line("dex: calling c.onSizeChanged(540, 960, 0, 0)...");
            tl_dex_val args[5];
            args[0].l = view_obj;
            args[1].i = 540;
            args[2].i = 960;
            args[3].i = 0;
            args[4].i = 0;
            tl_dex_invoke(g_run.dex_ctx, size_m, args, 5, NULL);
        }

        tl_dex_method *start_m = tl_dex_find_method(c_class, "b", NULL);
        if (start_m) {
            tl_log_line("dex: calling c.b() to start game loop...");
            tl_dex_val args[1];
            args[0].l = view_obj;
            tl_dex_invoke(g_run.dex_ctx, start_m, args, 1, NULL);
        }

        tl_loader_frame_posted();
        tl_log_line("dex: initial frame rendered and posted");
    } else {
        tl_log_line("dex: no supported game view class found in APK");
        attempt_stop(0);
        return 0;
    }

    if (pthread_create(&g_run.thread, NULL, attempt_thread,
                       (void *)(intptr_t)seconds) != 0) {
        tl_log_line("attempt: could not start the lifecycle thread");
        attempt_stop(1);
        return 0;
    }
    pthread_detach(g_run.thread);
    return 0;
}

bool husk_tl_attempt_done(int *exit_code)
{
    pthread_mutex_lock(&g_log_mutex);
    bool done = g_run.finished;
    if (exit_code) {
        *exit_code = g_run.exit_code;
    }
    pthread_mutex_unlock(&g_log_mutex);
    return done;
}

char *husk_tl_attempt_log(void)
{
    pthread_mutex_lock(&g_log_mutex);
    char *out = NULL;
    if (!g_run.log.failed && g_run.log.buf) {
        out = malloc(g_run.log.len + 1);
        if (out) {
            memcpy(out, g_run.log.buf, g_run.log.len + 1);
        }
    } else {
        out = strdup(g_run.log.failed ? "(log lost: out of memory)\n" : "");
    }
    pthread_mutex_unlock(&g_log_mutex);
    return out;
}

int husk_tl_attempt_frames(void)
{
    pthread_mutex_lock(&g_run.frame_mutex);
    int n = (int)g_run.frames;
    pthread_mutex_unlock(&g_run.frame_mutex);
    return n;
}

void husk_tl_attempt_reset(void)
{
    pthread_mutex_lock(&g_log_mutex);
    bool was_running = g_run.active && !g_run.finished;
    pthread_mutex_unlock(&g_log_mutex);
    if (was_running) {
        tl_loader_request_stop();
        /* Give the lifecycle thread a moment to notice. */
        for (int i = 0; i < 20 && !g_run.finished; i++) {
            struct timespec t = { 0, 50 * 1000000 };
            nanosleep(&t, NULL);
        }
    }

    if (g_run.dex_ctx) {
        tl_dex_context_destroy(g_run.dex_ctx);
        g_run.dex_ctx = NULL;
    }

    for (int i = 0; i < g_run.nlibs; i++) {
        unmap_lib(&g_run.libs[i]);
        free(g_run.libs[i].file);
    }
    g_prewarmed_used = 0;
    tl_window_release(g_run.window);
    tl_shim_free(g_run.queue);
    tl_shim_free(g_run.assets);
    slots_invalidate();

    pthread_mutex_lock(&g_log_mutex);
    char *buf = g_run.log.buf;
    size_t len = g_run.log.len, cap = g_run.log.cap;
    bool failed = g_run.log.failed;
    pthread_mutex_unlock(&g_log_mutex);

    tl_run fresh = {0};
    fresh.log.buf = buf;
    fresh.log.len = len;
    fresh.log.cap = cap;
    fresh.log.failed = failed;
    pthread_mutex_lock(&g_log_mutex);
    g_run = fresh;
    pthread_mutex_unlock(&g_log_mutex);
    /* The struct copy invalidated the mutexes; rebuild them. */
    pthread_mutex_init(&g_run.frame_mutex, NULL);
}

void husk_tl_attempt_stop(void)
{
    tl_loader_request_stop();
    tl_sound_stop_all();
}

void husk_tl_send_touch(int action, float x, float y)
{
    if (g_run.dex_ctx) {
        tl_dex_send_touch(g_run.dex_ctx, action, x, y);
    }
}
