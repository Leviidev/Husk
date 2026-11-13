/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "husk-tl-framework.h"
#include "husk-tl-sound.h"
#include "husk-tl-res.h"
#include "husk-tl-blit.h"
#include "husk-tl-prefs.h"
#include "husk-tl-dex.h"
#include "husk-tl-internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void tl_log_line(const char *fmt, ...);

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#include <pthread.h>

__attribute__((weak)) bool tl_host_display_wide_color(void);

static CGColorSpaceRef tl_fw_srgb_cs, tl_fw_p3_cs, tl_fw_fallback_cs;
static pthread_once_t tl_fw_cs_once = PTHREAD_ONCE_INIT;

static void tl_fw_cs_init(void)
{
    tl_fw_srgb_cs = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    tl_fw_p3_cs = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
    if (!tl_fw_srgb_cs) tl_fw_fallback_cs = CGColorSpaceCreateDeviceRGB();
}

static CGColorSpaceRef tl_fw_rgb_color_space(void)
{
    pthread_once(&tl_fw_cs_once, tl_fw_cs_init);
    if (tl_host_display_wide_color && tl_host_display_wide_color() && tl_fw_p3_cs) return tl_fw_p3_cs;
    return tl_fw_srgb_cs ? tl_fw_srgb_cs : tl_fw_fallback_cs;
}
#endif

/* ----------------------------------------------------- Data Structures */

typedef struct {
    int width;
    int height;
    uint32_t *pixels;       /* premultiplied RGBA, bytes in that order */
    int opaque;             /* 0 not yet known, 1 every pixel opaque, 2 some are not */
#if defined(__APPLE__)
    CGImageRef cg_image;
#endif
} tl_framework_bitmap;

typedef struct {
    float m[9];             /* 3x3 matrix in row-major order: [sx kx tx, ky sy ty, 0 0 1] */
} tl_framework_matrix;

typedef struct {
    uint32_t color;         /* ARGB */
    int alpha;
    bool filter;
    bool antialias;
} tl_framework_paint;

typedef struct {
    float left, top, right, bottom;
} tl_framework_rect;

typedef struct {
    int capacity;
    int size;
    tl_dex_val *items;
} tl_framework_list;

typedef struct {
    tl_framework_list *list;
    int cursor;
} tl_framework_iterator;

typedef struct {
    CGContextRef cg_ctx;
    /* The app's resource table, and the buffer it reads from. Loaded on first
     * use: most frames never ask for a resource, and parsing a 1.4 MB table
     * at startup would be paid by apps that never do. */
    tl_res *res;
    uint8_t *arsc;
    bool res_tried;
    tl_dex_object *canvas_obj;
    tl_dex_object *activity_obj;
    tl_dex_object *resources_obj;
    tl_dex_object *choreographer_obj;
    tl_dex_object *prefs_obj;
    tl_dex_object *editor_obj;
    tl_prefs *prefs;            /* what SharedPreferences reads and the Editor writes */
} tl_framework_state;

static void matrix_identity(tl_framework_matrix *mat)
{
    mat->m[0] = 1.0f; mat->m[1] = 0.0f; mat->m[2] = 0.0f;
    mat->m[3] = 0.0f; mat->m[4] = 1.0f; mat->m[5] = 0.0f;
    mat->m[6] = 0.0f; mat->m[7] = 0.0f; mat->m[8] = 1.0f;
}

static void matrix_multiply(tl_framework_matrix *dst, const tl_framework_matrix *a, const tl_framework_matrix *b)
{
    tl_framework_matrix res;
    for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) {
            res.m[r * 3 + c] = a->m[r * 3 + 0] * b->m[0 * 3 + c] +
                               a->m[r * 3 + 1] * b->m[1 * 3 + c] +
                               a->m[r * 3 + 2] * b->m[2 * 3 + c];
        }
    }
    memcpy(dst, &res, sizeof(res));
}

/* ------------------------------------------------------------- Resources */

/*
 * The screen density resource lookups are answered for.
 *
 * Android picks, among the versions of a drawable made for different densities,
 * the one nearest the device's, then scales it so it comes out the same physical
 * size. Only the first half is done here: the highest density wins and the
 * bitmap is used at its own pixel size. For an app that ships one copy of a
 * drawable the two are the same thing.
 */
#define TL_FRAMEWORK_DENSITY_DPI 480

/*
 * The app's resource table, loaded the first time anything asks.
 *
 * The bytes are copied into a buffer owned here. tl_zip_data hands back a
 * pointer into the APK's mapping when the entry is stored uncompressed, and
 * that mapping goes away with tl_zip_close -- while the table keeps pointing
 * into whatever it was given for as long as the app runs.
 */
static tl_res *framework_res(tl_dex_context *ctx)
{
    tl_framework_state *st = ctx ? ctx->framework_data : NULL;
    if (!st) return NULL;
    if (st->res || st->res_tried) return st->res;
    st->res_tried = true;
    if (!ctx->apk_path) return NULL;

    tl_zip z;
    char zerr[128] = {0};
    if (!tl_zip_open(&z, ctx->apk_path, zerr, sizeof(zerr))) return NULL;

    const tl_zip_entry *entry = tl_zip_find(&z, "resources.arsc");
    const uint8_t *data = NULL;
    size_t len = 0;
    bool owned = false;
    if (entry && tl_zip_data(&z, entry, 64 * 1024 * 1024, &data, &len, &owned,
                             zerr, sizeof(zerr))) {
        st->arsc = malloc(len);
        if (st->arsc) {
            memcpy(st->arsc, data, len);
            st->res = tl_res_create(st->arsc, len);
            if (!st->res) { free(st->arsc); st->arsc = NULL; }
        }
        if (owned) free((void *)data);
    }
    tl_zip_close(&z);
    return st->res;
}

/* ----------------------------------------------------- Asset & Bitmap Loading */

static tl_framework_bitmap *load_png_from_apk(const char *apk_path, const char *entry_name)
{
    tl_zip z;
    char zerr[128] = {0};
    if (!tl_zip_open(&z, apk_path, zerr, sizeof(zerr))) return NULL;

    const tl_zip_entry *entry = tl_zip_find(&z, entry_name);
    if (!entry) {
        tl_zip_close(&z);
        return NULL;
    }

    const uint8_t *data = NULL;
    size_t len = 0;
    bool owned = false;
    if (!tl_zip_data(&z, entry, 32 * 1024 * 1024, &data, &len, &owned, zerr, sizeof(zerr))) {
        tl_zip_close(&z);
        return NULL;
    }

    tl_framework_bitmap *bmp = NULL;
#if defined(__APPLE__)
    CFDataRef cf_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, data, len, kCFAllocatorNull);
    CGImageSourceRef isrc = CGImageSourceCreateWithData(cf_data, NULL);
    CGImageRef img = isrc ? CGImageSourceCreateImageAtIndex(isrc, 0, NULL) : NULL;

    if (img) {
        bmp = calloc(1, sizeof(*bmp));
        bmp->width = (int)CGImageGetWidth(img);
        bmp->height = (int)CGImageGetHeight(img);
        bmp->pixels = calloc(bmp->width * bmp->height, sizeof(uint32_t));
        bmp->cg_image = img;

        /* Rasterize pixels in ARGB format */
        CGColorSpaceRef cs = tl_fw_rgb_color_space();
        CGContextRef c = CGBitmapContextCreate(bmp->pixels, bmp->width, bmp->height, 8,
                                               bmp->width * 4, cs,
                                               kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGContextDrawImage(c, CGRectMake(0, 0, bmp->width, bmp->height), img);
        CGContextRelease(c);
    }
    if (isrc) CFRelease(isrc);
    if (cf_data) CFRelease(cf_data);
#endif

    if (owned) free((void *)data);
    tl_zip_close(&z);
    return bmp;
}

/* ----------------------------------------------------- Native Framework Methods */

/* java/lang/Object */
static bool obj_init(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs; (void)ret;
    return true;
}

/* android/graphics/Matrix */
static bool matrix_init(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    tl_framework_matrix *m = calloc(1, sizeof(*m));
    matrix_identity(m);
    tl_dex_set_native(this_obj, m);
    return true;
}

static bool matrix_reset(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        matrix_identity((tl_framework_matrix *)tl_dex_native(this_obj));
    }
    return true;
}

static bool matrix_postTranslate(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_matrix *m = tl_dex_native(this_obj);
        float dx = args[1].f;
        float dy = args[2].f;
        tl_framework_matrix t;
        matrix_identity(&t);
        t.m[2] = dx;
        t.m[5] = dy;
        matrix_multiply(m, &t, m);
    }
    if (ret) ret->i = 1;
    return true;
}

static bool matrix_postRotate(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_matrix *m = tl_dex_native(this_obj);
        float degrees = args[1].f;
        float rad = degrees * (float)(M_PI / 180.0);
        float c = cosf(rad);
        float s = sinf(rad);

        tl_framework_matrix r;
        matrix_identity(&r);
        r.m[0] = c;  r.m[1] = -s;
        r.m[3] = s;  r.m[4] = c;

        if (nargs >= 4) {
            float px = args[2].f;
            float py = args[3].f;
            tl_framework_matrix t1, t2;
            matrix_identity(&t1);
            t1.m[2] = -px; t1.m[5] = -py;
            matrix_identity(&t2);
            t2.m[2] = px;  t2.m[5] = py;

            tl_framework_matrix tmp;
            matrix_multiply(&tmp, &r, &t1);
            matrix_multiply(&tmp, &t2, &tmp);
            matrix_multiply(m, &tmp, m);
        } else {
            matrix_multiply(m, &r, m);
        }
    }
    if (ret) ret->i = 1;
    return true;
}

static bool matrix_postScale(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_matrix *m = tl_dex_native(this_obj);
        float sx = args[1].f;
        float sy = args[2].f;
        tl_framework_matrix s;
        matrix_identity(&s);
        s.m[0] = sx;
        s.m[4] = sy;
        matrix_multiply(m, &s, m);
    }
    if (ret) ret->i = 1;
    return true;
}

/* android/graphics/Paint */
static bool paint_init(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    tl_framework_paint *p = calloc(1, sizeof(*p));
    p->color = 0xffffffff;
    p->alpha = 255;
    tl_dex_set_native(this_obj, p);
    return true;
}

static bool paint_setFilterBitmap(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        ((tl_framework_paint *)tl_dex_native(this_obj))->filter = (args[1].i != 0);
    }
    return true;
}

static bool paint_setAntiAlias(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        ((tl_framework_paint *)tl_dex_native(this_obj))->antialias = (args[1].i != 0);
    }
    return true;
}

static bool paint_setARGB(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_paint *p = tl_dex_native(this_obj);
        int a = args[1].i & 0xff;
        int r = args[2].i & 0xff;
        int g = args[3].i & 0xff;
        int b = args[4].i & 0xff;
        p->alpha = a;
        p->color = (uint32_t)((a << 24) | (r << 16) | (g << 8) | b);
    }
    return true;
}

static bool paint_setColor(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_paint *p = tl_dex_native(this_obj);
        p->color = args[1].raw32;
        p->alpha = (int)((p->color >> 24) & 0xff);
    }
    return true;
}

static bool paint_setAlpha(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_paint *p = tl_dex_native(this_obj);
        p->alpha = args[1].i & 0xff;
        p->color = (p->color & 0x00ffffff) | ((uint32_t)p->alpha << 24);
    }
    return true;
}

static bool paint_getAlpha(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    int a = 255;
    if (this_obj && tl_dex_native(this_obj)) {
        a = ((tl_framework_paint *)tl_dex_native(this_obj))->alpha;
    }
    if (ret) ret->i = a;
    return true;
}

static bool paint_setColorFilter(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs; (void)ret;
    return true;
}

/* android/graphics/Rect & RectF */

/*
 * Rect's public fields, over the same native struct its methods use.
 *
 * Rect (ints) and RectF (floats) share one float representation here, so a
 * field access is a pointer into it. Reads convert to the field's declared type
 * and writes convert from it, which is what makes `rect.top = 5` and
 * `rect.set(0, 5, 0, 0)` agree about what the rectangle is.
 */
static float *rect_field_ptr(tl_framework_rect *r, const char *name)
{
    if (!strcmp(name, "left"))   return &r->left;
    if (!strcmp(name, "top"))    return &r->top;
    if (!strcmp(name, "right"))  return &r->right;
    if (!strcmp(name, "bottom")) return &r->bottom;
    return NULL;
}

/*
 * Copy one rectangle's four edges into another. Rect and RectF both have a copy
 * form -- `set(Rect)` and `new RectF(Rect)` -- and a game uses it to turn a hit
 * box into the rectangle it draws into. Handling only the four-number form left
 * every copied rectangle at zero, which draw calls then skip as empty: the
 * buttons were drawn every frame, into nothing.
 */
static bool rect_copy_from(tl_framework_rect *dst, const tl_dex_object *src_obj)
{
    const tl_framework_rect *src = tl_dex_native(src_obj);
    if (!dst || !src) return false;
    *dst = *src;
    return true;
}

static bool is_rect_class(const char *owner)
{
    return owner && (!strcmp(owner, "Landroid/graphics/Rect;") ||
                     !strcmp(owner, "Landroid/graphics/RectF;"));
}

static bool rect_init_void(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    tl_framework_rect *r = calloc(1, sizeof(*r));
    tl_dex_set_native(this_obj, r);
    return true;
}

static bool rect_init_int(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)ret;
    tl_framework_rect *r = calloc(1, sizeof(*r));
    if (nargs == 2) rect_copy_from(r, args[1].l);   /* the copy constructor */
    if (nargs >= 5) {
        r->left = (float)args[1].i;
        r->top = (float)args[2].i;
        r->right = (float)args[3].i;
        r->bottom = (float)args[4].i;
    }
    tl_dex_set_native(this_obj, r);
    return true;
}

static bool rect_init_float(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)ret;
    tl_framework_rect *r = calloc(1, sizeof(*r));
    if (nargs == 2) rect_copy_from(r, args[1].l);   /* the copy constructor */
    if (nargs >= 5) {
        r->left = args[1].f;
        r->top = args[2].f;
        r->right = args[3].f;
        r->bottom = args[4].f;
    }
    tl_dex_set_native(this_obj, r);
    return true;
}

static bool rect_set_int(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (nargs == 2) { rect_copy_from(tl_dex_native(this_obj), args[1].l); return true; }
    if (this_obj && tl_dex_native(this_obj) && nargs >= 5) {
        tl_framework_rect *r = tl_dex_native(this_obj);
        r->left   = (float)args[1].i;
        r->top    = (float)args[2].i;
        r->right  = (float)args[3].i;
        r->bottom = (float)args[4].i;
    }
    return true;
}

static bool rect_set_float(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs; (void)ret;
    if (nargs == 2) { rect_copy_from(tl_dex_native(this_obj), args[1].l); return true; }
    if (this_obj && tl_dex_native(this_obj) && nargs >= 5) {
        tl_framework_rect *r = tl_dex_native(this_obj);
        r->left   = args[1].f;
        r->top    = args[2].f;
        r->right  = args[3].f;
        r->bottom = args[4].f;
    }
    return true;
}

/*
 * RectF.offset(dx, dy) and Rect.offset(dx, dy): slide a rectangle without
 * resizing it. Games move their sprites' bounding boxes with it every frame.
 * Rect takes ints and RectF floats; the shorty tells them apart, and both
 * land on the same float storage.
 */
static bool rect_offset(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)ret;
    tl_framework_rect *r = tl_dex_native(this_obj);
    if (r && nargs >= 3) {
        r->left += args[1].f;  r->right  += args[1].f;
        r->top  += args[2].f;  r->bottom += args[2].f;
    }
    return true;
}
static bool rect_offset_int(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)ret;
    tl_framework_rect *r = tl_dex_native(this_obj);
    if (r && nargs >= 3) {
        r->left += (float)args[1].i;  r->right  += (float)args[1].i;
        r->top  += (float)args[2].i;  r->bottom += (float)args[2].i;
    }
    return true;
}

/*
 * java.util.concurrent.ExecutorService, run on the caller's thread.
 *
 * This layer interprets on a single thread and has no scheduler. A task handed
 * to an executor is therefore run immediately, to completion, inside submit() --
 * which is what a direct executor does, and is correct for anything that only
 * needs its task to happen. (Code that needs the task to happen LATER, or in
 * parallel with the caller, would behave differently; nothing seen so far does.)
 * It returns no Future: the games seen so far fire and forget.
 */
static bool executor_submit(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    if (ret) ret->raw64 = 0;
    tl_dex_object *task = nargs >= 2 ? args[1].l : NULL;
    if (task && task->clazz) {
        tl_dex_method *run = tl_dex_find_method(task->clazz, "run", "V");
        if (!run) run = tl_dex_find_method(task->clazz, "call", NULL);
        if (run) {
            tl_dex_val a[1];
            a[0].raw64 = 0;
            a[0].l = task;
            tl_dex_invoke(ctx, run, a, 1, NULL);
        }
    }
    return true;
}

static bool return_false(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)this_obj; (void)args; (void)nargs; if (ret) ret->raw64 = 0; return true; }

/*
 * View.post / postDelayed / removeCallbacks, and the same three on Handler.
 *
 * They hand the runnable to the layer's task queue (see tl_dex_post_delayed);
 * the receiver doesn't matter, because there is one queue and one thread.
 * postDelayed takes its delay as a long, which occupies two argument slots, so
 * the first argument after it would be at args[4] -- there is none here.
 */
static bool view_post(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    if (nargs >= 2) tl_dex_post_delayed(ctx, args[1].l, 0);
    if (ret) { ret->raw64 = 0; ret->i = 1; }
    return true;
}
static bool view_postDelayed(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    int64_t ms = nargs >= 3 ? args[2].j : 0;
    if (ms < 0) ms = 0;
    if (nargs >= 2) tl_dex_post_delayed(ctx, args[1].l, (uint64_t)ms);
    if (ret) { ret->raw64 = 0; ret->i = 1; }
    return true;
}
static bool view_removeCallbacks(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    if (nargs >= 2) tl_dex_remove_callbacks(ctx, args[1].l);
    if (ret) ret->raw64 = 0;
    return true;
}
static bool return_true(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)this_obj; (void)args; (void)nargs; if (ret) { ret->raw64 = 0; ret->i = 1; } return true; }

/*
 * java.lang.String.
 *
 * Strings are kept as UTF-8, but Java's length() and charAt() count UTF-16 code
 * units, and the two agree only for ASCII. Anything that indexes goes through
 * these helpers, which take the ASCII shortcut when it applies and otherwise
 * decode properly -- so a string with an emoji does not have a different length
 * on Android than here.
 */
static size_t utf16_length(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) == 0x80) continue;          /* continuation byte */
        n += (*p >= 0xF0) ? 2 : 1;                  /* astral: a surrogate pair */
    }
    return n;
}

static int utf16_at(const char *s, int index)
{
    int unit = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        uint32_t cp; int len;
        if (*p < 0x80)       { cp = *p;                         len = 1; }
        else if (*p < 0xE0)  { cp = *p & 0x1F;                  len = 2; }
        else if (*p < 0xF0)  { cp = *p & 0x0F;                  len = 3; }
        else                 { cp = *p & 0x07;                  len = 4; }
        for (int i = 1; i < len && p[i]; i++) cp = (cp << 6) | (p[i] & 0x3F);
        if (cp >= 0x10000) {
            cp -= 0x10000;
            if (unit == index)     return 0xD800 + (int)(cp >> 10);
            if (unit + 1 == index) return 0xDC00 + (int)(cp & 0x3FF);
            unit += 2;
        } else {
            if (unit == index) return (int)cp;
            unit++;
        }
        p += len;
    }
    return 0;
}

static bool string_length(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)args; (void)nargs; const char *s = tl_dex_string(this_obj);
  if (ret) { ret->raw64 = 0; ret->i = s ? (int32_t)utf16_length(s) : 0; } return true; }

static bool string_charAt(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)nargs; const char *s = tl_dex_string(this_obj);
  int idx = args[1].i;
  if (ret) { ret->raw64 = 0; ret->i = (s && idx >= 0 && (size_t)idx < utf16_length(s)) ? utf16_at(s, idx) : 0; }
  return true; }

/* String.toCharArray(): the UTF-16 code units, two bytes each, as a char[]. */
static bool string_toCharArray(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    if (!ret) return true;
    ret->raw64 = 0;
    const char *s = tl_dex_string(this_obj);
    if (!s) return true;
    size_t n = utf16_length(s);
    tl_dex_object *arr = tl_dex_alloc_array(NULL, (uint32_t)n, 2);
    if (!arr || !arr->array.elements) return true;
    uint16_t *out = arr->array.elements;
    for (size_t i = 0; i < n; i++) out[i] = (uint16_t)utf16_at(s, (int)i);
    ret->l = arr;
    return true;
}

static bool string_isEmpty(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)args; (void)nargs; const char *s = tl_dex_string(this_obj);
  if (ret) { ret->raw64 = 0; ret->i = (!s || !*s) ? 1 : 0; } return true; }

static bool string_toString(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)args; (void)nargs; if (ret) { ret->raw64 = 0; ret->l = this_obj; } return true; }

static bool string_equals(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; const char *a = tl_dex_string(this_obj);
  const char *b = nargs >= 2 ? tl_dex_string(args[1].l) : NULL;
  if (ret) { ret->raw64 = 0; ret->i = (a && b && !strcmp(a, b)) ? 1 : 0; } return true; }

/* Java's own hash, so anything that prints or compares hash codes agrees. */
static bool string_hashCode(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)args; (void)nargs; const char *s = tl_dex_string(this_obj);
  uint32_t h = 0;
  if (s) { size_t n = utf16_length(s); for (size_t i = 0; i < n; i++) h = h * 31u + (uint32_t)utf16_at(s, (int)i); }
  if (ret) { ret->raw64 = 0; ret->i = (int32_t)h; } return true; }

static bool string_concat(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    const char *a = tl_dex_string(this_obj), *b = nargs >= 2 ? tl_dex_string(args[1].l) : NULL;
    if (!ret) return true;
    ret->raw64 = 0;
    if (!a) a = "";
    if (!b) b = "";
    size_t la = strlen(a), lb = strlen(b);
    char *both = malloc(la + lb + 1);
    if (!both) return true;
    memcpy(both, a, la); memcpy(both + la, b, lb); both[la + lb] = 0;
    ret->l = tl_dex_alloc_string(ctx, both);
    free(both);
    return true;
}

/* String.valueOf(x): a new string from a primitive. It is static, so the value
 * is args[0] -- including a long or double, which lives whole in that one slot. */
#define STRING_VALUEOF(fname, fmt, expr) \
    static bool string_valueOf_##fname(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret) \
    { (void)this_obj; (void)nargs; char buf[64]; snprintf(buf, sizeof(buf), fmt, expr); \
      if (ret) { ret->raw64 = 0; ret->l = tl_dex_alloc_string(ctx, buf); } return true; }
STRING_VALUEOF(i, "%d",  args[0].i)
STRING_VALUEOF(j, "%lld", (long long)args[0].j)
STRING_VALUEOF(f, "%g",  (double)args[0].f)
STRING_VALUEOF(d, "%g",  args[0].d)
STRING_VALUEOF(z, "%s",  args[0].i ? "true" : "false")

static bool string_valueOf_c(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)nargs;
    char buf[8] = {0};
    uint32_t cp = (uint32_t)args[0].i & 0xFFFF;
    if (cp < 0x80)        { buf[0] = (char)cp; }
    else if (cp < 0x800)  { buf[0] = (char)(0xC0 | (cp >> 6)); buf[1] = (char)(0x80 | (cp & 0x3F)); }
    else                  { buf[0] = (char)(0xE0 | (cp >> 12)); buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F)); buf[2] = (char)(0x80 | (cp & 0x3F)); }
    if (ret) { ret->raw64 = 0; ret->l = tl_dex_alloc_string(ctx, buf); }
    return true;
}

/* valueOf(Object): a string stays itself, and anything else is its own name. */
static bool string_valueOf_l(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)nargs;
    const char *s = tl_dex_string(args[0].l);
    if (ret) { ret->raw64 = 0; ret->l = tl_dex_alloc_string(ctx, s ? s : (args[0].l ? "object" : "null")); }
    return true;
}

/*
 * The boxed number types.
 *
 * Integer.valueOf, Float.valueOf and the rest exist because collections and
 * generics hold objects, not primitives, so every int that goes into a List
 * is boxed on the way in and unboxed on the way out. A box here is a plain
 * object whose first field slot holds the value; there is no Integer class to
 * instantiate, because the framework classes are not loaded from anywhere.
 */
static tl_dex_object *box_make(tl_dex_context *ctx, const char *desc, tl_dex_val v)
{
    tl_dex_object *o = tl_dex_alloc_object(tl_dex_find_class(ctx, desc));
    if (o && o->nfields) { o->fields[0] = v; }
    return o;
}
static tl_dex_val box_value(const tl_dex_object *o)
{
    tl_dex_val z; z.raw64 = 0;
    if (o && tl_dex_kind(o) == TL_KIND_OBJECT && o->nfields) z = o->fields[0];
    return z;
}

#define BOX_VALUEOF(fname, desc, field) \
    static bool fname(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret) \
    { (void)this_obj; (void)nargs; tl_dex_val v; v.raw64 = 0; v.field = args[0].field; \
      if (ret) { ret->raw64 = 0; ret->l = box_make(ctx, desc, v); } return true; }
BOX_VALUEOF(integer_valueOf,   "Ljava/lang/Integer;", i)
BOX_VALUEOF(long_valueOf,      "Ljava/lang/Long;",    j)
BOX_VALUEOF(float_valueOf,     "Ljava/lang/Float;",   f)
BOX_VALUEOF(double_valueOf,    "Ljava/lang/Double;",  d)
BOX_VALUEOF(boolean_valueOf,   "Ljava/lang/Boolean;", i)
BOX_VALUEOF(character_valueOf, "Ljava/lang/Character;", i)

/* The unboxing accessors convert between types the way Java's do: intValue() on
 * a Float truncates, floatValue() on an Integer widens. `from` says what the
 * box holds. */
enum { BOX_I, BOX_J, BOX_F, BOX_D };
static double box_as_double(tl_dex_val v, int from)
{ return from == BOX_I ? (double)v.i : from == BOX_J ? (double)v.j : from == BOX_F ? (double)v.f : v.d; }
static int64_t box_as_long(tl_dex_val v, int from)
{ return from == BOX_I ? (int64_t)v.i : from == BOX_J ? v.j : from == BOX_F ? (int64_t)v.f : (int64_t)v.d; }

#define BOX_ACCESSOR(fname, from, out_kind) \
    static bool fname(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret) \
    { (void)ctx; (void)args; (void)nargs; tl_dex_val v = box_value(this_obj); \
      if (ret) { ret->raw64 = 0; \
        if (out_kind == BOX_I)      ret->i = (int32_t)box_as_long(v, from); \
        else if (out_kind == BOX_J) ret->j = box_as_long(v, from); \
        else if (out_kind == BOX_F) ret->f = (float)box_as_double(v, from); \
        else                        ret->d = box_as_double(v, from); } return true; }
BOX_ACCESSOR(integer_intValue,    BOX_I, BOX_I)
BOX_ACCESSOR(integer_longValue,   BOX_I, BOX_J)
BOX_ACCESSOR(integer_floatValue,  BOX_I, BOX_F)
BOX_ACCESSOR(integer_doubleValue, BOX_I, BOX_D)
BOX_ACCESSOR(long_longValue,      BOX_J, BOX_J)
BOX_ACCESSOR(long_intValue,       BOX_J, BOX_I)
BOX_ACCESSOR(float_floatValue,    BOX_F, BOX_F)
BOX_ACCESSOR(float_intValue,      BOX_F, BOX_I)
BOX_ACCESSOR(float_doubleValue,   BOX_F, BOX_D)
BOX_ACCESSOR(double_doubleValue,  BOX_D, BOX_D)
BOX_ACCESSOR(double_intValue,     BOX_D, BOX_I)
BOX_ACCESSOR(double_floatValue,   BOX_D, BOX_F)

static bool boolean_booleanValue(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)args; (void)nargs; if (ret) { ret->raw64 = 0; ret->i = box_value(this_obj).i ? 1 : 0; } return true; }

static bool integer_parseInt(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)this_obj; (void)nargs; const char *s = tl_dex_string(args[0].l);
  if (ret) { ret->raw64 = 0; ret->i = s ? (int32_t)strtol(s, NULL, 10) : 0; } return true; }

static bool integer_toString_static(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ return string_valueOf_i(ctx, this_obj, args, nargs, ret); }

static bool integer_compare(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)this_obj; (void)nargs;
  if (ret) { ret->raw64 = 0; ret->i = args[0].i < args[1].i ? -1 : args[0].i > args[1].i ? 1 : 0; } return true; }

/*
 * java.lang.Math and java.lang.System.
 *
 * Static methods, so args[] holds the call's own parameters from index 0 with no
 * receiver -- and a long or double takes TWO entries, because the call lists
 * both of its registers. (A wide value lives whole in the lower register of its
 * pair; the upper one is carried along and ignored.) That is why the second
 * operand of a (DD)D is args[2] and the second of an (FF)F is args[1].
 *
 * These were missing, and a missing static returns zero. Math.min and
 * Math.max returning zero turn every clamp into "stay at the edge", and
 * currentTimeMillis returning zero means no game timer ever expires.
 */
#define RET_INT(v)   do { if (ret) { ret->raw64 = 0; ret->i = (int32_t)(v); } } while (0)
#define RET_LONG(v)  do { if (ret) { ret->raw64 = 0; ret->j = (int64_t)(v); } } while (0)
#define RET_FLOAT(v) do { if (ret) { ret->raw64 = 0; ret->f = (float)(v); } } while (0)
#define RET_DOUBLE(v) do { if (ret) { ret->raw64 = 0; ret->d = (double)(v); } } while (0)

#define UNUSED_NATIVE_ARGS (void)ctx; (void)this_obj; (void)nargs

static bool math_min_i(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_INT(args[0].i < args[1].i ? args[0].i : args[1].i); return true; }
static bool math_max_i(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_INT(args[0].i > args[1].i ? args[0].i : args[1].i); return true; }
static bool math_min_j(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_LONG(args[0].j < args[2].j ? args[0].j : args[2].j); return true; }
static bool math_max_j(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_LONG(args[0].j > args[2].j ? args[0].j : args[2].j); return true; }
static bool math_min_f(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_FLOAT(fminf(args[0].f, args[1].f)); return true; }
static bool math_max_f(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_FLOAT(fmaxf(args[0].f, args[1].f)); return true; }
static bool math_min_d(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_DOUBLE(fmin(args[0].d, args[2].d)); return true; }
static bool math_max_d(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_DOUBLE(fmax(args[0].d, args[2].d)); return true; }

static bool math_abs_i(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; int32_t v = args[0].i; RET_INT(v < 0 ? (int32_t)(0u - (uint32_t)v) : v); return true; }
static bool math_abs_j(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; int64_t v = args[0].j; RET_LONG(v < 0 ? (int64_t)(0ull - (uint64_t)v) : v); return true; }
static bool math_abs_f(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_FLOAT(fabsf(args[0].f)); return true; }
static bool math_abs_d(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_DOUBLE(fabs(args[0].d)); return true; }

/* One-double-in, one-double-out functions share a shape. */
#define MATH_D1(fname, expr) \
    static bool math_##fname(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret) \
    { UNUSED_NATIVE_ARGS; double x = args[0].d; RET_DOUBLE(expr); return true; }
MATH_D1(sqrt, sqrt(x))
MATH_D1(sin, sin(x))
MATH_D1(cos, cos(x))
MATH_D1(tan, tan(x))
MATH_D1(asin, asin(x))
MATH_D1(acos, acos(x))
MATH_D1(atan, atan(x))
MATH_D1(exp, exp(x))
MATH_D1(log, log(x))
MATH_D1(log10, log10(x))
MATH_D1(floor, floor(x))
MATH_D1(ceil, ceil(x))
MATH_D1(toRadians, x / 180.0 * 3.14159265358979323846)
MATH_D1(toDegrees, x * 180.0 / 3.14159265358979323846)

static bool math_pow(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_DOUBLE(pow(args[0].d, args[2].d)); return true; }
static bool math_atan2(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_DOUBLE(atan2(args[0].d, args[2].d)); return true; }
static bool math_hypot(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_DOUBLE(hypot(args[0].d, args[2].d)); return true; }

/* Java rounds half up, which is neither C's round() (half away from zero) nor
 * lrint() (half to even): -2.5 rounds to -2, not -3. */
static bool math_round_f(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; double r = floor((double)args[0].f + 0.5);
  RET_INT(r > 2147483647.0 ? 2147483647 : r < -2147483648.0 ? INT32_MIN : (int32_t)r); return true; }
static bool math_round_d(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; double r = floor(args[0].d + 0.5);
  RET_LONG(r >= 9.2233720368547758e18 ? INT64_MAX : r <= -9.2233720368547758e18 ? INT64_MIN : (int64_t)r); return true; }

static bool math_random(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)this_obj; (void)args; (void)nargs; RET_DOUBLE(tl_dex_random(ctx)); return true; }

/* System. The clock is the layer's own; see tl_dex_context.clock_nanos. */
static bool system_nanoTime(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)this_obj; (void)args; (void)nargs; RET_LONG(ctx ? ctx->clock_nanos : 0); return true; }

/* A fixed epoch to count up from, so the value looks like a timestamp (and is
 * the same on every run) without being the machine's clock. */
static bool system_currentTimeMillis(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)this_obj; (void)args; (void)nargs;
  RET_LONG(1700000000000ll + (ctx ? (int64_t)(ctx->clock_nanos / 1000000ull) : 0)); return true; }

/*
 * System.arraycopy(src, srcPos, dest, destPos, length).
 *
 * Java's rule is all or nothing: a range that does not fit throws and copies
 * nothing, so every bound is checked before the first byte moves. memmove,
 * because source and destination may be the same array and overlap -- which is
 * the usual reason to call it.
 */
static bool system_arraycopy(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    UNUSED_NATIVE_ARGS; (void)ret;
    tl_dex_object *src = args[0].l, *dst = args[2].l;
    int32_t sp = args[1].i, dp = args[3].i, len = args[4].i;
    if (!src || !dst || !src->array.elements || !dst->array.elements) return true;
    if (sp < 0 || dp < 0 || len < 0) return true;
    if ((uint64_t)sp + (uint64_t)len > src->array.length) return true;
    if ((uint64_t)dp + (uint64_t)len > dst->array.length) return true;
    if (src->array.elem_size != dst->array.elem_size) return true;
    size_t es = src->array.elem_size;
    memmove((uint8_t *)dst->array.elements + (size_t)dp * es,
            (const uint8_t *)src->array.elements + (size_t)sp * es, (size_t)len * es);
    return true;
}

static bool system_identityHashCode(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ UNUSED_NATIVE_ARGS; RET_INT(((uintptr_t)args[0].l) >> 4); return true; }

/* Matrix.setScale(sx, sy) and setScale(sx, sy, px, py): a fresh scale, about the
 * origin or about a pivot -- unlike postScale, it replaces whatever was there. */
static bool matrix_setScale(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    UNUSED_NATIVE_ARGS; (void)ret;
    if (!this_obj || !tl_dex_native(this_obj)) return true;
    tl_framework_matrix *m = tl_dex_native(this_obj);
    float sx = args[1].f, sy = args[2].f;
    float px = nargs >= 5 ? args[3].f : 0.0f, py = nargs >= 5 ? args[4].f : 0.0f;
    matrix_identity(m);
    m->m[0] = sx;
    m->m[4] = sy;
    m->m[2] = px - sx * px;
    m->m[5] = py - sy * py;
    return true;
}

/*
 * Audio: android.media.SoundPool.
 *
 * SoundPool is built through two builders chained off each other, so every setter
 * has to hand back the builder it was called on -- a null there ends the chain
 * and the game ends up with no pool and a crash on its first sound. load() decodes
 * the sound the resource id names and play() mixes it to the speakers
 * (husk-tl-sound.c).
 */
static bool return_this(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{ (void)ctx; (void)args; (void)nargs; if (ret) { ret->raw64 = 0; ret->l = this_obj; } return true; }

static bool soundpool_build(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    if (ret) {
        ret->raw64 = 0;
        ret->l = tl_dex_alloc_object(tl_dex_find_class(ctx, "Landroid/media/SoundPool;"));
    }
    return true;
}

/* load(Context, int resId, int priority): the sound file the id names, read from the APK and decoded (husk-tl-sound.c). */
static bool soundpool_load(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    int id = 0;
    uint32_t res_id = nargs > 2 ? args[2].raw32 : 0;
    tl_res *res = framework_res(ctx);
    const char *entry = (res && res_id) ? tl_res_file(res, res_id, TL_FRAMEWORK_DENSITY_DPI) : NULL;
    if (entry && ctx->apk_path) {
        tl_zip z;
        char err[128] = {0};
        if (tl_zip_open(&z, ctx->apk_path, err, sizeof(err))) {
            const tl_zip_entry *e = tl_zip_find(&z, entry);
            const uint8_t *data = NULL; size_t len = 0; bool owned = false;
            if (e && tl_zip_data(&z, e, 16u << 20, &data, &len, &owned, err, sizeof(err))) {
                id = tl_sound_load(data, len);
                if (owned) free((void *)data);
            }
            tl_zip_close(&z);
        }
    }
    if (!id) tl_log_line("sound: no sound for resource 0x%08x (%s)", res_id, entry ? entry : "not in the table");
    RET_INT(id);
    return true;
}

/* play(int soundID, float left, float right, int priority, int loop, float rate) -> streamID */
static bool soundpool_play(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj;
    int stream = nargs > 6 ? tl_sound_play(args[1].i, args[2].f, args[3].f, args[5].i, args[6].f) : 0;
    RET_INT(stream);
    return true;
}

static bool soundpool_stop(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)ret;
    if (nargs > 1) tl_sound_stop(args[1].i);
    return true;
}

/*
 * java.lang.Enum.
 *
 * An enum constant is an ordinary object whose first two field slots belong to
 * Enum: its name and its ordinal (see dex_link_class). The compiler-generated
 * static initialiser builds every constant with `new E("NAME", 0)`, which
 * lands in Enum's constructor -- so for the whole lifetime of this layer every
 * ordinal() read back zero, and any `switch (state)` took its first branch
 * whatever the state was.
 *
 * In Flappy Bird that is the game stuck on its title screen: the touch is
 * delivered, the state never advances, and the frame never changes.
 */
static bool enum_init(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)ret;
    /* args: this, name, ordinal */
    if (this_obj && this_obj->nfields > TL_ENUM_SLOT_ORDINAL && nargs >= 3) {
        this_obj->fields[TL_ENUM_SLOT_NAME] = args[1];
        this_obj->fields[TL_ENUM_SLOT_ORDINAL].raw64 = 0;
        this_obj->fields[TL_ENUM_SLOT_ORDINAL].i = args[2].i;
    }
    return true;
}

static int enum_ordinal_of(const tl_dex_object *o)
{
    return (o && o->nfields > TL_ENUM_SLOT_ORDINAL) ? o->fields[TL_ENUM_SLOT_ORDINAL].i : 0;
}

static bool enum_ordinal(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    if (ret) { ret->raw64 = 0; ret->i = enum_ordinal_of(this_obj); }
    return true;
}

/* name() and toString() are the same string until an enum overrides one. */
static bool enum_name(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    if (ret) {
        ret->raw64 = 0;
        if (this_obj && this_obj->nfields > TL_ENUM_SLOT_NAME) {
            ret->l = this_obj->fields[TL_ENUM_SLOT_NAME].l;
        }
    }
    return true;
}

/* Enum constants are singletons, so identity is equality. */
static bool enum_equals(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    if (ret) { ret->raw64 = 0; ret->i = (nargs >= 2 && this_obj && args[1].l == this_obj) ? 1 : 0; }
    return true;
}

static bool enum_hashCode(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    if (ret) { ret->raw64 = 0; ret->i = (int32_t)(((uintptr_t)this_obj) >> 4); }
    return true;
}

static bool enum_compareTo(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    if (ret) {
        ret->raw64 = 0;
        ret->i = enum_ordinal_of(this_obj) - enum_ordinal_of(nargs >= 2 ? args[1].l : NULL);
    }
    return true;
}

/*
 * clone() on an array.
 *
 * Every enum has a compiler-generated values() that returns `$VALUES.clone()`,
 * so a game that so much as lists its states calls this. The class in the call
 * is the array type itself ("[Lcom/example/State;"), which no class here is
 * named, so the resolver sends all of them to the one shim registered as "[".
 */
static bool array_clone(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    if (!ret) return true;
    ret->raw64 = 0;
    if (!this_obj || !this_obj->array.elements) return true;

    tl_dex_object *copy = tl_dex_alloc_array(this_obj->clazz, this_obj->array.length,
                                             this_obj->array.elem_size);
    if (copy && copy->array.elements) {
        memcpy(copy->array.elements, this_obj->array.elements,
               (size_t)this_obj->array.length * this_obj->array.elem_size);
    }
    ret->l = copy;
    return true;
}

/*
 * Rect.intersects, in both of the shapes an app can call it.
 *
 * Android has a static `Rect.intersects(Rect, Rect)` and an instance
 * `rect.intersects(l, t, r, b)`, and which one the app meant is not something
 * the shim table can tell from the name -- a static call arrives with no `this`
 * at all, so the dispatcher hands the first argument through in its place.
 * Reading the arguments is what separates them.
 *
 * Unshimmed, this returned nothing, which a game reads as "no collision" or
 * "collision" at random depending on which way it tests. Flappy Bird compares
 * the bird against the pipes with it.
 */
static bool rect_intersects(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    int hit = 0;
    const tl_framework_rect *a = NULL, *b = NULL;
    tl_framework_rect box;

    if (nargs >= 2 && args[0].l && tl_dex_native(args[0].l) &&
        args[1].l && tl_dex_native(args[1].l)) {
        /* static intersects(Rect, Rect) */
        a = tl_dex_native(args[0].l);
        b = tl_dex_native(args[1].l);
    } else if (this_obj && tl_dex_native(this_obj) && nargs >= 5) {
        /* instance intersects(left, top, right, bottom) */
        a = tl_dex_native(this_obj);
        box.left   = (float)args[1].i;
        box.top    = (float)args[2].i;
        box.right  = (float)args[3].i;
        box.bottom = (float)args[4].i;
        b = &box;
    }

    if (a && b) {
        hit = (a->left < b->right && b->left < a->right &&
               a->top < b->bottom && b->top < a->bottom) ? 1 : 0;
    }
    if (ret) ret->i = hit;
    return true;
}

/*
 * Rect.intersect: the same test, except that it also narrows this rectangle to
 * the overlap when there is one. Apps use the returned boolean and then read
 * the rectangle back, so doing only half of it is worse than not shimming it.
 */
static bool rect_intersect(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    int hit = 0;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_rect *a = tl_dex_native(this_obj);
        tl_framework_rect b;
        bool have = false;

        if (nargs >= 2 && args[1].l && tl_dex_native(args[1].l)) {
            b = *(tl_framework_rect *)tl_dex_native(args[1].l);
            have = true;
        } else if (nargs >= 5) {
            b.left   = (float)args[1].i;
            b.top    = (float)args[2].i;
            b.right  = (float)args[3].i;
            b.bottom = (float)args[4].i;
            have = true;
        }

        if (have && a->left < b.right && b.left < a->right &&
            a->top < b.bottom && b.top < a->bottom) {
            if (b.left   > a->left)   a->left   = b.left;
            if (b.top    > a->top)    a->top    = b.top;
            if (b.right  < a->right)  a->right  = b.right;
            if (b.bottom < a->bottom) a->bottom = b.bottom;
            hit = 1;
        }
    }
    if (ret) ret->i = hit;
    return true;
}

static bool rect_contains(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs;
    int inside = 0;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_rect *r = tl_dex_native(this_obj);
        float x = (float)args[1].i;
        float y = (float)args[2].i;
        if (x >= r->left && x <= r->right && y >= r->top && y <= r->bottom) {
            inside = 1;
        }
    }
    if (ret) ret->i = inside;
    return true;
}

/* android/graphics/Bitmap */
static bool bitmap_getWidth(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    int w = 0;
    if (this_obj && tl_dex_native(this_obj)) {
        w = ((tl_framework_bitmap *)tl_dex_native(this_obj))->width;
    }
    if (ret) ret->i = w;
    return true;
}

static bool bitmap_getHeight(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    int h = 0;
    if (this_obj && tl_dex_native(this_obj)) {
        h = ((tl_framework_bitmap *)tl_dex_native(this_obj))->height;
    }
    if (ret) ret->i = h;
    return true;
}

static bool bitmap_recycle(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs; (void)ret;
    return true;
}

static bool bitmap_createBitmap(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    tl_dex_class *b_class = tl_dex_find_class(ctx, "Landroid/graphics/Bitmap;");
    tl_dex_object *new_obj = tl_dex_alloc_object(b_class);

    if (nargs >= 5 && args[0].l && tl_dex_native(args[0].l)) {
        /* createBitmap(Bitmap src, int x, int y, int width, int height) */
        tl_framework_bitmap *src = tl_dex_native(args[0].l);
        int sx = args[1].i;
        int sy = args[2].i;
        int sw = args[3].i;
        int sh = args[4].i;

        tl_framework_bitmap *dst = calloc(1, sizeof(*dst));
        dst->width = sw;
        dst->height = sh;
        dst->pixels = calloc(sw * sh, sizeof(uint32_t));

        for (int y = 0; y < sh; y++) {
            int src_y = sy + y;
            if (src_y >= 0 && src_y < src->height) {
                for (int x = 0; x < sw; x++) {
                    int src_x = sx + x;
                    if (src_x >= 0 && src_x < src->width) {
                        dst->pixels[y * sw + x] = src->pixels[src_y * src->width + src_x];
                    }
                }
            }
        }
#if defined(__APPLE__)
        CGColorSpaceRef cs = tl_fw_rgb_color_space();
        CGContextRef c = CGBitmapContextCreate(dst->pixels, sw, sh, 8, sw * 4, cs,
                                               kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        dst->cg_image = CGBitmapContextCreateImage(c);
        CGContextRelease(c);
#endif
        tl_dex_set_native(new_obj, dst);
    } else if (nargs >= 2) {
        /* createBitmap(int width, int height, ...) */
        int w = args[0].i;
        int h = args[1].i;
        tl_framework_bitmap *dst = calloc(1, sizeof(*dst));
        dst->width = w;
        dst->height = h;
        dst->pixels = calloc(w * h, sizeof(uint32_t));
        tl_dex_set_native(new_obj, dst);
    }
    if (ret) ret->l = new_obj;
    return true;
}

static bool bitmap_createScaledBitmap(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)nargs;
    tl_dex_class *b_class = tl_dex_find_class(ctx, "Landroid/graphics/Bitmap;");
    tl_dex_object *new_obj = tl_dex_alloc_object(b_class);

    if (args[0].l && tl_dex_native(args[0].l)) {
        tl_framework_bitmap *src = tl_dex_native(args[0].l);
        int dstW = args[1].i;
        int dstH = args[2].i;
        tl_framework_bitmap *dst = calloc(1, sizeof(*dst));
        dst->width = dstW;
        dst->height = dstH;
        dst->pixels = calloc(dstW * dstH, sizeof(uint32_t));

        /* Nearest-neighbor scale */
        for (int y = 0; y < dstH; y++) {
            int sy = (y * src->height) / dstH;
            for (int x = 0; x < dstW; x++) {
                int sx = (x * src->width) / dstW;
                dst->pixels[y * dstW + x] = src->pixels[sy * src->width + sx];
            }
        }
#if defined(__APPLE__)
        CGColorSpaceRef cs = tl_fw_rgb_color_space();
        CGContextRef c = CGBitmapContextCreate(dst->pixels, dstW, dstH, 8, dstW * 4, cs,
                                               kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        dst->cg_image = CGBitmapContextCreateImage(c);
        CGContextRelease(c);
#endif
        tl_dex_set_native(new_obj, dst);
    }
    if (ret) ret->l = new_obj;
    return true;
}

/* android/graphics/BitmapFactory */
static bool bitmapFactory_decodeResource(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)nargs;
    uint32_t res_id = args[1].raw32;

    /* Whatever file the id names, from the app's own table. */
    const char *entry = NULL;
    tl_res *res = framework_res(ctx);
    if (res && res_id) entry = tl_res_file(res, res_id, TL_FRAMEWORK_DENSITY_DPI);

    tl_framework_bitmap *bmp = NULL;
    if (entry && ctx->apk_path) {
        bmp = load_png_from_apk(ctx->apk_path, entry);
    }

    /* The contract is a Bitmap or null, never a Bitmap with nothing behind it:
     * apps test the result for null, and an empty Bitmap passes that test and
     * fails everywhere after it. */
    if (!bmp) {
        if (ret) ret->raw64 = 0;
        return true;
    }

    tl_dex_class *b_class = tl_dex_find_class(ctx, "Landroid/graphics/Bitmap;");
    tl_dex_object *b_obj = tl_dex_alloc_object(b_class);
    tl_dex_set_native(b_obj, bmp);

    if (ret) { ret->raw64 = 0; ret->l = b_obj; }
    return true;
}

/* android/graphics/Canvas */

/*
 * Where a rectangle in the canvas's coordinates lands in the framebuffer, when
 * the transform is just a scale and a translation -- nearly every draw. Anything
 * with rotation, skew or a mirror returns false and is left to CoreGraphics.
 *
 * The context's matrix maps the canvas to CoreGraphics' device space, which is
 * y-UP; the framebuffer's rows run the other way, so a row is the height minus
 * the device y. The base flip the context starts with (translate by the height,
 * scale y by -1) is exactly what cancels that, which is why Android's top-left
 * coordinates come out as framebuffer rows unchanged.
 */
static bool canvas_device_rect(const tl_dex_context *ctx, const tl_framework_state *st,
                               float l, float t, float r, float b,
                               float *dx0, float *dy0, float *dx1, float *dy1)
{
#if defined(__APPLE__)
    if (ctx->use_cg_only || !st || !st->cg_ctx || !ctx->framebuffer) return false;
    CGAffineTransform m = CGContextGetCTM(st->cg_ctx);
    if (fabs(m.b) > 1e-5 || fabs(m.c) > 1e-5) return false;      /* rotation or skew */
    if (m.a <= 0 || m.d >= 0) return false;                      /* a mirror */
    *dx0 = (float)(m.a * l + m.tx);
    *dx1 = (float)(m.a * r + m.tx);
    *dy0 = (float)ctx->fb_height - (float)(m.d * t + m.ty);
    *dy1 = (float)ctx->fb_height - (float)(m.d * b + m.ty);
    return true;
#else
    (void)ctx; (void)st; (void)l; (void)t; (void)r; (void)b; (void)dx0; (void)dy0; (void)dx1; (void)dy1;
    return false;
#endif
}

static bool bitmap_opaque(tl_framework_bitmap *bmp)
{
    if (bmp->opaque == 0) {
        bmp->opaque = tl_blit_image_is_opaque(bmp->pixels, bmp->width, bmp->height) ? 1 : 2;
    }
    return bmp->opaque == 1;
}

/* The alpha a draw carries: the paint's, or opaque with no paint. */
static int paint_draw_alpha(const tl_framework_paint *p)
{
    return p ? (p->alpha < 0 ? 0 : (p->alpha > 255 ? 255 : p->alpha)) : 255;
}

/* Try the blitter. True if it drew; false means the caller should use CoreGraphics. */
static bool blit_bitmap(tl_dex_context *ctx, tl_framework_state *st, tl_framework_bitmap *bmp,
                        const tl_framework_paint *paint,
                        float sx0, float sy0, float sx1, float sy1,
                        float ul, float ut, float ur, float ub)
{
    if (!bmp->pixels) return false;
    float dx0, dy0, dx1, dy1;
    if (!canvas_device_rect(ctx, st, ul, ut, ur, ub, &dx0, &dy0, &dx1, &dy1)) return false;

    /* A filtered draw that actually resizes needs interpolation the blitter does
     * not do. At 1:1 filtering changes nothing, so that stays on the fast path. */
    if (paint && paint->filter) {
        bool resized = fabsf((dx1 - dx0) - (sx1 - sx0)) > 0.01f ||
                       fabsf((dy1 - dy0) - (sy1 - sy0)) > 0.01f;
        if (resized) return false;
    }

    tl_blit_target target = { ctx->framebuffer, ctx->fb_width, ctx->fb_height, ctx->fb_width };
    tl_blit_draw(&target, bmp->pixels, bmp->width, bmp->height, bitmap_opaque(bmp),
                 sx0, sy0, sx1, sy1, dx0, dy0, dx1, dy1, paint_draw_alpha(paint));
    return true;
}

static bool canvas_save(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_framework_state *st = ctx->framework_data;
    int count = 1;
    if (st && st->cg_ctx) {
        CGContextSaveGState(st->cg_ctx);
    }
    if (ret) ret->i = count;
    return true;
}

static bool canvas_restore(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs; (void)ret;
    tl_framework_state *st = ctx->framework_data;
    if (st && st->cg_ctx) {
        CGContextRestoreGState(st->cg_ctx);
    }
    return true;
}

static bool canvas_scale(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)ret;
    tl_framework_state *st = ctx->framework_data;
    if (st && st->cg_ctx) {
        float sx = args[1].f;
        float sy = args[2].f;
        if (nargs >= 5) {
            float px = args[3].f;
            float py = args[4].f;
            CGContextTranslateCTM(st->cg_ctx, px, py);
            CGContextScaleCTM(st->cg_ctx, sx, sy);
            CGContextTranslateCTM(st->cg_ctx, -px, -py);
        } else {
            CGContextScaleCTM(st->cg_ctx, sx, sy);
        }
    }
    return true;
}

static bool canvas_drawBitmap_xy_impl(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)ret;
    tl_framework_state *st = ctx->framework_data;
    if (!st || !st->cg_ctx || !args[1].l || !tl_dex_native(args[1].l)) return true;
    tl_framework_bitmap *bmp = tl_dex_native(args[1].l);

    float x = args[2].f;
    float y = args[3].f;
    float w = bmp->width;
    float h = bmp->height;
    const tl_framework_paint *paint = (nargs >= 5 && args[4].l) ? tl_dex_native(args[4].l) : NULL;
    if (ctx->draw_observer) ctx->draw_observer(ctx->draw_observer_user, bmp->width, bmp->height, x, y);
#ifdef TL_DEX_TRACE
    if (ctx->trace)
        fprintf(stderr, "        drawBitmapXY %dx%d at (%g,%g)\n", bmp->width, bmp->height, x, y);
#endif

    if (blit_bitmap(ctx, st, bmp, paint, 0, 0, w, h, x, y, x + w, y + h)) return true;
    if (!bmp->cg_image) return true;

    CGContextSaveGState(st->cg_ctx);
    CGContextSetAlpha(st->cg_ctx, paint_draw_alpha(paint) / 255.0f);
    CGContextSetInterpolationQuality(st->cg_ctx, (paint && paint->filter) ? kCGInterpolationMedium
                                                                         : kCGInterpolationNone);
    CGContextTranslateCTM(st->cg_ctx, x, y + h);
    CGContextScaleCTM(st->cg_ctx, 1.0, -1.0);
    CGContextDrawImage(st->cg_ctx, CGRectMake(0, 0, w, h), bmp->cg_image);
    CGContextRestoreGState(st->cg_ctx);
    return true;
}

static bool canvas_drawBitmap_matrix_impl(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)ret;
    tl_framework_state *st = ctx->framework_data;
    if (!st || !st->cg_ctx || !args[1].l || !tl_dex_native(args[1].l)) return true;
    tl_framework_bitmap *bmp = tl_dex_native(args[1].l);
    if (!args[2].l || !tl_dex_native(args[2].l)) return true;

    const tl_framework_matrix *mat = tl_dex_native(args[2].l);
    const tl_framework_paint *paint = (nargs >= 4 && args[3].l) ? tl_dex_native(args[3].l) : NULL;
#ifdef TL_DEX_TRACE
    if (ctx->trace)
        fprintf(stderr, "        drawBitmapMatrix %dx%d m=[%g %g %g | %g %g %g]\n", bmp->width, bmp->height,
                mat->m[0], mat->m[1], mat->m[2], mat->m[3], mat->m[4], mat->m[5]);
#endif

    /* A matrix with no skew terms is a scale and a translation, so it is a
     * rectangle: the bitmap's corners mapped through it. Only a rotated sprite --
     * the bird, as it tilts -- needs CoreGraphics. */
    if (fabsf(mat->m[1]) < 1e-6f && fabsf(mat->m[3]) < 1e-6f && mat->m[0] > 0 && mat->m[4] > 0) {
        float l = mat->m[2], t = mat->m[5];
        float r = l + mat->m[0] * (float)bmp->width, b = t + mat->m[4] * (float)bmp->height;
        if (blit_bitmap(ctx, st, bmp, paint, 0, 0, (float)bmp->width, (float)bmp->height, l, t, r, b)) return true;
    }
    if (!bmp->cg_image) return true;

    CGContextSaveGState(st->cg_ctx);
    CGContextSetAlpha(st->cg_ctx, paint_draw_alpha(paint) / 255.0f);
    CGContextSetInterpolationQuality(st->cg_ctx, (paint && paint->filter) ? kCGInterpolationMedium
                                                                         : kCGInterpolationNone);
    CGAffineTransform t = CGAffineTransformMake(mat->m[0], mat->m[3],
                                                    mat->m[1], mat->m[4],
                                                    mat->m[2], mat->m[5]);
    CGContextConcatCTM(st->cg_ctx, t);
    CGContextTranslateCTM(st->cg_ctx, 0, bmp->height);
    CGContextScaleCTM(st->cg_ctx, 1.0, -1.0);
    CGContextDrawImage(st->cg_ctx, CGRectMake(0, 0, bmp->width, bmp->height), bmp->cg_image);
    CGContextRestoreGState(st->cg_ctx);
    return true;
}

static bool canvas_drawBitmap_rect_impl(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)ret;
    tl_framework_state *st = ctx->framework_data;
    if (!st || !st->cg_ctx || !args[1].l || !tl_dex_native(args[1].l)) return true;
    tl_framework_bitmap *bmp = tl_dex_native(args[1].l);

    tl_framework_rect *src = (args[2].l) ? tl_dex_native(args[2].l) : NULL;
    tl_framework_rect *dst = (args[3].l) ? tl_dex_native(args[3].l) : NULL;
    if (!dst) return true;
    const tl_framework_paint *paint = (nargs >= 5 && args[4].l) ? tl_dex_native(args[4].l) : NULL;

    float dst_x = dst->left;
    float dst_y = dst->top;
    float dst_w = dst->right - dst->left;
    float dst_h = dst->bottom - dst->top;
#ifdef TL_DEX_TRACE
    if (ctx->trace)
        fprintf(stderr, "        drawBitmap %dx%d src=%s(%g,%g,%g,%g) dst=(%g,%g,%g,%g)%s filter=%d alpha=%d\n",
                bmp->width, bmp->height, src ? "" : "none",
                src ? src->left : 0, src ? src->top : 0, src ? src->right : 0, src ? src->bottom : 0,
                dst->left, dst->top, dst->right, dst->bottom,
                (dst_w <= 0 || dst_h <= 0) ? "  [EMPTY dst: skipped]" : "",
                paint ? paint->filter : -1, paint ? paint->alpha : -1);
#endif
    if (dst_w <= 0 || dst_h <= 0) return true;

    /* The whole image unless a usable source rectangle was given. */
    float sx0 = 0, sy0 = 0, sx1 = (float)bmp->width, sy1 = (float)bmp->height;
    if (src && src->right > src->left && src->bottom > src->top) {
        sx0 = src->left; sy0 = src->top; sx1 = src->right; sy1 = src->bottom;
    }

    if (blit_bitmap(ctx, st, bmp, paint, sx0, sy0, sx1, sy1,
                    dst->left, dst->top, dst->right, dst->bottom)) return true;
    if (!bmp->cg_image) return true;

    CGImageRef img_to_draw = bmp->cg_image;
    bool need_release = false;
    if (src && src->right > src->left && src->bottom > src->top) {
        img_to_draw = CGImageCreateWithImageInRect(bmp->cg_image,
                          CGRectMake(sx0, sy0, sx1 - sx0, sy1 - sy0));
        need_release = true;
    }

    if (img_to_draw) {
        CGContextSaveGState(st->cg_ctx);
        CGContextSetAlpha(st->cg_ctx, paint_draw_alpha(paint) / 255.0f);
        CGContextSetInterpolationQuality(st->cg_ctx, (paint && paint->filter) ? kCGInterpolationMedium
                                                                             : kCGInterpolationNone);
        CGContextTranslateCTM(st->cg_ctx, dst_x, dst_y + dst_h);
        CGContextScaleCTM(st->cg_ctx, 1.0, -1.0);
        CGContextDrawImage(st->cg_ctx, CGRectMake(0, 0, dst_w, dst_h), img_to_draw);
        CGContextRestoreGState(st->cg_ctx);
        if (need_release) CGImageRelease(img_to_draw);
    }
    return true;
}


static bool canvas_drawRect_impl(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)ret;
    tl_framework_state *st = ctx->framework_data;
    if (!st || !st->cg_ctx) return true;

    float l = 0, t = 0, r = 0, b = 0;
    const tl_framework_paint *paint = NULL;

    if (nargs >= 6) {
        /* drawRect(float left, float top, float right, float bottom, Paint paint) */
        l = args[1].f; t = args[2].f; r = args[3].f; b = args[4].f;
        if (args[5].l) paint = tl_dex_native(args[5].l);
    } else if (nargs >= 3 && args[1].l && tl_dex_native(args[1].l)) {
        /* drawRect(Rect/RectF rect, Paint paint) */
        const tl_framework_rect *rc = tl_dex_native(args[1].l);
        l = rc->left; t = rc->top; r = rc->right; b = rc->bottom;
        if (args[2].l) paint = tl_dex_native(args[2].l);
    }

    /* The colour's own alpha channel IS the paint's alpha -- every setter keeps
     * the two equal -- so it is applied once. This used to multiply them, which
     * squares it: a fade at half alpha drew at a quarter. */
    uint32_t argb = paint ? ((paint->color & 0x00ffffffu) | ((uint32_t)paint_draw_alpha(paint) << 24))
                          : 0xff000000u;

    float dx0, dy0, dx1, dy1;
    if (canvas_device_rect(ctx, st, l, t, r, b, &dx0, &dy0, &dx1, &dy1)) {
        tl_blit_target target = { ctx->framebuffer, ctx->fb_width, ctx->fb_height, ctx->fb_width };
        tl_blit_fill(&target, dx0, dy0, dx1, dy1, argb);
        return true;
    }

    CGContextSetRGBFillColor(st->cg_ctx, ((argb >> 16) & 0xff) / 255.0f, ((argb >> 8) & 0xff) / 255.0f,
                             (argb & 0xff) / 255.0f, (argb >> 24) / 255.0f);
    CGContextFillRect(st->cg_ctx, CGRectMake(l, t, r - l, b - t));
    return true;
}

/*
 * Draw calls, timed.
 *
 * Whether a frame is slow because of the interpreter or because of CoreGraphics
 * is the first thing to know and the last thing to guess, so every canvas draw
 * adds its own duration to the frame's counters. Two clock reads a draw -- tens
 * of nanoseconds -- against calls that cost microseconds.
 */
#define TIMED_DRAW(name) \
    static bool timed_##name(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret) \
    { uint64_t t0 = tl_dex_now_ns(); bool r = name##_impl(ctx, this_obj, args, nargs, ret); \
      ctx->perf.ns_draw += tl_dex_now_ns() - t0; ctx->perf.draws++; return r; }
TIMED_DRAW(canvas_drawBitmap_xy)
TIMED_DRAW(canvas_drawBitmap_matrix)
TIMED_DRAW(canvas_drawBitmap_rect)
TIMED_DRAW(canvas_drawRect)

/* android/view/View */
static bool view_invalidate(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs; (void)ret;
    return true;
}

static bool view_getContext(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_framework_state *st = ctx->framework_data;
    if (ret && st) ret->l = st->activity_obj;
    return true;
}

static bool view_getResources(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_framework_state *st = ctx->framework_data;
    if (ret && st) ret->l = st->resources_obj;
    return true;
}

/* android/app/Activity & Context */
static bool activity_getSharedPreferences(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_framework_state *st = ctx->framework_data;
    if (ret && st) ret->l = st->prefs_obj;
    return true;
}

static bool activity_getPackageName(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    if (ret) ret->l = tl_dex_alloc_string(ctx, "com.flappybird.recreation");
    return true;
}

/*
 * SharedPreferences and its Editor, over one typed store (husk-tl-prefs.c).
 *
 * The reads used to be a stand-in: getInt ignored the key and the default and
 * returned the high score for every call, and the rest ignored the key and
 * returned the default. An app that asks for "score multiplier, default 1"
 * got 0 back, so Flappy Bird never scored; it asked for the score at which its
 * pipes start moving, default 2000, and got 0, so they moved from the first
 * frame. A preference is read by key, with the app's default as the answer when
 * nothing is stored, and that is all these do.
 *
 * One store serves every name an app passes to getSharedPreferences. An app
 * that keeps two files with colliding keys would notice; none seen so far does.
 */
static const char *pref_key(const tl_dex_val *args, int nargs)
{
    return nargs > 1 ? tl_dex_string(args[1].l) : NULL;
}

static tl_prefs *prefs_of(tl_dex_context *ctx)
{
    tl_framework_state *st = ctx ? ctx->framework_data : NULL;
    return st ? st->prefs : NULL;
}

static bool sharedPrefs_getInt(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    int32_t def = nargs >= 3 ? args[2].i : 0;
    if (ret) { ret->raw64 = 0; ret->i = tl_prefs_get_int(prefs_of(ctx), pref_key(args, nargs), def); }
    return true;
}

static bool sharedPrefs_getBoolean(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    bool def = nargs >= 3 ? (args[2].i != 0) : false;
    if (ret) { ret->raw64 = 0; ret->i = tl_prefs_get_bool(prefs_of(ctx), pref_key(args, nargs), def) ? 1 : 0; }
    return true;
}

static bool sharedPrefs_getFloat(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    float def = nargs >= 3 ? args[2].f : 0.0f;
    if (ret) { ret->raw64 = 0; ret->f = tl_prefs_get_float(prefs_of(ctx), pref_key(args, nargs), def); }
    return true;
}

static bool sharedPrefs_getLong(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    int64_t def = nargs >= 3 ? args[2].j : 0;      /* a long is one slot, at index 2 */
    if (ret) { ret->raw64 = 0; ret->j = tl_prefs_get_long(prefs_of(ctx), pref_key(args, nargs), def); }
    return true;
}

static bool sharedPrefs_getString(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    if (!ret) return true;
    ret->raw64 = 0;
    const char *key = pref_key(args, nargs);
    tl_prefs *p = prefs_of(ctx);
    if (p && key && tl_prefs_contains(p, key)) {
        const char *stored = tl_prefs_get_string(p, key, NULL);
        if (stored) { ret->l = tl_dex_alloc_string(ctx, stored); return true; }
    }
    ret->l = nargs >= 3 ? args[2].l : NULL;
    return true;
}

static bool sharedPrefs_contains(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    if (ret) { ret->raw64 = 0; ret->i = tl_prefs_contains(prefs_of(ctx), pref_key(args, nargs)) ? 1 : 0; }
    return true;
}

static bool sharedPrefs_edit(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_framework_state *st = ctx->framework_data;
    if (ret) { ret->raw64 = 0; ret->l = st ? st->editor_obj : NULL; }
    return true;
}

/*
 * The Editor. Android batches edits until apply() or commit(); these write
 * straight into the store, and apply() and commit() save it. The one difference
 * an app could see is reading its own edit back before applying it, which
 * would see the new value here a moment before Android would show it.
 */
static bool editor_return_this(tl_dex_object *this_obj, tl_dex_val *ret)
{
    if (ret) { ret->raw64 = 0; ret->l = this_obj; }
    return true;
}

static bool editor_putInt(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    if (nargs >= 3) tl_prefs_put_int(prefs_of(ctx), pref_key(args, nargs), args[2].i);
    return editor_return_this(this_obj, ret);
}
static bool editor_putLong(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    if (nargs >= 3) tl_prefs_put_long(prefs_of(ctx), pref_key(args, nargs), args[2].j);
    return editor_return_this(this_obj, ret);
}
static bool editor_putFloat(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    if (nargs >= 3) tl_prefs_put_float(prefs_of(ctx), pref_key(args, nargs), args[2].f);
    return editor_return_this(this_obj, ret);
}
static bool editor_putBoolean(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    if (nargs >= 3) tl_prefs_put_bool(prefs_of(ctx), pref_key(args, nargs), args[2].i != 0);
    return editor_return_this(this_obj, ret);
}
static bool editor_putString(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    if (nargs >= 3) tl_prefs_put_string(prefs_of(ctx), pref_key(args, nargs), tl_dex_string(args[2].l));
    return editor_return_this(this_obj, ret);
}
static bool editor_remove(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    tl_prefs_remove(prefs_of(ctx), pref_key(args, nargs));
    return editor_return_this(this_obj, ret);
}
static bool editor_clear(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)args; (void)nargs;
    tl_prefs_clear(prefs_of(ctx));
    return editor_return_this(this_obj, ret);
}
static bool editor_apply(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_prefs_save(prefs_of(ctx));
    if (ret) ret->raw64 = 0;
    return true;
}
static bool editor_commit(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    bool ok = tl_prefs_save(prefs_of(ctx));
    if (ret) { ret->raw64 = 0; ret->i = ok ? 1 : 0; }
    return true;
}

/* Keep the app's settings in a file. Opt-in: a context starts with them in
 * memory, which is what a test wants; the app that runs real games attaches a
 * path next to the APK so a high score survives the relaunch. */
bool tl_framework_attach_prefs(tl_dex_context *ctx, const char *path)
{
    tl_framework_state *st = ctx ? ctx->framework_data : NULL;
    return st && st->prefs && tl_prefs_attach(st->prefs, path);
}

/* android/content/res/Resources */
/*
 * Resources.getIdentifier(name, defType, defPackage): the id the app's own
 * table gives a name.
 *
 * This was a list of four names, written to suit the game it was first tried
 * on, with everything else answering zero. A zero id then goes straight into
 * decodeResource, which returns nothing for it, which the game reports as
 * "could not load" -- so any sprite not on the list was simply absent.
 */
static bool resources_getIdentifier(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj;
    const char *name = (nargs > 1 && args[1].l && tl_dex_string(args[1].l)) ? tl_dex_string(args[1].l) : NULL;
    const char *type = (nargs > 2 && args[2].l && tl_dex_string(args[2].l)) ? tl_dex_string(args[2].l) : NULL;
    uint32_t id = 0;
    tl_res *res = framework_res(ctx);
    if (res && name) id = tl_res_find(res, type, name);
    if (ret) { ret->raw64 = 0; ret->i = (int32_t)id; }
    return true;
}

/* android/view/Choreographer */
static bool choreographer_getInstance(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_framework_state *st = ctx->framework_data;
    if (ret && st) ret->l = st->choreographer_obj;
    return true;
}

static bool choreographer_postFrameCallback(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)nargs; (void)ret;
    if (args[1].l) {
        ctx->choreographer_cb = args[1].l;
    }
    return true;
}

/* java/util/Random */
static bool random_init(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs; (void)ret;
    return true;
}

static bool random_nextInt(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj;
    int bound = (nargs >= 2 && args[1].i > 0) ? args[1].i : 100;
    int val = rand() % bound;
    if (ret) ret->i = val;
    return true;
}

static bool random_nextFloat(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs;
    float val = (float)rand() / (float)RAND_MAX;
    if (ret) ret->f = val;
    return true;
}

static bool random_nextBoolean(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs;
    if (ret) ret->i = (rand() & 1);
    return true;
}

/* java/lang/Math */

/* java/util/ArrayList */
static bool arrayList_init(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    tl_framework_list *lst = calloc(1, sizeof(*lst));
    lst->capacity = 16;
    lst->items = calloc(lst->capacity, sizeof(tl_dex_val));
    tl_dex_set_native(this_obj, lst);
    return true;
}

static bool arrayList_add(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_list *lst = tl_dex_native(this_obj);
        if (lst->size >= lst->capacity) {
            lst->capacity *= 2;
            lst->items = realloc(lst->items, lst->capacity * sizeof(tl_dex_val));
        }
        lst->items[lst->size++] = args[1];
    }
    if (ret) ret->i = 1;
    return true;
}

static bool arrayList_size(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    int s = 0;
    if (this_obj && tl_dex_native(this_obj)) {
        s = ((tl_framework_list *)tl_dex_native(this_obj))->size;
    }
    if (ret) ret->i = s;
    return true;
}

static bool arrayList_get(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)nargs;
    if (this_obj && tl_dex_native(this_obj) && ret) {
        tl_framework_list *lst = tl_dex_native(this_obj);
        int idx = args[1].i;
        if (idx >= 0 && idx < lst->size) {
            *ret = lst->items[idx];
        } else {
            ret->l = NULL;
        }
    }
    return true;
}

static bool arrayList_clear(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    if (this_obj && tl_dex_native(this_obj)) {
        ((tl_framework_list *)tl_dex_native(this_obj))->size = 0;
    }
    return true;
}

static bool arrayList_iterator(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)args; (void)nargs;
    tl_dex_class *it_class = tl_dex_find_class(ctx, "Ljava/util/Iterator;");
    tl_dex_object *it_obj = tl_dex_alloc_object(it_class);
    tl_framework_iterator *it = calloc(1, sizeof(*it));
    it->list = this_obj ? tl_dex_native(this_obj) : NULL;
    it->cursor = 0;
    tl_dex_set_native(it_obj, it);
    if (ret) ret->l = it_obj;
    return true;
}

static bool iterator_hasNext(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    int has = 0;
    if (this_obj && tl_dex_native(this_obj)) {
        tl_framework_iterator *it = tl_dex_native(this_obj);
        if (it->list && it->cursor < it->list->size) {
            has = 1;
        }
    }
    if (ret) ret->i = has;
    return true;
}

static bool iterator_next(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    if (this_obj && tl_dex_native(this_obj) && ret) {
        tl_framework_iterator *it = tl_dex_native(this_obj);
        if (it->list && it->cursor < it->list->size) {
            *ret = it->list->items[it->cursor++];
        } else {
            ret->l = NULL;
        }
    }
    return true;
}

/* java/lang/reflect/Array */
/*
 * java.lang.reflect.Array.newInstance, in its two shapes.
 *
 *   newInstance(Class, int)    -> a one-dimensional array
 *   newInstance(Class, int...) -> one array per dimension, all allocated
 *
 * The two used to be one function that decided which it had been called as by
 * testing whether the second argument was a non-null pointer. For the int form
 * that argument is the LENGTH -- 3, say -- which is non-null, so it read the
 * contents of address 3. They have different shorties, so they are registered
 * separately and nothing has to guess.
 *
 * A multi-dimensional request allocates the rows too. Allocating only the outer
 * array left `Bitmap[3][3]` as three null rows, and the first `grid[i][j]`
 * indexed into null.
 */
#define TL_MAX_ARRAY_LEN (16 * 1024 * 1024)

static tl_dex_object *new_array_dims(tl_dex_class *elem_class, const int32_t *dims, uint32_t ndims)
{
    int32_t len = dims[0] > 0 ? dims[0] : 0;
    if (len > TL_MAX_ARRAY_LEN) len = 0;     /* a wild length, not a real request */
    tl_dex_object *arr = tl_dex_alloc_array(elem_class, (uint32_t)len, sizeof(void *));
    if (arr && arr->array.elements && ndims > 1) {
        tl_dex_object **rows = arr->array.elements;
        for (int32_t i = 0; i < len; i++) {
            rows[i] = new_array_dims(elem_class, dims + 1, ndims - 1);
        }
    }
    return arr;
}

static bool array_newInstance_len(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)nargs;
    tl_dex_class *elem_class = args[0].l ? args[0].l->clazz : NULL;
    int32_t dims[1] = { args[1].i };
    if (ret) { ret->raw64 = 0; ret->l = new_array_dims(elem_class, dims, 1); }
    return true;
}

static bool array_newInstance_dims(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)nargs;
    tl_dex_class *elem_class = args[0].l ? args[0].l->clazz : NULL;
    tl_dex_object *d = args[1].l;
    if (ret) ret->raw64 = 0;
    /* An int[] -- four bytes a slot -- and not empty, or there is no shape. */
    if (!d || !d->array.elements || d->array.elem_size != 4 || d->array.length == 0 ||
        d->array.length > 8) {
        return true;
    }
    if (ret) ret->l = new_array_dims(elem_class, (const int32_t *)d->array.elements, d->array.length);
    return true;
}

/* android/view/MotionEvent */
static bool motionEvent_getX(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    float x = 0;
    if (this_obj && this_obj->fields) x = this_obj->fields[0].f;
    if (ret) ret->f = x;
    return true;
}

static bool motionEvent_getY(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    float y = 0;
    if (this_obj && this_obj->fields) y = this_obj->fields[1].f;
    if (ret) ret->f = y;
    return true;
}

static bool motionEvent_getAction(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs;
    int act = 0;
    if (this_obj && this_obj->fields) act = this_obj->fields[2].i;
    if (ret) ret->i = act;
    return true;
}

/* java/util/concurrent/Executors */
static bool executors_newSingleThreadExecutor(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)this_obj; (void)args; (void)nargs;
    tl_dex_class *ex_class = tl_dex_find_class(ctx, "Ljava/util/concurrent/ExecutorService;");
    if (ret) ret->l = tl_dex_alloc_object(ex_class);
    return true;
}

/* android/util/Log */
static bool log_print(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)nargs;
    const char *tag = (args[0].l && tl_dex_string(args[0].l)) ? tl_dex_string(args[0].l) : "";
    const char *msg = (args[1].l && tl_dex_string(args[1].l)) ? tl_dex_string(args[1].l) : "";
    printf("[%s] %s\n", tag, msg);
    if (ret) ret->i = 0;
    return true;
}

/* java/lang/StringBuilder */
static bool stringBuilder_init_void(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)args; (void)nargs; (void)ret;
    /* tl_dex_set_string releases whatever the object held before. */
    if (this_obj) tl_dex_set_string(this_obj, strdup(""));
    return true;
}

static bool stringBuilder_init_str(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)ret;
    const char *s = (nargs >= 2 && args[1].l && tl_dex_string(args[1].l)) ? tl_dex_string(args[1].l) : "";
    if (this_obj) tl_dex_set_string(this_obj, strdup(s));
    return true;
}

static void stringBuilder_append_text(tl_dex_object *this_obj, const char *add)
{
    if (!this_obj || !add) return;
    size_t old_len = tl_dex_string(this_obj) ? strlen(tl_dex_string(this_obj)) : 0;
    size_t add_len = strlen(add);
    char *nb = malloc(old_len + add_len + 1);
    if (!nb) return;
    if (tl_dex_string(this_obj)) {
        memcpy(nb, tl_dex_string(this_obj), old_len);
    }
    memcpy(nb + old_len, add, add_len);
    nb[old_len + add_len] = '\0';
    /* Copied out above, so the old buffer can go now -- set_string frees it. */
    tl_dex_set_string(this_obj, nb);
}

static bool stringBuilder_append_str(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    const char *s = (nargs >= 2 && args[1].l && tl_dex_string(args[1].l)) ? tl_dex_string(args[1].l) : "null";
    stringBuilder_append_text(this_obj, s);
    if (ret) ret->l = this_obj;
    return true;
}

static bool stringBuilder_append_int(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    char buf[32];
    int32_t val = (nargs >= 2) ? args[1].i : 0;
    snprintf(buf, sizeof(buf), "%d", val);
    stringBuilder_append_text(this_obj, buf);
    if (ret) ret->l = this_obj;
    return true;
}

static bool stringBuilder_append_float(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    char buf[32];
    float val = (nargs >= 2) ? args[1].f : 0.0f;
    snprintf(buf, sizeof(buf), "%f", val);
    stringBuilder_append_text(this_obj, buf);
    if (ret) ret->l = this_obj;
    return true;
}

static bool stringBuilder_append_char(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    char buf[2];
    buf[0] = (nargs >= 2) ? (char)args[1].i : '\0';
    buf[1] = '\0';
    stringBuilder_append_text(this_obj, buf);
    if (ret) ret->l = this_obj;
    return true;
}

static bool stringBuilder_append_bool(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx;
    bool val = (nargs >= 2) ? (args[1].i != 0) : false;
    stringBuilder_append_text(this_obj, val ? "true" : "false");
    if (ret) ret->l = this_obj;
    return true;
}

static bool stringBuilder_toString(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)args; (void)nargs;
    const char *s = (this_obj && tl_dex_string(this_obj)) ? tl_dex_string(this_obj) : "";
    if (ret) ret->l = tl_dex_alloc_string(ctx, s);
    return true;
}

/* No-op stub for unneeded framework calls */
static bool noop_stub(tl_dex_context *ctx, tl_dex_object *this_obj, tl_dex_val *args, int nargs, tl_dex_val *ret)
{
    (void)ctx; (void)this_obj; (void)args; (void)nargs;
    if (ret) ret->raw64 = 0;
    return true;
}

/* ----------------------------------------------------- Method Table */

typedef struct {
    const char *class_desc;
    const char *method_name;
    const char *shorty;
    tl_dex_native_func func;
} tl_native_entry;

static const tl_native_entry s_native_methods[] = {
    { "Ljava/lang/Object;", "<init>", NULL, obj_init },
    { "Ljava/lang/Enum;", "<init>", NULL, enum_init },
    { "Ljava/lang/Enum;", "ordinal", NULL, enum_ordinal },
    { "Ljava/lang/Enum;", "name", NULL, enum_name },
    { "Ljava/lang/Enum;", "toString", NULL, enum_name },
    { "Ljava/lang/Enum;", "equals", NULL, enum_equals },
    { "Ljava/lang/Enum;", "hashCode", NULL, enum_hashCode },
    { "Ljava/lang/Enum;", "compareTo", NULL, enum_compareTo },
    { "[", "clone", NULL, array_clone },

    /* StringBuilder */
    { "Ljava/lang/StringBuilder;", "<init>", "V", stringBuilder_init_void },
    { "Ljava/lang/StringBuilder;", "<init>", "VL", stringBuilder_init_str },
    { "Ljava/lang/StringBuilder;", "<init>", "VI", stringBuilder_init_void },
    { "Ljava/lang/StringBuilder;", "<init>", NULL, stringBuilder_init_void },
    { "Ljava/lang/StringBuilder;", "append", "LL", stringBuilder_append_str },
    { "Ljava/lang/StringBuilder;", "append", "LI", stringBuilder_append_int },
    { "Ljava/lang/StringBuilder;", "append", "LF", stringBuilder_append_float },
    { "Ljava/lang/StringBuilder;", "append", "LC", stringBuilder_append_char },
    { "Ljava/lang/StringBuilder;", "append", "LZ", stringBuilder_append_bool },
    { "Ljava/lang/StringBuilder;", "append", NULL, stringBuilder_append_str },
    { "Ljava/lang/StringBuilder;", "toString", NULL, stringBuilder_toString },

    /* Matrix */
    { "Landroid/graphics/Matrix;", "<init>", NULL, matrix_init },
    { "Landroid/graphics/Matrix;", "reset", NULL, matrix_reset },
    { "Landroid/graphics/Matrix;", "postTranslate", NULL, matrix_postTranslate },
    { "Landroid/graphics/Matrix;", "postRotate", NULL, matrix_postRotate },
    { "Landroid/graphics/Matrix;", "postScale", NULL, matrix_postScale },

    /* Paint */
    { "Landroid/graphics/Paint;", "<init>", NULL, paint_init },
    { "Landroid/graphics/Paint;", "setFilterBitmap", NULL, paint_setFilterBitmap },
    { "Landroid/graphics/Paint;", "setAntiAlias", NULL, paint_setAntiAlias },
    { "Landroid/graphics/Paint;", "setARGB", NULL, paint_setARGB },
    { "Landroid/graphics/Paint;", "setColor", NULL, paint_setColor },
    { "Landroid/graphics/Paint;", "setAlpha", NULL, paint_setAlpha },
    { "Landroid/graphics/Paint;", "getAlpha", NULL, paint_getAlpha },
    { "Landroid/graphics/Paint;", "setColorFilter", NULL, paint_setColorFilter },

    /* Rect / RectF */
    { "Landroid/graphics/Rect;", "<init>", "V", rect_init_void },
    { "Landroid/graphics/Rect;", "<init>", "VIIII", rect_init_int },
    { "Landroid/graphics/Rect;", "<init>", NULL, rect_init_int },
    { "Landroid/graphics/Rect;", "set", "VIIII", rect_set_int },
    { "Landroid/graphics/Rect;", "set", NULL, rect_set_int },
    { "Landroid/graphics/Rect;", "contains", NULL, rect_contains },
    { "Landroid/graphics/Rect;", "intersects", NULL, rect_intersects },
    { "Landroid/graphics/Rect;", "intersect", NULL, rect_intersect },
    { "Landroid/graphics/RectF;", "<init>", "V", rect_init_void },
    { "Landroid/graphics/RectF;", "<init>", "VFFFF", rect_init_float },
    { "Landroid/graphics/RectF;", "<init>", NULL, rect_init_float },
    { "Landroid/graphics/RectF;", "set", "VFFFF", rect_set_float },
    { "Landroid/graphics/RectF;", "set", NULL, rect_set_float },
    { "Landroid/graphics/RectF;", "intersects", NULL, rect_intersects },
    { "Landroid/graphics/RectF;", "intersect", NULL, rect_intersect },

    /* Bitmap */
    { "Landroid/graphics/Bitmap;", "getWidth", NULL, bitmap_getWidth },
    { "Landroid/graphics/Bitmap;", "getHeight", NULL, bitmap_getHeight },
    { "Landroid/graphics/Bitmap;", "recycle", NULL, bitmap_recycle },
    { "Landroid/graphics/Bitmap;", "createBitmap", NULL, bitmap_createBitmap },
    { "Landroid/graphics/Bitmap;", "createScaledBitmap", NULL, bitmap_createScaledBitmap },

    /* BitmapFactory */
    { "Landroid/graphics/BitmapFactory;", "decodeResource", NULL, bitmapFactory_decodeResource },
    { "Landroid/graphics/BitmapFactory$Options;", "<init>", NULL, noop_stub },

    /* Canvas */
    { "Landroid/graphics/Canvas;", "save", NULL, canvas_save },
    { "Landroid/graphics/Canvas;", "restore", NULL, canvas_restore },
    { "Landroid/graphics/Canvas;", "scale", NULL, canvas_scale },
    { "Landroid/graphics/Canvas;", "drawBitmap", "VLFFL", timed_canvas_drawBitmap_xy },
    { "Landroid/graphics/Canvas;", "drawBitmap", "VLFF", timed_canvas_drawBitmap_xy },
    { "Landroid/graphics/Canvas;", "drawBitmap", "VLLL", timed_canvas_drawBitmap_matrix },
    { "Landroid/graphics/Canvas;", "drawBitmap", "VLLLL", timed_canvas_drawBitmap_rect },
    { "Landroid/graphics/Canvas;", "drawBitmap", NULL, timed_canvas_drawBitmap_xy },
    { "Landroid/graphics/Canvas;", "drawRect", NULL, timed_canvas_drawRect },

    /* View */
    { "Landroid/view/View;", "<init>", NULL, noop_stub },
    { "Landroid/view/View;", "onSizeChanged", NULL, noop_stub },
    { "Landroid/view/View;", "invalidate", NULL, view_invalidate },
    { "Landroid/view/View;", "getContext", NULL, view_getContext },
    { "Landroid/view/View;", "getResources", NULL, view_getResources },

    /* Activity & Context */
    { "Landroid/app/Activity;", "<init>", NULL, noop_stub },
    { "Landroid/app/Activity;", "onCreate", NULL, noop_stub },
    { "Landroid/app/Activity;", "getSharedPreferences", NULL, activity_getSharedPreferences },
    { "Landroid/content/Context;", "getSharedPreferences", NULL, activity_getSharedPreferences },
    { "Landroid/content/Context;", "getPackageName", NULL, activity_getPackageName },
    { "Landroid/content/Context;", "startActivity", NULL, noop_stub },
    { "Landroid/content/SharedPreferences;", "contains", NULL, sharedPrefs_contains },
    { "Landroid/content/SharedPreferences;", "edit", NULL, sharedPrefs_edit },
    { "Landroid/content/SharedPreferences$Editor;", "putInt", NULL, editor_putInt },
    { "Landroid/content/SharedPreferences$Editor;", "putLong", NULL, editor_putLong },
    { "Landroid/content/SharedPreferences$Editor;", "putFloat", NULL, editor_putFloat },
    { "Landroid/content/SharedPreferences$Editor;", "putBoolean", NULL, editor_putBoolean },
    { "Landroid/content/SharedPreferences$Editor;", "putString", NULL, editor_putString },
    { "Landroid/content/SharedPreferences$Editor;", "remove", NULL, editor_remove },
    { "Landroid/content/SharedPreferences$Editor;", "clear", NULL, editor_clear },
    { "Landroid/content/SharedPreferences$Editor;", "apply", NULL, editor_apply },
    { "Landroid/content/SharedPreferences$Editor;", "commit", NULL, editor_commit },
    { "Landroid/preference/PreferenceManager;", "getDefaultSharedPreferences", NULL, activity_getSharedPreferences },
    { "Landroidx/preference/PreferenceManager;", "getDefaultSharedPreferences", NULL, activity_getSharedPreferences },
    { "Landroid/content/SharedPreferences;", "getInt", NULL, sharedPrefs_getInt },
    { "Landroid/content/SharedPreferences;", "getBoolean", NULL, sharedPrefs_getBoolean },
    { "Landroid/content/SharedPreferences;", "getFloat", NULL, sharedPrefs_getFloat },
    { "Landroid/content/SharedPreferences;", "getString", NULL, sharedPrefs_getString },
    { "Landroid/content/SharedPreferences;", "getLong", NULL, sharedPrefs_getLong },
    { "Landroid/content/res/Resources;", "getIdentifier", NULL, resources_getIdentifier },

    /* Choreographer */
    { "Landroid/view/Choreographer;", "getInstance", NULL, choreographer_getInstance },
    { "Landroid/view/Choreographer;", "postFrameCallback", NULL, choreographer_postFrameCallback },

    /* MotionEvent */
    { "Landroid/view/MotionEvent;", "getX", NULL, motionEvent_getX },
    { "Landroid/view/MotionEvent;", "getY", NULL, motionEvent_getY },
    { "Landroid/view/MotionEvent;", "getAction", NULL, motionEvent_getAction },

    /* Math & Collections */
    { "Landroid/graphics/RectF;", "offset", NULL, rect_offset },
    { "Landroid/graphics/Rect;", "offset", NULL, rect_offset_int },
    { "Ljava/util/concurrent/ExecutorService;", "submit", NULL, executor_submit },
    { "Ljava/util/concurrent/ExecutorService;", "execute", NULL, executor_submit },
    { "Ljava/util/concurrent/ExecutorService;", "isShutdown", NULL, return_false },
    { "Ljava/lang/Math;", "min", "III", math_min_i },
    { "Ljava/lang/Math;", "max", "III", math_max_i },
    { "Ljava/lang/Math;", "min", "JJJ", math_min_j },
    { "Ljava/lang/Math;", "max", "JJJ", math_max_j },
    { "Ljava/lang/Math;", "min", "FFF", math_min_f },
    { "Ljava/lang/Math;", "max", "FFF", math_max_f },
    { "Ljava/lang/Math;", "min", "DDD", math_min_d },
    { "Ljava/lang/Math;", "max", "DDD", math_max_d },
    { "Ljava/lang/Math;", "abs", "II", math_abs_i },
    { "Ljava/lang/Math;", "abs", "JJ", math_abs_j },
    { "Ljava/lang/Math;", "abs", "FF", math_abs_f },
    { "Ljava/lang/Math;", "abs", "DD", math_abs_d },
    { "Ljava/lang/Math;", "sqrt", NULL, math_sqrt },
    { "Ljava/lang/Math;", "sin", NULL, math_sin },
    { "Ljava/lang/Math;", "cos", NULL, math_cos },
    { "Ljava/lang/Math;", "tan", NULL, math_tan },
    { "Ljava/lang/Math;", "asin", NULL, math_asin },
    { "Ljava/lang/Math;", "acos", NULL, math_acos },
    { "Ljava/lang/Math;", "atan", NULL, math_atan },
    { "Ljava/lang/Math;", "atan2", NULL, math_atan2 },
    { "Ljava/lang/Math;", "exp", NULL, math_exp },
    { "Ljava/lang/Math;", "log", NULL, math_log },
    { "Ljava/lang/Math;", "log10", NULL, math_log10 },
    { "Ljava/lang/Math;", "pow", NULL, math_pow },
    { "Ljava/lang/Math;", "hypot", NULL, math_hypot },
    { "Ljava/lang/Math;", "floor", NULL, math_floor },
    { "Ljava/lang/Math;", "ceil", NULL, math_ceil },
    { "Ljava/lang/Math;", "round", "IF", math_round_f },
    { "Ljava/lang/Math;", "round", "JD", math_round_d },
    { "Ljava/lang/Math;", "toRadians", NULL, math_toRadians },
    { "Ljava/lang/Math;", "toDegrees", NULL, math_toDegrees },
    { "Ljava/lang/Math;", "random", NULL, math_random },
    { "Ljava/lang/System;", "nanoTime", NULL, system_nanoTime },
    { "Ljava/lang/System;", "currentTimeMillis", NULL, system_currentTimeMillis },
    { "Ljava/lang/System;", "arraycopy", NULL, system_arraycopy },
    { "Ljava/lang/System;", "identityHashCode", NULL, system_identityHashCode },
    { "Ljava/util/WeakHashMap;", "<init>", NULL, noop_stub },
    { "Landroid/view/View;", "onDraw", NULL, noop_stub },
    { "Landroid/view/View;", "post", NULL, view_post },
    { "Landroid/view/View;", "postDelayed", NULL, view_postDelayed },
    { "Landroid/view/View;", "removeCallbacks", NULL, view_removeCallbacks },
    { "Landroid/view/View;", "performHapticFeedback", NULL, return_true },
    { "Landroid/os/Handler;", "<init>", NULL, noop_stub },
    { "Landroid/os/Handler;", "post", NULL, view_post },
    { "Landroid/os/Handler;", "postDelayed", NULL, view_postDelayed },
    { "Landroid/os/Handler;", "removeCallbacks", NULL, view_removeCallbacks },
    { "Ljava/lang/String;", "length", NULL, string_length },
    { "Ljava/lang/String;", "charAt", NULL, string_charAt },
    { "Ljava/lang/String;", "isEmpty", NULL, string_isEmpty },
    { "Ljava/lang/String;", "toCharArray", NULL, string_toCharArray },
    { "Landroid/view/View;", "setOnApplyWindowInsetsListener", NULL, noop_stub },
    { "Ljava/lang/String;", "toString", NULL, string_toString },
    { "Ljava/lang/String;", "equals", NULL, string_equals },
    { "Ljava/lang/String;", "hashCode", NULL, string_hashCode },
    { "Ljava/lang/String;", "concat", NULL, string_concat },
    { "Ljava/lang/String;", "valueOf", "LI", string_valueOf_i },
    { "Ljava/lang/String;", "valueOf", "LJ", string_valueOf_j },
    { "Ljava/lang/String;", "valueOf", "LF", string_valueOf_f },
    { "Ljava/lang/String;", "valueOf", "LD", string_valueOf_d },
    { "Ljava/lang/String;", "valueOf", "LZ", string_valueOf_z },
    { "Ljava/lang/String;", "valueOf", "LC", string_valueOf_c },
    { "Ljava/lang/String;", "valueOf", "LL", string_valueOf_l },
    { "Ljava/lang/Integer;", "valueOf", "LI", integer_valueOf },
    { "Ljava/lang/Integer;", "intValue", NULL, integer_intValue },
    { "Ljava/lang/Integer;", "longValue", NULL, integer_longValue },
    { "Ljava/lang/Integer;", "floatValue", NULL, integer_floatValue },
    { "Ljava/lang/Integer;", "doubleValue", NULL, integer_doubleValue },
    { "Ljava/lang/Integer;", "parseInt", NULL, integer_parseInt },
    { "Ljava/lang/Integer;", "toString", "LI", integer_toString_static },
    { "Ljava/lang/Integer;", "compare", NULL, integer_compare },
    { "Ljava/lang/Long;", "valueOf", "LJ", long_valueOf },
    { "Ljava/lang/Long;", "longValue", NULL, long_longValue },
    { "Ljava/lang/Long;", "intValue", NULL, long_intValue },
    { "Ljava/lang/Float;", "valueOf", "LF", float_valueOf },
    { "Ljava/lang/Float;", "floatValue", NULL, float_floatValue },
    { "Ljava/lang/Float;", "intValue", NULL, float_intValue },
    { "Ljava/lang/Float;", "doubleValue", NULL, float_doubleValue },
    { "Ljava/lang/Double;", "valueOf", "LD", double_valueOf },
    { "Ljava/lang/Double;", "doubleValue", NULL, double_doubleValue },
    { "Ljava/lang/Double;", "intValue", NULL, double_intValue },
    { "Ljava/lang/Double;", "floatValue", NULL, double_floatValue },
    { "Ljava/lang/Boolean;", "valueOf", "LZ", boolean_valueOf },
    { "Ljava/lang/Boolean;", "booleanValue", NULL, boolean_booleanValue },
    { "Ljava/lang/Character;", "valueOf", "LC", character_valueOf },
    { "Landroid/graphics/Matrix;", "setScale", NULL, matrix_setScale },
    { "Landroid/media/AudioAttributes$Builder;", "<init>", NULL, noop_stub },
    { "Landroid/media/AudioAttributes$Builder;", "setUsage", NULL, return_this },
    { "Landroid/media/AudioAttributes$Builder;", "setContentType", NULL, return_this },
    { "Landroid/media/AudioAttributes$Builder;", "build", NULL, return_this },
    { "Landroid/media/SoundPool$Builder;", "<init>", NULL, noop_stub },
    { "Landroid/media/SoundPool$Builder;", "setMaxStreams", NULL, return_this },
    { "Landroid/media/SoundPool$Builder;", "setAudioAttributes", NULL, return_this },
    { "Landroid/media/SoundPool$Builder;", "build", NULL, soundpool_build },
    { "Landroid/media/SoundPool;", "load", NULL, soundpool_load },
    { "Landroid/media/SoundPool;", "play", NULL, soundpool_play },
    { "Landroid/media/SoundPool;", "stop", NULL, soundpool_stop },
    { "Landroid/media/SoundPool;", "pause", NULL, noop_stub },
    { "Landroid/media/SoundPool;", "release", NULL, noop_stub },
    { "Ljava/util/Random;", "<init>", NULL, random_init },
    { "Ljava/util/Random;", "nextInt", NULL, random_nextInt },
    { "Ljava/util/Random;", "nextFloat", NULL, random_nextFloat },
    { "Ljava/util/Random;", "nextBoolean", NULL, random_nextBoolean },

    { "Ljava/util/ArrayList;", "<init>", NULL, arrayList_init },
    { "Ljava/util/ArrayList;", "add", NULL, arrayList_add },
    { "Ljava/util/ArrayList;", "get", NULL, arrayList_get },
    { "Ljava/util/ArrayList;", "size", NULL, arrayList_size },
    { "Ljava/util/ArrayList;", "clear", NULL, arrayList_clear },
    { "Ljava/util/ArrayList;", "iterator", NULL, arrayList_iterator },
    { "Ljava/util/List;", "add", NULL, arrayList_add },
    { "Ljava/util/List;", "get", NULL, arrayList_get },
    { "Ljava/util/List;", "size", NULL, arrayList_size },
    { "Ljava/util/List;", "clear", NULL, arrayList_clear },
    { "Ljava/util/List;", "iterator", NULL, arrayList_iterator },
    { "Ljava/util/Iterator;", "hasNext", NULL, iterator_hasNext },
    { "Ljava/util/Iterator;", "next", NULL, iterator_next },

    { "Ljava/lang/reflect/Array;", "newInstance", "LLI", array_newInstance_len },
    { "Ljava/lang/reflect/Array;", "newInstance", "LLL", array_newInstance_dims },
    { "Ljava/util/concurrent/Executors;", "newSingleThreadExecutor", NULL, executors_newSingleThreadExecutor },
    { "Ljava/util/concurrent/ExecutorService;", "execute", NULL, noop_stub },
    { "Landroid/util/Log;", "w", NULL, log_print },
    { "Landroid/util/Log;", "e", NULL, log_print },
    { "Landroid/util/Log;", "d", NULL, log_print },
    { "Landroid/util/Log;", "i", NULL, log_print },

    { NULL, NULL, NULL, NULL }
};

bool tl_framework_field_get(tl_dex_context *ctx, tl_dex_object *obj, const tl_dex_field *f, tl_dex_val *out)
{
    (void)ctx;
    if (!obj || !f || !f->name || !out) return false;
    if (is_rect_class(f->owner)) {
        tl_framework_rect *r = tl_dex_native(obj);
        float *p = r ? rect_field_ptr(r, f->name) : NULL;
        if (!p) return false;
        out->raw64 = 0;
        if (f->type && f->type[0] == 'F') out->f = *p;
        else                              out->i = (int32_t)*p;
        return true;
    }
    return false;
}

bool tl_framework_field_set(tl_dex_context *ctx, tl_dex_object *obj, const tl_dex_field *f, tl_dex_val value)
{
    (void)ctx; (void)value;
    if (!obj || !f || !f->name) return false;
    /* BitmapFactory.Options is a bag of hints -- inScaled, inSampleSize,
     * inPreferredConfig -- that an app sets before decoding. This layer decodes
     * one way regardless, so accepting the write and ignoring it is the correct
     * implementation, not a missing one. */
    if (f->owner && !strcmp(f->owner, "Landroid/graphics/BitmapFactory$Options;")) return true;
    if (is_rect_class(f->owner)) {
        tl_framework_rect *r = tl_dex_native(obj);
        float *p = r ? rect_field_ptr(r, f->name) : NULL;
        if (!p) return false;
        *p = (f->type && f->type[0] == 'F') ? value.f : (float)value.i;
        return true;
    }
    return false;
}

/*
 * Static fields of framework classes. Build.VERSION.SDK_INT is the one apps ask
 * about most: it gates which APIs they call, and zero reads as "older than
 * anything", sending code down compatibility paths meant for Android 1.x.
 * This layer answers as the API level its shims were written against.
 */
bool tl_framework_static_get(tl_dex_context *ctx, const tl_dex_field *f, tl_dex_val *out)
{
    (void)ctx;
    if (!f || !f->owner || !f->name || !out) return false;
    out->raw64 = 0;
    if (!strcmp(f->owner, "Landroid/os/Build$VERSION;") && !strcmp(f->name, "SDK_INT")) {
        out->i = 34;
        return true;
    }
    return false;
}

tl_dex_native_func tl_framework_lookup(const char *class_desc, const char *method_name, const char *shorty)
{
    if (!class_desc || !method_name) return NULL;
    /* First pass: exact shorty match */
    if (shorty) {
        for (int i = 0; s_native_methods[i].class_desc; i++) {
            if (!strcmp(s_native_methods[i].class_desc, class_desc) &&
                !strcmp(s_native_methods[i].method_name, method_name) &&
                s_native_methods[i].shorty &&
                !strcmp(s_native_methods[i].shorty, shorty)) {
                return s_native_methods[i].func;
            }
        }
    }
    /* Second pass: wild-card shorty */
    for (int i = 0; s_native_methods[i].class_desc; i++) {
        if (!strcmp(s_native_methods[i].class_desc, class_desc) &&
            !strcmp(s_native_methods[i].method_name, method_name) &&
            !s_native_methods[i].shorty) {
            return s_native_methods[i].func;
        }
    }
    return NULL;
}


bool tl_framework_init(tl_dex_context *ctx)
{
    tl_framework_state *st = calloc(1, sizeof(*st));
    ctx->framework_data = st;

    if (ctx->framebuffer && ctx->fb_width > 0 && ctx->fb_height > 0) {
#if defined(__APPLE__)
        CGColorSpaceRef cs = tl_fw_rgb_color_space();
        st->cg_ctx = CGBitmapContextCreate(ctx->framebuffer, ctx->fb_width, ctx->fb_height, 8,
                                           ctx->fb_width * 4, cs,
                                           kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        if (st->cg_ctx) {
            /* Pixel art is drawn without filtering unless a paint asks for it; the
             * draw calls set this per draw, and this is what anything that does
             * not goes through those gets. */
            CGContextSetInterpolationQuality(st->cg_ctx, kCGInterpolationNone);
            /* Flip CoreGraphics to match Android top-left origin */
            CGContextTranslateCTM(st->cg_ctx, 0, ctx->fb_height);
            CGContextScaleCTM(st->cg_ctx, 1.0f, -1.0f);
        }
#endif
    }

    tl_dex_class *c_canvas = tl_dex_find_class(ctx, "Landroid/graphics/Canvas;");
    st->canvas_obj = tl_dex_alloc_object(c_canvas);

    tl_dex_class *c_act = tl_dex_find_class(ctx, "Landroid/app/Activity;");
    st->activity_obj = tl_dex_alloc_object(c_act);

    tl_dex_class *c_res = tl_dex_find_class(ctx, "Landroid/content/res/Resources;");
    st->resources_obj = tl_dex_alloc_object(c_res);

    tl_dex_class *c_ch = tl_dex_find_class(ctx, "Landroid/view/Choreographer;");
    st->choreographer_obj = tl_dex_alloc_object(c_ch);

    tl_dex_class *c_pref = tl_dex_find_class(ctx, "Landroid/content/SharedPreferences;");
    st->prefs_obj = tl_dex_alloc_object(c_pref);
    st->editor_obj = tl_dex_alloc_object(tl_dex_find_class(ctx, "Landroid/content/SharedPreferences$Editor;"));
    st->prefs = tl_prefs_create();

    return true;
}

void tl_framework_cleanup(tl_dex_context *ctx)
{
    if (!ctx || !ctx->framework_data) return;
    tl_framework_state *st = ctx->framework_data;
#if defined(__APPLE__)
    if (st->cg_ctx) CGContextRelease(st->cg_ctx);
#endif
    tl_res_destroy(st->res);
    free(st->arsc);
    tl_prefs_destroy(st->prefs);
    free(st);
    ctx->framework_data = NULL;
}

void tl_framework_render_view(tl_dex_context *ctx)
{
    if (!ctx || !ctx->current_view) return;
    tl_framework_state *st = ctx->framework_data;
    if (!st || !st->canvas_obj) return;

    tl_dex_method *onDraw = tl_dex_find_method(ctx->current_view->clazz, "onDraw", "VL");
    if (onDraw) {
        tl_dex_val args[2];
        args[0].l = ctx->current_view;
        args[1].l = st->canvas_obj;
        tl_dex_invoke(ctx, onDraw, args, 2, NULL);
    }
}
