/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Run the actual CoreText input / measurement natives with a deterministic
 * two-pixel glyph backend. Font rasterization itself is checked on iOS/macOS. */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "husk-tl-text-color.h"

typedef ptrdiff_t CFIndex;
typedef struct { void *data; int32_t n; } jobj;
typedef union { jobj *l; int32_t i; int64_t j; float f; bool z; } jvalue;
typedef struct { unsigned refs; uint16_t *text; size_t n; } *CFStringRef;
typedef CFStringRef CTLineRef;
typedef CFStringRef CTFontRef;
typedef CFStringRef CFDictionaryRef;
typedef CFStringRef CFAttributedStringRef;
typedef CFStringRef CTTypesetterRef;
typedef struct { int unused; } gface;
typedef struct { struct { double x, y; } origin; struct { double width, height; } size; } CGRect;
static bool enabled;
static atomic_uint text_color_reports;
static int32_t paint[1] = { 1 };
static const void *kCTFontAttributeName = "font";
static int kCFTypeDictionaryKeyCallBacks, kCFTypeDictionaryValueCallBacks;
enum { P_SCALEX };
#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
#define Ii(v) ((jvalue){ .i = (v) })
#define Ff(v) ((jvalue){ .f = (v) })
static bool tl_trutien_text_active(void) { return enabled; }
static void tl_log_line(const char *format, ...) { (void)format; }
static const uint16_t *tl_dvm_string_chars(jobj *s, int32_t *n) { *n = s->n; return s->data; }
static void *arr_data(jobj *a) { return a->data; }
static uint32_t arr_len(jobj *a) { return (uint32_t)a->n; }
static const int32_t *paint_of(jobj *p) { (void)p; return paint; }
static float pf(const int32_t *p, int index) { return (float)p[index]; }
static CFStringRef CFStringCreateWithCharacters(void *allocator, const uint16_t *text, CFIndex n)
{
    (void)allocator;
    CFStringRef s = calloc(1, sizeof(*s)); assert(s);
    s->refs = 1; s->n = (size_t)n;
    s->text = malloc((s->n + 1) * sizeof(*s->text)); assert(s->text);
    if (n) memcpy(s->text, text, (size_t)n * sizeof(*text));
    s->text[n] = 0;
    return s;
}
static CFStringRef retain(CFStringRef s) { s->refs++; return s; }
static void CFRelease(CFStringRef s) { if (!--s->refs) { free(s->text); free(s); } }
static CTLineRef make_line(CFStringRef s, const int32_t *p, gface *g)
{ (void)p; (void)g; return retain(s); }
static float line_width(CTLineRef line, const int32_t *p) { return (float)line->n * 2 * pf(p, P_SCALEX); }
static CGRect CTLineGetImageBounds(CTLineRef line, void *context)
{ (void)context; return (CGRect){ .size = { (double)line->n * 2, 10 } }; }
static bool CGRectIsNull(CGRect r) { (void)r; return false; }
static bool CGRectIsEmpty(CGRect r) { return !r.size.width || !r.size.height; }
static double CTLineGetOffsetForStringIndex(CTLineRef line, CFIndex at, void *secondary)
{ (void)secondary; assert(at >= 0 && (size_t)at <= line->n); return (double)at * 2; }
static CTFontRef sized(gface *g, float size)
{ (void)g; (void)size; return CFStringCreateWithCharacters(NULL, NULL, 0); }
static CFDictionaryRef CFDictionaryCreate(void *a, const void **k, const void **v, int n, void *kc, void *vc)
{ (void)a; (void)k; (void)v; (void)n; (void)kc; (void)vc; return CFStringCreateWithCharacters(NULL, NULL, 0); }
static CFAttributedStringRef CFAttributedStringCreate(void *a, CFStringRef s, CFDictionaryRef attrs)
{ (void)a; (void)attrs; return retain(s); }
static CTTypesetterRef CTTypesetterCreateWithAttributedString(CFAttributedStringRef s) { return retain(s); }
static CFIndex CTTypesetterSuggestLineBreak(CTTypesetterRef ts, CFIndex start, double w)
{ (void)start; size_t at = w < 0 ? 0 : (size_t)(w / 2); return (CFIndex)(at < ts->n ? at : ts->n); }
static CFIndex CTTypesetterSuggestClusterBreak(CTTypesetterRef ts, CFIndex start, double w)
{ return CTTypesetterSuggestLineBreak(ts, start, w); }
#define P_TEXTSIZE 0
#include "graphics_text_under_test.h"

static void equal(CFStringRef s, const uint16_t *wanted, size_t n)
{ assert(s->n == n && !memcmp(s->text, wanted, n * sizeof(*wanted))); }
int main(void)
{
    const uint16_t source[] = u"^FFFFLogin^112233!";
    const uint16_t wanted[] = u"Login!";
    int32_t n = (int32_t)(sizeof(source) / sizeof(*source) - 1);
    jobj string = { (void *)source, n };
    enabled = false;
    CFStringRef s = cfstr(&string, 0, n); equal(s, source, (size_t)n); CFRelease(s);
    enabled = true;
    s = cfstr(&string, 0, n); equal(s, wanted, 6); CFRelease(s);
    s = cfstr(&string, 2, 5); equal(s, wanted, 0); CFRelease(s);
    s = cfstr(&string, 2, 8); equal(s, wanted, 3); CFRelease(s);
    s = cfstr(&string, n + 10, n + 20); equal(s, wanted, 0); CFRelease(s);
    s = cfstr(NULL, 0, 0); equal(s, wanted, 0); CFRelease(s);

    jvalue a[7] = { { .l = &string }, { .i = 0 }, { .i = n } }, result;
    assert(G_txMeasure(NULL, a, &result) && result.f == 12);
    int32_t rect[4] = { -1, -1, -1, -1 }; jobj rect_array = { rect, 4 };
    a[5].l = &rect_array;
    assert(G_txBounds(NULL, a, &result));
    assert(rect[0] == 0 && rect[1] == -10 && rect[2] == 12 && rect[3] == 0);
    float widths[64]; for (unsigned i = 0; i < 64; i++) widths[i] = -99;
    jobj widths_array = { widths, 64 }; a[5].l = &widths_array;
    assert(G_txWidths(NULL, a, &result));
    for (int32_t i = 0; i < n; i++) assert(widths[i] == ((i >= 5 && i < 10) || i == n - 1 ? 2 : 0));
    assert(widths[n] == -99);
    for (int width = 0; width <= 14; width += 2) {
        a[3].f = (float)width; a[5].j = 0; a[6].z = false;
        assert(G_txBreak(NULL, a, &result));
        int expected = width < 10 ? 5 + width / 2 : width < 12 ? n - 1 : n;
        assert(result.i == expected);
        /* Android returns an original-source count, and remeasuring that
         * range must equal the visible part that the typesetter fitted. */
        jvalue measure[5] = { { .l = &string }, { .i = 0 }, { .i = result.i } }, measured;
        assert(G_txMeasure(NULL, measure, &measured));
        assert(measured.f == (width < 12 ? (float)width : 12));
    }
    /* No caller-owned Java string changes, including its control bytes. */
    assert(!memcmp(source, u"^FFFFLogin^112233!", sizeof(source)));
    puts("PASS: actual graphics CoreText input, measurement, bounds, line-break indices and per-source-unit widths share scoped sanitized Unicode text");
    return 0;
}
