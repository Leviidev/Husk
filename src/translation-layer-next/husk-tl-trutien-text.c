/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-trutien-text.h"
#include "husk-tl-text-color.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

void tl_log_line(const char *fmt, ...);
static atomic_bool text_enabled;
void tl_trutien_text_set_package(const char *package_name)
{
    atomic_store(&text_enabled, package_name && !strcmp(package_name, "vn.trutien.client"));
}
bool tl_trutien_text_active(void) { return atomic_load(&text_enabled); }

void tl_trutien_text_probe(tl_lib *lib)
{
    const char *name = tl_ld_lib_name(lib);
    if (!atomic_load(&text_enabled) || !name || strcmp(name, "libmain.so")) return;
    /* These fixed-argument AArch64 exports use 32-bit wchar_t. GetTag reads
     * the existing control registry and calls const IsMyType; neither draws
     * text nor mutates it. The root position includes the caret; the leaf
     * color recognizer starts after it. No offsets,
     * code writes, guessed object layout beyond the root, or user text. */
    void *printer = tl_ld_sym(lib, "_ZN18AUIRichTextPrinter14ms_TextPrinterE");
    void *(*get_tag)(const void *, const uint32_t *, int, int) =
        tl_ld_sym(lib, "_ZNK18AUIRichTextPrinter6GetTagEPKwii");
    bool (*is_color)(const void *, const uint32_t *, int, int) =
        tl_ld_sym(lib, "_ZNK19AUITextTagCtrlColor8IsMyTypeEPKwii");
    if (!printer || !get_tag || !is_color) return;
    void *root = NULL;
    memcpy(&root, printer, sizeof(root));
    if (!root) {
        tl_log_line("trutien-text: color registry not initialized after native constructors");
        return;
    }
    static const uint32_t upper[] = { '^','F','F','C','B','4','A','T','e','s','t',0 };
    static const uint32_t lower[] = { '^','f','f','c','b','4','a','T','e','s','t',0 };
    static const uint32_t invalid[] = { '^','G','G','G','G','G','G',0 };
    void *tag = get_tag(printer, upper, 0, 11);
    bool upper_ok = tag && is_color(tag, upper, 1, 11);
    bool lower_ok = tag && is_color(tag, lower, 1, 11);
    bool short_rejected = tag && !is_color(tag, upper, 1, 6);
    bool invalid_rejected = tag && !is_color(tag, invalid, 1, 7);
    tl_log_line("trutien-text: native color registry probe uppercase=%d lowercase=%d short-rejected=%d invalid-rejected=%d (read-only)",
                upper_ok, lower_ok, short_rejected, invalid_rejected);
}

/* Keep the final font/HUD boundary free of leaked caret color controls. The
 * user explicitly requested that these controls not appear as text. A normal
 * color token has six hex digits; four/five-digit fragments seen in labels
 * are also hidden. Short mathematical carets and all nonhex text survive.
 * An escaped caret immediately preceding a leaked token is hidden with it.
 * This acts at verified native draw/layout boundaries. The Java graphics
 * bridge shares the same grammar; source strings and stored data stay intact. */
#define TEXT_LIMIT 65536u
#define TEXT_SCRATCH_SLOTS 64u
typedef struct {
    uint32_t *buffers[TEXT_SCRATCH_SLOTS];
    size_t capacity[TEXT_SCRATCH_SLOTS];
    uintptr_t links[TEXT_SCRATCH_SLOTS];
    bool changed[TEXT_SCRATCH_SLOTS];
    void *stack_count[TEXT_SCRATCH_SLOTS];
    int32_t original_count[TEXT_SCRATCH_SLOTS];
    /* Verified const TAG_OP_IN_PARAMS layout: 0x70 bytes, wchar_t* at +8.
     * AUI list-box rendering retains this clone until its wrapper returns. */
    uint64_t params[TEXT_SCRATCH_SLOTS][14];
    unsigned depth;
} text_scratch;
static pthread_key_t text_key;
static pthread_once_t text_key_once = PTHREAD_ONCE_INIT;
static int text_key_error;
static atomic_uint text_reports;

static void text_scratch_free(void *value)
{
    text_scratch *s = value;
    if (!s) return;
    for (unsigned i = 0; i < TEXT_SCRATCH_SLOTS; i++) free(s->buffers[i]);
    free(s);
}
static void text_key_init(void) { text_key_error = pthread_key_create(&text_key, text_scratch_free); }
static text_scratch *text_scratch_get(void)
{
    pthread_once(&text_key_once, text_key_init);
    if (text_key_error) return NULL;
    text_scratch *s = pthread_getspecific(text_key);
    if (!s) {
        s = calloc(1, sizeof(*s));
        if (!s) return NULL;
        if (pthread_setspecific(text_key, s)) { free(s); return NULL; }
    }
    return s;
}
extern void tl_entry_filter_return(void);
void tl_trutien_text_enter(uint64_t *regs, uint64_t *saved_link, tl_trutien_text_filter filter, void *guest_sp)
{
    if (!atomic_load(&text_enabled)) return;
    text_scratch *s = text_scratch_get();
    if (!s || s->depth == TEXT_SCRATCH_SLOTS) return;
    unsigned slot = s->depth++;
    s->links[slot] = (uintptr_t)*saved_link;
    s->changed[slot] = false;
    s->stack_count[slot] = NULL;
    filter(regs, guest_sp);
    if (s->changed[slot])
        *saved_link = (uintptr_t)tl_entry_filter_return;
    else s->depth--;
}
uintptr_t tl_trutien_text_leave(void)
{
    text_scratch *s = pthread_getspecific(text_key);
    /* Reached only from a substituted LR after a successful enter. */
    if (!s || !s->depth) abort();
    unsigned slot = --s->depth;
    if (s->stack_count[slot])
        memcpy(s->stack_count[slot], &s->original_count[slot], sizeof(int32_t));
    return s->links[slot];
}
static const uint32_t *text_without_color_controls(const uint32_t *text, int count, int *result_count)
{
    if (!text || !count) return text;
    size_t bound = count < 0 ? TEXT_LIMIT : (size_t)count;
    if (bound > TEXT_LIMIT) return text;
    size_t len = 0;
    bool has_control = false;
    while (len < bound && text[len]) len++;
    /* An unbounded string needs a terminator within the inspection limit. */
    if (count < 0 && len == bound) return text;
    for (size_t i = 0; i < len; i++)
        if (tl_text_color_end32(text, i, len) != i) { has_control = true; break; }
    if (!has_control) return text;
    text_scratch *s = text_scratch_get();
    if (!s) return text;
    unsigned slot = s->depth ? s->depth - 1 : 0;
    if (s->capacity[slot] < len + 1) {
        uint32_t *next = realloc(s->buffers[slot], (len + 1) * sizeof(uint32_t));
        if (!next) return text;
        s->buffers[slot] = next; s->capacity[slot] = len + 1;
    }
    uint32_t *out = s->buffers[slot];
    size_t at = 0, controls = 0;
    for (size_t i = 0; i < len;) {
        size_t end = tl_text_color_end32(text, i, len);
        if (end != i) { i = end; controls++; }
        else out[at++] = text[i++];
    }
    out[at] = 0;
    if (s->depth) s->changed[slot] = true;
    *result_count = (int)at;
    if (atomic_fetch_add(&text_reports, 1) < 4)
        tl_log_line("trutien-text: hidden %zu leaked color control(s) before native draw/measure (%zu -> %zu code points)", controls, len, at);
    return out;
}
static void filter_text_args(uint64_t *regs, unsigned text_reg, int count_reg)
{
    if (!atomic_load(&text_enabled)) return;
    const uint32_t *text = (const uint32_t *)(uintptr_t)regs[text_reg];
    int count = count_reg < 0 ? -1 : (int32_t)regs[count_reg];
    int next_count = count;
    const uint32_t *next = text_without_color_controls(text, count, &next_count);
    if (next == text && next_count == count) return;
    regs[text_reg] = (uint64_t)(uintptr_t)next;
    if (count_reg >= 0) regs[count_reg] = (uint32_t)next_count;
}
void tl_trutien_font_filter(uint64_t *regs, void *guest_sp) { (void)guest_sp; filter_text_args(regs, 3, 7); }
void tl_trutien_extent_filter(uint64_t *regs, void *guest_sp) { (void)guest_sp; filter_text_args(regs, 1, 2); }
void tl_trutien_hud_filter(uint64_t *regs, void *guest_sp) { (void)guest_sp; filter_text_args(regs, 1, -1); }
void tl_trutien_plain_filter(uint64_t *regs, void *guest_sp) { (void)guest_sp; filter_text_args(regs, 3, -1); }
void tl_trutien_float_filter(uint64_t *regs, void *guest_sp) { (void)guest_sp; filter_text_args(regs, 1, 5); }
void tl_trutien_legacy_extent_filter(uint64_t *regs, void *guest_sp) { (void)guest_sp; filter_text_args(regs, 2, 3); }
void tl_trutien_clipped_filter(uint64_t *regs, void *guest_sp)
{
    if (!tl_trutien_text_active() || !guest_sp) return;
    text_scratch *s = text_scratch_get();
    /* A stack edit requires an active return bridge to restore the caller's
     * exact argument bytes after DrawTextW consumes the sanitized count. */
    if (!s || !s->depth) return;
    int32_t count;
    memcpy(&count, guest_sp, sizeof(count));
    int next_count = count;
    const uint32_t *text = (const uint32_t *)(uintptr_t)regs[3];
    const uint32_t *next = text_without_color_controls(text, count, &next_count);
    if (next == text) return;
    unsigned slot = s->depth - 1;
    s->stack_count[slot] = guest_sp;
    s->original_count[slot] = count;
    memcpy(guest_sp, &next_count, sizeof(int32_t));
    regs[3] = (uint64_t)(uintptr_t)next;
}
void tl_trutien_params_filter(uint64_t *regs, void *guest_sp)
{
    (void)guest_sp;
    if (!tl_trutien_text_active() || !regs[0]) return;
    text_scratch *s = text_scratch_get();
    if (!s || !s->depth) return;
    const void *params = (const void *)(uintptr_t)regs[0];
    const uint32_t *text;
    memcpy(&text, (const uint8_t *)params + 8, sizeof(text));
    int count = -1;
    const uint32_t *next = text_without_color_controls(text, count, &count);
    if (next == text) return;
    unsigned slot = s->depth - 1;
    memcpy(s->params[slot], params, sizeof(s->params[slot]));
    memcpy((uint8_t *)s->params[slot] + 8, &next, sizeof(next));
    regs[0] = (uint64_t)(uintptr_t)s->params[slot];
}
