/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

typedef struct { int id; } jobj;
typedef union { uint8_t z; int32_t i; int64_t j; jobj *l; } jvalue;
#define L(v) ((jvalue){ .l = (v) })
#define I(v) ((jvalue){ .i = (v) })
static struct { atomic_bool render_requested; } A;
static jobj view = {1}, renderer = {2}, gl = {3};
static bool alive, draw_requested, getter_fails, pending;
static int queued, begins, queries, resize_takes, changes, draws, errors, width, height;
static int64_t resize;
static bool tl_jni_pending(void) { return pending; }
static void log_pending(const char *where) { (void)where; errors++; pending = false; }
static void tl_log_line(const char *format, int w, int h) { (void)format; (void)w; (void)h; }
static jvalue tl_jni_call(jobj *object, const char *name, const char *sig, const jvalue *args)
{
    if (!strcmp(name, "huskBeginFrame")) {
        assert(object == &view && !strcmp(sig, "()Z"));
        begins++; queued++;
        return (jvalue){ .z = draw_requested };
    }
    if (!strcmp(name, "huskHasSurface")) {
        assert(object == &view && !strcmp(sig, "()Z")); queries++;
        pending = getter_fails;
        return (jvalue){ .z = alive };
    }
    if (!strcmp(name, "huskTakeResize")) {
        assert(object == &view && !strcmp(sig, "()J")); resize_takes++;
        int64_t old = resize; resize = 0;
        return (jvalue){ .j = old };
    }
    if (!strcmp(name, "onSurfaceChanged")) {
        assert(object == &renderer && args[0].l == &gl);
        assert(!strcmp(sig, "(Ljavax/microedition/khronos/opengles/GL10;II)V"));
        changes++; width = args[1].i; height = args[2].i;
        return (jvalue){0};
    }
    assert(!strcmp(name, "onDrawFrame") && object == &renderer && args[0].l == &gl);
    assert(!strcmp(sig, "(Ljavax/microedition/khronos/opengles/GL10;)V"));
    draws++;
    return (jvalue){0};
}
#include "java_renderer_frame_under_test.h"

int main(void)
{
    jvalue args[] = { L(&gl) };
    resize = ((int64_t)640 << 32) | 360;
    atomic_store(&A.render_requested, true);
    alive = false; draw_requested = false;
    assert(!java_renderer_frame(&view, &renderer, args, true));
    assert(begins == 1 && queued == 1 && queries == 1);
    assert(draws == 0 && changes == 0 && resize_takes == 0);
    assert(resize != 0 && atomic_load(&A.render_requested));

    /* Even a stale dirty result must not override the current surface state. */
    draw_requested = true;
    assert(!java_renderer_frame(&view, &renderer, args, true));
    assert(draws == 0 && changes == 0 && resize_takes == 0);
    assert(resize != 0 && atomic_load(&A.render_requested));

    /* Recreate: the retained renderer receives the pending size once. */
    alive = true; draw_requested = false;
    assert(java_renderer_frame(&view, &renderer, args, false));
    assert(draws == 1 && changes == 1 && resize_takes == 1 && width == 640 && height == 360);
    assert(resize == 0 && !atomic_load(&A.render_requested));
    assert(!java_renderer_frame(&view, &renderer, args, false));
    assert(draws == 1 && changes == 1);

    /* A UI wake is allowed with a surface; it is rejected after destruction. */
    assert(java_renderer_frame(&view, &renderer, args, true));
    assert(draws == 2);
    alive = false;
    assert(!java_renderer_frame(&view, &renderer, args, true));
    assert(draws == 2);

    /* Query failure is conservative, preserves pending work, and reports it. */
    alive = true; getter_fails = true;
    resize = ((int64_t)320 << 32) | 180;
    atomic_store(&A.render_requested, true);
    assert(!java_renderer_frame(&view, &renderer, args, true));
    assert(errors == 1 && !pending && draws == 2 && changes == 1);
    assert(resize != 0 && atomic_load(&A.render_requested));

    puts("PASS: actual JNI renderer callback admission rejects destroyed surfaces despite UI wakes, resize and render requests, preserves pending work for recreation and services queued events before the surface gate");
    return 0;
}
