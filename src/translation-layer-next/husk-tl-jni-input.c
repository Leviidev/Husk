/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Input, as the engines expect to receive it: a Java android.view.MotionEvent (touches, and a controller's
 * sticks and triggers) or android.view.KeyEvent (a controller's buttons) handed to the engine, which reads it
 * back through JNI one getter at a time.
 *
 * An event here is a small C record behind a MotionEvent or KeyEvent object; the getters read the
 * record. The caller describes touches in screen pixels with y down, which is what
 * Android reports, so there is nothing to convert. Controller events come from husk-tl-gamepad.c.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-jni.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_POINTERS 10
#define MAX_AXES 48

enum { ACTION_DOWN = 0, ACTION_UP = 1, ACTION_MOVE = 2, ACTION_CANCEL = 3, ACTION_POINTER_DOWN = 5, ACTION_POINTER_UP = 6, SOURCE_TOUCHSCREEN = 0x1002 };

/* One record serves both kinds of event: a key has a key code (and no pointers), a controller's motion has axes. */
typedef struct motion {
    int action, count;
    int ids[MAX_POINTERS];
    float x[MAX_POINTERS], y[MAX_POINTERS];
    int64_t down_ms, event_ms;
    int source, device;                     /* 0: a touch screen, device 0 */
    bool joystick;                          /* the values are in axes[], by Android axis number */
    float axes[MAX_AXES];
    int keycode, repeat, meta, scan;
} motion;

static jvalue vi(int i) { jvalue v; v.j = 0; v.i = i; return v; }
static jvalue vj(int64_t j) { jvalue v; v.j = j; return v; }
static jvalue vf(float f) { jvalue v; v.j = 0; v.f = f; return v; }
static jvalue vz(int z) { jvalue v; v.j = 0; v.z = z != 0; return v; }
static const motion *M_(const tl_jcall *c) { return c->self ? c->self->native : NULL; }

static int pidx(const motion *m, int i) { return i >= 0 && i < m->count ? i : 0; }

static void ME_getAction(tl_jcall *c)       { const motion *m = M_(c); c->ret = vi(m ? m->action : 0); }
static void ME_getActionMasked(tl_jcall *c) { const motion *m = M_(c); c->ret = vi(m ? m->action & 0xFF : 0); }
static void ME_getActionIndex(tl_jcall *c)  { const motion *m = M_(c); c->ret = vi(m ? (m->action >> 8) & 0xFF : 0); }
static void ME_getPointerCount(tl_jcall *c) { const motion *m = M_(c); c->ret = vi(m ? m->count : 0); }
static void ME_getPointerId(tl_jcall *c)    { const motion *m = M_(c); c->ret = vi(m ? m->ids[pidx(m, c->args[0].i)] : 0); }
static void ME_findPointerIndex(tl_jcall *c)
{
    const motion *m = M_(c);
    int r = -1;
    for (int i = 0; m && i < m->count; i++) if (m->ids[i] == c->args[0].i) r = i;
    c->ret = vi(r);
}
static void ME_getXi(tl_jcall *c)  { const motion *m = M_(c); c->ret = vf(m ? m->x[pidx(m, c->args[0].i)] : 0); }
static void ME_getYi(tl_jcall *c)  { const motion *m = M_(c); c->ret = vf(m ? m->y[pidx(m, c->args[0].i)] : 0); }
static void ME_getX(tl_jcall *c)   { const motion *m = M_(c); c->ret = vf(m ? m->x[0] : 0); }
static void ME_getY(tl_jcall *c)   { const motion *m = M_(c); c->ret = vf(m ? m->y[0] : 0); }
static void ME_getPressure(tl_jcall *c) { const motion *m = M_(c); c->ret = vf(m && m->joystick ? 0.0f : 1.0f); }
static void ME_getSize(tl_jcall *c)     { c->ret = vf(0.1f); }
static void ME_zeroF(tl_jcall *c)       { c->ret = vf(0.0f); }
static void ME_getToolType(tl_jcall *c) { const motion *m = M_(c); c->ret = vi(m && m->joystick ? 0 : 1); }   /* TOOL_TYPE_UNKNOWN for a stick, else FINGER */
static void ME_getEventTime(tl_jcall *c){ const motion *m = M_(c); c->ret = vj(m ? m->event_ms : 0); }
static void ME_getDownTime(tl_jcall *c) { const motion *m = M_(c); c->ret = vj(m ? m->down_ms : 0); }
static void ME_getSource(tl_jcall *c)   { const motion *m = M_(c); c->ret = vi(m && m->source ? m->source : SOURCE_TOUCHSCREEN); }
static void ME_zeroI(tl_jcall *c)       { c->ret = vi(0); }
static void ME_getDeviceId(tl_jcall *c) { const motion *m = M_(c); c->ret = vi(m ? m->device : 0); }
static void ME_getKeyCode(tl_jcall *c)  { const motion *m = M_(c); c->ret = vi(m ? m->keycode : 0); }
static void ME_getRepeat(tl_jcall *c)   { const motion *m = M_(c); c->ret = vi(m ? m->repeat : 0); }
static void ME_getMeta(tl_jcall *c)     { const motion *m = M_(c); c->ret = vi(m ? m->meta : 0); }
static void ME_getScan(tl_jcall *c)     { const motion *m = M_(c); c->ret = vi(m ? m->scan : 0); }
static void ME_isFalse(tl_jcall *c)     { c->ret = vz(0); }
static void ME_nullObj(tl_jcall *c)     { c->ret.l = NULL; }
/* KEYCODE_BUTTON_A..BUTTON_MODE, and the D-pad keys */
static void KE_isGamepadButton(tl_jcall *c) { int k = c->args[0].i; c->ret = vz((k >= 96 && k <= 110) || (k >= 188 && k <= 223)); }
static void ME_getAxisValue(tl_jcall *c)
{
    const motion *m = M_(c);
    int axis = c->args[0].i, i = c->args[1].i;
    float v = 0;
    if (m && m->joystick) v = axis >= 0 && axis < MAX_AXES ? m->axes[axis] : 0;
    else if (m && axis == 0) v = m->x[pidx(m, i)];
    else if (m && axis == 1) v = m->y[pidx(m, i)];
    else if (axis == 2) v = 1.0f;
    else if (axis == 3) v = 0.1f;
    c->ret = vf(v);
}
static void ME_getAxisValue1(tl_jcall *c)
{
    const motion *m = M_(c);
    int axis = c->args[0].i;
    if (m && m->joystick) { c->ret = vf(axis >= 0 && axis < MAX_AXES ? m->axes[axis] : 0); return; }
    c->ret = vf(m && axis == 0 ? m->x[0] : m && axis == 1 ? m->y[0] : 0);
}
static void ME_obtainCopy(tl_jcall *c)
{
    const motion *m = c->args[0].l ? ((const jobj *)c->args[0].l)->native : NULL;
    if (!m) return;
    jobj *o = tl_jni_new_object(tl_jni_class("android/view/MotionEvent"));
    motion *copy = malloc(sizeof(*copy));
    memcpy(copy, m, sizeof(*copy));
    o->native = copy;
    c->ret.l = o;
}
static void ME_recycle(tl_jcall *c) { (void)c; }
static void ME_isTrue(tl_jcall *c)      { c->ret = vz(1); }

/* The event for the pointers currently down; `action` already carries any pointer index. */
jobj *tl_input_motion_event(int action, int count, const int *ids, const float *xs, const float *ys, int64_t down_ms, int64_t event_ms)
{
    motion *m = calloc(1, sizeof(*m));
    m->action = action;
    m->count = count > MAX_POINTERS ? MAX_POINTERS : count;
    for (int i = 0; i < m->count; i++) { m->ids[i] = ids[i]; m->x[i] = xs[i]; m->y[i] = ys[i]; }
    m->down_ms = down_ms;
    m->event_ms = event_ms;
    jobj *o = tl_jni_new_object(tl_jni_class("android/view/MotionEvent"));
    o->native = m;
    return o;
}

/*
 * Controller events come many times a second, so they are not allocated each time: a ring of objects is reused, each
 * rewritten for the next event. The ring is deep enough that an event is long delivered before its object comes round
 * again. The caller gets its own reference, as it would for a new object, and releases it the same way.
 */
#define POOL 256
static pthread_mutex_t g_pool_lock = PTHREAD_MUTEX_INITIALIZER;
static jobj *g_key_ring[POOL], *g_joy_ring[POOL];
static unsigned g_key_next, g_joy_next;

static jobj *pooled(const char *cls, jobj **ring, unsigned *next, motion **rec)
{
    pthread_mutex_lock(&g_pool_lock);
    unsigned i = (*next)++ % POOL;
    if (!ring[i]) {
        ring[i] = tl_jni_new_object(tl_jni_class(cls));
        ring[i]->native = calloc(1, sizeof(motion));
    }
    *rec = ring[i]->native;
    memset(*rec, 0, sizeof(motion));
    jobj *o = tl_jni_ref(ring[i]);
    pthread_mutex_unlock(&g_pool_lock);
    return o;
}

jobj *tl_input_key_event(int action, int keycode, int device, int source, int repeat, int64_t down_ms, int64_t event_ms)
{
    motion *m;
    jobj *o = pooled("android/view/KeyEvent", g_key_ring, &g_key_next, &m);
    m->action = action; m->keycode = keycode; m->device = device; m->source = source; m->repeat = repeat;
    m->down_ms = down_ms; m->event_ms = event_ms; m->scan = 0;
    return o;
}

/* A controller's sticks, triggers and hat, as one MOVE event of a joystick source; axes is by Android axis number (MAX_AXES of them). */
jobj *tl_input_joystick_event(int device, int source, int64_t down_ms, int64_t event_ms, const float *axes)
{
    motion *m;
    jobj *o = pooled("android/view/MotionEvent", g_joy_ring, &g_joy_next, &m);
    m->action = ACTION_MOVE; m->count = 1; m->ids[0] = 0; m->device = device; m->source = source; m->joystick = true;
    memcpy(m->axes, axes, sizeof(m->axes));
    m->x[0] = axes[0]; m->y[0] = axes[1];
    m->down_ms = down_ms; m->event_ms = event_ms;
    return o;
}

/* The axes of a joystick event (by Android axis number), for an engine that wants values rather than an event object. False if it is not one. */
bool tl_input_event_axes(const jobj *ev, float *out)
{
    const motion *m = ev ? (const motion *)ev->native : NULL;
    if (!m || !m->joystick) return false;
    memcpy(out, m->axes, sizeof(m->axes));
    return true;
}

#define K(c, n, s, f) { c, n, s, f }
static const tl_jhle k_input_hle[] = {
    K("android/view/MotionEvent", "getAction", "()I", ME_getAction),
    K("android/view/MotionEvent", "getActionMasked", "()I", ME_getActionMasked),
    K("android/view/MotionEvent", "getActionIndex", "()I", ME_getActionIndex),
    K("android/view/MotionEvent", "getPointerCount", "()I", ME_getPointerCount),
    K("android/view/MotionEvent", "getPointerId", "(I)I", ME_getPointerId),
    K("android/view/MotionEvent", "findPointerIndex", "(I)I", ME_findPointerIndex),
    K("android/view/MotionEvent", "getX", "(I)F", ME_getXi), K("android/view/MotionEvent", "getY", "(I)F", ME_getYi),
    K("android/view/MotionEvent", "getX", "()F", ME_getX), K("android/view/MotionEvent", "getY", "()F", ME_getY),
    K("android/view/MotionEvent", "getRawX", "()F", ME_getX), K("android/view/MotionEvent", "getRawY", "()F", ME_getY),
    K("android/view/MotionEvent", "getRawX", "(I)F", ME_getXi), K("android/view/MotionEvent", "getRawY", "(I)F", ME_getYi),
    K("android/view/MotionEvent", "getPressure", "(I)F", ME_getPressure), K("android/view/MotionEvent", "getPressure", "()F", ME_getPressure),
    K("android/view/MotionEvent", "getSize", "(I)F", ME_getSize), K("android/view/MotionEvent", "getSize", "()F", ME_getSize),
    K("android/view/MotionEvent", "getTouchMajor", "(I)F", ME_zeroF), K("android/view/MotionEvent", "getTouchMinor", "(I)F", ME_zeroF),
    K("android/view/MotionEvent", "getOrientation", "(I)F", ME_zeroF),
    K("android/view/MotionEvent", "getToolType", "(I)I", ME_getToolType),
    K("android/view/MotionEvent", "getEventTime", "()J", ME_getEventTime),
    K("android/view/MotionEvent", "getDownTime", "()J", ME_getDownTime),
    K("android/view/MotionEvent", "getSource", "()I", ME_getSource),
    K("android/view/InputEvent", "getSource", "()I", ME_getSource),
    K("android/view/MotionEvent", "getDeviceId", "()I", ME_getDeviceId), K("android/view/InputEvent", "getDeviceId", "()I", ME_getDeviceId),
    /* a controller's buttons */
    K("android/view/KeyEvent", "getAction", "()I", ME_getAction), K("android/view/KeyEvent", "getKeyCode", "()I", ME_getKeyCode),
    K("android/view/KeyEvent", "getRepeatCount", "()I", ME_getRepeat), K("android/view/KeyEvent", "getMetaState", "()I", ME_getMeta),
    K("android/view/KeyEvent", "getScanCode", "()I", ME_getScan), K("android/view/KeyEvent", "getDeviceId", "()I", ME_getDeviceId),
    K("android/view/KeyEvent", "getSource", "()I", ME_getSource), K("android/view/KeyEvent", "getEventTime", "()J", ME_getEventTime),
    K("android/view/KeyEvent", "getDownTime", "()J", ME_getDownTime), K("android/view/KeyEvent", "getFlags", "()I", ME_zeroI),
    K("android/view/KeyEvent", "getModifiers", "()I", ME_zeroI), K("android/view/KeyEvent", "getUnicodeChar", "()I", ME_zeroI),
    K("android/view/KeyEvent", "getUnicodeChar", "(I)I", ME_zeroI), K("android/view/KeyEvent", "isCanceled", "()Z", ME_isFalse),
    K("android/view/KeyEvent", "isLongPress", "()Z", ME_isFalse), K("android/view/KeyEvent", "isSystem", "()Z", ME_isFalse),
    K("android/view/KeyEvent", "getCharacters", "()Ljava/lang/String;", ME_nullObj), K("android/view/KeyEvent", "getKeyCharacterMap", "()Landroid/view/KeyCharacterMap;", ME_nullObj),
    K("android/view/KeyEvent", "isGamepadButton", "(I)Z", KE_isGamepadButton),
    K("android/view/KeyEvent", "recycle", "()V", ME_recycle),
    K("android/view/MotionEvent", "getFlags", "()I", ME_zeroI), K("android/view/MotionEvent", "getMetaState", "()I", ME_zeroI),
    K("android/view/MotionEvent", "getButtonState", "()I", ME_zeroI), K("android/view/MotionEvent", "getEdgeFlags", "()I", ME_zeroI),
    K("android/view/MotionEvent", "getHistorySize", "()I", ME_zeroI),
    K("android/view/MotionEvent", "getAxisValue", "(II)F", ME_getAxisValue), K("android/view/MotionEvent", "getAxisValue", "(I)F", ME_getAxisValue1),
    K("android/view/MotionEvent", "isFromSource", "(I)Z", ME_isTrue),
    K("android/view/MotionEvent", "obtain", "(Landroid/view/MotionEvent;)Landroid/view/MotionEvent;", ME_obtainCopy),
    K("android/view/MotionEvent", "recycle", "()V", ME_recycle),
    { NULL, NULL, NULL, NULL }
};

extern void tl_pad_install(void);

static void set_int_const(const char *cls, const char *name, int value)
{
    jvalue v; v.j = 0; v.i = value;
    tl_jni_set_static(cls, name, "I", v);
}

void tl_input_install(void)
{
    tl_jni_declare("android/view/InputEvent", "java/lang/Object");
    tl_jni_declare("android/view/MotionEvent", "android/view/InputEvent");
    tl_jni_declare("android/view/KeyEvent", "android/view/InputEvent");
    tl_jni_register_hle(k_input_hle);
    static const struct { const char *n; int v; } key_consts[] = {
        { "ACTION_DOWN", 0 }, { "ACTION_UP", 1 }, { "KEYCODE_BACK", 4 }, { "KEYCODE_DPAD_UP", 19 }, { "KEYCODE_DPAD_DOWN", 20 }, { "KEYCODE_DPAD_LEFT", 21 },
        { "KEYCODE_DPAD_RIGHT", 22 }, { "KEYCODE_DPAD_CENTER", 23 }, { "KEYCODE_BUTTON_A", 96 }, { "KEYCODE_BUTTON_B", 97 }, { "KEYCODE_BUTTON_C", 98 },
        { "KEYCODE_BUTTON_X", 99 }, { "KEYCODE_BUTTON_Y", 100 }, { "KEYCODE_BUTTON_Z", 101 }, { "KEYCODE_BUTTON_L1", 102 }, { "KEYCODE_BUTTON_R1", 103 },
        { "KEYCODE_BUTTON_L2", 104 }, { "KEYCODE_BUTTON_R2", 105 }, { "KEYCODE_BUTTON_THUMBL", 106 }, { "KEYCODE_BUTTON_THUMBR", 107 },
        { "KEYCODE_BUTTON_START", 108 }, { "KEYCODE_BUTTON_SELECT", 109 }, { "KEYCODE_BUTTON_MODE", 110 },
    };
    for (size_t i = 0; i < sizeof(key_consts) / sizeof(key_consts[0]); i++) set_int_const("android/view/KeyEvent", key_consts[i].n, key_consts[i].v);
    static const struct { const char *n; int v; } motion_consts[] = {
        { "ACTION_DOWN", 0 }, { "ACTION_UP", 1 }, { "ACTION_MOVE", 2 }, { "ACTION_CANCEL", 3 }, { "ACTION_POINTER_DOWN", 5 }, { "ACTION_POINTER_UP", 6 },
        { "AXIS_X", 0 }, { "AXIS_Y", 1 }, { "AXIS_PRESSURE", 2 }, { "AXIS_SIZE", 3 }, { "AXIS_Z", 11 }, { "AXIS_RX", 12 }, { "AXIS_RY", 13 }, { "AXIS_RZ", 14 },
        { "AXIS_HAT_X", 15 }, { "AXIS_HAT_Y", 16 }, { "AXIS_LTRIGGER", 17 }, { "AXIS_RTRIGGER", 18 }, { "AXIS_THROTTLE", 19 }, { "AXIS_RUDDER", 20 },
        { "AXIS_WHEEL", 21 }, { "AXIS_GAS", 22 }, { "AXIS_BRAKE", 23 },
    };
    for (size_t i = 0; i < sizeof(motion_consts) / sizeof(motion_consts[0]); i++) set_int_const("android/view/MotionEvent", motion_consts[i].n, motion_consts[i].v);
    tl_pad_install();
}
void tl_jni_input_reset_session(void)
{
    pthread_mutex_lock(&g_pool_lock);
    for (int i = 0; i < POOL; i++) {
        if (g_key_ring[i]) { free(g_key_ring[i]->native); g_key_ring[i]->native = NULL; g_key_ring[i] = NULL; }
        if (g_joy_ring[i]) { free(g_joy_ring[i]->native); g_joy_ring[i]->native = NULL; g_joy_ring[i] = NULL; }
    }
    g_key_next = g_joy_next = 0;
    pthread_mutex_unlock(&g_pool_lock);
}
