/* SPDX-License-Identifier: GPL-2.0-or-later */
#define _DARWIN_C_SOURCE
#include "husk-tl-gamepad.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "husk-tl-bionic.h"

#define MAX_AXES 48
#define DEVICE_BASE 41                    /* Android device ids of the controllers: 41, 42, ... */
#define SOURCE_GAMEPAD_STICKS 0x01000411  /* SOURCE_GAMEPAD | SOURCE_JOYSTICK */
#define SOURCE_ALL 0x01000611             /* ... and SOURCE_DPAD: everything the device offers */

enum { AXIS_X = 0, AXIS_Y = 1, AXIS_Z = 11, AXIS_RZ = 14, AXIS_HAT_X = 15, AXIS_HAT_Y = 16, AXIS_LTRIGGER = 17, AXIS_RTRIGGER = 18, AXIS_GAS = 22, AXIS_BRAKE = 23 };

static struct {
    pthread_mutex_t lock;
    bool connected[TL_PADS];
    char name[TL_PADS][64];
    tl_pad_state last[TL_PADS];
    float axes[TL_PADS][MAX_AXES];
    bool trigger_key[TL_PADS][2];
    int64_t down_ms[TL_PADS][TL_PAD_BUTTONS + 2];
    tl_pad_sink sink;
    int dpad_mode;                        /* 0: the hat, 1: the D-pad keys, 2: both (TL_PAD_DPAD) */
} P = { .lock = PTHREAD_MUTEX_INITIALIZER, .dpad_mode = -1 };

static int64_t now_ms(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; }

/* A controller is usually connected (and updated) before the game starts, so TL_PAD_DPAD, which the engine's driver sets just before it installs its sink, is read again here. */
void tl_pad_set_sink(const tl_pad_sink *sink) { pthread_mutex_lock(&P.lock); if (sink) P.sink = *sink; else memset(&P.sink, 0, sizeof(P.sink)); P.dpad_mode = -1; pthread_mutex_unlock(&P.lock); }

bool tl_pad_connected(int slot) { return slot >= 0 && slot < TL_PADS && P.connected[slot]; }

void tl_pad_connect(int slot, const char *name)
{
    if (slot < 0 || slot >= TL_PADS) return;
    pthread_mutex_lock(&P.lock);
    P.connected[slot] = true;
    snprintf(P.name[slot], sizeof(P.name[slot]), "%s", name && name[0] ? name : "controller");
    memset(&P.last[slot], 0, sizeof(P.last[slot]));
    memset(P.axes[slot], 0, sizeof(P.axes[slot]));
    memset(P.trigger_key[slot], 0, sizeof(P.trigger_key[slot]));
    pthread_mutex_unlock(&P.lock);
    tl_log_line("pad: controller %d connected (%s)", slot, P.name[slot]);
}

void tl_pad_disconnect(int slot)
{
    if (slot < 0 || slot >= TL_PADS) return;
    pthread_mutex_lock(&P.lock);
    P.connected[slot] = false;
    pthread_mutex_unlock(&P.lock);
    tl_log_line("pad: controller %d disconnected", slot);
}

/* Android key code of each button bit; the D-pad has none here (it is the hat) unless TL_PAD_DPAD says so. */
static const int k_keycode[TL_PAD_BUTTONS] = { 96, 97, 99, 100, 102, 103, 106, 107, 108, 109, 110, 19, 20, 21, 22 };

void tl_pad_update(int slot, const tl_pad_state *s)
{
    if (slot < 0 || slot >= TL_PADS) return;
    struct { int action, keycode; int64_t down; } keys[TL_PAD_BUTTONS + 2];
    int nkeys = 0;
    float axes[MAX_AXES];
    bool moved = false;
    tl_pad_sink sink;
    int64_t down_ms, ms = now_ms();

    pthread_mutex_lock(&P.lock);
    if (!P.connected[slot]) { pthread_mutex_unlock(&P.lock); return; }
    if (P.dpad_mode < 0) { const char *m = getenv("TL_PAD_DPAD"); P.dpad_mode = m && !strcmp(m, "keys") ? 1 : m && !strcmp(m, "both") ? 2 : 0; }
    tl_pad_state *old = &P.last[slot];
    uint32_t changed = old->buttons ^ s->buttons;
    for (int b = 0; b < TL_PAD_BUTTONS; b++) {
        if (!(changed & (1u << b))) continue;
        if (b >= TL_PAD_DPAD_UP && P.dpad_mode == 0) continue;
        bool down = (s->buttons >> b) & 1;
        if (down) P.down_ms[slot][b] = ms;
        keys[nkeys].action = down ? 0 : 1; keys[nkeys].keycode = k_keycode[b]; keys[nkeys].down = P.down_ms[slot][b]; nkeys++;
    }
    /* The triggers are also the L2/R2 buttons, which a game may read instead of the axes. */
    const float trig[2] = { s->lt, s->rt };
    for (int t = 0; t < 2; t++) {
        bool was = P.trigger_key[slot][t], now = was ? trig[t] > 0.3f : trig[t] > 0.55f;
        if (now == was) continue;
        P.trigger_key[slot][t] = now;
        if (now) P.down_ms[slot][TL_PAD_BUTTONS + t] = ms;
        keys[nkeys].action = now ? 0 : 1; keys[nkeys].keycode = 104 + t; keys[nkeys].down = P.down_ms[slot][TL_PAD_BUTTONS + t]; nkeys++;
    }

    memset(axes, 0, sizeof(axes));
    axes[AXIS_X] = s->lx; axes[AXIS_Y] = s->ly; axes[AXIS_Z] = s->rx; axes[AXIS_RZ] = s->ry;
    axes[AXIS_LTRIGGER] = axes[AXIS_BRAKE] = s->lt; axes[AXIS_RTRIGGER] = axes[AXIS_GAS] = s->rt;
    if (P.dpad_mode != 1) {
        axes[AXIS_HAT_X] = ((s->buttons >> TL_PAD_DPAD_RIGHT) & 1) ? 1.0f : ((s->buttons >> TL_PAD_DPAD_LEFT) & 1) ? -1.0f : 0.0f;
        axes[AXIS_HAT_Y] = ((s->buttons >> TL_PAD_DPAD_DOWN) & 1) ? 1.0f : ((s->buttons >> TL_PAD_DPAD_UP) & 1) ? -1.0f : 0.0f;
    }
    for (int a = 0; a < MAX_AXES; a++) if (fabsf(axes[a] - P.axes[slot][a]) >= 0.004f) { moved = true; break; }
    if (moved) memcpy(P.axes[slot], axes, sizeof(axes));
    *old = *s;
    sink = P.sink;
    down_ms = ms;
    pthread_mutex_unlock(&P.lock);

    int device = DEVICE_BASE + slot;
    for (int i = 0; i < nkeys && sink.key; i++) {
        jobj *ev = tl_input_key_event(keys[i].action, keys[i].keycode, device, 0x401 /* SOURCE_GAMEPAD */, 0, keys[i].down, ms);
        sink.key(ev, device, keys[i].action, keys[i].keycode, keys[i].down, ms);
    }
    if (moved && sink.motion) {
        jobj *ev = tl_input_joystick_event(device, SOURCE_GAMEPAD_STICKS, down_ms, ms, axes);
        sink.motion(ev, device, SOURCE_GAMEPAD_STICKS, down_ms, ms);
    }
}

/* ------------------------------------------------------------- InputDevice */

static jvalue vl(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }

static jobj *g_dev[TL_PADS];
static int slot_of(const jobj *o) { return o && o->native ? (int)(intptr_t)o->native - 1 : -1; }

static jobj *device_object(int slot)
{
    if (slot < 0 || slot >= TL_PADS || !P.connected[slot]) return NULL;
    if (!g_dev[slot]) { g_dev[slot] = tl_jni_new_object(tl_jni_class("android/view/InputDevice")); g_dev[slot]->native = (void *)(intptr_t)(slot + 1); }
    return tl_jni_ref(g_dev[slot]);
}

static void ID_getDeviceIds(tl_jcall *c)
{
    int ids[TL_PADS], n = 0;
    for (int s = 0; s < TL_PADS; s++) if (P.connected[s]) ids[n++] = DEVICE_BASE + s;
    jobj *a = tl_jni_new_prim_array('I', (uint32_t)n);
    if (n) memcpy(a->arr.data, ids, (size_t)n * sizeof(int));
    c->ret = vl(a);
}
static void ID_getDevice(tl_jcall *c) { c->ret = vl(device_object(c->args[0].i - DEVICE_BASE)); }
static void ID_getId(tl_jcall *c) { int s = slot_of(c->self); c->ret = vi(s >= 0 ? DEVICE_BASE + s : -1); }
/* Whatever it really is (a DualSense, a Switch Pro Controller...), a game is told it is an Xbox controller: the layout the games know. */
static void ID_getName(tl_jcall *c) { int s = slot_of(c->self); c->ret = vl(s >= 0 ? tl_jni_new_string("Xbox Wireless Controller") : NULL); }
static void ID_getDescriptor(tl_jcall *c) { char d[40]; snprintf(d, sizeof(d), "husk-controller-%d", slot_of(c->self)); c->ret = vl(tl_jni_new_string(d)); }
static void ID_getSources(tl_jcall *c) { c->ret = vi(SOURCE_ALL); }
static void ID_vendor(tl_jcall *c) { c->ret = vi(0x045e); }          /* Microsoft */
static void ID_product(tl_jcall *c) { c->ret = vi(0x02e0); }         /* Xbox Wireless Controller, over Bluetooth */
static void ID_one(tl_jcall *c) { c->ret = vi(slot_of(c->self) + 1); }
static void ID_zero(tl_jcall *c) { c->ret = vi(0); }
static void ID_false(tl_jcall *c) { c->ret = vz(0); }
static void ID_true(tl_jcall *c) { c->ret = vz(1); }
static void ID_null(tl_jcall *c) { c->ret.l = NULL; }
static void ID_supportsSource(tl_jcall *c) { int m = c->args[0].i; c->ret = vz((SOURCE_ALL & m) == m); }
static void ID_hasKeys(tl_jcall *c)
{
    jobj *keys = c->args[0].l;
    uint32_t n = keys && keys->kind == TL_K_PRIM_ARRAY ? keys->arr.len : 0;
    jobj *out = tl_jni_new_prim_array('Z', n);
    for (uint32_t i = 0; i < n; i++) {
        int k = ((int *)keys->arr.data)[i];
        ((uint8_t *)out->arr.data)[i] = (k >= 96 && k <= 110) || (k >= 19 && k <= 23) || k == 4;
    }
    c->ret = vl(out);
}

/* The axes a controller has, and the range of each. */
static bool axis_range(int axis, float *lo, float *hi)
{
    switch (axis) {
    case AXIS_X: case AXIS_Y: case AXIS_Z: case AXIS_RZ: case AXIS_HAT_X: case AXIS_HAT_Y: *lo = -1; *hi = 1; return true;
    case AXIS_LTRIGGER: case AXIS_RTRIGGER: case AXIS_GAS: case AXIS_BRAKE: *lo = 0; *hi = 1; return true;
    default: return false;
    }
}
typedef struct { int axis, source; float lo, hi, flat; } mrange;
static jobj *make_range(int axis)
{
    float lo, hi;
    if (!axis_range(axis, &lo, &hi)) return NULL;
    jobj *r = tl_jni_new_object(tl_jni_class("android/view/InputDevice$MotionRange"));
    mrange *m = calloc(1, sizeof(*m));
    m->axis = axis; m->source = SOURCE_GAMEPAD_STICKS; m->lo = lo; m->hi = hi; m->flat = (axis >= AXIS_HAT_X && axis <= AXIS_HAT_Y) ? 0.0f : 0.08f;
    r->native = m;
    return r;
}
static void ID_getMotionRange(tl_jcall *c) { c->ret = vl(make_range(c->args[0].i)); }

/* getMotionRanges(): a List of MotionRange, one for each axis the controller has. */
typedef struct { jobj **items; uint32_t n; } alist;
static void ID_getMotionRanges(tl_jcall *c)
{
    static const int axes[] = { AXIS_X, AXIS_Y, AXIS_Z, AXIS_RZ, AXIS_HAT_X, AXIS_HAT_Y, AXIS_LTRIGGER, AXIS_RTRIGGER, AXIS_GAS, AXIS_BRAKE };
    jobj *l = tl_jni_new_object(tl_jni_class("java/util/ArrayList"));
    alist *a = calloc(1, sizeof(*a));
    a->items = calloc(sizeof(axes) / sizeof(axes[0]), sizeof(jobj *));
    for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++) if ((a->items[a->n] = make_range(axes[i]))) a->n++;
    l->native = a;
    c->ret = vl(l);
}
static alist *AL(const tl_jcall *c) { return c->self ? c->self->native : NULL; }
static void AL_size(tl_jcall *c) { const alist *a = AL(c); c->ret = vi(a ? (int)a->n : 0); }
static void AL_isEmpty(tl_jcall *c) { const alist *a = AL(c); c->ret = vz(!a || a->n == 0); }
static void AL_get(tl_jcall *c)
{
    const alist *a = AL(c);
    int i = c->args[0].i;
    if (!a || i < 0 || (uint32_t)i >= a->n) { tl_jni_throw("java/lang/IndexOutOfBoundsException", ""); c->ret.l = NULL; return; }
    c->ret = vl(tl_jni_ref(a->items[i]));
}
static void AL_iterator(tl_jcall *c) { const alist *a = AL(c); c->ret = vl(tl_jni_new_list_iterator(a ? a->items : NULL, a ? a->n : 0)); }
static const mrange *MR(const tl_jcall *c) { return c->self ? c->self->native : NULL; }
static void MR_axis(tl_jcall *c) { const mrange *m = MR(c); c->ret = vi(m ? m->axis : 0); }
static void MR_source(tl_jcall *c) { const mrange *m = MR(c); c->ret = vi(m ? m->source : 0); }
static void MR_min(tl_jcall *c) { const mrange *m = MR(c); c->ret = vf(m ? m->lo : 0); }
static void MR_max(tl_jcall *c) { const mrange *m = MR(c); c->ret = vf(m ? m->hi : 0); }
static void MR_range(tl_jcall *c) { const mrange *m = MR(c); c->ret = vf(m ? m->hi - m->lo : 0); }
static void MR_flat(tl_jcall *c) { const mrange *m = MR(c); c->ret = vf(m ? m->flat : 0); }
static void MR_zerof(tl_jcall *c) { c->ret = vf(0); }

static void IM_noop(tl_jcall *c) { (void)c; }

#define K(c, n, s, f) { c, n, s, f }
static const tl_jhle k_pad_hle[] = {
    K("android/view/InputDevice", "getDeviceIds", "()[I", ID_getDeviceIds), K("android/view/InputDevice", "getDevice", "(I)Landroid/view/InputDevice;", ID_getDevice),
    K("android/hardware/input/InputManager", "getInputDeviceIds", "()[I", ID_getDeviceIds),
    K("android/hardware/input/InputManager", "getInputDevice", "(I)Landroid/view/InputDevice;", ID_getDevice),
    K("android/hardware/input/InputManager", "registerInputDeviceListener", "(Landroid/hardware/input/InputManager$InputDeviceListener;Landroid/os/Handler;)V", IM_noop),
    K("android/hardware/input/InputManager", "unregisterInputDeviceListener", "(Landroid/hardware/input/InputManager$InputDeviceListener;)V", IM_noop),
    K("android/view/InputDevice", "getId", "()I", ID_getId), K("android/view/InputDevice", "getName", "()Ljava/lang/String;", ID_getName),
    K("android/view/InputDevice", "getDescriptor", "()Ljava/lang/String;", ID_getDescriptor), K("android/view/InputDevice", "getSources", "()I", ID_getSources),
    K("android/view/InputDevice", "getVendorId", "()I", ID_vendor), K("android/view/InputDevice", "getProductId", "()I", ID_product),
    K("android/view/InputDevice", "getControllerNumber", "()I", ID_one), K("android/view/InputDevice", "getKeyboardType", "()I", ID_zero),
    K("android/view/InputDevice", "isVirtual", "()Z", ID_false), K("android/view/InputDevice", "isExternal", "()Z", ID_true),
    K("android/view/InputDevice", "supportsSource", "(I)Z", ID_supportsSource), K("android/view/InputDevice", "hasKeys", "([I)[Z", ID_hasKeys),
    K("android/view/InputDevice", "getMotionRange", "(I)Landroid/view/InputDevice$MotionRange;", ID_getMotionRange),
    K("android/view/InputDevice", "getMotionRange", "(II)Landroid/view/InputDevice$MotionRange;", ID_getMotionRange),
    K("android/view/InputDevice", "getMotionRanges", "()Ljava/util/List;", ID_getMotionRanges), K("android/view/InputDevice", "getVibrator", "()Landroid/os/Vibrator;", ID_null),
    K("android/view/InputDevice", "getKeyCharacterMap", "()Landroid/view/KeyCharacterMap;", ID_null),
    K("java/util/List", "size", "()I", AL_size), K("java/util/List", "get", "(I)Ljava/lang/Object;", AL_get),
    K("java/util/List", "isEmpty", "()Z", AL_isEmpty), K("java/util/List", "iterator", "()Ljava/util/Iterator;", AL_iterator),
    K("java/util/ArrayList", "size", "()I", AL_size), K("java/util/ArrayList", "get", "(I)Ljava/lang/Object;", AL_get),
    K("java/util/ArrayList", "isEmpty", "()Z", AL_isEmpty), K("java/util/ArrayList", "iterator", "()Ljava/util/Iterator;", AL_iterator),
    K("android/view/InputDevice$MotionRange", "getAxis", "()I", MR_axis), K("android/view/InputDevice$MotionRange", "getSource", "()I", MR_source),
    K("android/view/InputDevice$MotionRange", "getMin", "()F", MR_min), K("android/view/InputDevice$MotionRange", "getMax", "()F", MR_max),
    K("android/view/InputDevice$MotionRange", "getRange", "()F", MR_range), K("android/view/InputDevice$MotionRange", "getFlat", "()F", MR_flat),
    K("android/view/InputDevice$MotionRange", "getFuzz", "()F", MR_zerof), K("android/view/InputDevice$MotionRange", "getResolution", "()F", MR_zerof),
    { NULL, NULL, NULL, NULL }
};

void tl_pad_install(void)
{
    tl_jni_declare("android/view/InputDevice", "java/lang/Object");
    tl_jni_declare("android/view/InputDevice$MotionRange", "java/lang/Object");
    tl_jni_declare("android/hardware/input/InputManager", "java/lang/Object");
    tl_jni_declare("java/util/ArrayList", "java/lang/Object");
    static const struct { const char *n; int v; } src[] = {
        { "SOURCE_KEYBOARD", 0x101 }, { "SOURCE_DPAD", 0x201 }, { "SOURCE_GAMEPAD", 0x401 }, { "SOURCE_TOUCHSCREEN", 0x1002 }, { "SOURCE_MOUSE", 0x2002 },
        { "SOURCE_JOYSTICK", 0x01000010 }, { "SOURCE_CLASS_BUTTON", 1 }, { "SOURCE_CLASS_POINTER", 2 }, { "SOURCE_CLASS_TRACKBALL", 4 },
        { "SOURCE_CLASS_POSITION", 8 }, { "SOURCE_CLASS_JOYSTICK", 0x10 }, { "SOURCE_ANY", (int)0xffffff00 }, { "SOURCE_UNKNOWN", 0 },
    };
    for (size_t i = 0; i < sizeof(src) / sizeof(src[0]); i++) { jvalue v; v.j = 0; v.i = src[i].v; tl_jni_set_static("android/view/InputDevice", src[i].n, "I", v); }
    tl_jni_register_hle(k_pad_hle);
}
