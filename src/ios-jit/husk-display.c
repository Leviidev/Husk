/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Husk -- DisplayChangeListener bridge.  See husk-display.h.
 *
 * Written fresh rather than adapted from UTM's CocoaSpice, which is Apache-2.0
 * and therefore incompatible with QEMU's GPLv2. See docs/01-licensing.md.
 */

#include "qemu/osdep.h"
#include "qemu/main-loop.h"
#include "qemu/thread.h"
#include "ui/console.h"
#include "ui/input.h"
#include "qapi/error.h"
#include "qapi/util.h"
#include "qapi/qapi-types-ui.h"

#include "husk-display.h"

#include <os/log.h>
#include <sys/time.h>

static double husk_dpy_now_ms(void)
{
    static double base = 0;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    double t = tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
    if (base == 0) { base = t; }
    return t - base;
}

#define HUSK_DLOG(fmt, ...)                                                    \
    do {                                                                       \
        os_log(OS_LOG_DEFAULT, "[husk-dpy] " fmt, ##__VA_ARGS__);              \
        fprintf(stderr, "[%9.2fms][husk-dpy] " fmt "\n",                       \
                husk_dpy_now_ms(), ##__VA_ARGS__);                             \
        fflush(stderr);                                                        \
    } while (0)

typedef struct HuskDisplayState {
    DisplayChangeListener dcl;
    QemuMutex             lock;

    DisplaySurface *surface;
    uint64_t        generation;
    uint64_t        sequence;
    bool            inited;
} HuskDisplayState;

static HuskDisplayState husk;

/* ------------------------------------------------------------ DCL callbacks */
/* All of these run on the QEMU main-loop thread with the BQL held. */

static void husk_dpy_gfx_update(DisplayChangeListener *dcl,
                                int x, int y, int w, int h)
{
    (void)dcl;
    /*
     * Deliberately not tracking dirty rectangles. The guest framebuffer for a
     * phone-sized surface uploads to a Metal texture in well under a frame, and
     * partial-rect bookkeeping across two threads buys nothing until that stops
     * being true. Revisit if profiling on device says otherwise.
     */
    uint64_t seq = qatomic_fetch_inc(&husk.sequence) + 1;

    /* The first few draws are the interesting ones -- they prove the guest is
     * alive and rendering. After that, log sparsely so a long session does not
     * drown the log. */
    if (seq <= 5 || (seq % 600) == 0) {
        HUSK_DLOG("gfx_update #%llu rect=%dx%d@%d,%d",
                  (unsigned long long)seq, w, h, x, y);
    }
}

static void husk_dpy_gfx_switch(DisplayChangeListener *dcl,
                                DisplaySurface *new_surface)
{
    (void)dcl;
    qemu_mutex_lock(&husk.lock);
    husk.surface = new_surface;
    husk.generation++;
    uint64_t gen = husk.generation;
    int w = new_surface ? surface_width(new_surface) : 0;
    int h = new_surface ? surface_height(new_surface) : 0;
    int stride = new_surface ? surface_stride(new_surface) : 0;
    int bpp = new_surface ? surface_bits_per_pixel(new_surface) : 0;
    const void *data = new_surface ? surface_data(new_surface) : NULL;
    qemu_mutex_unlock(&husk.lock);
    qatomic_inc(&husk.sequence);

    HUSK_DLOG("gfx_switch gen=%llu surface=%p %dx%d stride=%d bpp=%d data=%p",
              (unsigned long long)gen, (void *)new_surface, w, h, stride, bpp, data);
}

static bool husk_dpy_gfx_check_format(DisplayChangeListener *dcl,
                                      pixman_format_code_t format)
{
    (void)dcl;
    /* 32bpp only: it is what virtio-gpu gives us and what Metal wants. */
    bool ok = (format == PIXMAN_x8r8g8b8 || format == PIXMAN_a8r8g8b8);
    HUSK_DLOG("gfx_check_format 0x%x -> %s", (unsigned)format, ok ? "accept" : "reject");
    return ok;
}

static void husk_dpy_refresh(DisplayChangeListener *dcl)
{
    graphic_hw_update(dcl->con);
}

static const DisplayChangeListenerOps husk_dcl_ops = {
    .dpy_name             = "husk",
    .dpy_refresh          = husk_dpy_refresh,
    .dpy_gfx_update       = husk_dpy_gfx_update,
    .dpy_gfx_switch       = husk_dpy_gfx_switch,
    .dpy_gfx_check_format = husk_dpy_gfx_check_format,
};

/* ------------------------------------------------------------- public API */

void husk_display_init(void)
{
    if (husk.inited) {
        HUSK_DLOG("init: already initialised, ignoring");
        return;
    }
    HUSK_DLOG("init: registering DisplayChangeListener");

    qemu_mutex_init(&husk.lock);
    husk.dcl.ops = &husk_dcl_ops;
    husk.dcl.con = qemu_console_lookup_by_index(0);

    if (husk.dcl.con == NULL) {
        /* The most likely way this whole path fails quietly. With -display none
         * QEMU still creates a console for the graphics device, but if the device
         * is missing or named differently there is nothing to attach to, and every
         * later symptom is just "black screen". */
        HUSK_DLOG("init: FATAL -- qemu_console_lookup_by_index(0) returned NULL. "
                  "No graphics console exists; check that -device virtio-gpu-pci "
                  "is on the command line.");
        return;
    }
    HUSK_DLOG("init: console[0]=%p graphic=%d",
              (void *)husk.dcl.con, QEMU_IS_GRAPHIC_CONSOLE(husk.dcl.con) ? 1 : 0);

    register_displaychangelistener(&husk.dcl);
    husk.inited = true;
    HUSK_DLOG("init: listener registered; QEMU will now drive dpy_refresh");
}

bool husk_display_lock_frame(HuskFrameInfo *out)
{
    if (!husk.inited || out == NULL) {
        return false;
    }
    qemu_mutex_lock(&husk.lock);
    if (husk.surface == NULL) {
        qemu_mutex_unlock(&husk.lock);
        return false;
    }
    out->pixels     = surface_data(husk.surface);
    out->width      = surface_width(husk.surface);
    out->height     = surface_height(husk.surface);
    out->stride     = surface_stride(husk.surface);
    out->bpp        = surface_bits_per_pixel(husk.surface);
    out->generation = husk.generation;
    out->sequence   = qatomic_read(&husk.sequence);

    if (out->pixels == NULL || out->width <= 0 || out->height <= 0) {
        qemu_mutex_unlock(&husk.lock);
        return false;
    }
    return true; /* caller must unlock */
}

void husk_display_unlock_frame(void)
{
    if (husk.inited) {
        qemu_mutex_unlock(&husk.lock);
    }
}

uint64_t husk_display_sequence(void)
{
    return husk.inited ? qatomic_read(&husk.sequence) : 0;
}

void husk_display_send_pointer(int32_t x, int32_t y, bool button_down)
{
    int w, h;

    if (!husk.inited) {
        return;
    }

    /*
     * qemu_input_* must run under the BQL. Taking it here rather than marshalling
     * through a bottom half keeps touch latency down; the hold is a few
     * microseconds and the UI thread blocking that long is not perceptible.
     */
    bql_lock();
    qemu_mutex_lock(&husk.lock);
    w = husk.surface ? surface_width(husk.surface) : 0;
    h = husk.surface ? surface_height(husk.surface) : 0;
    qemu_mutex_unlock(&husk.lock);

    static uint64_t pointer_events = 0;
    uint64_t ev = ++pointer_events;

    if (w > 0 && h > 0) {
        if (x < 0) { x = 0; } else if (x >= w) { x = w - 1; }
        if (y < 0) { y = 0; } else if (y >= h) { y = h - 1; }
        qemu_input_queue_abs(husk.dcl.con, INPUT_AXIS_X, x, 0, w);
        qemu_input_queue_abs(husk.dcl.con, INPUT_AXIS_Y, y, 0, h);
        qemu_input_queue_btn(husk.dcl.con, INPUT_BUTTON_LEFT, button_down);
        qemu_input_event_sync();
        if (ev <= 20 || (ev % 200) == 0) {
            HUSK_DLOG("pointer #%llu -> guest (%d,%d) down=%d [surface %dx%d]",
                      (unsigned long long)ev, x, y, button_down ? 1 : 0, w, h);
        }
    } else {
        HUSK_DLOG("pointer #%llu DROPPED -- no surface yet", (unsigned long long)ev);
    }
    bql_unlock();
}

bool husk_display_send_key(const char *qcode_name, bool down)
{
    if (!husk.inited || qcode_name == NULL) {
        return false;
    }

    /*
     * Resolved by name through QEMU's own QKeyCode table rather than by passing a
     * raw enum value across the boundary. The numbers are positional in
     * qapi/ui.json -- 162 of them -- so hardcoding them on the Swift side would
     * silently remap every key the moment that list gains an entry.
     */
    int qcode = qapi_enum_parse(&QKeyCode_lookup, qcode_name, -1, NULL);
    if (qcode < 0) {
        HUSK_DLOG("key '%s' is not a QKeyCode; ignored", qcode_name);
        return false;
    }

    static uint64_t keys = 0;
    uint64_t n = ++keys;

    bql_lock();
    qemu_input_event_send_key_qcode(husk.dcl.con, (QKeyCode)qcode, down);
    bql_unlock();

    if (n <= 20 || (n % 100) == 0) {
        HUSK_DLOG("key #%llu '%s' (qcode %d) down=%d",
                  (unsigned long long)n, qcode_name, qcode, down ? 1 : 0);
    }
    return true;
}

void husk_display_request_update(void)
{
    if (!husk.inited) {
        return;
    }
    bql_lock();
    graphic_hw_update(husk.dcl.con);
    bql_unlock();
}
