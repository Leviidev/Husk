/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * android.graphics for Java apps on Husk's Dalvik runtime: Bitmap, Canvas, Paint's text, Typeface and the shaders, drawn with
 * CoreGraphics and CoreText (the natives of husk.Gfx; the classes themselves are Java, in src/java-framework).
 *
 * A Bitmap is premultiplied RGBA in memory, rows top first, which is what a CGBitmapContext draws into and what GL uploads as is.
 * A Canvas is a CGContext over a bitmap, its CTM flipped once so that y grows down as Android's does. A Paint is an int[] the Java
 * side keeps current (the P_* slots below), so a draw call reads it in one go instead of calling back into Java.
 *
 * Also here: the frame the view system hands over (husk.Native.present) for the GL thread to put on screen.
 */
#define _DARWIN_C_SOURCE
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "husk-tl-dvm-internal.h"
#include "husk-tl-dvm-javaapp.h"
#include "husk-tl-internal.h"

const char *tl_path_resolve(const char *path, char *buf, size_t n);

/* ================================================================== the Paint packet (android.graphics.Paint.mP) */

enum {
    P_COLOR, P_STYLE, P_STROKE, P_FLAGS, P_CAP, P_JOIN, P_TEXTSIZE, P_ALIGN, P_MITER, P_SHRAD, P_SHDX, P_SHDY, P_SHCOL,
    P_SCALEX, P_SKEWX, P_LSPACE, P_XFER, P_CFCOLOR, P_CFMODE, P_ALPHA, P_COUNT
};
enum { PF_AA = 1, PF_FILTER = 2, PF_FAKEBOLD = 4, PF_UNDERLINE = 8, PF_STRIKE = 16 };

static float pf(const int32_t *p, int i) { union { int32_t i; float f; } u = { p[i] }; return u.f; }
static const int32_t *paint_of(jobj *a)
{
    static const int32_t k_default[P_COUNT] = { (int32_t)0xFF000000, 0, 0, PF_AA, 0, 0, 0x41400000 /* 12.0f */, 0, 0x40800000, 0, 0, 0, 0,
                                                0x3F800000, 0, 0, 3, 0, -1, 255 };
    if (!a || a->kind != TL_K_PRIM_ARRAY || a->arr.len < P_COUNT) return k_default;
    return a->arr.data;
}

/* ================================================================== bitmaps */

typedef struct gbitmap {
    int w, h;
    size_t stride;
    uint8_t *px;
    CGContextRef ctx;               /* drawn into by a Canvas; also how its pixels become a CGImage */
} gbitmap;

static CGColorSpaceRef rgb(void)
{
    static CGColorSpaceRef cs;
    if (!cs) cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    return cs;
}

static gbitmap *bm_new(int w, int h)
{
    if (w <= 0 || h <= 0 || (int64_t)w * h > (1ll << 28)) return NULL;
    gbitmap *b = calloc(1, sizeof(*b));
    b->w = w; b->h = h; b->stride = (size_t)w * 4;
    b->px = calloc((size_t)h, b->stride);
    if (!b->px) { free(b); return NULL; }
    b->ctx = CGBitmapContextCreate(b->px, (size_t)w, (size_t)h, 8, b->stride, rgb(), kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    if (!b->ctx) { free(b->px); free(b); return NULL; }
    return b;
}
static void bm_free(gbitmap *b)
{
    if (!b) return;
    CGContextRelease(b->ctx);
    free(b->px);
    free(b);
}
static CGImageRef bm_image(gbitmap *b) { return CGBitmapContextCreateImage(b->ctx); }

/* ARGB (unpremultiplied, as Android hands colours) to and from the premultiplied RGBA bytes */
static inline uint32_t to_argb(const uint8_t *p)
{
    uint32_t a = p[3];
    if (!a) return 0;
    uint32_t r = p[0], g = p[1], b = p[2];
    if (a != 255) { r = (r * 255 + a / 2) / a; g = (g * 255 + a / 2) / a; b = (b * 255 + a / 2) / a; if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255; }
    return a << 24 | r << 16 | g << 8 | b;
}
static inline void from_argb(uint8_t *p, uint32_t c)
{
    uint32_t a = c >> 24, r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
    if (a != 255) { r = (r * a + 127) / 255; g = (g * a + 127) / 255; b = (b * a + 127) / 255; }
    p[0] = (uint8_t)r; p[1] = (uint8_t)g; p[2] = (uint8_t)b; p[3] = (uint8_t)a;
}

static gbitmap *bm_from_image(CGImageRef img, int sample)
{
    if (sample < 1) sample = 1;
    int w = (int)CGImageGetWidth(img) / sample, h = (int)CGImageGetHeight(img) / sample;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    gbitmap *b = bm_new(w, h);
    if (!b) return NULL;
    CGContextSetInterpolationQuality(b->ctx, kCGInterpolationHigh);
    CGContextSetBlendMode(b->ctx, kCGBlendModeCopy);
    CGContextDrawImage(b->ctx, CGRectMake(0, 0, w, h), img);
    CGContextSetBlendMode(b->ctx, kCGBlendModeNormal);
    return b;
}

/* ================================================================== typefaces */

typedef struct gface {
    CTFontRef base;                 /* at 12 pt; sized copies are cached below */
    int style;                      /* Typeface.BOLD 1, ITALIC 2 */
    int weight;
} gface;

static CTFontRef roboto(void)
{
    static CTFontRef f; static bool tried;
    if (tried) return f;
    tried = true;
    char buf[1100];
    const char *p = tl_path_resolve("/system/fonts/Roboto-Regular.ttf", buf, sizeof(buf));
    struct stat st;
    if (getenv("TL_SYSFONTS")) { snprintf(buf, sizeof(buf), "%s/fonts/Roboto-Regular.ttf", getenv("TL_SYSFONTS")); p = buf; }
    if (!p || stat(p, &st) != 0 || p[0] != '/' || !strncmp(p, "/system/", 8)) return NULL;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)p, (CFIndex)strlen(p), false);
    CFArrayRef ds = url ? CTFontManagerCreateFontDescriptorsFromURL(url) : NULL;
    if (ds && CFArrayGetCount(ds) > 0) f = CTFontCreateWithFontDescriptor(CFArrayGetValueAtIndex(ds, 0), 12, NULL);
    if (ds) CFRelease(ds);
    if (url) CFRelease(url);
    return f;
}

/* A variable font (Roboto is one) at a weight: its 'wght' axis; others get the bold trait from 700 up. */
static CTFontRef with_weight(CTFontRef f, int weight, bool italic)
{
    CTFontRef out = NULL;
    CFArrayRef axes = CTFontCopyVariationAxes(f);
    if (axes && CFArrayGetCount(axes) > 0 && weight != 400) {
        int tag = 0x77676874; /* 'wght' */
        CFNumberRef k = CFNumberCreate(NULL, kCFNumberIntType, &tag);
        double w = weight;
        CFNumberRef v = CFNumberCreate(NULL, kCFNumberDoubleType, &w);
        CFDictionaryRef var = CFDictionaryCreate(NULL, (const void **)&k, (const void **)&v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        const void *ak[] = { kCTFontVariationAttribute }; const void *av[] = { var };
        CFDictionaryRef attrs = CFDictionaryCreate(NULL, ak, av, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CTFontDescriptorRef d = CTFontDescriptorCreateWithAttributes(attrs);
        out = CTFontCreateCopyWithAttributes(f, 12, NULL, d);
        CFRelease(d); CFRelease(attrs); CFRelease(var); CFRelease(v); CFRelease(k);
    }
    if (axes) CFRelease(axes);
    if (!out) {
        CTFontSymbolicTraits t = (weight >= 600 ? kCTFontBoldTrait : 0) | (italic ? kCTFontItalicTrait : 0);
        if (t) out = CTFontCreateCopyWithSymbolicTraits(f, 12, NULL, t, t);
        if (!out) { CFRetain(f); out = f; }
    } else if (italic) {
        CTFontRef it = CTFontCreateCopyWithSymbolicTraits(out, 12, NULL, kCTFontItalicTrait, kCTFontItalicTrait);
        if (it) { CFRelease(out); out = it; }
    }
    return out;
}

static gface *face_make(CTFontRef base, int style, int weight)
{
    gface *g = calloc(1, sizeof(*g));
    if (!weight) weight = (style & 1) ? 700 : 400;
    g->base = with_weight(base, weight, (style & 2) != 0);
    g->style = style; g->weight = weight;
    return g;
}

static gface *face_family(const char *family, int style)
{
    CTFontRef base = NULL; bool own = false;
    int weight = (style & 1) ? 700 : 400;
    char fam[96]; snprintf(fam, sizeof(fam), "%s", family ? family : "sans-serif");
    for (char *c = fam; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
    if (strstr(fam, "medium")) weight = (style & 1) ? 800 : 500;
    else if (strstr(fam, "light")) weight = 300;
    else if (strstr(fam, "thin")) weight = 200;
    else if (strstr(fam, "black")) weight = 900;
    if (strstr(fam, "mono")) { base = CTFontCreateWithName(CFSTR("Menlo"), 12, NULL); own = true; }
    else if (strstr(fam, "serif") && !strstr(fam, "sans")) { base = CTFontCreateWithName(CFSTR("Georgia"), 12, NULL); own = true; }
    else if ((base = roboto())) own = false;
    else { base = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 12, NULL); own = true; }
    gface *g = face_make(base, style, weight);
    if (own) CFRelease(base);
    return g;
}

static gface *default_face(void)
{
    static gface *f;
    if (!f) f = face_family("sans-serif", 0);
    return f;
}

/* sized fonts, cached by face and size; the caller releases what it is given */
static CTFontRef sized(gface *g, float size)
{
    static struct { gface *g; int q; CTFontRef f; } cache[512];
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    if (!g) g = default_face();
    if (!(size > 0)) size = 12;
    int q = (int)lrintf(size * 8);
    unsigned h = ((unsigned)(uintptr_t)g * 2654435761u ^ (unsigned)q * 40503u) % 512;
    pthread_mutex_lock(&lock);
    if (cache[h].g == g && cache[h].q == q) { CTFontRef f = cache[h].f; CFRetain(f); pthread_mutex_unlock(&lock); return f; }
    CTFontRef f = CTFontCreateCopyWithAttributes(g->base, q / 8.0, NULL, NULL);
    if (cache[h].f) CFRelease(cache[h].f);
    cache[h].g = g; cache[h].q = q; cache[h].f = f;
    CFRetain(f);
    pthread_mutex_unlock(&lock);
    return f;
}

/* ================================================================== shaders */

enum { SH_LINEAR, SH_RADIAL, SH_SWEEP, SH_BITMAP };
typedef struct gshader {
    int kind;
    float x0, y0, x1, y1, r;
    CGGradientRef grad;
    int ncolors;
    uint32_t *colors; float *pos;
    int tile;                       /* 0 clamp, 1 repeat, 2 mirror */
    CGImageRef image;               /* SH_BITMAP */
    int iw, ih;
    CGAffineTransform local;
} gshader;

static CGColorRef color_of(uint32_t c, float alpha)
{
    CGFloat comp[4] = { ((c >> 16) & 255) / 255.0, ((c >> 8) & 255) / 255.0, (c & 255) / 255.0, (c >> 24) / 255.0 * alpha };
    return CGColorCreate(rgb(), comp);
}

static gshader *shader_new(int kind, const uint32_t *colors, const float *pos, int n, int tile)
{
    gshader *s = calloc(1, sizeof(*s));
    s->kind = kind; s->tile = tile; s->local = CGAffineTransformIdentity;
    if (n < 1) n = 1;
    s->ncolors = n;
    s->colors = malloc(sizeof(uint32_t) * (size_t)n);
    s->pos = malloc(sizeof(float) * (size_t)n);
    CGFloat *comp = malloc(sizeof(CGFloat) * 4 * (size_t)n), *loc = malloc(sizeof(CGFloat) * (size_t)n);
    for (int i = 0; i < n; i++) {
        uint32_t c = colors ? colors[i] : 0xFF000000u;
        s->colors[i] = c;
        s->pos[i] = pos ? pos[i] : (n == 1 ? 0 : (float)i / (n - 1));
        comp[4 * i] = ((c >> 16) & 255) / 255.0; comp[4 * i + 1] = ((c >> 8) & 255) / 255.0; comp[4 * i + 2] = (c & 255) / 255.0; comp[4 * i + 3] = (c >> 24) / 255.0;
        loc[i] = s->pos[i];
    }
    s->grad = CGGradientCreateWithColorComponents(rgb(), comp, loc, (size_t)n);
    free(comp); free(loc);
    return s;
}

/* The colour a sweep gradient has at t in [0, 1] */
static uint32_t sweep_at(const gshader *s, float t)
{
    if (s->ncolors == 1 || t <= s->pos[0]) return s->colors[0];
    for (int i = 1; i < s->ncolors; i++) {
        if (t <= s->pos[i]) {
            float span = s->pos[i] - s->pos[i - 1], k = span > 0 ? (t - s->pos[i - 1]) / span : 0;
            uint32_t a = s->colors[i - 1], b = s->colors[i], out = 0;
            for (int sh = 0; sh < 32; sh += 8) {
                float ca = (a >> sh) & 255, cb = (b >> sh) & 255;
                out |= (uint32_t)lrintf(ca + (cb - ca) * k) << sh;
            }
            return out;
        }
    }
    return s->colors[s->ncolors - 1];
}

/* Fill the current clip with the shader, the paint's alpha applied. The path to fill is already the clip. */
static void shader_fill(CGContextRef c, const gshader *s, float alpha)
{
    CGContextSaveGState(c);
    CGContextSetAlpha(c, alpha);
    CGContextConcatCTM(c, s->local);
    CGGradientDrawingOptions ext = s->tile == 0 ? (kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation) : 0;
    if (s->kind == SH_LINEAR) {
        if (s->tile == 0) CGContextDrawLinearGradient(c, s->grad, CGPointMake(s->x0, s->y0), CGPointMake(s->x1, s->y1), ext);
        else {
            /* repeat or mirror: the gradient laid out again along its axis, over what the clip covers */
            CGRect box = CGContextGetClipBoundingBox(c);
            float dx = s->x1 - s->x0, dy = s->y1 - s->y0, len = sqrtf(dx * dx + dy * dy);
            if (len < 1e-3f) { CGContextRestoreGState(c); return; }
            float reach = (float)(fabs(box.size.width) + fabs(box.size.height) + fabs(box.origin.x - s->x0) + fabs(box.origin.y - s->y0));
            int nrep = (int)(reach / len) + 2;
            if (nrep > 400) nrep = 400;
            for (int k = -nrep; k <= nrep; k++) {
                bool flip = s->tile == 2 && (k & 1);
                CGPoint a = CGPointMake(s->x0 + dx * k, s->y0 + dy * k), b = CGPointMake(s->x1 + dx * k, s->y1 + dy * k);
                CGContextSaveGState(c);
                /* the band this repetition covers */
                CGMutablePathRef band = CGPathCreateMutable();
                float nx = -dy / len * reach * 2, ny = dx / len * reach * 2;
                CGPathMoveToPoint(band, NULL, a.x + nx, a.y + ny); CGPathAddLineToPoint(band, NULL, b.x + nx, b.y + ny);
                CGPathAddLineToPoint(band, NULL, b.x - nx, b.y - ny); CGPathAddLineToPoint(band, NULL, a.x - nx, a.y - ny);
                CGPathCloseSubpath(band);
                CGContextAddPath(c, band); CGContextClip(c); CGPathRelease(band);
                if (flip) CGContextDrawLinearGradient(c, s->grad, b, a, kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
                else CGContextDrawLinearGradient(c, s->grad, a, b, kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation);
                CGContextRestoreGState(c);
            }
        }
    } else if (s->kind == SH_RADIAL) {
        CGContextDrawRadialGradient(c, s->grad, CGPointMake(s->x0, s->y0), 0, CGPointMake(s->x0, s->y0), s->r, ext);
    } else if (s->kind == SH_SWEEP) {
        /* wedges around the centre; Android's sweep starts at 3 o'clock and turns clockwise (y down) */
        CGRect box = CGContextGetClipBoundingBox(c);
        float R = (float)(fabs(box.size.width) + fabs(box.size.height) + fabs(box.origin.x - s->x0) + fabs(box.origin.y - s->y0)) + 1;
        const int N = 180;
        for (int i = 0; i < N; i++) {
            float a0 = (float)(2 * M_PI * i / N), a1 = (float)(2 * M_PI * (i + 1) / N) + 0.01f;
            CGColorRef col = color_of(sweep_at(s, (i + 0.5f) / N), 1);
            CGContextSetFillColorWithColor(c, col);
            CGColorRelease(col);
            CGContextMoveToPoint(c, s->x0, s->y0);
            CGContextAddLineToPoint(c, s->x0 + R * cosf(a0), s->y0 + R * sinf(a0));
            CGContextAddLineToPoint(c, s->x0 + R * cosf(a1), s->y0 + R * sinf(a1));
            CGContextClosePath(c);
            CGContextFillPath(c);
        }
    } else if (s->kind == SH_BITMAP && s->image) {
        CGRect box = CGContextGetClipBoundingBox(c);
        if (s->tile == 0) {
            CGContextTranslateCTM(c, 0, s->ih); CGContextScaleCTM(c, 1, -1);
            CGContextDrawImage(c, CGRectMake(0, 0, s->iw, s->ih), s->image);
        } else {
            int x0 = (int)floor(box.origin.x / s->iw) - 1, y0 = (int)floor(box.origin.y / s->ih) - 1;
            int x1 = (int)ceil((box.origin.x + box.size.width) / s->iw) + 1, y1 = (int)ceil((box.origin.y + box.size.height) / s->ih) + 1;
            if ((int64_t)(x1 - x0) * (y1 - y0) > 20000) { CGContextRestoreGState(c); return; }
            for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
                CGContextSaveGState(c);
                CGContextTranslateCTM(c, x * s->iw, (y + 1) * s->ih); CGContextScaleCTM(c, 1, -1);
                CGContextDrawImage(c, CGRectMake(0, 0, s->iw, s->ih), s->image);
                CGContextRestoreGState(c);
            }
        }
    }
    CGContextRestoreGState(c);
}

/* ================================================================== canvases */

typedef struct gcanvas {
    gbitmap *bm;
    CGContextRef c;
    CGAffineTransform base;         /* the flip: Android's identity */
    int depth;                      /* Android's save count - 1 */
    uint64_t layer_bits[4];         /* which save levels are layers (end a transparency layer on restore) */
} gcanvas;

static gcanvas *cv_new(gbitmap *b)
{
    gcanvas *v = calloc(1, sizeof(*v));
    v->bm = b; v->c = b->ctx;
    CGContextSaveGState(v->c);              /* the canvas's own level, undone when it is freed */
    CGContextTranslateCTM(v->c, 0, b->h);
    CGContextScaleCTM(v->c, 1, -1);
    v->base = CGContextGetCTM(v->c);
    CGContextSetTextMatrix(v->c, CGAffineTransformMakeScale(1, -1));
    return v;
}
static void cv_free(gcanvas *v)
{
    if (!v) return;
    while (v->depth > 0) {
        if (v->layer_bits[(v->depth - 1) >> 6] & (1ull << ((v->depth - 1) & 63))) CGContextEndTransparencyLayer(v->c);
        CGContextRestoreGState(v->c);
        v->depth--;
    }
    CGContextRestoreGState(v->c);
    free(v);
}
static int cv_save(gcanvas *v, bool layer, float alpha, CGRect bounds)
{
    CGContextSaveGState(v->c);
    if (v->depth < 256) {
        uint64_t bit = 1ull << (v->depth & 63);
        if (layer) {
            v->layer_bits[v->depth >> 6] |= bit;
            CGContextSetAlpha(v->c, alpha);
            if (bounds.size.width > 0 && bounds.size.height > 0) CGContextBeginTransparencyLayerWithRect(v->c, bounds, NULL);
            else CGContextBeginTransparencyLayer(v->c, NULL);
            CGContextSetAlpha(v->c, 1);
        } else v->layer_bits[v->depth >> 6] &= ~bit;
    }
    return ++v->depth;
}
static void cv_restore(gcanvas *v)
{
    if (v->depth <= 0) return;
    v->depth--;
    if (v->depth < 256 && (v->layer_bits[v->depth >> 6] & (1ull << (v->depth & 63)))) CGContextEndTransparencyLayer(v->c);
    CGContextRestoreGState(v->c);
}

static CGBlendMode blend_of(int mode)
{
    switch (mode) {
    case 0: return kCGBlendModeClear;
    case 1: return kCGBlendModeCopy;
    case 4: return kCGBlendModeDestinationOver;
    case 5: return kCGBlendModeSourceIn;
    case 6: return kCGBlendModeDestinationIn;
    case 7: return kCGBlendModeSourceOut;
    case 8: return kCGBlendModeDestinationOut;
    case 9: return kCGBlendModeSourceAtop;
    case 10: return kCGBlendModeDestinationAtop;
    case 11: return kCGBlendModeXOR;
    case 12: return kCGBlendModePlusLighter;
    case 13: return kCGBlendModeMultiply;
    case 14: return kCGBlendModeScreen;
    case 15: return kCGBlendModeOverlay;
    case 16: return kCGBlendModeDarken;
    case 17: return kCGBlendModeLighten;
    default: return kCGBlendModeNormal;
    }
}

/* Set up the context for one draw with this paint: colour, stroke, blend, shadow. Returns false when nothing would show. */
static bool paint_begin(CGContextRef c, const int32_t *p, bool *layered)
{
    *layered = false;
    if (p[P_XFER] == 2) return false;                               /* DST: leaves the destination as it is */
    CGContextSaveGState(c);
    uint32_t col = (uint32_t)p[P_COLOR];
    CGColorRef cc = color_of(col, 1);
    CGContextSetFillColorWithColor(c, cc);
    CGContextSetStrokeColorWithColor(c, cc);
    CGColorRelease(cc);
    CGContextSetShouldAntialias(c, (p[P_FLAGS] & PF_AA) != 0);
    float sw = pf(p, P_STROKE);
    CGContextSetLineWidth(c, sw > 0 ? sw : 1.0f);    /* hairline: one pixel */
    CGContextSetLineCap(c, p[P_CAP] == 1 ? kCGLineCapRound : p[P_CAP] == 2 ? kCGLineCapSquare : kCGLineCapButt);
    CGContextSetLineJoin(c, p[P_JOIN] == 1 ? kCGLineJoinRound : p[P_JOIN] == 2 ? kCGLineJoinBevel : kCGLineJoinMiter);
    CGContextSetMiterLimit(c, pf(p, P_MITER) > 0 ? pf(p, P_MITER) : 4);
    CGContextSetInterpolationQuality(c, (p[P_FLAGS] & PF_FILTER) ? kCGInterpolationDefault : kCGInterpolationNone);
    if (pf(p, P_SHRAD) > 0) {
        CGColorRef sc = color_of((uint32_t)p[P_SHCOL], 1);
        CGContextSetShadowWithColor(c, CGSizeMake(pf(p, P_SHDX), -pf(p, P_SHDY)), pf(p, P_SHRAD), sc);
        CGColorRelease(sc);
    }
    if (p[P_CFMODE] >= 0) {
        /* a colour filter: the draw goes into a layer, then the filter colour is laid over it with its mode */
        CGContextBeginTransparencyLayer(c, NULL);
        *layered = true;
    } else CGContextSetBlendMode(c, blend_of(p[P_XFER]));
    return true;
}
static void paint_end(CGContextRef c, const int32_t *p, bool layered, CGRect area)
{
    if (layered) {
        CGColorRef fc = color_of((uint32_t)p[P_CFCOLOR], 1);
        CGContextSetFillColorWithColor(c, fc);
        CGColorRelease(fc);
        CGContextSetShadowWithColor(c, CGSizeZero, 0, NULL);
        int m = p[P_CFMODE];
        CGContextSetBlendMode(c, m == 3 ? kCGBlendModeNormal : blend_of(m));
        CGContextFillRect(c, CGRectInset(area, -2, -2));
        if (m == 13) {                                  /* MULTIPLY keeps the drawing's own alpha */
            CGContextSetBlendMode(c, kCGBlendModeDestinationIn);
        }
        CGContextEndTransparencyLayer(c);
    }
    CGContextRestoreGState(c);
}

/* Draw the path now in the context with this paint (fill, stroke or both; through the shader when there is one). */
static void draw_path(CGContextRef c, CGPathRef path, const int32_t *p, gshader *sh, bool even_odd)
{
    bool layered;
    if (!paint_begin(c, p, &layered)) return;
    int style = p[P_STYLE];
    float alpha = (float)((uint32_t)p[P_COLOR] >> 24) / 255.0f;
    CGRect area = CGPathGetBoundingBox(path);
    area = CGRectInset(area, -pf(p, P_STROKE), -pf(p, P_STROKE));
    if (sh) {
        CGContextSaveGState(c);
        CGContextAddPath(c, path);
        if (style == 1) { CGContextReplacePathWithStrokedPath(c); CGContextClip(c); }
        else if (even_odd) CGContextEOClip(c); else CGContextClip(c);
        shader_fill(c, sh, alpha);
        CGContextRestoreGState(c);
        if (style == 2) {
            CGContextSaveGState(c);
            CGContextAddPath(c, path); CGContextReplacePathWithStrokedPath(c); CGContextClip(c);
            shader_fill(c, sh, alpha);
            CGContextRestoreGState(c);
        }
    } else {
        CGContextAddPath(c, path);
        CGContextDrawPath(c, style == 1 ? kCGPathStroke : style == 2 ? (even_odd ? kCGPathEOFillStroke : kCGPathFillStroke) : (even_odd ? kCGPathEOFill : kCGPathFill));
    }
    paint_end(c, p, layered, area);
}

/* ---- Path, as husk.Gfx streams it: op codes and their floats */
enum { OP_MOVE, OP_LINE, OP_QUAD, OP_CUBIC, OP_CLOSE, OP_ARC, OP_RECT, OP_OVAL, OP_RRECT, OP_TRANSFORM, OP_RMOVE, OP_RLINE, OP_RQUAD, OP_RCUBIC, OP_RRECT8 };

static void add_rrect8(CGMutablePathRef p, const CGAffineTransform *t, float l, float tp, float r, float b, const float *rad, bool ccw)
{
    /* corners: top-left, top-right, bottom-right, bottom-left, each (rx, ry), as Android's addRoundRect(float[]) */
    float w = r - l, h = b - tp;
    float rx[4], ry[4];
    for (int i = 0; i < 4; i++) { rx[i] = fminf(fmaxf(rad[2 * i], 0), w / 2); ry[i] = fminf(fmaxf(rad[2 * i + 1], 0), h / 2); }
    const float k = 0.5522847f;
    (void)ccw;
    CGPathMoveToPoint(p, t, l + rx[0], tp);
    CGPathAddLineToPoint(p, t, r - rx[1], tp);
    CGPathAddCurveToPoint(p, t, r - rx[1] + rx[1] * k, tp, r, tp + ry[1] - ry[1] * k, r, tp + ry[1]);
    CGPathAddLineToPoint(p, t, r, b - ry[2]);
    CGPathAddCurveToPoint(p, t, r, b - ry[2] + ry[2] * k, r - rx[2] + rx[2] * k, b, r - rx[2], b);
    CGPathAddLineToPoint(p, t, l + rx[3], b);
    CGPathAddCurveToPoint(p, t, l + rx[3] - rx[3] * k, b, l, b - ry[3] + ry[3] * k, l, b - ry[3]);
    CGPathAddLineToPoint(p, t, l, tp + ry[0]);
    CGPathAddCurveToPoint(p, t, l, tp + ry[0] - ry[0] * k, l + rx[0] - rx[0] * k, tp, l + rx[0], tp);
    CGPathCloseSubpath(p);
}

static void add_arc(CGMutablePathRef p, float l, float t, float r, float b, float start, float sweep, bool move)
{
    float cx = (l + r) / 2, cy = (t + b) / 2, rx = (r - l) / 2, ry = (b - t) / 2;
    if (rx <= 0 || ry <= 0) return;
    CGAffineTransform m = CGAffineTransformMakeTranslation(cx, cy);
    m = CGAffineTransformScale(m, rx, ry);
    double a0 = start * M_PI / 180, a1 = (start + sweep) * M_PI / 180;
    CGPoint s = CGPointApplyAffineTransform(CGPointMake(cos(a0), sin(a0)), m);
    if (move || CGPathIsEmpty(p)) CGPathMoveToPoint(p, NULL, s.x, s.y);
    else CGPathAddLineToPoint(p, NULL, s.x, s.y);
    if (fabsf(sweep) >= 360) {
        CGPathAddEllipseInRect(p, NULL, CGRectMake(l, t, r - l, b - t));
        CGPathMoveToPoint(p, NULL, s.x, s.y);
        return;
    }
    CGPathAddRelativeArc(p, &m, 0, 0, 1, a0, a1 - a0);
}

static CGPathRef build_path(const float *f, int n)
{
    CGMutablePathRef p = CGPathCreateMutable();
    float lx = 0, ly = 0;                       /* the last point, for the relative ops */
    for (int i = 0; i < n; ) {
        int op = (int)f[i++];
        switch (op) {
        case OP_MOVE: CGPathMoveToPoint(p, NULL, f[i], f[i + 1]); lx = f[i]; ly = f[i + 1]; i += 2; break;
        case OP_RMOVE: lx += f[i]; ly += f[i + 1]; CGPathMoveToPoint(p, NULL, lx, ly); i += 2; break;
        case OP_LINE: case OP_RLINE: {
            float x = f[i], y = f[i + 1]; if (op == OP_RLINE) { x += lx; y += ly; }
            if (CGPathIsEmpty(p)) CGPathMoveToPoint(p, NULL, 0, 0);
            CGPathAddLineToPoint(p, NULL, x, y); lx = x; ly = y; i += 2; break; }
        case OP_QUAD: case OP_RQUAD: {
            float ox = op == OP_RQUAD ? lx : 0, oy = op == OP_RQUAD ? ly : 0;
            if (CGPathIsEmpty(p)) CGPathMoveToPoint(p, NULL, 0, 0);
            CGPathAddQuadCurveToPoint(p, NULL, f[i] + ox, f[i + 1] + oy, f[i + 2] + ox, f[i + 3] + oy);
            lx = f[i + 2] + ox; ly = f[i + 3] + oy; i += 4; break; }
        case OP_CUBIC: case OP_RCUBIC: {
            float ox = op == OP_RCUBIC ? lx : 0, oy = op == OP_RCUBIC ? ly : 0;
            if (CGPathIsEmpty(p)) CGPathMoveToPoint(p, NULL, 0, 0);
            CGPathAddCurveToPoint(p, NULL, f[i] + ox, f[i + 1] + oy, f[i + 2] + ox, f[i + 3] + oy, f[i + 4] + ox, f[i + 5] + oy);
            lx = f[i + 4] + ox; ly = f[i + 5] + oy; i += 6; break; }
        case OP_CLOSE: if (!CGPathIsEmpty(p)) { CGPathCloseSubpath(p); CGPoint c = CGPathGetCurrentPoint(p); lx = (float)c.x; ly = (float)c.y; } break;
        case OP_ARC: add_arc(p, f[i], f[i + 1], f[i + 2], f[i + 3], f[i + 4], f[i + 5], f[i + 6] != 0); { CGPoint c = CGPathGetCurrentPoint(p); lx = (float)c.x; ly = (float)c.y; } i += 7; break;
        case OP_RECT: {
            float l = f[i], t = f[i + 1], r = f[i + 2], b = f[i + 3];
            if (f[i + 4] != 0) { CGPathMoveToPoint(p, NULL, l, t); CGPathAddLineToPoint(p, NULL, l, b); CGPathAddLineToPoint(p, NULL, r, b); CGPathAddLineToPoint(p, NULL, r, t); CGPathCloseSubpath(p); }
            else CGPathAddRect(p, NULL, CGRectMake(fminf(l, r), fminf(t, b), fabsf(r - l), fabsf(b - t)));
            lx = l; ly = t; i += 5; break; }
        case OP_OVAL: CGPathAddEllipseInRect(p, NULL, CGRectMake(f[i], f[i + 1], f[i + 2] - f[i], f[i + 3] - f[i + 1])); i += 5; break;
        case OP_RRECT: { float rad[8] = { f[i + 4], f[i + 5], f[i + 4], f[i + 5], f[i + 4], f[i + 5], f[i + 4], f[i + 5] };
            add_rrect8(p, NULL, f[i], f[i + 1], f[i + 2], f[i + 3], rad, f[i + 6] != 0); i += 7; break; }
        case OP_RRECT8: add_rrect8(p, NULL, f[i], f[i + 1], f[i + 2], f[i + 3], f + i + 4, f[i + 12] != 0); i += 13; break;
        case OP_TRANSFORM: {
            CGAffineTransform m = CGAffineTransformMake(f[i], f[i + 3], f[i + 1], f[i + 4], f[i + 2], f[i + 5]);
            CGMutablePathRef q = CGPathCreateMutableCopyByTransformingPath(p, &m);
            CGPathRelease(p); p = q;
            CGPoint c = CGPointApplyAffineTransform(CGPointMake(lx, ly), m); lx = (float)c.x; ly = (float)c.y;
            i += 6; break; }
        default: i = n; break;
        }
    }
    return p;
}

/* ================================================================== text */

static CFStringRef cfstr(jobj *s, int start, int end)
{
    int32_t n = 0;
    const uint16_t *u = s ? tl_dvm_string_chars(s, &n) : NULL;
    if (!u) return CFStringCreateWithCharacters(NULL, NULL, 0);
    if (start < 0) start = 0;
    if (end > n || end < 0) end = n;
    if (end < start) end = start;
    return CFStringCreateWithCharacters(NULL, u + start, end - start);
}

static CTLineRef make_line(CFStringRef str, const int32_t *p, gface *g)
{
    CTFontRef f = sized(g, pf(p, P_TEXTSIZE));
    float ls = pf(p, P_LSPACE) * pf(p, P_TEXTSIZE);
    CFNumberRef kern = CFNumberCreate(NULL, kCFNumberFloatType, &ls);
    const void *k[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName, kCTKernAttributeName };
    const void *v[] = { f, kCFBooleanTrue, kern };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, k, v, ls != 0 ? 3 : 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, str, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(as);
    CFRelease(as); CFRelease(attrs); CFRelease(kern); CFRelease(f);
    return line;
}

static float line_width(CTLineRef line, const int32_t *p)
{
    return (float)CTLineGetTypographicBounds(line, NULL, NULL, NULL) * (pf(p, P_SCALEX) > 0 ? pf(p, P_SCALEX) : 1);
}

static void draw_text(gcanvas *v, jobj *s, int start, int end, float x, float y, const int32_t *p, gface *g, gshader *sh)
{
    CFStringRef str = cfstr(s, start, end);
    CTLineRef line = make_line(str, p, g);
    CFRelease(str);
    float w = line_width(line, p);
    if (p[P_ALIGN] == 1) x -= w / 2;
    else if (p[P_ALIGN] == 2) x -= w;
    CGContextRef c = v->c;
    bool layered;
    if (paint_begin(c, p, &layered)) {
        float sx = pf(p, P_SCALEX) > 0 ? pf(p, P_SCALEX) : 1, skew = pf(p, P_SKEWX);
        CGContextSetTextMatrix(c, CGAffineTransformMake(sx, 0, -skew, -1, 0, 0));
        int style = p[P_STYLE];
        bool bold = (p[P_FLAGS] & PF_FAKEBOLD) != 0;
        if (bold || style != 0) {
            CGContextSetLineWidth(c, style != 0 ? pf(p, P_STROKE) : pf(p, P_TEXTSIZE) / 24);
            CGContextSetTextDrawingMode(c, style == 1 ? kCGTextStroke : kCGTextFillStroke);
        } else CGContextSetTextDrawingMode(c, kCGTextFill);
        CGFloat asc = 0, desc = 0;
        CTLineGetTypographicBounds(line, &asc, &desc, NULL);
        CGRect area = CGRectMake(x - 2, y - asc - 2, w + 4, asc + desc + 4);
        if (sh) {
            CGContextSaveGState(c);
            CGContextSetTextDrawingMode(c, kCGTextClip);
            CGContextSetTextPosition(c, x, y);
            CTLineDraw(line, c);
            shader_fill(c, sh, (float)((uint32_t)p[P_COLOR] >> 24) / 255.0f);
            CGContextRestoreGState(c);
        } else {
            CGContextSetTextPosition(c, x, y);
            CTLineDraw(line, c);
        }
        if (p[P_FLAGS] & (PF_UNDERLINE | PF_STRIKE)) {
            float ts = pf(p, P_TEXTSIZE), th = ts / 18 > 1 ? ts / 18 : 1;
            if (p[P_FLAGS] & PF_UNDERLINE) CGContextFillRect(c, CGRectMake(x, y + ts / 9, w, th));
            if (p[P_FLAGS] & PF_STRIKE) CGContextFillRect(c, CGRectMake(x, y - ts * 0.3f, w, th));
        }
        paint_end(c, p, layered, area);
    }
    CFRelease(line);
}

/* ================================================================== the natives (husk.Gfx) */

#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)
static jvalue J(int64_t j) { jvalue v; v.j = j; return v; }
static jvalue Ii(int32_t i) { jvalue v; v.j = (uint32_t)i; return v; }
static jvalue Ff(float f) { jvalue v; v.j = 0; v.f = f; return v; }
static jvalue Ll(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
#define H(T, k) ((T *)(uintptr_t)a[k].j)
static void *arr_data(jobj *x) { return x && x->kind == TL_K_PRIM_ARRAY ? x->arr.data : NULL; }
static uint32_t arr_len(jobj *x) { return x && x->kind == TL_K_PRIM_ARRAY ? x->arr.len : 0; }

/* ---- Bitmap */
NAT(G_bmNew) { (void)self; *ret = J((int64_t)(uintptr_t)bm_new(a[0].i, a[1].i)); return true; }
NAT(G_bmFree) { (void)self; (void)ret; bm_free(H(gbitmap, 0)); return true; }
NAT(G_bmErase)
{
    (void)self; (void)ret;
    gbitmap *b = H(gbitmap, 0);
    if (!b) return true;
    uint8_t px[4]; from_argb(px, (uint32_t)a[1].i);
    uint32_t v; memcpy(&v, px, 4);
    uint32_t *d = (uint32_t *)b->px;
    size_t n = (size_t)b->w * b->h;
    if (!v) memset(d, 0, n * 4); else for (size_t i = 0; i < n; i++) d[i] = v;
    return true;
}
NAT(G_bmGetPixel)
{
    (void)self;
    gbitmap *b = H(gbitmap, 0); int x = a[1].i, y = a[2].i;
    *ret = Ii(b && x >= 0 && y >= 0 && x < b->w && y < b->h ? (int32_t)to_argb(b->px + (size_t)y * b->stride + (size_t)x * 4) : 0);
    return true;
}
NAT(G_bmSetPixel)
{
    (void)self; (void)ret;
    gbitmap *b = H(gbitmap, 0); int x = a[1].i, y = a[2].i;
    if (b && x >= 0 && y >= 0 && x < b->w && y < b->h) from_argb(b->px + (size_t)y * b->stride + (size_t)x * 4, (uint32_t)a[3].i);
    return true;
}
/* (long bm, int[] px, int offset, int stride, int x, int y, int w, int h, boolean set) */
NAT(G_bmPixels)
{
    (void)self; (void)ret;
    gbitmap *b = H(gbitmap, 0);
    int32_t *px = arr_data(a[1].l); uint32_t len = arr_len(a[1].l);
    int off = a[2].i, stride = a[3].i, x = a[4].i, y = a[5].i, w = a[6].i, h = a[7].i;
    bool set = a[8].z;
    if (!b || !px || x < 0 || y < 0 || x + w > b->w || y + h > b->h) return dvm_throw("java/lang/IllegalArgumentException", "pixels out of bounds");
    for (int r = 0; r < h; r++) {
        int64_t base = off + (int64_t)r * stride;
        if (base < 0 || base + w > len) return dvm_throw("java/lang/ArrayIndexOutOfBoundsException", "pixels");
        uint8_t *row = b->px + (size_t)(y + r) * b->stride + (size_t)x * 4;
        for (int k = 0; k < w; k++) {
            if (set) from_argb(row + 4 * k, (uint32_t)px[base + k]);
            else px[base + k] = (int32_t)to_argb(row + 4 * k);
        }
    }
    return true;
}
/* raw premultiplied bytes, for copyPixelsToBuffer / copyPixelsFromBuffer (long bm, byte[] or a buffer's address, boolean set) */
NAT(G_bmRaw)
{
    (void)self; (void)ret;
    gbitmap *b = H(gbitmap, 0);
    uint8_t *d = (uint8_t *)(uintptr_t)a[1].j;
    if (!b || !d) return true;
    if (a[2].z) memcpy(b->px, d, b->stride * (size_t)b->h); else memcpy(d, b->px, b->stride * (size_t)b->h);
    return true;
}
NAT(G_bmAddress) { (void)self; gbitmap *b = H(gbitmap, 0); *ret = J(b ? (int64_t)(uintptr_t)b->px : 0); return true; }

/* (byte[] data, int offset, int length, int sample, int[] outWH, boolean boundsOnly) -> long */
NAT(G_bmDecode)
{
    (void)self;
    uint8_t *d = arr_data(a[0].l); uint32_t len = arr_len(a[0].l);
    int off = a[1].i, n = a[2].i, sample = a[3].i;
    int32_t *wh = arr_data(a[4].l);
    *ret = J(0);
    if (!d || off < 0 || n <= 0 || (uint32_t)off + (uint32_t)n > len) return true;
    CFDataRef data = CFDataCreate(NULL, d + off, n);
    CGImageSourceRef src = CGImageSourceCreateWithData(data, NULL);
    CFRelease(data);
    if (!src) return true;
    if (a[5].z) {
        CFDictionaryRef props = CGImageSourceCopyPropertiesAtIndex(src, 0, NULL);
        if (props && wh) {
            int w = 0, h = 0;
            CFNumberRef nw = CFDictionaryGetValue(props, kCGImagePropertyPixelWidth), nh = CFDictionaryGetValue(props, kCGImagePropertyPixelHeight);
            if (nw) CFNumberGetValue(nw, kCFNumberIntType, &w);
            if (nh) CFNumberGetValue(nh, kCFNumberIntType, &h);
            wh[0] = w / (sample > 1 ? sample : 1); wh[1] = h / (sample > 1 ? sample : 1);
        }
        if (props) CFRelease(props);
        CFRelease(src);
        return true;
    }
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, NULL);
    CFRelease(src);
    if (!img) return true;
    gbitmap *b = bm_from_image(img, sample);
    CGImageRelease(img);
    if (b && wh) { wh[0] = b->w; wh[1] = b->h; }
    *ret = J((int64_t)(uintptr_t)b);
    return true;
}
/* (long bm, int format 0 jpeg 1 png 2 webp, int quality) -> byte[] */
NAT(G_bmCompress)
{
    (void)self;
    gbitmap *b = H(gbitmap, 0);
    *ret = Ll(NULL);
    if (!b) return true;
    CGImageRef img = bm_image(b);
    CFMutableDataRef out = CFDataCreateMutable(NULL, 0);
    CFStringRef type = a[1].i == 0 ? CFSTR("public.jpeg") : CFSTR("public.png");
    CGImageDestinationRef dst = CGImageDestinationCreateWithData(out, type, 1, NULL);
    if (dst) {
        float q = a[2].i / 100.0f;
        CFNumberRef qn = CFNumberCreate(NULL, kCFNumberFloatType, &q);
        const void *k[] = { kCGImageDestinationLossyCompressionQuality }; const void *v[] = { qn };
        CFDictionaryRef opt = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CGImageDestinationAddImage(dst, img, opt);
        CGImageDestinationFinalize(dst);
        CFRelease(opt); CFRelease(qn); CFRelease(dst);
        jobj *arr = tl_jni_new_prim_array('B', (uint32_t)CFDataGetLength(out));
        memcpy(arr->arr.data, CFDataGetBytePtr(out), (size_t)CFDataGetLength(out));
        *ret = Ll(arr);
    }
    CFRelease(out);
    CGImageRelease(img);
    return true;
}
/* (long src, int x, int y, int w, int h, float[] matrix or null, boolean filter) -> long: a new bitmap of the region, transformed */
NAT(G_bmCopy)
{
    (void)self;
    gbitmap *s = H(gbitmap, 0);
    int x = a[1].i, y = a[2].i, w = a[3].i, h = a[4].i;
    float *m = arr_data(a[5].l);
    *ret = J(0);
    if (!s || w <= 0 || h <= 0) return true;
    CGAffineTransform t = m ? CGAffineTransformMake(m[0], m[3], m[1], m[4], m[2], m[5]) : CGAffineTransformIdentity;
    CGRect box = CGRectApplyAffineTransform(CGRectMake(0, 0, w, h), t);
    int nw = (int)lrint(box.size.width), nh = (int)lrint(box.size.height);
    gbitmap *d = bm_new(nw > 0 ? nw : 1, nh > 0 ? nh : 1);
    if (!d) return true;
    CGImageRef full = bm_image(s);
    CGImageRef sub = CGImageCreateWithImageInRect(full, CGRectMake(x, y, w, h));
    CGContextRef c = d->ctx;
    CGContextSaveGState(c);
    CGContextTranslateCTM(c, 0, d->h); CGContextScaleCTM(c, 1, -1);           /* y down */
    CGContextTranslateCTM(c, -box.origin.x, -box.origin.y);
    CGContextConcatCTM(c, t);
    CGContextSetInterpolationQuality(c, a[6].z ? kCGInterpolationHigh : kCGInterpolationNone);
    CGContextTranslateCTM(c, 0, h); CGContextScaleCTM(c, 1, -1);
    CGContextDrawImage(c, CGRectMake(0, 0, w, h), sub);
    CGContextRestoreGState(c);
    CGImageRelease(sub); CGImageRelease(full);
    *ret = J((int64_t)(uintptr_t)d);
    return true;
}

/* ---- Canvas */
NAT(G_cvNew) { (void)self; gbitmap *b = H(gbitmap, 0); *ret = J(b ? (int64_t)(uintptr_t)cv_new(b) : 0); return true; }
NAT(G_cvFree) { (void)self; (void)ret; cv_free(H(gcanvas, 0)); return true; }
NAT(G_cvSave) { (void)self; gcanvas *v = H(gcanvas, 0); *ret = Ii(v ? cv_save(v, false, 1, CGRectNull) : 1); return true; }
/* (long, l, t, r, b, int alpha) */
NAT(G_cvSaveLayer)
{
    (void)self;
    gcanvas *v = H(gcanvas, 0);
    CGRect r = CGRectMake(a[1].f, a[2].f, a[3].f - a[1].f, a[4].f - a[2].f);
    *ret = Ii(v ? cv_save(v, true, a[5].i / 255.0f, r) : 1);
    return true;
}
NAT(G_cvRestore) { (void)self; (void)ret; gcanvas *v = H(gcanvas, 0); if (v) cv_restore(v); return true; }
NAT(G_cvRestoreTo) { (void)self; (void)ret; gcanvas *v = H(gcanvas, 0); int n = a[1].i; if (v) while (v->depth + 1 > n && v->depth > 0) cv_restore(v); return true; }
NAT(G_cvSaveCount) { (void)self; gcanvas *v = H(gcanvas, 0); *ret = Ii(v ? v->depth + 1 : 1); return true; }
/* (long, float[6] affine: sx kx tx ky sy ty, boolean set) */
NAT(G_cvMatrix)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0); float *m = arr_data(a[1].l);
    if (!v || !m) return true;
    CGAffineTransform t = CGAffineTransformMake(m[0], m[3], m[1], m[4], m[2], m[5]);
    if (a[2].z) {
        CGAffineTransform cur = CGContextGetCTM(v->c);
        CGContextConcatCTM(v->c, CGAffineTransformInvert(cur));
        CGContextConcatCTM(v->c, CGAffineTransformConcat(t, v->base));
    } else CGContextConcatCTM(v->c, t);
    return true;
}
NAT(G_cvGetMatrix)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0); float *m = arr_data(a[1].l);
    if (!v || !m) return true;
    CGAffineTransform t = CGAffineTransformConcat(CGContextGetCTM(v->c), CGAffineTransformInvert(v->base));
    m[0] = (float)t.a; m[1] = (float)t.c; m[2] = (float)t.tx; m[3] = (float)t.b; m[4] = (float)t.d; m[5] = (float)t.ty;
    return true;
}
/* (long, l, t, r, b, int op: 1 intersect, 0 difference) -> boolean */
NAT(G_cvClipRect)
{
    (void)self;
    gcanvas *v = H(gcanvas, 0);
    *ret = Ii(1);
    if (!v) return true;
    CGRect r = CGRectMake(a[1].f, a[2].f, a[3].f - a[1].f, a[4].f - a[2].f);
    if (a[5].i == 0) {
        CGRect all = CGContextGetClipBoundingBox(v->c);
        CGContextAddRect(v->c, CGRectInset(all, -1, -1));
        CGContextAddRect(v->c, r);
        CGContextEOClip(v->c);
    } else CGContextClipToRect(v->c, r);
    *ret = Ii(!CGRectIsEmpty(CGContextGetClipBoundingBox(v->c)));
    return true;
}
/* (long, float[] path, int n, int fill (1 even-odd), int op) */
NAT(G_cvClipPath)
{
    (void)self;
    gcanvas *v = H(gcanvas, 0); float *f = arr_data(a[1].l);
    *ret = Ii(1);
    if (!v || !f) return true;
    CGPathRef p = build_path(f, a[2].i);
    if (a[4].i == 0) {
        CGRect all = CGContextGetClipBoundingBox(v->c);
        CGContextAddRect(v->c, CGRectInset(all, -1, -1));
        CGContextAddPath(v->c, p);
        CGContextEOClip(v->c);
    } else {
        CGContextAddPath(v->c, p);
        if (a[3].i == 1) CGContextEOClip(v->c); else CGContextClip(v->c);
    }
    CGPathRelease(p);
    *ret = Ii(!CGRectIsEmpty(CGContextGetClipBoundingBox(v->c)));
    return true;
}
NAT(G_cvClipBounds)
{
    (void)self;
    gcanvas *v = H(gcanvas, 0); int32_t *o = arr_data(a[1].l);
    *ret = Ii(0);
    if (!v || !o) return true;
    CGRect r = CGContextGetClipBoundingBox(v->c);
    if (CGRectIsNull(r) || CGRectIsEmpty(r)) { o[0] = o[1] = o[2] = o[3] = 0; return true; }
    o[0] = (int32_t)floor(r.origin.x); o[1] = (int32_t)floor(r.origin.y);
    o[2] = (int32_t)ceil(r.origin.x + r.size.width); o[3] = (int32_t)ceil(r.origin.y + r.size.height);
    *ret = Ii(1);
    return true;
}
/* (long, int color, int mode) */
NAT(G_cvDrawColor)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0);
    if (!v) return true;
    CGContextRef c = v->c;
    CGContextSaveGState(c);
    CGRect all = CGContextGetClipBoundingBox(c);
    int mode = a[2].i;
    if (mode == 0) CGContextClearRect(c, all);
    else {
        CGColorRef col = color_of((uint32_t)a[1].i, 1);
        CGContextSetFillColorWithColor(c, col);
        CGColorRelease(col);
        CGContextSetBlendMode(c, blend_of(mode));
        CGContextFillRect(c, all);
    }
    CGContextRestoreGState(c);
    return true;
}
/* shapes: (long, l, t, r, b, [extra...], int[] paint, long shader) */
static void shape(gcanvas *v, CGPathRef p, jobj *paint, int64_t shader)
{
    draw_path(v->c, p, paint_of(paint), (gshader *)(uintptr_t)shader, false);
}
NAT(G_cvDrawRect)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0);
    if (!v) return true;
    CGRect r = CGRectMake(a[1].f, a[2].f, a[3].f - a[1].f, a[4].f - a[2].f);
    const int32_t *p = paint_of(a[5].l);
    if (!a[6].j && p[P_STYLE] == 0 && p[P_CFMODE] < 0 && pf(p, P_SHRAD) <= 0 && p[P_XFER] == 3) {
        /* the common case, without a path */
        CGContextSaveGState(v->c);
        CGColorRef cc = color_of((uint32_t)p[P_COLOR], 1);
        CGContextSetFillColorWithColor(v->c, cc);
        CGColorRelease(cc);
        CGContextSetShouldAntialias(v->c, (p[P_FLAGS] & PF_AA) != 0);
        CGContextFillRect(v->c, r);
        CGContextRestoreGState(v->c);
        return true;
    }
    CGPathRef path = CGPathCreateWithRect(r, NULL);
    shape(v, path, a[5].l, a[6].j);
    CGPathRelease(path);
    return true;
}
NAT(G_cvDrawRRect)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0);
    if (!v) return true;
    float rad[8]; for (int i = 0; i < 8; i++) rad[i] = (i & 1) ? a[6].f : a[5].f;
    CGMutablePathRef p = CGPathCreateMutable();
    add_rrect8(p, NULL, a[1].f, a[2].f, a[3].f, a[4].f, rad, false);
    shape(v, p, a[7].l, a[8].j);
    CGPathRelease(p);
    return true;
}
NAT(G_cvDrawOval)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0);
    if (!v) return true;
    CGPathRef p = CGPathCreateWithEllipseInRect(CGRectMake(a[1].f, a[2].f, a[3].f - a[1].f, a[4].f - a[2].f), NULL);
    shape(v, p, a[5].l, a[6].j);
    CGPathRelease(p);
    return true;
}
/* (long, l, t, r, b, float start, float sweep, boolean useCenter, int[] paint, long shader) */
NAT(G_cvDrawArc)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0);
    if (!v) return true;
    CGMutablePathRef p = CGPathCreateMutable();
    float l = a[1].f, t = a[2].f, r = a[3].f, b = a[4].f;
    if (a[7].z) {
        CGPathMoveToPoint(p, NULL, (l + r) / 2, (t + b) / 2);
        add_arc(p, l, t, r, b, a[5].f, a[6].f, false);
        CGPathCloseSubpath(p);
    } else add_arc(p, l, t, r, b, a[5].f, a[6].f, true);
    shape(v, p, a[8].l, a[9].j);
    CGPathRelease(p);
    return true;
}
/* (long, float[] path, int n, int fill, int[] paint, long shader) */
NAT(G_cvDrawPath)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0); float *f = arr_data(a[1].l);
    if (!v || !f) return true;
    CGPathRef p = build_path(f, a[2].i);
    draw_path(v->c, p, paint_of(a[4].l), (gshader *)(uintptr_t)a[5].j, a[3].i == 1);
    CGPathRelease(p);
    return true;
}
/* (long, float[] pts, int offset, int count (floats), int[] paint, long shader, boolean points) */
NAT(G_cvDrawLines)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0); float *f = arr_data(a[1].l);
    if (!v || !f) return true;
    int off = a[2].i, n = a[3].i;
    if (off < 0 || (uint32_t)(off + n) > arr_len(a[1].l)) return true;
    const int32_t *p = paint_of(a[4].l);
    int32_t q[P_COUNT]; memcpy(q, p, sizeof(q));
    q[P_STYLE] = 1;
    CGMutablePathRef path = CGPathCreateMutable();
    if (a[6].z) {
        float sw = pf(p, P_STROKE) > 0 ? pf(p, P_STROKE) : 1;
        q[P_STYLE] = 0;
        for (int i = off; i + 1 < off + n; i += 2) {
            if (p[P_CAP] == 1) CGPathAddEllipseInRect(path, NULL, CGRectMake(f[i] - sw / 2, f[i + 1] - sw / 2, sw, sw));
            else CGPathAddRect(path, NULL, CGRectMake(f[i] - sw / 2, f[i + 1] - sw / 2, sw, sw));
        }
    } else for (int i = off; i + 3 < off + n; i += 4) { CGPathMoveToPoint(path, NULL, f[i], f[i + 1]); CGPathAddLineToPoint(path, NULL, f[i + 2], f[i + 3]); }
    draw_path(v->c, path, q, (gshader *)(uintptr_t)a[5].j, false);
    CGPathRelease(path);
    return true;
}
/* (long, long bm, sl, st, sr, sb, dl, dt, dr, db, int[] paint or null) */
NAT(G_cvDrawBitmap)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0); gbitmap *b = H(gbitmap, 1);
    if (!v || !b) return true;
    const int32_t *p = paint_of(a[10].l);
    int32_t q[P_COUNT]; memcpy(q, p, sizeof(q));
    if (!a[10].l) q[P_FLAGS] |= PF_FILTER;
    float sl = a[2].f, st = a[3].f, sr = a[4].f, sb = a[5].f, dl = a[6].f, dt = a[7].f, dr = a[8].f, db = a[9].f;
    if (sr - sl <= 0 || sb - st <= 0 || dr - dl == 0 || db - dt == 0) return true;
    CGImageRef full = bm_image(b);
    bool whole = sl == 0 && st == 0 && sr == b->w && sb == b->h;
    CGImageRef img = whole ? full : CGImageCreateWithImageInRect(full, CGRectMake(sl, st, sr - sl, sb - st));
    CGContextRef c = v->c;
    bool layered;
    if (img && paint_begin(c, q, &layered)) {
        CGContextSetFillColorWithColor(c, CGColorGetConstantColor(kCGColorBlack));
        CGContextSetAlpha(c, (float)((uint32_t)q[P_COLOR] >> 24) / 255.0f);
        CGContextSaveGState(c);
        CGContextTranslateCTM(c, dl, db); CGContextScaleCTM(c, 1, -1);
        CGContextDrawImage(c, CGRectMake(0, 0, dr - dl, db - dt), img);
        CGContextRestoreGState(c);
        paint_end(c, q, layered, CGRectMake(dl, dt, dr - dl, db - dt));
    }
    if (img && !whole) CGImageRelease(img);
    CGImageRelease(full);
    return true;
}
/* (long, long bm, float[6] matrix, int[] paint or null) */
NAT(G_cvDrawBitmapM)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0); gbitmap *b = H(gbitmap, 1); float *m = arr_data(a[2].l);
    if (!v || !b || !m) return true;
    const int32_t *p = paint_of(a[3].l);
    int32_t q[P_COUNT]; memcpy(q, p, sizeof(q));
    if (!a[3].l) q[P_FLAGS] |= PF_FILTER;
    CGImageRef img = bm_image(b);
    CGContextRef c = v->c;
    bool layered;
    if (paint_begin(c, q, &layered)) {
        CGContextSetAlpha(c, (float)((uint32_t)q[P_COLOR] >> 24) / 255.0f);
        CGContextSaveGState(c);
        CGContextConcatCTM(c, CGAffineTransformMake(m[0], m[3], m[1], m[4], m[2], m[5]));
        CGContextTranslateCTM(c, 0, b->h); CGContextScaleCTM(c, 1, -1);
        CGContextDrawImage(c, CGRectMake(0, 0, b->w, b->h), img);
        CGContextRestoreGState(c);
        paint_end(c, q, layered, CGRectApplyAffineTransform(CGRectMake(0, 0, b->w, b->h), CGAffineTransformMake(m[0], m[3], m[1], m[4], m[2], m[5])));
    }
    CGImageRelease(img);
    return true;
}
/* (long, String, int start, int end, float x, float y, int[] paint, long typeface, long shader) */
NAT(G_cvDrawText)
{
    (void)self; (void)ret;
    gcanvas *v = H(gcanvas, 0);
    if (!v || !a[1].l) return true;
    draw_text(v, a[1].l, a[2].i, a[3].i, a[4].f, a[5].f, paint_of(a[6].l), (gface *)(uintptr_t)a[7].j, (gshader *)(uintptr_t)a[8].j);
    return true;
}

/* ---- text measuring: (String, int start, int end, int[] paint, long typeface, ...) */
NAT(G_txMeasure)
{
    (void)self;
    CFStringRef s = cfstr(a[0].l, a[1].i, a[2].i);
    const int32_t *p = paint_of(a[3].l);
    CTLineRef line = make_line(s, p, (gface *)(uintptr_t)a[4].j);
    *ret = Ff(line_width(line, p));
    CFRelease(line); CFRelease(s);
    return true;
}
/* (int[] paint, long typeface, float[] out: top ascent descent bottom leading) -> float spacing */
NAT(G_txMetrics)
{
    (void)self;
    const int32_t *p = paint_of(a[0].l);
    CTFontRef f = sized((gface *)(uintptr_t)a[1].j, pf(p, P_TEXTSIZE));
    float *o = arr_data(a[2].l);
    float asc = (float)CTFontGetAscent(f), desc = (float)CTFontGetDescent(f), lead = (float)CTFontGetLeading(f);
    CGRect bb = CTFontGetBoundingBox(f);
    if (o && arr_len(a[2].l) >= 5) {
        o[0] = -(float)fmax(bb.origin.y + bb.size.height, asc);
        o[1] = -asc; o[2] = desc;
        o[3] = (float)fmax(-bb.origin.y, desc);
        o[4] = lead;
    }
    *ret = Ff(asc + desc + lead);
    CFRelease(f);
    return true;
}
/* (String, int start, int end, int[] paint, long typeface, int[] outRect) */
NAT(G_txBounds)
{
    (void)self; (void)ret;
    CFStringRef s = cfstr(a[0].l, a[1].i, a[2].i);
    const int32_t *p = paint_of(a[3].l);
    CTLineRef line = make_line(s, p, (gface *)(uintptr_t)a[4].j);
    int32_t *o = arr_data(a[5].l);
    if (o) {
        CGRect r = CTLineGetImageBounds(line, NULL);
        if (CGRectIsNull(r) || CGRectIsEmpty(r)) { o[0] = o[1] = o[2] = o[3] = 0; }
        else {
            float sx = pf(p, P_SCALEX) > 0 ? pf(p, P_SCALEX) : 1;
            o[0] = (int32_t)floor(r.origin.x * sx); o[2] = (int32_t)ceil((r.origin.x + r.size.width) * sx);
            o[1] = (int32_t)floor(-(r.origin.y + r.size.height)); o[3] = (int32_t)ceil(-r.origin.y);
        }
    }
    CFRelease(line); CFRelease(s);
    return true;
}
/* (String, int start, int end, float maxWidth, int[] paint, long typeface, boolean words) -> chars that fit */
NAT(G_txBreak)
{
    (void)self;
    CFStringRef s = cfstr(a[0].l, a[1].i, a[2].i);
    const int32_t *p = paint_of(a[4].l);
    CTFontRef f = sized((gface *)(uintptr_t)a[5].j, pf(p, P_TEXTSIZE));
    const void *k[] = { kCTFontAttributeName }; const void *v[] = { f };
    CFDictionaryRef attrs = CFDictionaryCreate(NULL, k, v, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFAttributedStringRef as = CFAttributedStringCreate(NULL, s, attrs);
    CTTypesetterRef ts = CTTypesetterCreateWithAttributedString(as);
    float sx = pf(p, P_SCALEX) > 0 ? pf(p, P_SCALEX) : 1;
    double w = a[3].f / sx;
    CFIndex n = a[6].z ? CTTypesetterSuggestLineBreak(ts, 0, w) : CTTypesetterSuggestClusterBreak(ts, 0, w);
    *ret = Ii((int32_t)n);
    CFRelease(ts); CFRelease(as); CFRelease(attrs); CFRelease(s); CFRelease(f);
    return true;
}
/* (String, int start, int end, int[] paint, long typeface, float[] out) -> advances per UTF-16 unit */
NAT(G_txWidths)
{
    (void)self; (void)ret;
    CFStringRef s = cfstr(a[0].l, a[1].i, a[2].i);
    const int32_t *p = paint_of(a[3].l);
    float *o = arr_data(a[5].l);
    CFIndex n = CFStringGetLength(s);
    if (o && (CFIndex)arr_len(a[5].l) >= n) {
        CTLineRef line = make_line(s, p, (gface *)(uintptr_t)a[4].j);
        float sx = pf(p, P_SCALEX) > 0 ? pf(p, P_SCALEX) : 1;
        double prev = 0;
        for (CFIndex i = 0; i < n; i++) {
            double x = CTLineGetOffsetForStringIndex(line, i + 1, NULL);
            o[i] = (float)((x - prev) * sx);
            prev = x;
        }
        CFRelease(line);
    }
    CFRelease(s);
    return true;
}

/* ---- Typeface */
NAT(G_tfCreate) { (void)self; const char *fam = a[0].l ? tl_jni_string(a[0].l) : NULL; *ret = J((int64_t)(uintptr_t)face_family(fam, a[1].i)); return true; }
NAT(G_tfDefault) { (void)self; (void)a; *ret = J((int64_t)(uintptr_t)default_face()); return true; }
NAT(G_tfDerive)
{
    (void)self;
    gface *g = H(gface, 0);
    if (!g) g = default_face();
    int style = a[1].i, weight = a[2].i;
    *ret = J((int64_t)(uintptr_t)face_make(g->base, style, weight > 0 ? weight : ((style & 1) ? 700 : 400)));
    return true;
}
/* (byte[] data) -> long, 0 when it is not a font */
NAT(G_tfFromData)
{
    (void)self;
    uint8_t *d = arr_data(a[0].l); uint32_t n = arr_len(a[0].l);
    *ret = J(0);
    if (!d || !n) return true;
    CFDataRef data = CFDataCreate(NULL, d, n);
    CTFontDescriptorRef desc = CTFontManagerCreateFontDescriptorFromData(data);
    CFRelease(data);
    if (!desc) return true;
    CTFontRef f = CTFontCreateWithFontDescriptor(desc, 12, NULL);
    CFRelease(desc);
    gface *g = calloc(1, sizeof(*g));
    g->base = f; g->weight = 400;
    *ret = J((int64_t)(uintptr_t)g);
    return true;
}
NAT(G_tfWeight) { (void)self; gface *g = H(gface, 0); *ret = Ii(g ? g->weight : 400); return true; }

/* ---- shaders */
static gshader *grad_shader(int kind, jobj *colors, jobj *pos, int tile)
{
    int n = (int)arr_len(colors);
    const float *p = pos && (int)arr_len(pos) >= n ? arr_data(pos) : NULL;
    return shader_new(kind, arr_data(colors), p, n, tile);
}
/* (x0, y0, x1, y1, int[] colors, float[] pos, int tile) */
NAT(G_shLinear)
{
    (void)self;
    gshader *s = grad_shader(SH_LINEAR, a[4].l, a[5].l, a[6].i);
    s->x0 = a[0].f; s->y0 = a[1].f; s->x1 = a[2].f; s->y1 = a[3].f;
    *ret = J((int64_t)(uintptr_t)s);
    return true;
}
/* (cx, cy, r, int[] colors, float[] pos, int tile) */
NAT(G_shRadial)
{
    (void)self;
    gshader *s = grad_shader(SH_RADIAL, a[3].l, a[4].l, a[5].i);
    s->x0 = a[0].f; s->y0 = a[1].f; s->r = a[2].f;
    *ret = J((int64_t)(uintptr_t)s);
    return true;
}
/* (cx, cy, int[] colors, float[] pos) */
NAT(G_shSweep)
{
    (void)self;
    gshader *s = grad_shader(SH_SWEEP, a[2].l, a[3].l, 0);
    s->x0 = a[0].f; s->y0 = a[1].f;
    *ret = J((int64_t)(uintptr_t)s);
    return true;
}
/* (long bm, int tile) */
NAT(G_shBitmap)
{
    (void)self;
    gbitmap *b = H(gbitmap, 0);
    gshader *s = calloc(1, sizeof(*s));
    s->kind = SH_BITMAP; s->tile = a[1].i; s->local = CGAffineTransformIdentity;
    if (b) { s->image = bm_image(b); s->iw = b->w; s->ih = b->h; }
    *ret = J((int64_t)(uintptr_t)s);
    return true;
}
NAT(G_shMatrix)
{
    (void)self; (void)ret;
    gshader *s = H(gshader, 0); float *m = arr_data(a[1].l);
    if (s) s->local = m ? CGAffineTransformMake(m[0], m[3], m[1], m[4], m[2], m[5]) : CGAffineTransformIdentity;
    return true;
}
NAT(G_shFree)
{
    (void)self; (void)ret;
    gshader *s = H(gshader, 0);
    if (!s) return true;
    if (s->grad) CGGradientRelease(s->grad);
    if (s->image) CGImageRelease(s->image);
    free(s->colors); free(s->pos); free(s);
    return true;
}

/* ---- Path queries: (float[] path, int n, ...) */
NAT(G_pathBounds)
{
    (void)self; (void)ret;
    float *f = arr_data(a[0].l), *o = arr_data(a[2].l);
    if (!f || !o) return true;
    CGPathRef p = build_path(f, a[1].i);
    CGRect r = CGPathIsEmpty(p) ? CGRectZero : CGPathGetPathBoundingBox(p);
    o[0] = (float)r.origin.x; o[1] = (float)r.origin.y; o[2] = (float)(r.origin.x + r.size.width); o[3] = (float)(r.origin.y + r.size.height);
    CGPathRelease(p);
    return true;
}
NAT(G_pathContains)
{
    (void)self;
    float *f = arr_data(a[0].l);
    *ret = Ii(0);
    if (!f) return true;
    CGPathRef p = build_path(f, a[1].i);
    *ret = Ii(CGPathContainsPoint(p, NULL, CGPointMake(a[3].f, a[4].f), a[2].i == 1));
    CGPathRelease(p);
    return true;
}
/* (float[] path, int n, float[] out) -> length of the first contour, and out[0..1] its first point (for PathMeasure) */
NAT(G_pathLength)
{
    (void)self;
    float *f = arr_data(a[0].l);
    *ret = Ff(0);
    if (!f) return true;
    CGPathRef p = build_path(f, a[1].i);
    /* flatten by dashing-free stroke approximation: sample the path's elements */
    __block struct st { CGPoint last, first; double len; bool started; } st = { 0 };
    CGPathApplyWithBlock(p, ^(const CGPathElement *e) {
        CGPoint *pt = e->points;
        switch (e->type) {
        case kCGPathElementMoveToPoint: if (!st.started) { st.first = pt[0]; st.started = true; } st.last = pt[0]; break;
        case kCGPathElementAddLineToPoint: st.len += hypot(pt[0].x - st.last.x, pt[0].y - st.last.y); st.last = pt[0]; break;
        case kCGPathElementAddQuadCurveToPoint: case kCGPathElementAddCurveToPoint: {
            int np = e->type == kCGPathElementAddQuadCurveToPoint ? 2 : 3;
            CGPoint p0 = st.last, prev = p0;
            for (int i = 1; i <= 16; i++) {
                double t = i / 16.0, u = 1 - t; CGPoint q;
                if (np == 2) { q.x = u * u * p0.x + 2 * u * t * pt[0].x + t * t * pt[1].x; q.y = u * u * p0.y + 2 * u * t * pt[0].y + t * t * pt[1].y; }
                else { q.x = u * u * u * p0.x + 3 * u * u * t * pt[0].x + 3 * u * t * t * pt[1].x + t * t * t * pt[2].x; q.y = u * u * u * p0.y + 3 * u * u * t * pt[0].y + 3 * u * t * t * pt[1].y + t * t * t * pt[2].y; }
                st.len += hypot(q.x - prev.x, q.y - prev.y); prev = q;
            }
            st.last = pt[np - 1]; break; }
        case kCGPathElementCloseSubpath: st.len += hypot(st.first.x - st.last.x, st.first.y - st.last.y); st.last = st.first; break;
        }
    });
    *ret = Ff((float)st.len);
    CGPathRelease(p);
    return true;
}

/* ================================================================== the frame on screen */

static struct {
    pthread_mutex_t lock;
    uint8_t *px; int w, h;          /* the latest frame the view system drew */
    atomic_bool fresh;
    atomic_bool any;
} S = { .lock = PTHREAD_MUTEX_INITIALIZER };

/* husk.Native.present(long bitmap): the window's views, drawn; the GL thread puts them on screen. */
NAT(G_present)
{
    (void)self; (void)ret;
    gbitmap *b = H(gbitmap, 0);
    if (!b) { atomic_store(&S.any, false); atomic_store(&S.fresh, true); return true; }
    pthread_mutex_lock(&S.lock);
    if (S.w != b->w || S.h != b->h) { free(S.px); S.px = malloc(b->stride * (size_t)b->h); S.w = b->w; S.h = b->h; }
    memcpy(S.px, b->px, b->stride * (size_t)b->h);
    pthread_mutex_unlock(&S.lock);
    atomic_store(&S.any, true);
    atomic_store(&S.fresh, true);
    return true;
}

/* For the GL thread: when there is a new frame, its pixels (locked until tl_ui_frame_done). */
bool tl_ui_frame_take(const uint8_t **px, int *w, int *h, bool *any)
{
    if (!atomic_exchange(&S.fresh, false)) return false;
    pthread_mutex_lock(&S.lock);
    *px = S.px; *w = S.w; *h = S.h; *any = atomic_load(&S.any);
    return true;
}
void tl_ui_frame_done(void) { pthread_mutex_unlock(&S.lock); }
bool tl_ui_has_frame(void) { return atomic_load(&S.any); }

static const struct { const char *name, *sig; dvm_native_fn fn; } k_gfx[] = {
    { "bmNew", "(II)J", G_bmNew },
    { "bmFree", "(J)V", G_bmFree },
    { "bmErase", "(JI)V", G_bmErase },
    { "bmGetPixel", "(JII)I", G_bmGetPixel },
    { "bmSetPixel", "(JIII)V", G_bmSetPixel },
    { "bmPixels", "(J[IIIIIIIZ)V", G_bmPixels },
    { "bmRaw", "(JJZ)V", G_bmRaw },
    { "bmAddress", "(J)J", G_bmAddress },
    { "bmDecode", "([BIII[IZ)J", G_bmDecode },
    { "bmCompress", "(JII)[B", G_bmCompress },
    { "bmCopy", "(JIIII[FZ)J", G_bmCopy },
    { "cvNew", "(J)J", G_cvNew },
    { "cvFree", "(J)V", G_cvFree },
    { "cvSave", "(J)I", G_cvSave },
    { "cvSaveLayer", "(JFFFFI)I", G_cvSaveLayer },
    { "cvRestore", "(J)V", G_cvRestore },
    { "cvRestoreTo", "(JI)V", G_cvRestoreTo },
    { "cvSaveCount", "(J)I", G_cvSaveCount },
    { "cvMatrix", "(J[FZ)V", G_cvMatrix },
    { "cvGetMatrix", "(J[F)V", G_cvGetMatrix },
    { "cvClipRect", "(JFFFFI)Z", G_cvClipRect },
    { "cvClipPath", "(J[FIII)Z", G_cvClipPath },
    { "cvClipBounds", "(J[I)Z", G_cvClipBounds },
    { "cvDrawColor", "(JII)V", G_cvDrawColor },
    { "cvDrawRect", "(JFFFF[IJ)V", G_cvDrawRect },
    { "cvDrawRRect", "(JFFFFFF[IJ)V", G_cvDrawRRect },
    { "cvDrawOval", "(JFFFF[IJ)V", G_cvDrawOval },
    { "cvDrawArc", "(JFFFFFFZ[IJ)V", G_cvDrawArc },
    { "cvDrawPath", "(J[FII[IJ)V", G_cvDrawPath },
    { "cvDrawLines", "(J[FII[IJZ)V", G_cvDrawLines },
    { "cvDrawBitmap", "(JJFFFFFFFF[I)V", G_cvDrawBitmap },
    { "cvDrawBitmapM", "(JJ[F[I)V", G_cvDrawBitmapM },
    { "cvDrawText", "(JLjava/lang/String;IIFF[IJJ)V", G_cvDrawText },
    { "txMeasure", "(Ljava/lang/String;II[IJ)F", G_txMeasure },
    { "txMetrics", "([IJ[F)F", G_txMetrics },
    { "txBounds", "(Ljava/lang/String;II[IJ[I)V", G_txBounds },
    { "txBreak", "(Ljava/lang/String;IIF[IJZ)I", G_txBreak },
    { "txWidths", "(Ljava/lang/String;II[IJ[F)V", G_txWidths },
    { "tfCreate", "(Ljava/lang/String;I)J", G_tfCreate },
    { "tfDefault", "()J", G_tfDefault },
    { "tfDerive", "(JII)J", G_tfDerive },
    { "tfFromData", "([B)J", G_tfFromData },
    { "tfWeight", "(J)I", G_tfWeight },
    { "shLinear", "(FFFF[I[FI)J", G_shLinear },
    { "shRadial", "(FFF[I[FI)J", G_shRadial },
    { "shSweep", "(FF[I[F)J", G_shSweep },
    { "shBitmap", "(JI)J", G_shBitmap },
    { "shMatrix", "(J[F)V", G_shMatrix },
    { "shFree", "(J)V", G_shFree },
    { "pathBounds", "([FI[F)V", G_pathBounds },
    { "pathContains", "([FIIFF)Z", G_pathContains },
    { "pathLength", "([FI[F)F", G_pathLength },
    { "present", "(J)V", G_present },
    { NULL, NULL, NULL },
};

dvm_native_fn tl_gfx_native(const char *name, const char *sig);
dvm_native_fn tl_gfx_native(const char *name, const char *sig)
{
    for (int i = 0; k_gfx[i].name; i++) if (!strcmp(k_gfx[i].name, name) && !strcmp(k_gfx[i].sig, sig)) return k_gfx[i].fn;
    return NULL;
}
