/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-trutien-text.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

void tl_log_line(const char *fmt, ...);

void tl_trutien_text_probe(tl_lib *lib)
{
    const char *name = tl_ld_lib_name(lib);
    if (!name || strcmp(name, "libmain.so")) return;
    /* These fixed-argument AArch64 exports use 32-bit wchar_t. GetTag reads
     * the existing control registry and calls const IsMyType; neither draws
     * text nor mutates it. The position points AFTER the caret. No offsets,
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
    void *tag = get_tag(printer, upper, 1, 11);
    bool upper_ok = tag && is_color(tag, upper, 1, 11);
    bool lower_ok = tag && is_color(tag, lower, 1, 11);
    bool short_rejected = tag && !is_color(tag, upper, 1, 6);
    bool invalid_rejected = tag && !is_color(tag, invalid, 1, 7);
    tl_log_line("trutien-text: native color registry probe uppercase=%d lowercase=%d short-rejected=%d invalid-rejected=%d (read-only)",
                upper_ok, lower_ok, short_rejected, invalid_rejected);
}
