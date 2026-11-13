/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Text for cocos2d-x labels. Cocos2dxBitmap draws a label with Android's Canvas and Paint and hands the
 * pixels back to the engine; this does the same with CoreText and CoreGraphics, following Cocos2dxBitmap's
 * own rules for what size the bitmap is, how a line is aligned, how a long line wraps, and how the
 * shadow and the outline are drawn.
 */
#define _DARWIN_C_SOURCE
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "husk-tl-bionic.h"
#include "husk-tl-cocos.h"
#include "husk-tl-host-device.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"

/* ------------------------------------------------------------------- fonts */

typedef struct { char name[160]; float size; CTFontRef font; } font_entry;
static font_entry g_fonts[64];
static int g_nfonts;
static pthread_mutex_t g_font_mu = PTHREAD_MUTEX_INITIALIZER;

/* A .ttf the APK ships under assets/, as Cocos2dxTypefaces loads it. */
static CTFontRef font_from_apk(const char *path, float size)
{
    char entry[400];
    snprintf(entry, sizeof(entry), "assets/%s", path);
    for (int i = 0;; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) return NULL;
        const tl_zip_entry *e = tl_zip_find(z, entry);
        if (!e) continue;
        const uint8_t *data; size_t n; bool owned; char err[160];
        if (!tl_zip_data(z, e, (size_t)64 << 20, &data, &n, &owned, err, sizeof(err))) return NULL;
        CFDataRef cf = CFDataCreate(NULL, data, (CFIndex)n);      /* copies */
        if (owned) free((void *)data);
        CGDataProviderRef prov = CGDataProviderCreateWithCFData(cf);
        CGFontRef cg = prov ? CGFontCreateWithDataProvider(prov) : NULL;
        CTFontRef f = cg ? CTFontCreateWithGraphicsFont(cg, size, NULL, NULL) : NULL;
        if (cg) CFRelease(cg);
        if (prov) CFRelease(prov);
        CFRelease(cf);
        return f;
    }
}

static bool font_matches(CTFontRef f, const char *name)
{
    bool ok = false;
    CFStringRef want = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    CFStringRef have[3] = { CTFontCopyFullName(f), CTFontCopyFamilyName(f), CTFontCopyPostScriptName(f) };
    for (int i = 0; i < 3; i++) {
        if (have[i] && CFStringCompare(have[i], want, kCFCompareCaseInsensitive) == kCFCompareEqualTo) ok = true;
        if (have[i]) CFRelease(have[i]);
    }
    CFRelease(want);
    return ok;
}

/* Android falls back to its default typeface for a name it does not have; here that is the system font. */
static CTFontRef font_for(const char *name, float size)
{
    pthread_mutex_lock(&g_font_mu);
    for (int i = 0; i < g_nfonts; i++)
        if (g_fonts[i].size == size && !strcmp(g_fonts[i].name, name)) { CTFontRef f = g_fonts[i].font; pthread_mutex_unlock(&g_font_mu); return f; }
    pthread_mutex_unlock(&g_font_mu);

    CTFontRef f = NULL;
    size_t len = strlen(name);
    if (len > 4 && !strcasecmp(name + len - 4, ".ttf")) f = font_from_apk(name, size);
    if (!f && name[0]) {
        CFStringRef n = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
        CTFontRef c = CTFontCreateWithName(n, size, NULL);
        CFRelease(n);
        if (c && font_matches(c, name)) f = c; else if (c) CFRelease(c);
    }
    if (!f) f = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, size, NULL);
    pthread_mutex_lock(&g_font_mu);
    if (g_nfonts < 64) { snprintf(g_fonts[g_nfonts].name, sizeof(g_fonts[0].name), "%s", name); g_fonts[g_nfonts].size = size; g_fonts[g_nfonts].font = f; g_nfonts++; }
    pthread_mutex_unlock(&g_font_mu);
    return f;
}

/* ------------------------------------------------------------------ layout */

static CFAttributedStringRef attributed(const char *utf8, CTFontRef font, CGColorRef color)
{
    CFStringRef s = CFStringCreateWithCString(NULL, utf8, kCFStringEncodingUTF8);
    if (!s) s = CFStringCreateWithCString(NULL, "", kCFStringEncodingUTF8);
    CFMutableAttributedStringRef a = CFAttributedStringCreateMutable(NULL, 0);
    CFAttributedStringReplaceString(a, CFRangeMake(0, 0), s);
    CFRange all = CFRangeMake(0, CFAttributedStringGetLength(a));
    CFAttributedStringSetAttribute(a, all, kCTFontAttributeName, font);
    if (color) CFAttributedStringSetAttribute(a, all, kCTForegroundColorAttributeName, color);
    CFRelease(s);
    return a;
}

static double line_width(const char *utf8, CTFontRef font)
{
    CFAttributedStringRef a = attributed(utf8, font, NULL);
    CTLineRef l = CTLineCreateWithAttributedString(a);
    double w = CTLineGetTypographicBounds(l, NULL, NULL, NULL);
    CFRelease(l); CFRelease(a);
    return ceil(w);
}

typedef struct { char **v; int n, cap; } lines_t;
static void lines_add(lines_t *L, const char *s, size_t n)
{
    if (L->n == L->cap) { L->cap = L->cap ? L->cap * 2 : 8; L->v = realloc(L->v, (size_t)L->cap * sizeof(char *)); }
    L->v[L->n] = strndup(s, n);
    L->n++;
}

/* Wrap one line to `max` pixels at spaces (a word longer than the width is cut where it overflows). */
static void wrap_line(lines_t *L, const char *line, CTFontRef font, int max)
{
    size_t len = strlen(line);
    if (max <= 0 || line_width(line, font) <= max) { lines_add(L, line, len); return; }
    size_t start = 0;
    while (start < len) {
        size_t end = start, last_space = (size_t)-1;
        while (end < len) {
            size_t step = 1;
            while (end + step < len && ((unsigned char)line[end + step] & 0xC0) == 0x80) step++;     /* a whole UTF-8 character */
            char *probe = strndup(line + start, end + step - start);
            double w = line_width(probe, font);
            free(probe);
            if (w > max && end > start) break;
            if (line[end] == ' ') last_space = end;
            end += step;
        }
        if (end >= len) { lines_add(L, line + start, len - start); break; }
        size_t cut = last_space != (size_t)-1 && last_space > start ? last_space : end;
        lines_add(L, line + start, cut - start);
        start = cut;
        while (start < len && line[start] == ' ') start++;
    }
}

/* ----------------------------------------------------------------- the draw */

static void rasterise(const tl_cocos_text_request *r)
{
    /* refactorString: an empty text is one space, and an empty line gets one so that it has a height */
    char *text = malloc(strlen(r->text) * 2 + 2);
    size_t k = 0;
    if (!r->text[0]) text[k++] = ' ';
    for (size_t i = 0; r->text[i]; i++) {
        if (r->text[i] == '\n' && (i == 0 || r->text[i - 1] == '\n')) text[k++] = ' ';
        text[k++] = r->text[i];
    }
    text[k] = 0;

    int hAlign = r->align & 0xF, vAlign = (r->align >> 4) & 0xF;
    CTFontRef font = font_for(r->font, (float)r->size);
    double ascent = ceil(CTFontGetAscent(font)), descent = ceil(CTFontGetDescent(font));
    int lineHeight = (int)(ascent + descent);
    if (lineHeight < 1) lineHeight = 1;

    lines_t L = { 0 };
    for (char *p = text;;) {
        char *nl = strchr(p, '\n');
        size_t n = nl ? (size_t)(nl - p) : strlen(p);
        char *one = strndup(p, n);
        wrap_line(&L, one, font, r->width);
        free(one);
        if (!nl) break;
        p = nl + 1;
    }
    int max_lines = r->height > 0 ? r->height / lineHeight : 0;
    if (max_lines > 0 && L.n > max_lines) { for (int i = max_lines; i < L.n; i++) free(L.v[i]); L.n = max_lines; }

    int text_w = r->width;
    if (text_w == 0) for (int i = 0; i < L.n; i++) { int w = (int)line_width(L.v[i], font); if (w > text_w) text_w = w; }
    int total_h = lineHeight * L.n;
    int box_h = r->height == 0 ? total_h : r->height;
    float dx = r->shadow ? r->shadow_dx : 0, dy = r->shadow ? r->shadow_dy : 0;
    int off_x = dx < 0 ? (int)-dx : 0, off_y = dy < 0 ? (int)-dy : 0;
    int bw = text_w + (r->shadow ? (int)fabsf(dx) : 0), bh = box_h + (r->shadow ? (int)fabsf(dy) : 0);
    if (bw <= 0 || bh <= 0 || bw > 8192 || bh > 8192) goto done;

    CGColorSpaceRef cs = CGColorSpaceCreateWithName(tl_host_display_wide_color() ? kCGColorSpaceDisplayP3 : kCGColorSpaceSRGB);
    if (!cs) cs = CGColorSpaceCreateDeviceRGB();
    uint8_t *px = calloc((size_t)bw * (size_t)bh, 4);
    CGContextRef ctx = CGBitmapContextCreate(px, (size_t)bw, (size_t)bh, 8, (size_t)bw * 4, cs, (CGBitmapInfo)kCGImageAlphaPremultipliedLast);
    if (!ctx) { CGColorSpaceRelease(cs); free(px); goto done; }
    CGContextSetAllowsAntialiasing(ctx, true);
    CGContextSetShouldSmoothFonts(ctx, false);

    /* the baseline of the first line: computeY */
    double y0 = ascent;
    if (box_h > total_h) y0 += vAlign == 1 ? 0 : vAlign == 2 ? (box_h - total_h) : vAlign == 3 ? (box_h - total_h) / 2 : 0;

    CGFloat fill[4] = { r->r, r->g, r->b, 1 };
    CGColorSpaceRef rgb = cs;
    CGColorRef fill_color = CGColorCreate(rgb, fill);
    CGFloat stroke_rgba[4] = { (int)r->stroke_r * 255, (int)r->stroke_g * 255, (int)r->stroke_b * 255, 1 };
    for (int i = 0; i < 3; i++) if (stroke_rgba[i] > 1) stroke_rgba[i] = 1;
    CGColorRef stroke_color = CGColorCreate(rgb, stroke_rgba);

    for (int pass = 0; pass < (r->stroke ? 2 : 1); pass++) {
        if (pass == 0 && r->shadow) {
            CGFloat sc[4] = { 125 / 255.0, 125 / 255.0, 125 / 255.0, 1 };
            CGColorRef shadow_color = CGColorCreate(rgb, sc);
            CGContextSetShadowWithColor(ctx, CGSizeMake(dx, -dy), r->shadow_blur, shadow_color);     /* base space is y-up */
            CGColorRelease(shadow_color);
        } else if (pass == 1) {
            CGContextSetShadowWithColor(ctx, CGSizeZero, 0, NULL);
            CGContextSetTextDrawingMode(ctx, kCGTextStroke);
            CGContextSetLineWidth(ctx, 0.5 * r->stroke_w);
            CGContextSetStrokeColorWithColor(ctx, stroke_color);
        }
        for (int i = 0; i < L.n; i++) {
            CFAttributedStringRef a = attributed(L.v[i], font, pass == 0 ? fill_color : stroke_color);
            CTLineRef line = CTLineCreateWithAttributedString(a);
            double w = CTLineGetTypographicBounds(line, NULL, NULL, NULL);
            double x = hAlign == 2 ? text_w - w : hAlign == 3 ? (text_w - w) / 2 : 0;       /* computeX, as an anchor */
            double base = y0 + (double)i * lineHeight;
            CGContextSetTextPosition(ctx, x + off_x, bh - (base + off_y));
            CTLineDraw(line, ctx);
            CFRelease(line); CFRelease(a);
        }
    }
    CGColorRelease(fill_color); CGColorRelease(stroke_color); CGColorSpaceRelease(rgb);
    CGContextRelease(ctx);
    if (getenv("TL_TEXT_DUMP")) {                      /* diagnostics: every label as a BMP, composited on mid-grey */
        static int n; char path[600];
        snprintf(path, sizeof(path), "%s/text-%03d.bmp", getenv("TL_TEXT_DUMP"), n++);
        FILE *f = fopen(path, "wb");
        if (f) {
            uint32_t rb = ((uint32_t)bw * 3 + 3) & ~3u, size = 54 + rb * (uint32_t)bh, off = 54, dib = 40, img = rb * (uint32_t)bh;
            uint8_t hd[54] = { 'B', 'M' };
            memcpy(hd + 2, &size, 4); memcpy(hd + 10, &off, 4); memcpy(hd + 14, &dib, 4); memcpy(hd + 18, &bw, 4);
            int nh = bh; memcpy(hd + 22, &nh, 4); hd[26] = 1; hd[28] = 24; memcpy(hd + 34, &img, 4);
            fwrite(hd, 1, 54, f);
            uint8_t *row = calloc(1, rb);
            for (int y = bh - 1; y >= 0; y--) {
                for (int x = 0; x < bw; x++) {
                    const uint8_t *q = px + ((size_t)y * (size_t)bw + (size_t)x) * 4;
                    int a = q[3];
                    for (int c = 0; c < 3; c++) row[x * 3 + (2 - c)] = (uint8_t)(q[c] + (128 * (255 - a)) / 255);
                }
                fwrite(row, 1, rb, f);
            }
            free(row); fclose(f);
        }
    }
    tl_cocos_deliver_bitmap(bw, bh, px);
    free(px);
done:
    for (int i = 0; i < L.n; i++) free(L.v[i]);
    free(L.v);
    free(text);
}

void tl_cocos_text_install(void) { tl_cocos_set_text_rasteriser(rasterise); }
