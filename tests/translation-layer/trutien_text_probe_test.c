/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-trutien-text.h"
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *library_name = "other.so";
static bool exports_present, root_present, registered;
static unsigned lookups, queries, colors, messages;
static char message[400];
static void *root, *printer;
const char *tl_ld_lib_name(const tl_lib *lib) { assert(lib); return library_name; }
static void *get_tag(const void *p, const uint32_t *text, int pos, int len)
{
    assert(p == &printer && pos == 0 && len == 11 && text[0] == '^');
    queries++;
    return registered ? &root : NULL;
}
static bool is_color(const void *tag, const uint32_t *text, int pos, int len)
{
    assert(tag == &root && pos == 1); colors++;
    if (pos + 6 > len) return false;
    for (int i = pos; i < pos + 6; i++) {
        uint32_t c = text[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}
void *tl_ld_sym(tl_lib *lib, const char *name)
{
    assert(lib); lookups++;
    if (!exports_present) return NULL;
    if (strstr(name, "ms_TextPrinter")) { printer = root_present ? &root : NULL; return &printer; }
    if (strstr(name, "GetTag")) return get_tag;
    if (strstr(name, "IsMyType")) return is_color;
    assert(!"unexpected export"); return NULL;
}
void tl_log_line(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vsnprintf(message, sizeof(message), fmt, ap); va_end(ap); messages++;
}
void tl_entry_filter_return(void) {}
int main(void)
{
    tl_lib *lib = (tl_lib *)(uintptr_t)1;
    tl_trutien_text_set_package("vn.trutien.client");
    tl_trutien_text_probe(lib); assert(!lookups && !messages);
    library_name = "libmain.so";
    tl_trutien_text_probe(lib); assert(lookups == 3 && !queries && !messages);
    exports_present = true;
    tl_trutien_text_probe(lib); assert(!queries && messages == 1 && strstr(message, "not initialized"));
    root_present = true;
    tl_trutien_text_probe(lib); assert(queries == 1 && !colors && strstr(message, "uppercase=0"));
    registered = true;
    tl_trutien_text_probe(lib);
    assert(queries == 2 && colors == 4 && strstr(message, "uppercase=1 lowercase=1 short-rejected=1 invalid-rejected=1"));
    assert(printer == &root && messages == 3);
    puts("PASS: bounded native color probe skips unrelated/missing exports, reports uninitialized/missing controls, and checks both hex cases and invalid/short tags without writes");
    return 0;
}
