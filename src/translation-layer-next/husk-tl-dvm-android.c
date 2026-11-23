/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Java apps on Husk's Dalvik runtime: the natives of Husk's Java framework (husk.Native, android.opengl.GLES20), the GL thread a
 * GLSurfaceView's renderer runs on, the touch queue, and the driver that starts an app (husk.AppRunner).
 *
 * The framework itself is Java (src/java-framework): Activity, View, Handler and the rest are classes run by the interpreter. What
 * they cannot do in Java is here.
 */
#define _DARWIN_C_SOURCE
#include "husk-tl-dvm-javaapp.h"
#include "husk-tl-dvm-gl.h"
#include "husk-tl-frame-buffer.h"
#include "husk-tl-autorelease.h"
#include "husk-tl-aosp.h"
#include "husk-tl-dvm.h"
#include "husk-tl-session.h"

#include <CoreFoundation/CoreFoundation.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "husk-tl-bionic.h"
#include "husk-tl-audio.h"
#include "husk-tl-memory.h"
#include "husk-tl-host-device.h"
#include "husk-tl-visual.h"
#include "husk-tl-display-timing.h"
#include "husk-tl-frame-perf.h"
#include "husk-tl-egl.h"
#include "husk-tl-vulkan.h"
#include "husk-tl-vhtg-quest.h"
#include "husk-tl-trutien-text.h"
#include "husk-tl-ld.h"
#include "husk-tl-guest-thread.h"
#include "husk-tl-guest-pool.h"
#include "husk-tl-egl.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-vhtg-quest.h"
#include "husk-tl-qos.h"

void tl_nwindow_configure(int w, int h, void *layer);
void *tl_nwindow_get(void);

static struct {
    tl_javaapp_config cfg;
    char apk[1024], data[512], ext[512], pkg[200], activity[260], application[260], frame_dir[512], angle_egl[600], angle_gles[600];
    pthread_mutex_t q_lock;
    pthread_cond_t q_cv;
    bool q_woken;
    int32_t q[4096]; int qn;                    /* packed input events: phase, id, x bits, y bits */
    int64_t down_ns[16];
    uint64_t down_frame[16];
    atomic_llong defer_until_ns;
    atomic_ulong frames;
    atomic_ullong perf_last_ns, perf_since_ns, perf_total_ns, perf_max_ns;
    atomic_ulong perf_frames, perf_over50, perf_over100;
    atomic_bool ended, paused, render_requested;
    atomic_bool app_done, gl_done;
    jobj *gl_view, *gl_renderer;
    int gl_version;
    pthread_t gl_thread, app_thread;
    bool gl_started, app_started, gl_thread_started;   /* gl_started: the app's GLSurfaceView renderer was handed over (N_startGL) */
    jobj *content;
    char fwres[1024];
    tl_zip fwres_zip; bool fwres_tried, fwres_ok;
    pthread_mutex_t t_lock;
    char **text; int ntext, ctext;              /* typed text waiting for Native.pollText: "t<text>", "d", "e", "h" */
    int insets[5];                              /* left, top, right, bottom, keyboard */
} A = { .q_lock = PTHREAD_MUTEX_INITIALIZER, .q_cv = PTHREAD_COND_INITIALIZER, .t_lock = PTHREAD_MUTEX_INITIALIZER };

void tl_javaapp_wake_gl(void);
static int64_t now_ns(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (int64_t)ts.tv_sec * 1000000000ll + ts.tv_nsec; }
static uint64_t rendered_frame_count(void) { return (uint64_t)atomic_load(&A.frames) + (uint64_t)tl_egl_frames_presented(); }

static jvalue L(void *p) { jvalue v; v.j = 0; v.l = p; return v; }
static jvalue I(int32_t i) { jvalue v; v.j = (uint32_t)i; return v; }
static jvalue J(int64_t j) { jvalue v; v.j = j; return v; }
static jvalue F(float f) { jvalue v; v.j = 0; v.f = f; return v; }

/* ================================================================== GL helpers for the generated natives */

void *gl_fn(const char *name) { return tl_egl_resolve(name); }

const char *gl_str(jobj *s) { return s ? tl_jni_string(s) : NULL; }

void *gl_array(jobj *arr, int32_t offset)
{
    if (!arr) return NULL;
    if (arr->kind == TL_K_PRIM_ARRAY) return (uint8_t *)arr->arr.data + (size_t)offset * arr->arr.esz;
    return NULL;
}

/* A java.nio buffer's address: a direct one's memory, or a heap one's backing array, from its position. */
void *gl_buffer(jobj *b)
{
    if (!b) return NULL;
    static dvm_field *f_addr, *f_pos, *f_shift;
    dvm_class *bc = dvm_class_of(dvm_class_named("java/nio/Buffer"));
    if (!f_addr && bc) { f_addr = dvm_find_field(bc, "address", false); f_pos = dvm_find_field(bc, "position", false); f_shift = dvm_find_field(bc, "_elementSizeShift", false); }
    if (!f_addr) return NULL;
    jvalue *s = dvm_slots(b);
    int64_t addr = s[f_addr->slot].j;
    int32_t pos = f_pos ? s[f_pos->slot].i : 0, shift = f_shift ? s[f_shift->slot].i : 0;
    if (addr) return (void *)(uintptr_t)(addr + ((int64_t)pos << shift));
    /* a heap buffer: hb (its array) and offset, on the concrete class */
    dvm_class *c = dvm_class_of(dvm_object_class(b));
    dvm_field *hb = c ? dvm_find_field(c, "hb", false) : NULL, *off = c ? dvm_find_field(c, "offset", false) : NULL;
    if (!hb) return NULL;
    jobj *arr = s[hb->slot].l;
    if (!arr || arr->kind != TL_K_PRIM_ARRAY) return NULL;
    int32_t o = off ? s[off->slot].i : 0;
    return (uint8_t *)arr->arr.data + ((int64_t)(o + pos) << shift);
}

/* ---- the GLES20 calls that hand back strings, written by hand */

static jobj *jstr(const char *s) { return dvm_new_string_utf8(s ? s : ""); }
#define NAT(fn) static bool fn(jobj *self, const jvalue *a, jvalue *ret)

NAT(G_glGetString)
{
    (void)self;
    const unsigned char *(*f)(uint32_t) = gl_fn("glGetString");
    const unsigned char *s = f ? f((uint32_t)a[0].i) : NULL;
    *ret = L(s ? jstr((const char *)s) : NULL);
    return true;
}
static jobj *info_log(const char *getiv, const char *getlog, int32_t obj)
{
    void (*iv)(uint32_t, uint32_t, int32_t *) = gl_fn(getiv);
    void (*lg)(uint32_t, int32_t, int32_t *, char *) = gl_fn(getlog);
    int32_t len = 0;
    if (iv) iv((uint32_t)obj, 0x8B84 /* GL_INFO_LOG_LENGTH */, &len);
    if (len <= 0 || !lg) return jstr("");
    char *buf = calloc(1, (size_t)len + 1);
    lg((uint32_t)obj, len, NULL, buf);
    jobj *r = jstr(buf);
    free(buf);
    return r;
}
NAT(G_glGetProgramInfoLog) { (void)self; *ret = L(info_log("glGetProgramiv", "glGetProgramInfoLog", a[0].i)); return true; }
NAT(G_glGetShaderInfoLog) { (void)self; *ret = L(info_log("glGetShaderiv", "glGetShaderInfoLog", a[0].i)); return true; }
NAT(G_glGetShaderSource)
{
    (void)self;
    void (*f)(uint32_t, int32_t, int32_t *, char *) = gl_fn("glGetShaderSource");
    char buf[65536] = "";
    if (f) f((uint32_t)a[0].i, sizeof(buf), NULL, buf);
    *ret = L(jstr(buf));
    return true;
}
static jobj *active(const char *fn, int32_t program, int32_t index, int32_t *size, int32_t *type)
{
    void (*f)(uint32_t, uint32_t, int32_t, int32_t *, int32_t *, uint32_t *, char *) = gl_fn(fn);
    char name[512] = ""; int32_t len = 0, sz = 0; uint32_t ty = 0;
    if (f) f((uint32_t)program, (uint32_t)index, sizeof(name), &len, &sz, &ty, name);
    if (size) *size = sz;
    if (type) *type = (int32_t)ty;
    return jstr(name);
}
NAT(G_glGetActiveAttrib6)
{
    (void)self;
    *ret = L(active("glGetActiveAttrib", a[0].i, a[1].i, gl_array(a[2].l, a[3].i), gl_array(a[4].l, a[5].i)));
    return true;
}
NAT(G_glGetActiveAttrib4) { (void)self; *ret = L(active("glGetActiveAttrib", a[0].i, a[1].i, gl_buffer(a[2].l), gl_buffer(a[3].l))); return true; }
NAT(G_glGetActiveUniform6)
{
    (void)self;
    *ret = L(active("glGetActiveUniform", a[0].i, a[1].i, gl_array(a[2].l, a[3].i), gl_array(a[4].l, a[5].i)));
    return true;
}
NAT(G_glGetActiveUniform4) { (void)self; *ret = L(active("glGetActiveUniform", a[0].i, a[1].i, gl_buffer(a[2].l), gl_buffer(a[3].l))); return true; }
static void active11(const char *fn, const jvalue *a)
{
    void (*f)(uint32_t, uint32_t, int32_t, int32_t *, int32_t *, uint32_t *, char *) = gl_fn(fn);
    if (f) f((uint32_t)a[0].i, (uint32_t)a[1].i, a[2].i, gl_array(a[3].l, a[4].i), gl_array(a[5].l, a[6].i), gl_array(a[7].l, a[8].i), gl_array(a[9].l, a[10].i));
}
NAT(G_glGetActiveAttrib11) { (void)self; (void)ret; active11("glGetActiveAttrib", a); return true; }
NAT(G_glGetActiveUniform11) { (void)self; (void)ret; active11("glGetActiveUniform", a); return true; }

NAT(G_glShaderSource)
{
    (void)self; (void)ret;
    void (*f)(uint32_t, int32_t, const char *const *, const int32_t *) = gl_fn("glShaderSource");
    const char *src = gl_str(a[1].l);
    if (f && src) f((uint32_t)a[0].i, 1, &src, NULL);
    return true;
}

static const gl_native k_gles_special[] = {
    { "glShaderSource", "(ILjava/lang/String;)V", G_glShaderSource },
    { "glGetString", "(I)Ljava/lang/String;", G_glGetString },
    { "glGetProgramInfoLog", "(I)Ljava/lang/String;", G_glGetProgramInfoLog },
    { "glGetShaderInfoLog", "(I)Ljava/lang/String;", G_glGetShaderInfoLog },
    { "glGetShaderSource", "(I)Ljava/lang/String;", G_glGetShaderSource },
    { "glGetActiveAttrib", "(II[II[II)Ljava/lang/String;", G_glGetActiveAttrib6 },
    { "glGetActiveAttrib", "(IILjava/nio/IntBuffer;Ljava/nio/IntBuffer;)Ljava/lang/String;", G_glGetActiveAttrib4 },
    { "glGetActiveUniform", "(II[II[II)Ljava/lang/String;", G_glGetActiveUniform6 },
    { "glGetActiveUniform", "(IILjava/nio/IntBuffer;Ljava/nio/IntBuffer;)Ljava/lang/String;", G_glGetActiveUniform4 },
    { "glGetActiveAttrib", "(III[II[II[II[BI)V", G_glGetActiveAttrib11 },
    { "glGetActiveUniform", "(III[II[II[II[BI)V", G_glGetActiveUniform11 },
    { NULL, NULL, NULL },
};

/* ================================================================== husk.Native */

NAT(N_log)
{
    (void)self; (void)ret;
    static const char lv[] = "??VDIWEF";
    int p = a[0].i;
    tl_log_line("%c/%s: %s", p >= 0 && p < 8 ? lv[p] : '?', tl_jni_string(a[1].l) ? tl_jni_string(a[1].l) : "", tl_jni_string(a[2].l) ? tl_jni_string(a[2].l) : "");
    return true;
}
/* Assets: the app's APK first, then its splits and asset packs (a Play install keeps a game's data in an install-time asset pack). */
static const tl_zip_entry *asset_find(const char *n, const tl_zip **zout)
{
    /* "/lib/x.so": a file from the APK's root (openNonAssetFd), which asset names never start with */
    char path[1024]; if (n && n[0] == '/') snprintf(path, sizeof(path), "%s", n + 1); else snprintf(path, sizeof(path), "assets/%s", n ? n : "");
    for (int i = 0; i < 16; i++) {
        const tl_zip *z = tl_ld_apk_at(i);
        if (!z) { if (i == 0) continue; break; }
        const tl_zip_entry *e = tl_zip_find(z, path);
        if (e) { if (zout) *zout = z; return e; }
    }
    return NULL;
}
NAT(N_readAsset)
{
    (void)self;
    const tl_zip *z = NULL;
    const tl_zip_entry *e = asset_find(tl_jni_string(a[0].l), &z);
    if (!e) { *ret = L(NULL); return true; }
    const uint8_t *d; size_t len; bool owned; char err[160];
    if (!tl_zip_data(z, e, (size_t)1 << 30, &d, &len, &owned, err, sizeof(err))) { *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_prim_array('B', (uint32_t)len);
    arr->refs = 1u << 30;
    memcpy(arr->arr.data, d, len);
    if (owned) free((void *)d);
    *ret = L(arr);
    return true;
}
NAT(N_listAssets)
{
    (void)self;
    const char *dir = tl_jni_string(a[0].l);
    char prefix[1024]; snprintf(prefix, sizeof(prefix), "assets/%s%s", dir ? dir : "", dir && *dir ? "/" : "");
    size_t pl = strlen(prefix);
    char **names = NULL; int n = 0, cap = 0;
    for (int zi = 0; zi < 16; zi++) {
        const tl_zip *z = tl_ld_apk_at(zi);
        if (!z) { if (zi == 0) continue; break; }
        for (size_t i = 0; i < z->count; i++) {
            const char *en = z->entries[i].name;
            if (strncmp(en, prefix, pl)) continue;
            const char *rest = en + pl;
            if (!*rest) continue;
            size_t l = strcspn(rest, "/");
            bool dup = false;
            for (int k = 0; k < n && !dup; k++) dup = strlen(names[k]) == l && !strncmp(names[k], rest, l);
            if (dup) continue;
            if (n == cap) { cap = cap ? cap * 2 : 32; names = realloc(names, (size_t)cap * sizeof(*names)); }
            names[n++] = strndup(rest, l);
        }
    }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/String;");
    arr->refs = 1u << 30;
    for (int i = 0; i < n; i++) { arr->oarr.v[i] = jstr(names[i]); free(names[i]); }
    free(names);
    *ret = L(arr);
    return true;
}
/* An asset stored (not compressed) can be read straight out of its APK: a file descriptor and offset, as Android gives them. */
NAT(N_assetFd)
{
    (void)self;
    const tl_zip *z = NULL;
    const tl_zip_entry *e = asset_find(tl_jni_string(a[0].l), &z);
    if (!e || e->method != 0) { *ret = J(-1); return true; }
    const uint8_t *lh = z->map + e->local_offset;
    uint64_t data = e->local_offset + 30 + (lh[26] | lh[27] << 8) + (lh[28] | lh[29] << 8);
    int fd = z == tl_ld_apk_at(0) ? open(A.apk, O_RDONLY) : dup(z->fd);
    *ret = J(fd < 0 ? -1 : ((int64_t)fd << 40) | (int64_t)data);
    return true;
}
NAT(N_assetLength) { (void)self; const tl_zip_entry *e = asset_find(tl_jni_string(a[0].l), NULL); *ret = J(e ? (int64_t)e->usize : -1); return true; }
NAT(N_dataDir) { (void)self; (void)a; *ret = L(jstr(A.data)); return true; }
NAT(N_externalDir) { (void)self; (void)a; *ret = L(jstr(A.ext)); return true; }
NAT(N_packageName) { (void)self; (void)a; *ret = L(jstr(A.pkg)); return true; }
NAT(N_screenWidth) { (void)self; (void)a; *ret = I(A.cfg.width); return true; }
NAT(N_screenHeight) { (void)self; (void)a; *ret = I(A.cfg.height); return true; }
NAT(N_density) { (void)self; (void)a; *ret = F(A.cfg.density > 0 ? A.cfg.density : 3.0f); return true; }
NAT(N_refreshRate) { (void)self; (void)a; *ret = F(TL_JAVA_DISPLAY_HZ); return true; }
NAT(N_maxRefreshRate) { (void)self; (void)a; float hz = A.cfg.max_refresh_rate; *ret = F(hz >= 1.0f && hz <= 1000.0f ? hz : TL_JAVA_DISPLAY_HZ); return true; }
NAT(N_wideColorGamut) { (void)self; (void)a; *ret = I(tl_host_display_wide_color() ? 1 : 0); return true; }
NAT(N_deviceProperty)
{
    (void)self;
    const char *value = tl_host_device_property(tl_jni_string(a[0].l));
    *ret = L(value ? jstr(value) : NULL);
    return true;
}
NAT(N_memoryInfo)
{
    (void)self; (void)a;
    tl_memory_info info; tl_host_memory_info(&info);
    jobj *values = tl_jni_new_prim_array('J', 4);
    if (!values) { *ret = L(NULL); return true; }
    int64_t *data = values->arr.data;
    data[0] = (int64_t)info.total; data[1] = (int64_t)info.available;
    data[2] = (int64_t)info.resident; data[3] = (int64_t)info.footprint;
    *ret = L(values);
    return true;
}
NAT(N_requestRender) { (void)self; (void)a; (void)ret; atomic_store(&A.render_requested, true); tl_javaapp_wake_gl(); return true; }
NAT(N_setContentView) { (void)self; (void)ret; A.content = a[0].l; return true; }
NAT(N_vibrate) { (void)self; (void)a; (void)ret; if (A.cfg.vibrate) A.cfg.vibrate((int)a[0].j); return true; }
NAT(N_openUrl) { (void)self; (void)ret; const char *u = tl_jni_string(a[0].l); tl_log_line("javaapp: open %s", u ? u : "?"); if (A.cfg.open_url && u) A.cfg.open_url(u); return true; }
NAT(N_exit)
{
    (void)self; (void)a; (void)ret;
    tl_log_line("javaapp: the app finished");
    atomic_store(&A.ended, true);
    pthread_mutex_lock(&A.q_lock); pthread_cond_broadcast(&A.q_cv); pthread_mutex_unlock(&A.q_lock);
    tl_javaapp_wake_gl();
    return true;
}
NAT(N_uptimeNanos) { (void)self; (void)a; *ret = J(now_ns()); return true; }
static void tl_input_queue_push(int32_t *q, int *qn, int cap_words,
                                int32_t phase, int32_t id, int32_t xbits, int32_t ybits)
{
    /* Coalesce contiguous trailing MOVE samples for the same pointer so high-rate
     * touch drags never queue stale coordinates or dt=0 bursts behind a slow frame. */
    if (phase == 1) {
        for (int i = *qn - 4; i >= 0 && q[i] == 1; i -= 4) {
            if (q[i + 1] == id) {
                q[i + 2] = xbits;
                q[i + 3] = ybits;
                return;
            }
        }
    }
    if (*qn + 4 <= cap_words) {
        q[(*qn)++] = phase;
        q[(*qn)++] = id;
        q[(*qn)++] = xbits;
        q[(*qn)++] = ybits;
    }
}

static bool tl_input_up_deferred(int32_t id, int64_t t_now, uint64_t f_now,
                                 uint32_t batch_down, const int64_t *down_ns,
                                 const uint64_t *down_frame, int64_t *ev_defer_out)
{
    int64_t hold_until = down_ns[id] + 16000000ll;
    bool same_batch = (batch_down & (1u << id)) != 0;
    bool no_frame_yet = (f_now <= down_frame[id]) && (t_now < hold_until);
    if (!same_batch && !no_frame_yet) return false;
    *ev_defer_out = t_now < hold_until ? hold_until : (t_now + 8000000ll);
    return true;
}

static int tl_input_queue_plan(const int32_t *q, int total, int64_t t_now, uint64_t f_now,
                               int64_t *down_ns, uint64_t *down_frame,
                               uint8_t *keep_mask, int64_t *defer_until_out)
{
    uint32_t batch_down = 0, blocked = 0;
    int take = 0;
    int64_t defer_until = 0;
    for (int k = 0; k < total; k++) {
        int32_t phase = q[4 * k], id = q[4 * k + 1];
        bool touch_id = (phase >= 0 && phase <= 3 && id >= 0 && id < 16);
        if (touch_id && (blocked & (1u << id))) {
            keep_mask[k] = 1;
            continue;
        }
        if (phase == 0 && touch_id) {
            batch_down |= (1u << id);
            down_ns[id] = t_now;
            down_frame[id] = f_now;
        } else if ((phase == 2 || phase == 3) && touch_id) {
            int64_t ev_defer = 0;
            if (tl_input_up_deferred(id, t_now, f_now, batch_down, down_ns, down_frame, &ev_defer)) {
                if (!defer_until || ev_defer < defer_until) defer_until = ev_defer;
                /* Block only this pointer's subsequent stream; other fingers' camera drags continue uninterrupted. */
                blocked |= (1u << id);
                keep_mask[k] = 1;
                continue;
            }
        }
        keep_mask[k] = 0;
        take++;
    }
    *defer_until_out = defer_until;
    return take;
}

static void tl_input_queue_commit(int32_t *q, int *qn, const uint8_t *keep_mask,
                                  int total, int32_t *taken_out)
{
    int out_w = 0, keep_w = 0;
    for (int k = 0; k < total; k++) {
        const int32_t *src = q + 4 * k;
        if (keep_mask[k]) {
            int32_t *dst = q + 4 * keep_w;
            if (dst != src) { dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = src[3]; }
            keep_w++;
        } else {
            taken_out[4 * out_w + 0] = src[0];
            taken_out[4 * out_w + 1] = src[1];
            taken_out[4 * out_w + 2] = src[2];
            taken_out[4 * out_w + 3] = src[3];
            out_w++;
        }
    }
    *qn = keep_w * 4;
}

static bool tl_input_queue_has_ready(const int32_t *q, int total, int64_t t_now, uint64_t f_now,
                                     const int64_t *down_ns, const uint64_t *down_frame,
                                     int64_t *min_rem_out)
{
    uint32_t blocked = 0;
    int64_t min_rem = 0;
    for (int k = 0; k < total; k++) {
        int32_t phase = q[4 * k], id = q[4 * k + 1];
        bool touch_id = (phase >= 0 && phase <= 3 && id >= 0 && id < 16);
        if (touch_id && (blocked & (1u << id))) continue;
        if ((phase == 2 || phase == 3) && touch_id) {
            int64_t ev_defer = 0;
            if (tl_input_up_deferred(id, t_now, f_now, 0, down_ns, down_frame, &ev_defer)) {
                int64_t rem = ev_defer - t_now;
                if (rem > 0 && (!min_rem || rem < min_rem)) min_rem = rem;
                blocked |= (1u << id);
                continue;
            }
        }
        *min_rem_out = 0;
        return true;
    }
    *min_rem_out = min_rem;
    return false;
}

NAT(N_pollInput)
{
    (void)self; (void)a;
    pthread_mutex_lock(&A.q_lock);
    int total = A.qn / 4;
    if (!total) { atomic_store(&A.defer_until_ns, 0); pthread_mutex_unlock(&A.q_lock); *ret = L(NULL); return true; }
    if (total > 1024) total = 1024;
    uint8_t keep_mask[1024];
    int64_t defer_until = 0;
    int take = tl_input_queue_plan(A.q, total, now_ns(), rendered_frame_count(),
                                   A.down_ns, A.down_frame, keep_mask, &defer_until);
    atomic_store(&A.defer_until_ns, defer_until);
    if (!take) { pthread_mutex_unlock(&A.q_lock); *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_prim_array('I', (uint32_t)(1 + take * 4));
    arr->refs = 1u << 30;
    int32_t *v = arr->arr.data;
    v[0] = take;
    tl_input_queue_commit(A.q, &A.qn, keep_mask, total, v + 1);
    pthread_mutex_unlock(&A.q_lock);
    *ret = L(arr);
    return true;
}
NAT(N_prepareMainWait)
{
    (void)self; (void)a; (void)ret;
    pthread_mutex_lock(&A.q_lock);
    A.q_woken = false;
    pthread_mutex_unlock(&A.q_lock);
    return true;
}
NAT(N_wakeMainLooper)
{
    (void)self; (void)a; (void)ret;
    pthread_mutex_lock(&A.q_lock);
    A.q_woken = true;
    pthread_cond_broadcast(&A.q_cv);
    pthread_mutex_unlock(&A.q_lock);
    return true;
}
NAT(N_waitMainLooper)
{
    (void)self; (void)ret;
    int64_t ms = a[0].j;
    if (ms < 0) ms = 0;
    else if (ms > 100) ms = 100;
    pthread_mutex_lock(&A.q_lock);
    int64_t wait_ns = ms * 1000000ll;
    if (A.qn > 0) {
        int64_t def = atomic_load(&A.defer_until_ns);
        if (def <= 0) { pthread_mutex_unlock(&A.q_lock); return true; }
        int total = A.qn / 4;
        if (total > 1024) total = 1024;
        int64_t rem = 0;
        if (tl_input_queue_has_ready(A.q, total, now_ns(), rendered_frame_count(),
                                     A.down_ns, A.down_frame, &rem)) {
            pthread_mutex_unlock(&A.q_lock);
            return true;
        }
        if (rem <= 0) {
            atomic_store(&A.defer_until_ns, 0);
            pthread_mutex_unlock(&A.q_lock);
            return true;
        }
        if (rem < wait_ns || wait_ns == 0) wait_ns = rem;
    }
    if (!A.q_woken && !atomic_load(&A.ended) && wait_ns > 0) {
        struct timespec rt;
        clock_gettime(CLOCK_REALTIME, &rt);
        int64_t end_rt = (int64_t)rt.tv_sec * 1000000000ll + rt.tv_nsec + wait_ns;
        struct timespec ts = { (time_t)(end_rt / 1000000000ll), (long)(end_rt % 1000000000ll) };
        pthread_cond_timedwait(&A.q_cv, &A.q_lock, &ts);
    }
    pthread_mutex_unlock(&A.q_lock);
    return true;
}

/* ---- files in the APKs: 0 the app's, 1 the platform's resources, 2.. the app's splits */
static const tl_zip *apk_n(int n, const char **path)
{
    if (path) *path = NULL;
    if (n == 0) { if (path) *path = A.apk; return tl_ld_apk_at(0); }
    if (n == 1) {
        if (!A.fwres_tried) {
            A.fwres_tried = true;
            char err[160];
            if (A.fwres[0]) A.fwres_ok = tl_zip_open(&A.fwres_zip, A.fwres, err, sizeof(err));
            if (!A.fwres_ok) tl_log_line("javaapp: no platform resources (%s): %s", A.fwres[0] ? A.fwres : "none given", A.fwres[0] ? err : "-");
        }
        if (path) *path = A.fwres;
        return A.fwres_ok ? &A.fwres_zip : NULL;
    }
    return tl_ld_apk_at(n - 1);
}
const char *tl_ld_queued_split(int i);
void dvm_set_app_loader(jobj *loader);
NAT(N_setAppClassLoader) { (void)self; (void)ret; dvm_set_app_loader(a[0].l); return true; }
/* The NDK's ALooper for a Java Looper's thread: native code that asks for its UI thread's looper (ALooper_forThread) and watches
 * file descriptors on it (Unity 6, GameActivity-style code) has its callbacks run between that thread's messages */
NAT(N_looperPrepare) { (void)self; (void)a; (void)ret; void *(*prep)(int) = tl_bionic_find("ALooper_prepare"); if (prep) prep(0); return true; }
NAT(N_sessionEnded) { (void)self; (void)a; *ret = I(atomic_load(&A.ended) ? 1 : 0); return true; }
NAT(N_looperPoll)
{
    (void)self; (void)a;
    ret->j = 0;
    void *(*cur)(void) = tl_bionic_find("ALooper_forThread");
    int (*poll_all)(int, int *, int *, void **) = tl_bionic_find("ALooper_pollAll");
    if (cur && poll_all && cur()) { poll_all(0, NULL, NULL, NULL); ret->z = true; }
    return true;
}
NAT(N_splitPaths)
{
    (void)self; (void)a;
    int n = 0;
    while (n < 16 && tl_ld_queued_split(n)) n++;
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/String;");
    arr->refs = 1u << 30;
    for (int i = 0; i < n; i++) arr->oarr.v[i] = jstr(tl_ld_queued_split(i));
    *ret = L(arr);
    return true;
}
NAT(N_readApkFile)
{
    (void)self;
    const char *n = tl_jni_string(a[1].l);
    const tl_zip *z = apk_n(a[0].i, NULL);
    const tl_zip_entry *e = z && n ? tl_zip_find(z, n) : NULL;
    if (!e) { *ret = L(NULL); return true; }
    const uint8_t *d; size_t len; bool owned; char err[160];
    if (!tl_zip_data(z, e, (size_t)1 << 30, &d, &len, &owned, err, sizeof(err))) { *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_prim_array('B', (uint32_t)len);
    arr->refs = 1u << 30;
    memcpy(arr->arr.data, d, len);
    if (owned) free((void *)d);
    *ret = L(arr);
    return true;
}
NAT(N_apkFileFd)
{
    (void)self;
    const char *n = tl_jni_string(a[1].l), *path;
    const tl_zip *z = apk_n(a[0].i, &path);
    const tl_zip_entry *e = z && n ? tl_zip_find(z, n) : NULL;
    if (!e || e->method != 0) { *ret = J(-1); return true; }
    const uint8_t *lh = z->map + e->local_offset;
    uint64_t data = e->local_offset + 30 + (lh[26] | lh[27] << 8) + (lh[28] | lh[29] << 8);
    int fd = path && path[0] ? open(path, O_RDONLY) : dup(z->fd);
    *ret = J(fd < 0 ? -1 : ((int64_t)fd << 40) | (int64_t)data);
    return true;
}
NAT(N_apkFileLength)
{
    (void)self;
    const char *n = tl_jni_string(a[1].l);
    const tl_zip *z = apk_n(a[0].i, NULL);
    const tl_zip_entry *e = z && n ? tl_zip_find(z, n) : NULL;
    *ret = J(e ? (int64_t)e->usize : -1);
    return true;
}
NAT(N_apkPath) { (void)self; (void)a; *ret = L(jstr(A.apk)); return true; }

/* ---- the host keyboard, clipboard, share sheet, orientation, insets */
static void text_push(const char *kind, const char *utf8)
{
    pthread_mutex_lock(&A.t_lock);
    if (A.ntext == A.ctext) { A.ctext = A.ctext ? A.ctext * 2 : 16; A.text = realloc(A.text, (size_t)A.ctext * sizeof(*A.text)); }
    size_t l = strlen(kind) + (utf8 ? strlen(utf8) : 0) + 1;
    char *e = malloc(l);
    snprintf(e, l, "%s%s", kind, utf8 ? utf8 : "");
    A.text[A.ntext++] = e;
    pthread_mutex_unlock(&A.t_lock);
    tl_javaapp_touch(7, 0, 0, 0);
}
void tl_javaapp_text(const char *utf8) { if (utf8 && *utf8) text_push("t", utf8); }
void tl_javaapp_text_delete(void) { text_push("d", NULL); }
void tl_javaapp_text_action(void) { text_push("e", NULL); }
void tl_javaapp_keyboard_closed(void) { text_push("h", NULL); }
void tl_javaapp_documents_picked(const char *const *paths, int n)
{
    for (int i = 0; i < n; i++) if (paths[i] && *paths[i]) text_push("f", paths[i]);
    text_push("F", NULL);
}
void tl_javaapp_set_insets(int l, int t, int r, int b, int ime)
{
    tl_vhtg_quest_insets(l,t,r,b);
    if (tl_vhtg_quest_active()) l=t=r=b=0;
    A.insets[0] = l; A.insets[1] = t; A.insets[2] = r; A.insets[3] = b; A.insets[4] = ime;
    tl_javaapp_touch(8, 0, 0, 0);
}
NAT(N_pollText)
{
    (void)self; (void)a;
    pthread_mutex_lock(&A.t_lock);
    int n = A.ntext;
    if (!n) { pthread_mutex_unlock(&A.t_lock); *ret = L(NULL); return true; }
    jobj *arr = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), (uint32_t)n);
    arr->cls = tl_jni_class("[Ljava/lang/String;");
    arr->refs = 1u << 30;
    for (int i = 0; i < n; i++) { arr->oarr.v[i] = jstr(A.text[i]); free(A.text[i]); }
    A.ntext = 0;
    pthread_mutex_unlock(&A.t_lock);
    *ret = L(arr);
    return true;
}
NAT(N_showKeyboard) { (void)self; (void)ret; if (A.cfg.show_keyboard) A.cfg.show_keyboard(a[0].i != 0, a[1].i, a[2].i); else tl_log_line("javaapp: the app asked for the keyboard (%s)", a[0].i ? "show" : "hide"); return true; }
NAT(N_setClipboard) { (void)self; (void)ret; const char *t = tl_jni_string(a[0].l); if (A.cfg.set_clipboard) A.cfg.set_clipboard(t ? t : ""); return true; }
NAT(N_getClipboard)
{
    (void)self; (void)a;
    char *t = A.cfg.get_clipboard ? A.cfg.get_clipboard() : NULL;
    *ret = L(t ? jstr(t) : NULL);
    free(t);
    return true;
}
NAT(N_share) { (void)self; (void)ret; const char *t = tl_jni_string(a[0].l); tl_log_line("javaapp: share %.80s", t ? t : ""); if (A.cfg.share && t) A.cfg.share(t); return true; }
NAT(N_pickDocuments)
{
    (void)self; (void)ret;
    const char *t = tl_jni_string(a[0].l);
    tl_log_line("javaapp: file picker for %s%s", t ? t : "*/*", a[1].i ? " (several)" : "");
    if (A.cfg.pick_documents) A.cfg.pick_documents(t ? t : "*/*", a[1].i != 0);
    else tl_javaapp_documents_picked(NULL, 0);     /* no picker here (host tests): the user backed out */
    return true;
}
NAT(N_setOrientation) { (void)self; (void)ret; if (A.cfg.set_orientation) A.cfg.set_orientation(a[0].i); return true; }
NAT(N_nightMode) { (void)self; (void)a; jvalue v; v.j = A.cfg.night_mode ? 1 : 0; *ret = v; return true; }
/* 24-hour time as the phone's region and Settings > General > Date & Time choose it: the locale's hour pattern ("j") has no am/pm */
NAT(N_is24Hour)
{
    (void)self; (void)a;
    bool h24 = true;
    CFLocaleRef loc = CFLocaleCopyCurrent();
    CFStringRef f = loc ? CFDateFormatterCreateDateFormatFromTemplate(NULL, CFSTR("j"), 0, loc) : NULL;
    if (f) { h24 = CFStringFind(f, CFSTR("a"), 0).location == kCFNotFound; CFRelease(f); }
    if (loc) CFRelease(loc);
    jvalue v; v.j = h24; *ret = v;
    return true;
}
NAT(N_insets)
{
    (void)self; (void)a;
    jobj *arr = tl_jni_new_prim_array('I', 5);
    arr->refs = 1u << 30;
    memcpy(arr->arr.data, A.insets, sizeof(A.insets));
    *ret = L(arr);
    return true;
}

static void *gl_main(void *arg);
NAT(N_startGL)
{
    void tl_thread_altstack(void); tl_thread_altstack();
    (void)self; (void)ret;
    if (A.gl_started) return true;
    A.gl_version = a[2].i; A.gl_view = a[0].l;
    A.gl_started = true;
    /* the render thread picks it up; released after the version and view, so it never sees the renderer without them */
    __atomic_store_n(&A.gl_renderer, a[1].l, __ATOMIC_RELEASE);
    tl_javaapp_wake_gl();
    return true;
}

static const struct { const char *name, *sig; dvm_native_fn fn; } k_native[] = {
    { "log", "(ILjava/lang/String;Ljava/lang/String;)V", N_log },
    { "readAsset", "(Ljava/lang/String;)[B", N_readAsset },
    { "listAssets", "(Ljava/lang/String;)[Ljava/lang/String;", N_listAssets },
    { "assetFd", "(Ljava/lang/String;)J", N_assetFd },
    { "assetLength", "(Ljava/lang/String;)J", N_assetLength },
    { "dataDir", "()Ljava/lang/String;", N_dataDir },
    { "externalDir", "()Ljava/lang/String;", N_externalDir },
    { "packageName", "()Ljava/lang/String;", N_packageName },
    { "screenWidth", "()I", N_screenWidth },
    { "screenHeight", "()I", N_screenHeight },
    { "density", "()F", N_density },
    { "refreshRate", "()F", N_refreshRate },
    { "maxRefreshRate", "()F", N_maxRefreshRate },
    { "wideColorGamut", "()Z", N_wideColorGamut },
    { "deviceProperty", "(Ljava/lang/String;)Ljava/lang/String;", N_deviceProperty },
    { "memoryInfo", "()[J", N_memoryInfo },
    { "startGL", "(Ljava/lang/Object;Ljava/lang/Object;I)V", N_startGL },
    { "requestRender", "()V", N_requestRender },
    { "setContentView", "(Ljava/lang/Object;)V", N_setContentView },
    { "pollInput", "()[I", N_pollInput },
    { "prepareMainWait", "()V", N_prepareMainWait },
    { "waitMainLooper", "(J)V", N_waitMainLooper },
    { "wakeMainLooper", "()V", N_wakeMainLooper },
    { "vibrate", "(J)V", N_vibrate },
    { "openUrl", "(Ljava/lang/String;)V", N_openUrl },
    { "exit", "()V", N_exit },
    { "uptimeNanos", "()J", N_uptimeNanos },
    { "readApkFile", "(ILjava/lang/String;)[B", N_readApkFile },
    { "splitPaths", "()[Ljava/lang/String;", N_splitPaths },
    { "nativeLooperPrepare", "()V", N_looperPrepare },
    { "nativeLooperPoll", "()Z", N_looperPoll },
    { "sessionEnded", "()Z", N_sessionEnded },
    { "setAppClassLoader", "(Ljava/lang/ClassLoader;)V", N_setAppClassLoader },
    { "apkFileFd", "(ILjava/lang/String;)J", N_apkFileFd },
    { "apkFileLength", "(ILjava/lang/String;)J", N_apkFileLength },
    { "apkPath", "()Ljava/lang/String;", N_apkPath },
    { "pollText", "()[Ljava/lang/String;", N_pollText },
    { "showKeyboard", "(ZII)V", N_showKeyboard },
    { "setClipboard", "(Ljava/lang/String;)V", N_setClipboard },
    { "getClipboard", "()Ljava/lang/String;", N_getClipboard },
    { "share", "(Ljava/lang/String;)V", N_share },
    { "pickDocuments", "(Ljava/lang/String;Z)V", N_pickDocuments },
    { "setOrientation", "(I)V", N_setOrientation },
    { "insets", "()[I", N_insets },
    { "nightMode", "()Z", N_nightMode },
    { "is24Hour", "()Z", N_is24Hour },
    { NULL, NULL, NULL },
};

static dvm_native_fn egl_native_lookup(const char *name, const char *sig);
dvm_native_fn dvm_android_native(const char *cls, const char *name, const char *sig);
dvm_native_fn tl_gfx_native(const char *name, const char *sig);
dvm_native_fn tl_audio_native(const char *name, const char *sig);
dvm_native_fn tl_sensors_native(const char *name, const char *sig);
dvm_native_fn tl_sqlite_native(const char *name, const char *sig);
dvm_native_fn tl_media_native(const char *name, const char *sig);
dvm_native_fn tl_web_native(const char *name, const char *sig);
bool tl_web_any_visible(void);
dvm_native_fn dvm_android_native(const char *cls, const char *name, const char *sig)
{
    if (!strcmp(cls, "husk/Sqlite")) return tl_sqlite_native(name, sig);
    if (!strcmp(cls, "husk/MediaCodecNative")) return tl_media_native(name, sig);
    if (!strcmp(cls, "husk/Web")) return tl_web_native(name, sig);
    if (!strcmp(cls, "husk/Sensors")) return tl_sensors_native(name, sig);
    if (!strcmp(cls, "husk/Gfx")) return tl_gfx_native(name, sig);
    if (!strcmp(cls, "husk/Audio")) return tl_audio_native(name, sig);
    if (!strcmp(cls, "husk/EGLNative")) {
        return egl_native_lookup(name, sig);
    }
    if (!strcmp(cls, "husk/Native")) {
        for (int i = 0; k_native[i].name; i++) if (!strcmp(k_native[i].name, name) && !strcmp(k_native[i].sig, sig)) return k_native[i].fn;
    } else if (!strcmp(cls, "android/opengl/GLES20")) {
        for (int i = 0; k_gles_special[i].name; i++) if (!strcmp(k_gles_special[i].name, name) && !strcmp(k_gles_special[i].sig, sig)) return k_gles_special[i].fn;
        for (int i = 0; k_gles20[i].name; i++) if (!strcmp(k_gles20[i].name, name) && !strcmp(k_gles20[i].sig, sig)) return k_gles20[i].fn;
    } else if (!strncmp(cls, "android/opengl/GLES3", 20) || !strncmp(cls, "android/opengl/GLES1", 20)) {
        const char *v = cls + 19;
        const gl_native *t = !strcmp(v, "30") ? k_gles30 : !strcmp(v, "31") ? k_gles31 : !strcmp(v, "32") ? k_gles32 : !strcmp(v, "10") ? k_gles10
                           : !strcmp(v, "10Ext") ? k_gles10ext : !strcmp(v, "11") ? k_gles11 : !strcmp(v, "11Ext") ? k_gles11ext : NULL;
        for (int i = 0; t && t[i].name; i++) if (!strcmp(t[i].name, name) && !strcmp(t[i].sig, sig)) return t[i].fn;
    }
    return NULL;
}

/* ================================================================== the GL thread */

typedef void *EGLDisplay, *EGLSurface, *EGLContext, *EGLConfig;
typedef int32_t EGLint;
#define EGL_NONE 0x3038

static void log_pending(const char *where)
{
    char buf[600];
    if (tl_dvm_describe_pending(buf, sizeof(buf))) tl_log_line("javaapp: %s threw %s", where, buf);
    tl_jni_clear();
}

/* ---- the view system's frame, drawn over whatever the app's GL drew (its own context, so the app's GL state is never touched) */

bool tl_ui_frame_take(const uint8_t **px, int *w, int *h, bool *any);
void tl_ui_frame_done(void);
bool tl_ui_has_frame(void);
bool tl_ui_frame_pending(void);
void tl_ui_frame_reset(void);

typedef struct {
    uint32_t prog, plain_prog, tex, vbo;
    int tw, th, opaque_uniform, plain_opaque_uniform, texel_uniform, enhance_uniform;
} overlay;

static uint32_t ov_shader(uint32_t kind, const char *src)
{
    uint32_t (*create)(uint32_t) = gl_fn("glCreateShader");
    void (*source)(uint32_t, int32_t, const char *const *, const int32_t *) = gl_fn("glShaderSource");
    void (*compile)(uint32_t) = gl_fn("glCompileShader");
    uint32_t sh = create(kind);
    source(sh, 1, &src, NULL);
    compile(sh);
    return sh;
}

static uint32_t ov_build_program(const char *vs, const char *fs)
{
    uint32_t (*createProgram)(void) = gl_fn("glCreateProgram");
    void (*attach)(uint32_t, uint32_t) = gl_fn("glAttachShader");
    void (*bindAttrib)(uint32_t, uint32_t, const char *) = gl_fn("glBindAttribLocation");
    void (*link)(uint32_t) = gl_fn("glLinkProgram");
    void (*getShaderiv)(uint32_t, uint32_t, int32_t *) = gl_fn("glGetShaderiv");
    void (*getProgramiv)(uint32_t, uint32_t, int32_t *) = gl_fn("glGetProgramiv");
    void (*deleteShader)(uint32_t) = gl_fn("glDeleteShader");
    void (*deleteProgram)(uint32_t) = gl_fn("glDeleteProgram");
    uint32_t vsh = ov_shader(0x8B31, vs);
    uint32_t fsh = ov_shader(0x8B30, fs);
    int32_t v_ok = 1, f_ok = 1, l_ok = 1;
    if (getShaderiv) {
        if (vsh) getShaderiv(vsh, 0x8B81 /* GL_COMPILE_STATUS */, &v_ok);
        if (fsh) getShaderiv(fsh, 0x8B81 /* GL_COMPILE_STATUS */, &f_ok);
    }
    uint32_t prog = createProgram();
    attach(prog, vsh);
    attach(prog, fsh);
    bindAttrib(prog, 0, "p");
    link(prog);
    if (getProgramiv && prog) getProgramiv(prog, 0x8B82 /* GL_LINK_STATUS */, &l_ok);
    if (deleteShader) {
        if (vsh) deleteShader(vsh);
        if (fsh) deleteShader(fsh);
    }
    if (!v_ok || !f_ok || !l_ok || !prog) {
        if (deleteProgram && prog) deleteProgram(prog);
        return 0;
    }
    return prog;
}

static void ov_init_flip_mode(overlay *o, bool bottom_up, bool enhance)
{
    static const char *vs_top = "attribute vec2 p; varying vec2 t; void main() { t = vec2((p.x + 1.0) * 0.5, (1.0 - p.y) * 0.5); gl_Position = vec4(p, 0.0, 1.0); }";
    static const char *vs_bottom = "attribute vec2 p; varying vec2 t; void main() { t = vec2((p.x + 1.0) * 0.5, (1.0 + p.y) * 0.5); gl_Position = vec4(p, 0.0, 1.0); }";
    const char *vs = bottom_up ? vs_bottom : vs_top;
    static const char *fs_plain =
        "precision mediump float;"
        "uniform sampler2D s;"
        "uniform bool forceOpaque;"
        "varying vec2 t;"
        "void main() { vec4 c = texture2D(s, t); gl_FragColor = vec4(c.rgb, forceOpaque ? 1.0 : c.a); }";
    static const char *fs_enhance =
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
        "precision highp float;\n"
        "#else\n"
        "precision mediump float;\n"
        "#endif\n"
        "uniform sampler2D s;"
        "uniform bool forceOpaque;"
        "uniform vec2 invTexSize;"
        "uniform vec4 enhanceParams;"
        "varying vec2 t;"
        "void main() {"
        "    const vec3 LUMA = vec3(0.2126, 0.7152, 0.0722);"
        "    vec4 center = texture2D(s, t);"
        "    vec3 e = center.rgb;"
        "    float alpha = forceOpaque ? 1.0 : center.a;"
        "    if (enhanceParams.x > 0.001 && invTexSize.x > 0.0 && invTexSize.y > 0.0) {"
        "        vec2 lo = invTexSize * 0.5;"
        "        vec2 hi = vec2(1.0) - lo;"
        "        vec2 dx = vec2(invTexSize.x, 0.0);"
        "        vec2 dy = vec2(0.0, invTexSize.y);"
        "        vec3 cNW = texture2D(s, clamp(t - dx - dy, lo, hi)).rgb;"
        "        vec3 cN  = texture2D(s, clamp(t - dy,      lo, hi)).rgb;"
        "        vec3 cNE = texture2D(s, clamp(t + dx - dy, lo, hi)).rgb;"
        "        vec3 cW  = texture2D(s, clamp(t - dx,      lo, hi)).rgb;"
        "        vec3 cE  = texture2D(s, clamp(t + dx,      lo, hi)).rgb;"
        "        vec3 cSW = texture2D(s, clamp(t - dx + dy, lo, hi)).rgb;"
        "        vec3 cS  = texture2D(s, clamp(t + dy,      lo, hi)).rgb;"
        "        vec3 cSE = texture2D(s, clamp(t + dx + dy, lo, hi)).rgb;"
        "        float lNW = dot(cNW, LUMA); float lN = dot(cN, LUMA); float lNE = dot(cNE, LUMA);"
        "        float lW  = dot(cW,  LUMA); float lM = dot(e,  LUMA); float lE  = dot(cE,  LUMA);"
        "        float lSW = dot(cSW, LUMA); float lS = dot(cS, LUMA); float lSE = dot(cSE, LUMA);"
        "        float lMin = min(lM, min(min(min(lN, lS), min(lW, lE)), min(min(lNW, lSE), min(lNE, lSW))));"
        "        float lMax = max(lM, max(max(max(lN, lS), max(lW, lE)), max(max(lNW, lSE), max(lNE, lSW))));"
        "        float range = lMax - lMin;"
        "        vec3 aaColor = e;"
        "        float aaAmount = clamp(enhanceParams.x / 0.52, 0.35, 1.0);"
        "        if (range > max(0.04, lMax * 0.11)) {"
        "            float gx = (lNE + 2.0 * lE + lSE) - (lNW + 2.0 * lW + lSW);"
        "            float gy = (lSW + 2.0 * lS + lSE) - (lNW + 2.0 * lN + lNE);"
        "            vec2 tang = vec2(gy, -gx);"
        "            float tangLen = length(tang);"
        "            if (enhanceParams.x >= 0.42 && tangLen > 0.035 * range) {"
        "                vec2 dir = tang / tangLen;"
        "                vec3 cA = texture2D(s, clamp(t - dir * (invTexSize * 0.65), lo, hi)).rgb;"
        "                vec3 cB = texture2D(s, clamp(t + dir * (invTexSize * 0.65), lo, hi)).rgb;"
        "                vec3 edgeMix = 0.5 * (cA + cB);"
        "                float lEdge = dot(edgeMix, LUMA);"
        "                if (lEdge >= lMin - 0.025 && lEdge <= lMax + 0.025) {"
        "                    float dirFactor = clamp(tangLen / (2.1 * max(range, 0.001)), 0.0, 1.0);"
        "                    float edgeBlend = clamp((range - 0.04) * 1.9, 0.0, 0.62) * dirFactor * aaAmount;"
        "                    aaColor = mix(e, edgeMix, edgeBlend);"
        "                }"
        "            }"
        "            float cardDev = max(max(abs(lN - lM), abs(lS - lM)), max(abs(lW - lM), abs(lE - lM)));"
        "            if (cardDev < 0.32 * range) {"
        "                float sameD1 = max(0.0, 1.0 - abs(lNW - lSE) / max(range, 0.001));"
        "                float sameD2 = max(0.0, 1.0 - abs(lNE - lSW) / max(range, 0.001));"
        "                float gapD1 = sameD1 * clamp(abs(0.5 * (lNW + lSE) - lM) / max(range, 0.001), 0.0, 1.0);"
        "                float gapD2 = sameD2 * clamp(abs(0.5 * (lNE + lSW) - lM) / max(range, 0.001), 0.0, 1.0);"
        "                vec3 diagRef = gapD1 >= gapD2 ? 0.5 * (cNW + cSE) : 0.5 * (cNE + cSW);"
        "                float diagStrength = clamp((max(gapD1, gapD2) - min(gapD1, gapD2)) * 0.56, 0.0, 0.56) * aaAmount;"
        "                aaColor = mix(aaColor, diagRef, diagStrength);"
        "            }"
        "        }"
        "        vec3 crossAvg = 0.25 * (cN + cS + cW + cE);"
        "        vec3 rgbMin = min(aaColor, min(min(cN, cS), min(cW, cE)));"
        "        vec3 rgbMax = max(aaColor, max(max(cN, cS), max(cW, cE)));"
        "        float edgeProtect = 1.0 - 0.35 * smoothstep(0.28, 0.72, range);"
        "        vec3 sharp = aaColor + (aaColor - crossAvg) * (enhanceParams.x * 1.55 * edgeProtect);"
        "        float headroom = 0.015 + 0.045 * enhanceParams.x;"
        "        e = clamp(sharp, max(rgbMin - headroom, vec3(0.0)), min(rgbMax + headroom, vec3(1.0)));"
        "    }"
        "    if (enhanceParams.w > 0.5 || enhanceParams.y > 0.001 || enhanceParams.z > 0.001) {"
        "        vec3 lin = e * (e * (e * 0.305306011 + 0.682171111) + 0.012522878);"
        "        if (enhanceParams.y > 0.001) {"
        "            float lLin = dot(lin, LUMA);"
        "            if (lLin > 0.0001 && lLin < 1.0) {"
        "                const float mid = 0.21;"
        "                float k = enhanceParams.y;"
        "                float targetL = lLin < mid"
        "                    ? lLin * (1.0 - k * 0.42 * (mid - lLin) / mid)"
        "                    : 1.0 - (1.0 - lLin) * (1.0 - k * 0.26 * (lLin - mid) / (1.0 - mid));"
        "                lin = clamp(lin * (targetL / lLin), 0.0, 1.0);"
        "            }"
        "        }"
        "        if (enhanceParams.z > 0.001) {"
        "            float greenExcess = max(0.0, lin.g - max(lin.r, lin.b));"
        "            vec3 foliageShift = vec3(0.08, -0.04, -0.10) * (greenExcess * clamp(enhanceParams.z * 2.2, 0.0, 0.75));"
        "            lin = clamp(lin + foliageShift, 0.0, 1.0);"
        "            float lumaLin = dot(lin, LUMA);"
        "            float sat = max(lin.r, max(lin.g, lin.b)) - min(lin.r, min(lin.g, lin.b));"
        "            float vibScale = 1.0 + enhanceParams.z * max(1.25 - sat * 0.9, 0.2);"
        "            lin = clamp(mix(vec3(lumaLin), lin, vibScale), 0.0, 1.0);"
        "        }"
        "        if (enhanceParams.w > 0.5) {"
        "            mat3 srgbToP3 = mat3("
        "                0.822462, 0.033194, 0.017083,"
        "                0.177538, 0.966806, 0.072397,"
        "                0.000000, 0.000000, 0.910520"
        "            );"
        "            lin = clamp(srgbToP3 * lin, 0.0, 1.0);"
        "        }"
        "        vec3 s1 = sqrt(lin);"
        "        vec3 s2 = sqrt(s1);"
        "        vec3 s3 = sqrt(s2);"
        "        e = clamp(0.662002687 * s1 + 0.684122060 * s2 - 0.323583601 * s3 - 0.0225411470, 0.0, 1.0);"
        "    }"
        "    gl_FragColor = vec4(e, alpha);"
        "}";
    int (*uniformLocation)(uint32_t, const char *) = gl_fn("glGetUniformLocation");
    void (*genTex)(int32_t, uint32_t *) = gl_fn("glGenTextures");
    void (*genBuf)(int32_t, uint32_t *) = gl_fn("glGenBuffers");
    void (*bindBuf)(uint32_t, uint32_t) = gl_fn("glBindBuffer");
    void (*bufData)(uint32_t, intptr_t, const void *, uint32_t) = gl_fn("glBufferData");
    o->prog = enhance ? ov_build_program(vs, fs_enhance) : 0;
    bool using_enhance = o->prog != 0;
    o->plain_prog = ov_build_program(vs, fs_plain);
    if (!o->prog) {
        if (enhance) tl_log_line("javaapp: compositor enhanced shader unavailable; falling back to passthrough");
        o->prog = o->plain_prog;
    }
    o->opaque_uniform = o->prog ? uniformLocation(o->prog, "forceOpaque") : -1;
    o->plain_opaque_uniform = o->plain_prog ? uniformLocation(o->plain_prog, "forceOpaque") : -1;
    o->texel_uniform = using_enhance ? uniformLocation(o->prog, "invTexSize") : -1;
    o->enhance_uniform = using_enhance ? uniformLocation(o->prog, "enhanceParams") : -1;
    genTex(1, &o->tex);
    static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
    genBuf(1, &o->vbo);
    bindBuf(0x8892, o->vbo);
    bufData(0x8892, sizeof(quad), quad, 0x88E4);
}
static void ov_init(overlay *o) { ov_init_flip_mode(o, false, false); }

static void ov_upload(overlay *o, const uint8_t *px, int w, int h)
{
    void (*bindTex)(uint32_t, uint32_t) = gl_fn("glBindTexture");
    void (*texParam)(uint32_t, uint32_t, int32_t) = gl_fn("glTexParameteri");
    void (*texImage)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, const void *) = gl_fn("glTexImage2D");
    void (*texSub)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, const void *) = gl_fn("glTexSubImage2D");
    void (*activeTex)(uint32_t) = gl_fn("glActiveTexture");
    void (*pixelStore)(uint32_t, int32_t) = gl_fn("glPixelStorei");
    activeTex(0x84C0);
    bindTex(0x0DE1, o->tex);
    pixelStore(0x0CF5, 4);
    if (o->tw != w || o->th != h) {
        texParam(0x0DE1, 0x2801, 0x2601); texParam(0x0DE1, 0x2800, 0x2601);
        texParam(0x0DE1, 0x2802, 0x812F); texParam(0x0DE1, 0x2803, 0x812F);
        texImage(0x0DE1, 0, 0x1908, w, h, 0, 0x1908, 0x1401, px);
        o->tw = w; o->th = h;
    } else texSub(0x0DE1, 0, 0, 0, w, h, 0x1908, 0x1401, px);
}

static void ov_draw(overlay *o, int vw, int vh)
{
    void (*viewport)(int32_t, int32_t, int32_t, int32_t) = gl_fn("glViewport");
    void (*useProgram)(uint32_t) = gl_fn("glUseProgram");
    void (*enable)(uint32_t) = gl_fn("glEnable");
    void (*disable)(uint32_t) = gl_fn("glDisable");
    void (*blendFunc)(uint32_t, uint32_t) = gl_fn("glBlendFunc");
    void (*bindTex)(uint32_t, uint32_t) = gl_fn("glBindTexture");
    void (*bindBuf)(uint32_t, uint32_t) = gl_fn("glBindBuffer");
    void (*attrPtr)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void *) = gl_fn("glVertexAttribPointer");
    void (*enableAttr)(uint32_t) = gl_fn("glEnableVertexAttribArray");
    void (*draw)(uint32_t, int32_t, int32_t) = gl_fn("glDrawArrays");
    void (*activeTex)(uint32_t) = gl_fn("glActiveTexture");
    if (!o->tw) return;
    viewport(0, 0, vw, vh);
    disable(0x0B71); disable(0x0B44); disable(0x0C11); disable(0x0B90);    /* depth, cull, scissor, stencil */
    enable(0x0BE2);
    blendFunc(1, 0x0303);                                                  /* premultiplied: ONE, ONE_MINUS_SRC_ALPHA */
    useProgram(o->prog);
    void (*uniform1i)(int32_t, int32_t) = gl_fn("glUniform1i");
    uniform1i(o->opaque_uniform, 0); /* Java UI always retains its premultiplied alpha. */
    activeTex(0x84C0);
    bindTex(0x0DE1, o->tex);
    bindBuf(0x8892, o->vbo);
    attrPtr(0, 2, 0x1406, 0, 0, NULL);
    enableAttr(0);
    draw(0x0005, 0, 4);
}

/* ---- EGL for the app's own threads (a GLSurfaceView copied into the app, an engine's GL thread, EGL14 users)
 *
 * The app's contexts are real ANGLE contexts made on its threads, sharing the render thread's objects. A window surface is a pbuffer
 * the size of the SurfaceView; eglSwapBuffers copies it into a texture (two, in turn) and hands that to the render thread, which
 * draws it where the SurfaceView is, under the window's views. A context that cannot share (ANGLE refuses some pairs) reads its
 * frame back into memory instead. */
typedef struct { EGLContext c; int version; bool readback; } app_ctx;
typedef struct { EGLSurface pb; int w, h; uint32_t tex[3]; int tw[3], th[3]; int cur; uint8_t *px; size_t px_capacity; } app_surf;
static struct {
    pthread_mutex_t lk;
    pthread_cond_t cv;
    EGLDisplay dpy; EGLConfig cfg; EGLContext ui, renderer; EGLSurface screen; bool es3, ready;
    /* the frame to show: a shared texture, or pixels (bottom row first) */
    uint32_t tex; uint8_t *px; int pw, ph; int x, y, w, h; uint64_t seq, drawn_seq; bool on, opaque;
    size_t px_capacity;
} E2 = { .lk = PTHREAD_MUTEX_INITIALIZER, .cv = PTHREAD_COND_INITIALIZER };

static pthread_t g_render_thread;
static atomic_bool g_compositor_active;

static bool e2_wait_ready(void)
{
    pthread_mutex_lock(&E2.lk);
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 5;
    while (!E2.ready && !atomic_load(&A.ended) && atomic_load(&g_compositor_active))
        if (pthread_cond_timedwait(&E2.cv, &E2.lk, &ts)) break;
    bool r = E2.ready;
    pthread_mutex_unlock(&E2.lk);
    return r;
}

NAT(EN_createContext)
{
    (void)self;
    ret->j = 0;
    if (!e2_wait_ready()) return true;
    EGLContext (*createContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *) = tl_egl_resolve("eglCreateContext");
    int v = a[0].i <= 1 ? 1 : a[0].i >= 3 && E2.es3 ? 3 : 2;
    app_ctx *share = (app_ctx *)(uintptr_t)a[1].j;
    app_ctx *c = calloc(1, sizeof(*c));
    for (int tries = 0; tries < 2 && !c->c; tries++) {
        const EGLint attr[] = { 0x3098, v, EGL_NONE };
        c->c = createContext(E2.dpy, E2.cfg, share && share->c ? share->c : E2.ui, attr);
        if (!c->c) { c->c = createContext(E2.dpy, E2.cfg, share && share->c ? share->c : NULL, attr); c->readback = c->c != NULL; }
        if (!c->c && v == 1) v = 2;
    }
    if (!c->c) { tl_log_line("egl: no ES %d context for the app's thread", v); free(c); return true; }
    c->version = v;
    tl_log_line("egl: the app made an ES %d context on its own thread%s", v, c->readback ? " (frames read back)" : "");
    ret->j = (int64_t)(uintptr_t)c;
    return true;
}
NAT(EN_destroyContext)
{
    (void)self; (void)ret;
    app_ctx *c = (app_ctx *)(uintptr_t)a[0].j;
    if (!c) return true;
    unsigned (*destroy)(EGLDisplay, EGLContext) = tl_egl_resolve("eglDestroyContext");
    if (destroy) destroy(E2.dpy, c->c);
    free(c);
    return true;
}
NAT(EN_createSurface)
{
    (void)self;
    ret->j = 0;
    if (!e2_wait_ready()) return true;
    EGLSurface (*createPbuffer)(EGLDisplay, EGLConfig, const EGLint *) = tl_egl_resolve("eglCreatePbufferSurface");
    int w = a[0].i > 0 ? a[0].i : 1, h = a[1].i > 0 ? a[1].i : 1;
    const EGLint attr[] = { 0x3057, w, 0x3056, h, EGL_NONE };
    EGLSurface pb = createPbuffer ? createPbuffer(E2.dpy, E2.cfg, attr) : NULL;
    if (!pb) { tl_log_line("egl: no %dx%d surface for the app", w, h); return true; }
    app_surf *s = calloc(1, sizeof(*s));
    s->pb = pb; s->w = w; s->h = h;
    ret->j = (int64_t)(uintptr_t)s;
    return true;
}
NAT(EN_destroySurface)
{
    (void)self; (void)ret;
    app_surf *s = (app_surf *)(uintptr_t)a[0].j;
    if (!s) return true;
    unsigned (*destroy)(EGLDisplay, EGLSurface) = tl_egl_resolve("eglDestroySurface");
    pthread_mutex_lock(&E2.lk);
    if (E2.on && (E2.tex == s->tex[0] || E2.tex == s->tex[1] || E2.tex == s->tex[2])) { E2.on = false; E2.seq++; pthread_cond_broadcast(&E2.cv); }
    pthread_mutex_unlock(&E2.lk);
    if (destroy) destroy(E2.dpy, s->pb);
    free(s->px);
    free(s);
    return true;
}
/* makeCurrent(surface, context): no context releases the thread's */
NAT(EN_makeCurrent)
{
    (void)self;
    unsigned (*makeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext) = tl_egl_resolve("eglMakeCurrent");
    app_surf *s = (app_surf *)(uintptr_t)a[0].j;
    app_ctx *c = (app_ctx *)(uintptr_t)a[1].j;
    ret->j = 0;
    if (!E2.ready) { ret->z = !c; return true; }
    if (!c) { ret->z = makeCurrent(E2.dpy, NULL, NULL, NULL) != 0; return true; }
    EGLSurface surf = s ? s->pb : NULL;
    if (!surf) {
        /* a context with no surface: a small pbuffer per thread when the display cannot go surfaceless */
        static __thread EGLSurface tiny;
        if (makeCurrent(E2.dpy, NULL, NULL, c->c)) { ret->z = true; return true; }
        if (!tiny) { EGLSurface (*createPbuffer)(EGLDisplay, EGLConfig, const EGLint *) = tl_egl_resolve("eglCreatePbufferSurface"); const EGLint at[] = { 0x3057, 1, 0x3056, 1, EGL_NONE }; tiny = createPbuffer(E2.dpy, E2.cfg, at); }
        surf = tiny;
    }
    ret->z = makeCurrent(E2.dpy, surf, surf, c->c) != 0;
    if (!ret->z) tl_log_line("egl: makeCurrent failed for the app's thread");
    return true;
}
/* swap(surface, context, x, y, w, h): the frame the app drew goes on screen at the view's place */
NAT(EN_swap)
{
    (void)self;
    app_surf *s = (app_surf *)(uintptr_t)a[0].j;
    app_ctx *c = (app_ctx *)(uintptr_t)a[1].j;
    ret->j = 0;
    if (!s || !c) return true;
    void (*getIntegerv)(uint32_t, int32_t *) = gl_fn("glGetIntegerv");
    void (*bindTex)(uint32_t, uint32_t) = gl_fn("glBindTexture");
    void (*activeTex)(uint32_t) = gl_fn("glActiveTexture");
    void (*genTex)(int32_t, uint32_t *) = gl_fn("glGenTextures");
    void (*texParam)(uint32_t, uint32_t, int32_t) = gl_fn("glTexParameteri");
    void (*copyTex)(uint32_t, int32_t, uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t) = gl_fn("glCopyTexImage2D");
    void (*copyTexSub)(uint32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) = gl_fn("glCopyTexSubImage2D");
    void (*bindFb)(uint32_t, uint32_t) = gl_fn("glBindFramebuffer");
    void (*readPixels)(int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t, void *) = gl_fn("glReadPixels");
    void (*pixelStore)(uint32_t, int32_t) = gl_fn("glPixelStorei");
    void (*flush)(void) = gl_fn("glFlush");
    int32_t fb = 0, active = 0x84C0, tex = 0, pack = 4;
    uint32_t fb_target = c->version >= 3 ? 0x8CA8u : 0x8D40u;
    uint32_t fb_query = c->version >= 3 ? 0x8CAAu : 0x8CA6u;
    if (c->version >= 2) getIntegerv(fb_query, &fb);              /* the app's framebuffer: the frame is the surface's own */
    getIntegerv(0x84E0, &active);
    if (fb && bindFb) bindFb(fb_target, 0);
    if (c->readback) {
        getIntegerv(0x0D05, &pack);
        if (!tl_rgba_buffer_reserve(&s->px, &s->px_capacity, s->w, s->h)) {
            if (fb && bindFb) bindFb(fb_target, (uint32_t)fb);
            tl_log_line("javaapp: readback frame allocation failed (%dx%d)", s->w, s->h);
            ret->z = false;
            return true;
        }
        pixelStore(0x0D05, 4);
        readPixels(0, 0, s->w, s->h, 0x1908, 0x1401, s->px);
        pixelStore(0x0D05, pack);
        pthread_mutex_lock(&E2.lk);
        if (!tl_rgba_buffer_reserve(&E2.px, &E2.px_capacity, s->w, s->h)) {
            pthread_mutex_unlock(&E2.lk);
            if (fb && bindFb) bindFb(fb_target, (uint32_t)fb);
            tl_log_line("javaapp: compositor frame allocation failed (%dx%d)", s->w, s->h);
            ret->z = false;
            return true;
        }
        memcpy(E2.px, s->px, (size_t)s->w * s->h * 4);
        E2.pw = s->w; E2.ph = s->h; E2.tex = 0;
    } else {
        activeTex(0x84C0);
        getIntegerv(0x8069, &tex);
        int k = s->cur;
        if (!s->tex[k]) genTex(1, &s->tex[k]);
        bindTex(0x0DE1, s->tex[k]);
        if (s->tw[k] != s->w || s->th[k] != s->h) {
            texParam(0x0DE1, 0x2801, 0x2601); texParam(0x0DE1, 0x2800, 0x2601);
            texParam(0x0DE1, 0x2802, 0x812F); texParam(0x0DE1, 0x2803, 0x812F);
            copyTex(0x0DE1, 0, 0x1908, 0, 0, s->w, s->h, 0);
            s->tw[k] = s->w; s->th[k] = s->h;
        } else copyTexSub(0x0DE1, 0, 0, 0, 0, 0, s->w, s->h);
        bindTex(0x0DE1, (uint32_t)tex);
        activeTex((uint32_t)active);
        if (flush) flush();
        s->cur = (s->cur + 1) % 3;
        pthread_mutex_lock(&E2.lk);
        E2.tex = s->tex[k]; E2.pw = s->w; E2.ph = s->h;
    }
    uint64_t prev_seq = E2.seq;
    E2.x = a[2].i; E2.y = a[3].i; E2.w = a[4].i > 0 ? a[4].i : s->w; E2.h = a[5].i > 0 ? a[5].i : s->h;
    E2.opaque = false; /* Legacy EGLNative has no SurfaceView composition contract. */
    E2.on = true; E2.seq = prev_seq + 1;
    pthread_cond_broadcast(&E2.cv);
    if (!atomic_load(&A.ended) && !atomic_load(&A.paused) && E2.drawn_seq < prev_seq) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 16000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        while (!atomic_load(&A.ended) && !atomic_load(&A.paused) && E2.drawn_seq < prev_seq)
            if (pthread_cond_timedwait(&E2.cv, &E2.lk, &ts)) break;
    }
    pthread_mutex_unlock(&E2.lk);
    if (fb && bindFb) bindFb(fb_target, (uint32_t)fb);
    ret->z = true;
    return true;
}
/* the GL thread Husk drives GLSurfaceView from: there EGL10 answers for the context it made */
NAT(EN_isRenderThread) { (void)self; (void)a; ret->j = 0; ret->z = g_render_thread && pthread_equal(pthread_self(), g_render_thread) != 0; return true; }
NAT(EN_hide) { (void)self; (void)a; (void)ret; pthread_mutex_lock(&E2.lk); E2.on = false; E2.seq++; pthread_cond_broadcast(&E2.cv); pthread_mutex_unlock(&E2.lk); return true; }
static const struct { const char *name, *sig; dvm_native_fn fn; } k_eglnative[] = {
    { "createContext", "(IJ)J", EN_createContext }, { "destroyContext", "(J)V", EN_destroyContext },
    { "createSurface", "(II)J", EN_createSurface }, { "destroySurface", "(J)V", EN_destroySurface },
    { "makeCurrent", "(JJ)Z", EN_makeCurrent }, { "swap", "(JJIIII)Z", EN_swap },
    { "isRenderThread", "()Z", EN_isRenderThread }, { "hide", "()V", EN_hide },
    { NULL, NULL, NULL },
};
static dvm_native_fn egl_native_lookup(const char *name, const char *sig)
{
    for (int i = 0; k_eglnative[i].name; i++) if (!strcmp(k_eglnative[i].name, name) && !strcmp(k_eglnative[i].sig, sig)) return k_eglnative[i].fn;
    return NULL;
}

/* drawing the app's frame on the render thread: a quad at the view's place, the frame's bottom row first */
static uint64_t app_frame_draw(overlay *o, overlay *up, int vw, int vh, uint64_t *handoff_ns)
{
    int64_t handoff_start = now_ns();
    tl_egl_compositor_frame_begin();
    int64_t handoff_end = now_ns();
    *handoff_ns = handoff_end >= handoff_start ? (uint64_t)(handoff_end - handoff_start) : 0;
    pthread_mutex_lock(&E2.lk);
    bool on = E2.on, opaque = E2.opaque; uint32_t tex = E2.tex; int x = E2.x, y = E2.y, w = E2.w, h = E2.h;
    int pw = E2.pw, ph = E2.ph;
    uint64_t sampled_seq = E2.seq;
    if (on && !tex && E2.px) { ov_upload(up, E2.px, pw, ph); }
    pthread_mutex_unlock(&E2.lk);
    if (!on) { tl_egl_compositor_frame_end(0); return 0; }
    tl_egl_compositor_frame_wait(tex);
    void (*viewport)(int32_t, int32_t, int32_t, int32_t) = gl_fn("glViewport");
    void (*useProgram)(uint32_t) = gl_fn("glUseProgram");
    void (*disable)(uint32_t) = gl_fn("glDisable");
    void (*bindTex)(uint32_t, uint32_t) = gl_fn("glBindTexture");
    void (*bindBuf)(uint32_t, uint32_t) = gl_fn("glBindBuffer");
    void (*attrPtr)(uint32_t, int32_t, uint32_t, uint8_t, int32_t, const void *) = gl_fn("glVertexAttribPointer");
    void (*enableAttr)(uint32_t) = gl_fn("glEnableVertexAttribArray");
    void (*draw)(uint32_t, int32_t, int32_t) = gl_fn("glDrawArrays");
    void (*activeTex)(uint32_t) = gl_fn("glActiveTexture");
    viewport(x, vh - y - h, w, h);
    disable(0x0B71); disable(0x0B44); disable(0x0C11); disable(0x0B90); disable(0x0BE2);
    void (*uniform1i)(int32_t, int32_t) = gl_fn("glUniform1i");
    bool use_plain = o->plain_prog != 0 && o->texel_uniform < 0;
    if (o->texel_uniform >= 0 && o->enhance_uniform >= 0) {
        int src_w = pw > 0 ? pw : w;
        int src_h = ph > 0 ? ph : h;
        bool wide_color = tl_host_display_wide_color();
        int thermal = 0;
        { extern int tl_host_thermal_state(void) __attribute__((weak)); if (&tl_host_thermal_state != NULL) thermal = tl_host_thermal_state(); }
        tl_visual_params vp = tl_visual_compute_params_thermal(tl_host_visual_quality(), thermal, wide_color,
                                                               src_w, src_h, w, h);
        use_plain = o->plain_prog != 0 && !wide_color
            && vp.sharpness <= 0.001f && vp.contrast <= 0.001f && vp.vibrance <= 0.001f;
        if (use_plain) {
            useProgram(o->plain_prog);
            uniform1i(o->plain_opaque_uniform, opaque ? 1 : 0);
        } else {
            useProgram(o->prog);
            uniform1i(o->opaque_uniform, opaque ? 1 : 0);
            void (*uniform2f)(int32_t, float, float) = gl_fn("glUniform2f");
            void (*uniform4f)(int32_t, float, float, float, float) = gl_fn("glUniform4f");
            if (uniform2f) uniform2f(o->texel_uniform, src_w > 0 ? 1.0f / (float)src_w : 0.0f, src_h > 0 ? 1.0f / (float)src_h : 0.0f);
            if (uniform4f) uniform4f(o->enhance_uniform, vp.sharpness, vp.contrast, vp.vibrance, wide_color ? 1.0f : 0.0f);
        }
    } else {
        useProgram(use_plain ? o->plain_prog : o->prog);
        uniform1i(use_plain ? o->plain_opaque_uniform : o->opaque_uniform, opaque ? 1 : 0);
    }
    activeTex(0x84C0);
    bindTex(0x0DE1, tex ? tex : up->tex);
    bindBuf(0x8892, o->vbo);
    attrPtr(0, 2, 0x1406, 0, 0, NULL);
    enableAttr(0);
    draw(0x0005, 0, 4);
    tl_egl_compositor_frame_end(tex);
    viewport(0, 0, vw, vh);
    return sampled_seq;
}
/* For EGL: whether this runtime's compositor holds the screen, and an app's own EGL window's frame for it (NULL: no longer shown) */
bool tl_javaapp_compositing(void)
{
    if (!atomic_load(&g_compositor_active)) return false;
    if (g_render_thread && pthread_equal(pthread_self(), g_render_thread)) return false;
    if (A.gl_thread_started && pthread_equal(pthread_self(), A.gl_thread)) return false;
    return e2_wait_ready();
}
void *tl_javaapp_ui_context(void)
{
    if (!tl_javaapp_compositing()) return NULL;
    pthread_mutex_lock(&E2.lk);
    void *u = E2.ready ? E2.ui : NULL;
    pthread_mutex_unlock(&E2.lk);
    return u;
}
void tl_javaapp_wake_gl(void)
{
    pthread_mutex_lock(&E2.lk);
    pthread_cond_broadcast(&E2.cv);
    pthread_mutex_unlock(&E2.lk);
}
void tl_javaapp_app_pixels(const void *px, int w, int h)
{
    pthread_mutex_lock(&E2.lk);
    if (!px) { E2.on = false; E2.seq++; pthread_mutex_unlock(&E2.lk); return; }
    if (!tl_rgba_buffer_reserve(&E2.px, &E2.px_capacity, w, h)) {
        pthread_mutex_unlock(&E2.lk);
        tl_log_line("javaapp: compositor frame allocation failed (%dx%d); keeping previous frame", w, h);
        return;
    }
    memcpy(E2.px, px, (size_t)w * (size_t)h * 4);
    E2.pw = w; E2.ph = h; E2.tex = 0; E2.opaque = false;
    E2.x = 0; E2.y = 0; E2.w = A.cfg.width; E2.h = A.cfg.height;
    E2.on = true; E2.seq++;
    pthread_mutex_unlock(&E2.lk);
}
/* Publication is nonblocking so EGL can keep the frame ownership lock until
 * the texture and its matching dimensions become visible together. */
uint64_t tl_javaapp_app_tex_publish_with_opacity(uint32_t tex, int pw, int ph, int x, int y, int w, int h, bool opaque)
{
    pthread_mutex_lock(&E2.lk);
    if (!tex || pw <= 0 || ph <= 0) {
        E2.on = false; E2.tex = 0; E2.opaque = false; E2.seq++;
        pthread_cond_broadcast(&E2.cv);
        pthread_mutex_unlock(&E2.lk);
        return 0;
    }
    uint64_t prev_seq = E2.seq;
    E2.tex = tex; E2.pw = pw; E2.ph = ph; E2.opaque = opaque;
    E2.x = x; E2.y = y;
    E2.w = w > 0 ? w : A.cfg.width;
    E2.h = h > 0 ? h : A.cfg.height;
    E2.on = true; E2.seq = prev_seq + 1;
    pthread_cond_broadcast(&E2.cv);
    pthread_mutex_unlock(&E2.lk);
    return prev_seq;
}
uint64_t tl_javaapp_app_tex_publish(uint32_t tex, int pw, int ph, int x, int y, int w, int h)
{
    return tl_javaapp_app_tex_publish_with_opacity(tex, pw, ph, x, y, w, h, false);
}
void tl_javaapp_app_frame_pace(uint64_t prev_seq)
{
    pthread_mutex_lock(&E2.lk);
    if (!atomic_load(&A.ended) && !atomic_load(&A.paused) && E2.drawn_seq < prev_seq) {
        struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += 16000000L;
        if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
        while (!atomic_load(&A.ended) && !atomic_load(&A.paused) && E2.drawn_seq < prev_seq)
            if (pthread_cond_timedwait(&E2.cv, &E2.lk, &ts)) break;
    }
    pthread_mutex_unlock(&E2.lk);
}
void tl_javaapp_app_tex(uint32_t tex, int pw, int ph, int x, int y, int w, int h)
{
    uint64_t prev_seq = tl_javaapp_app_tex_publish(tex, pw, ph, x, y, w, h);
    tl_javaapp_app_frame_pace(prev_seq);
}
/* EGL changes surface ownership only when the compositor accepted this frame. */
bool tl_javaapp_app_pixels_rect_publish_with_opacity(const void *px, int pw, int ph, int x, int y, int w, int h, bool opaque)
{
    pthread_mutex_lock(&E2.lk);
    if (!px || pw <= 0 || ph <= 0) {
        E2.on = false; E2.opaque = false; E2.seq++;
        pthread_cond_broadcast(&E2.cv);
        pthread_mutex_unlock(&E2.lk);
        return true;
    }
    if (!tl_rgba_buffer_reserve(&E2.px, &E2.px_capacity, pw, ph)) {
        pthread_mutex_unlock(&E2.lk);
        tl_log_line("javaapp: compositor frame allocation failed (%dx%d); keeping previous frame", pw, ph);
        return false;
    }
    uint64_t prev_seq = E2.seq;
    memcpy(E2.px, px, (size_t)pw * (size_t)ph * 4);
    E2.pw = pw; E2.ph = ph; E2.tex = 0; E2.opaque = opaque;
    E2.x = x; E2.y = y;
    E2.w = w > 0 ? w : A.cfg.width;
    E2.h = h > 0 ? h : A.cfg.height;
    E2.on = true; E2.seq = prev_seq + 1;
    pthread_cond_broadcast(&E2.cv);
    pthread_mutex_unlock(&E2.lk);
    return true;
}
bool tl_javaapp_app_pixels_rect_publish(const void *px, int pw, int ph, int x, int y, int w, int h)
{
    return tl_javaapp_app_pixels_rect_publish_with_opacity(px, pw, ph, x, y, w, h, false);
}
void tl_javaapp_app_pixels_rect(const void *px, int pw, int ph, int x, int y, int w, int h)
{
    (void)tl_javaapp_app_pixels_rect_publish(px, pw, ph, x, y, w, h);
}
static uint64_t app_frame_seq(void) { pthread_mutex_lock(&E2.lk); uint64_t q = E2.seq; pthread_mutex_unlock(&E2.lk); return q; }
static bool app_frame_on(void) { pthread_mutex_lock(&E2.lk); bool q = E2.on; pthread_mutex_unlock(&E2.lk); return q; }

/* The producer waits for consumption of its PREVIOUS publication. Metadata
 * may advance after the render loop's first snapshot, before drawing obtains
 * handoff ownership. Acknowledge the actual sampled frame, including a draw
 * triggered only by Java UI, and keep newer post-draw publications pending. */
static void app_frame_presented(uint64_t observed_seq, bool observed_fresh, uint64_t sampled_seq, bool success, bool external, uint64_t *seen_seq)
{
    if (atomic_load(&A.ended) || atomic_load(&A.paused)) return;
    if (!success) {
        /* No acknowledgement or seen-sequence advance on a failed present.
         * External renderers otherwise retry immediately because seq differs;
         * bound that failure-only retry outside all frame/queue locks. */
        if (external) { struct timespec retry = { 0, 16000000L }; nanosleep(&retry, NULL); }
        return;
    }
    uint64_t consumed = sampled_seq ? sampled_seq : observed_fresh ? observed_seq : 0;
    if (!consumed) return;
    if (*seen_seq < consumed) *seen_seq = consumed;
    pthread_mutex_lock(&E2.lk);
    if (E2.drawn_seq < consumed) {
        E2.drawn_seq = consumed;
        pthread_cond_broadcast(&E2.cv);
    }
    pthread_mutex_unlock(&E2.lk);
}

/*
 * The render thread: one EGL surface on the app's screen, and on it, each frame, what the app's GLSurfaceView renderer draws (in
 * the app's own context) with the window's views over it (in Husk's). An app without GL still gets this thread for its views.
 */
/* Java render callbacks are admitted separately from UI/external-frame
 * composition: a UI wake must not draw into a destroyed GLSurfaceView. */
static bool java_renderer_surface_available(jobj *view)
{
    jvalue surface = tl_jni_call(view, "huskHasSurface", "()Z", NULL);
    if (tl_jni_pending()) { log_pending("huskHasSurface"); return false; }
    return surface.z;
}

static bool java_renderer_frame(jobj *view, jobj *renderer, const jvalue *gl_arg, bool fresh)
{
    jvalue draw = tl_jni_call(view, "huskBeginFrame", "()Z", NULL);
    if (tl_jni_pending()) log_pending("a queued GL event");
    if (!java_renderer_surface_available(view)) return false;
    jvalue rs = tl_jni_call(view, "huskTakeResize", "()J", NULL);
    if (tl_jni_pending()) log_pending("huskTakeResize");
    if (rs.j) {
        jvalue p3[3] = { L(gl_arg[0].l), I((int)(rs.j >> 32)), I((int)(rs.j & 0xffffffff)) };
        tl_log_line("javaapp: the GL view is now %dx%d", (int)(rs.j >> 32), (int)(rs.j & 0xffffffff));
        tl_jni_call(renderer, "onSurfaceChanged", "(Ljavax/microedition/khronos/opengles/GL10;II)V", p3);
        if (tl_jni_pending()) log_pending("onSurfaceChanged");
        draw.z = true;
    }
    bool requested = atomic_exchange(&A.render_requested, false);
    if (!(draw.z || fresh || requested)) return false;
    tl_jni_call(renderer, "onDrawFrame", "(Ljavax/microedition/khronos/opengles/GL10;)V", gl_arg);
    if (tl_jni_pending()) log_pending("onDrawFrame");
    return true;
}

static void *gl_run(void *arg)
{
    TL_AUTORELEASE_SCOPE(thread_pool);
    (void)arg;
    pthread_setname_np("GLThread");
    g_render_thread = pthread_self();
    EGLDisplay (*getDisplay)(void *) = tl_egl_resolve("eglGetDisplay");
    unsigned (*initialize)(EGLDisplay, EGLint *, EGLint *) = tl_egl_resolve("eglInitialize");
    unsigned (*bindAPI)(unsigned) = tl_egl_resolve("eglBindAPI");
    unsigned (*chooseConfig)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *) = tl_egl_resolve("eglChooseConfig");
    EGLContext (*createContext)(EGLDisplay, EGLConfig, EGLContext, const EGLint *) = tl_egl_resolve("eglCreateContext");
    EGLSurface (*createWindowSurface)(EGLDisplay, EGLConfig, void *, const EGLint *) = tl_egl_resolve("eglCreateWindowSurface");
    unsigned (*makeCurrent)(EGLDisplay, EGLSurface, EGLSurface, EGLContext) = tl_egl_resolve("eglMakeCurrent");
    unsigned (*swapBuffers)(EGLDisplay, EGLSurface) = tl_egl_resolve("eglSwapBuffers");
    unsigned (*swapInterval)(EGLDisplay, EGLint) = tl_egl_resolve("eglSwapInterval");
    if (!getDisplay || !createWindowSurface || !swapBuffers) {
        tl_log_line("javaapp: EGL is not available");
        atomic_store(&g_compositor_active, false);
        tl_javaapp_wake_gl();
        return NULL;
    }
    EGLDisplay dpy = getDisplay(NULL);
    EGLint major = 0, minor = 0;
    initialize(dpy, &major, &minor);
    bindAPI(0x30A0);
    EGLConfig cfg = NULL; EGLint ncfg = 0;
    const EGLint want3[] = { 0x3040, 0x40, 0x3033, 0x5, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025, 24, 0x3026, 8, EGL_NONE };
    const EGLint want2[] = { 0x3040, 0x4, 0x3033, 0x5, 0x3024, 8, 0x3023, 8, 0x3022, 8, 0x3021, 8, 0x3025, 24, 0x3026, 8, EGL_NONE };
    bool es3 = chooseConfig(dpy, want3, &cfg, 1, &ncfg) && ncfg >= 1;
    if (!es3 && (!chooseConfig(dpy, want2, &cfg, 1, &ncfg) || ncfg < 1)) {
        tl_log_line("javaapp: no EGL config");
        atomic_store(&g_compositor_active, false);
        tl_javaapp_wake_gl();
        return NULL;
    }
    const EGLint ui_attr3[] = { 0x3098, 3, EGL_NONE };
    const EGLint ui_attr2[] = { 0x3098, 2, EGL_NONE };
    EGLContext ui = es3 ? createContext(dpy, cfg, NULL, ui_attr3) : NULL;
    if (!ui) { ui = createContext(dpy, cfg, NULL, ui_attr2); es3 = false; }
    EGLContext app = NULL;
    EGLSurface surf = createWindowSurface(dpy, cfg, tl_nwindow_get(), NULL);
    E2.dpy = dpy; E2.ui = ui; E2.screen = surf;
    if (!ui || !surf || !makeCurrent(dpy, surf, surf, ui)) {
        tl_log_line("javaapp: no GL context");
        atomic_store(&g_compositor_active, false);
        tl_javaapp_wake_gl();
        return NULL;
    }
#if defined(__APPLE__)
    /* Triple-buffered CAMetalLayer handles tear-free presentation without locking the compositor onto 60 Hz vsync cliffs. */
    if (swapInterval) swapInterval(dpy, 0);
#else
    if (swapInterval) swapInterval(dpy, 1);
#endif
    overlay ov = { 0 }, appov = { 0 }, appup = { 0 };
    ov_init(&ov);
    ov_init_flip_mode(&appov, true, true);
    ov_init_flip_mode(&appup, true, true);
    tl_egl_compositor_context(ui, es3);
    tl_egl_frame_perf_set_active(true);
    pthread_mutex_lock(&E2.lk);
    E2.dpy = dpy; E2.cfg = cfg; E2.ui = ui; E2.es3 = es3; E2.ready = true;
    pthread_cond_broadcast(&E2.cv);
    pthread_mutex_unlock(&E2.lk);
    uint64_t app_seq = 0;
    void (*clearColor)(float, float, float, float) = gl_fn("glClearColor");
    void (*clear)(uint32_t) = gl_fn("glClear");
    tl_log_line("javaapp: render thread ready (ES %d available), %dx%d", es3 ? 3 : 2, A.cfg.width, A.cfg.height);
#if defined(__APPLE__)
    tl_log_line("javaapp: Objective-C temporary objects drained after each GL frame");
#endif

    EGLContext cur_ctx = ui;
    jobj *gl10 = NULL;
    jvalue p1[1];
    int64_t next = now_ns();
    tl_frame_perf_window host_perf = {0};
    uint64_t last_sampled_seq = 0;
    bool perf_paused = false;
    while (!atomic_load(&A.ended)) {
        if (atomic_load(&A.paused)) {
            if (!perf_paused) {
                tl_egl_frame_perf_set_active(false);
                tl_frame_perf_reset(&host_perf);
                last_sampled_seq = 0;
                perf_paused = true;
            }
            pthread_mutex_lock(&E2.lk);
            E2.drawn_seq = E2.seq;
            pthread_cond_broadcast(&E2.cv);
            pthread_mutex_unlock(&E2.lk);
            usleep(20000);
            next = now_ns();
            continue;
        }
        if (perf_paused) {
            tl_egl_frame_perf_set_active(true);
            perf_paused = false;
        }
        TL_AUTORELEASE_SCOPE(frame_pool);
        bool drew = false;
        /* the app's renderer, once its GLSurfaceView is on screen */
        if (__atomic_load_n(&A.gl_renderer, __ATOMIC_ACQUIRE) && !app && java_renderer_surface_available(A.gl_view)) {
            /* GLSurfaceView's default is OpenGL ES 1 (fixed function, through ANGLE's ES1 front end); 2 and 3 as asked */
            int es = A.gl_version >= 3 && es3 ? 3 : A.gl_version <= 1 ? 1 : 2;
            const EGLint attr[] = { 0x3098, es, EGL_NONE };
            app = createContext(dpy, cfg, NULL, attr);
            if (!app && es == 1) {
                tl_log_line("javaapp: no GL ES 1 context; the app gets ES 2");
                es = 2;
                const EGLint attr2[] = { 0x3098, 2, EGL_NONE };
                app = createContext(dpy, cfg, NULL, attr2);
            }
            E2.renderer = app;
            if (!app || !makeCurrent(dpy, surf, surf, app)) { tl_log_line("javaapp: no GL ES %d context for the app", es); A.gl_renderer = NULL; app = NULL; }
            else {
                cur_ctx = app;
                tl_log_line("javaapp: the app's GL ES %d context is ready", es);
                gl10 = dvm_new_object(dvm_class_named("android/opengl/HuskGL10"));
                dvm_slots(gl10);
                tl_jclass *cfgc = dvm_class_named("javax/microedition/khronos/egl/HuskEGL$Config");
                jobj *eglcfg = cfgc ? dvm_new_object(cfgc) : NULL;
                if (eglcfg) dvm_slots(eglcfg);
                jvalue p2[2] = { L(gl10), L(eglcfg) };
                tl_jni_call(A.gl_renderer, "onSurfaceCreated", "(Ljavax/microedition/khronos/opengles/GL10;Ljavax/microedition/khronos/egl/EGLConfig;)V", p2);
                if (tl_jni_pending()) log_pending("onSurfaceCreated");
                jvalue p3[3] = { L(gl10), I(A.cfg.width), I(A.cfg.height) };
                tl_jni_call(A.gl_renderer, "onSurfaceChanged", "(Ljavax/microedition/khronos/opengles/GL10;II)V", p3);
                if (tl_jni_pending()) log_pending("onSurfaceChanged");
                p1[0] = L(gl10);
            }
        }
        const uint8_t *px; int fw, fh; bool any;
        bool fresh = tl_ui_frame_take(&px, &fw, &fh, &any);
        if (fresh) {
            if (any) {
                if (cur_ctx != ui) { makeCurrent(dpy, surf, surf, ui); cur_ctx = ui; }
                ov_upload(&ov, px, fw, fh);
            } else {
                ov.tw = ov.th = 0;
            }
            tl_ui_frame_done();
        }
        if (app) {
            if (cur_ctx != app) { makeCurrent(dpy, surf, surf, app); cur_ctx = app; }
            drew = java_renderer_frame(A.gl_view, A.gl_renderer, p1, fresh);
        }
        uint64_t q = app_frame_seq();
        bool app_fresh = q != app_seq;
        if (drew || fresh || app_fresh) {
            int64_t compose_begin = now_ns();
            uint64_t sampled_seq = 0, handoff_ns = 0;
            bool ui_vis = tl_ui_has_frame();
            if (!app || ui_vis) {
                if (cur_ctx != ui) { makeCurrent(dpy, surf, surf, ui); cur_ctx = ui; }
                if (!app) {
                    bool opaque_cover = !tl_web_any_visible();
                    if (opaque_cover) {
                        pthread_mutex_lock(&E2.lk);
                        opaque_cover = E2.on && (E2.tex || E2.px) && E2.opaque && E2.x <= 0 && E2.y <= 0 && E2.w >= A.cfg.width && E2.h >= A.cfg.height;
                        pthread_mutex_unlock(&E2.lk);
                    }
                    if (!opaque_cover) {
                        clearColor(0, 0, 0, tl_web_any_visible() && !app_frame_on() ? 0 : 1);
                        clear(0x4000);     /* web views show through from under the screen */
                    }
                    sampled_seq = app_frame_draw(&appov, &appup, A.cfg.width, A.cfg.height, &handoff_ns);
                    if (opaque_cover && sampled_seq == 0) {
                        clearColor(0, 0, 0, 1);
                        clear(0x4000);
                    }
                }
                if (ui_vis) ov_draw(&ov, A.cfg.width, A.cfg.height);
            }
            int64_t swap_begin = now_ns();
            unsigned swap_ok = swapBuffers(dpy, surf);
            int64_t swap_end = now_ns();
            if (swap_ok) {
                atomic_fetch_add(&A.frames, 1);
                uint64_t now_u = (uint64_t)swap_end;
                uint64_t prev = atomic_exchange(&A.perf_last_ns, now_u);
                if (prev == 0) {
                    atomic_store(&A.perf_since_ns, now_u);
                } else if (now_u > prev) {
                    uint64_t dt = now_u - prev;
                    if (dt < UINT64_C(5000000000)) {
                        atomic_fetch_add(&A.perf_total_ns, dt);
                        atomic_fetch_add(&A.perf_frames, 1);
                        uint64_t cur_max = atomic_load(&A.perf_max_ns);
                        while (dt > cur_max && !atomic_compare_exchange_weak(&A.perf_max_ns, &cur_max, dt)) {}
                        if (dt >= UINT64_C(50000000)) atomic_fetch_add(&A.perf_over50, 1);
                        if (dt >= UINT64_C(100000000)) atomic_fetch_add(&A.perf_over100, 1);
                    }
                }
            }
            app_frame_presented(q, app_fresh, sampled_seq, swap_ok != 0, app == NULL, &app_seq);
            /* Only a distinct external-game frame counts here. Java UI-only
             * updates or a second presentation of the same texture must not
             * inflate game FPS. Composition wall time includes handoff time;
             * both are CPU observations, not a GPU execution timer. */
            if (swap_ok && sampled_seq && sampled_seq != last_sampled_seq
                && swap_begin >= compose_begin && swap_end >= swap_begin) {
                last_sampled_seq = sampled_seq;
                if (tl_frame_perf_note(&host_perf, (uint64_t)swap_end,
                        (uint64_t)(swap_begin - compose_begin), (uint64_t)(swap_end - swap_begin), handoff_ns)
                    && tl_frame_perf_due(&host_perf)) {
                    char line[1024];
                    tl_frame_perf_format(line, sizeof(line), "host-fresh-game", "compose-wall", "screen-swap", "handoff",
                        &host_perf);
                    tl_frame_perf_roll(&host_perf);
                    tl_log_line("%s", line);
                }
            }
            if (atomic_load(&A.defer_until_ns) != 0) {
                pthread_mutex_lock(&A.q_lock);
                pthread_cond_broadcast(&A.q_cv);
                pthread_mutex_unlock(&A.q_lock);
            }
        }
        int thermal = 0, captured = 0;
        { extern int tl_host_thermal_state(void) __attribute__((weak)); if (tl_host_thermal_state) thermal = tl_host_thermal_state(); }
        { extern int tl_host_screen_captured(void) __attribute__((weak)); if (tl_host_screen_captured) captured = tl_host_screen_captured(); }
        int host_hz = (A.cfg.max_refresh_rate >= (float)TL_JAVA_DISPLAY_HZ && A.cfg.max_refresh_rate <= 240.0f)
            ? (int)(A.cfg.max_refresh_rate + 0.5f) : TL_JAVA_DISPLAY_HZ;
        int target_hz = tl_host_compositor_target_hz(host_hz, thermal, captured);
        if (app) {
            next += 1000000000ll / target_hz;
            int64_t late = now_ns() - next;
            if (late > 100000000ll) next = now_ns();
            else if (late < 0) { struct timespec ts = { 0, (long)(-late) }; nanosleep(&ts, NULL); }
        } else {
            /* Both external swaps and UI invalidations broadcast E2.cv directly,
             * so wait 50ms between idle checks instead of waking spuriously at 120 Hz. */
            long wait_ns = 50000000L;
            pthread_mutex_lock(&E2.lk);
            if (!atomic_load(&A.ended) && !atomic_load(&A.paused) && E2.seq == app_seq
                && !__atomic_load_n(&A.gl_renderer, __ATOMIC_ACQUIRE) && !tl_ui_frame_pending()
                && !atomic_load(&A.render_requested)) {
                struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_nsec += wait_ns;
                if (ts.tv_nsec >= 1000000000L) { ts.tv_sec += ts.tv_nsec / 1000000000L; ts.tv_nsec %= 1000000000L; }
                pthread_cond_timedwait(&E2.cv, &E2.lk, &ts);
            }
            pthread_mutex_unlock(&E2.lk);
            next = now_ns();
        }
    }
    /* Fence cleanup needs a current member of the shared group. A separate
     * GLSurfaceView renderer may have left its private app context current. */
    bool reset_current = makeCurrent(dpy, surf, surf, ui);
    tl_egl_compositor_frame_reset(ui, reset_current);
    makeCurrent(dpy, NULL, NULL, NULL);
    return NULL;
}

/* ================================================================== the driver */

static uint32_t rd32le(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t rd16le(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static bool pool_str(const uint8_t *pool, size_t sz, uint32_t idx, char *out, size_t n)
{
    uint32_t count = rd32le(pool + 8), flags = rd32le(pool + 16), strings = rd32le(pool + 20);
    if (idx >= count) return false;
    uint32_t off = rd32le(pool + 28 + 4 * idx);
    const uint8_t *p = pool + strings + off;
    if (p >= pool + sz) return false;
    if (flags & 0x100) {                                       /* UTF-8 */
        if (*p & 0x80) p += 2; else p++;
        size_t len = *p; if (*p & 0x80) { len = ((size_t)(p[0] & 0x7f) << 8) | p[1]; p += 2; } else p++;
        snprintf(out, n, "%.*s", (int)len, (const char *)p);
    } else {
        size_t len = rd16le(p); p += 2;
        size_t k = 0;
        for (size_t i = 0; i < len && k + 1 < n; i++) { uint16_t c = rd16le(p + 2 * i); out[k++] = c < 0x80 ? (char)c : '?'; }
        out[k] = 0;
    }
    return true;
}

/* The launcher activity and package, from the binary manifest. */
bool tl_javaapp_manifest(const char *apk, char *pkg, size_t pn, char *activity, size_t an, char *application, size_t apn)
{
    tl_zip z; char err[160];
    if (!tl_zip_open(&z, apk, err, sizeof(err))) return false;
    bool ok = false;
    const tl_zip_entry *e = tl_zip_find(&z, "AndroidManifest.xml");
    const uint8_t *d; size_t len; bool owned = false;
    char act[260] = "", found[260] = "", app[260] = "";
    if (application && apn) application[0] = 0;
    if (e && tl_zip_data(&z, e, 8u << 20, &d, &len, &owned, err, sizeof(err)) && len > 8 && rd16le(d) == 0x0003) {
        const uint8_t *pool = NULL; size_t psz = 0;
        const uint8_t *resmap = NULL; uint32_t nres = 0;              /* string index -> attribute resource id */
        for (size_t off = rd16le(d + 2); off + 8 <= len; ) {
            uint16_t type = rd16le(d + off); uint32_t size = rd32le(d + off + 4);
            if (size < 8 || off + size > len) break;
            if (type == 0x0001) { pool = d + off; psz = size; }
            else if (type == 0x0180) { resmap = d + off + rd16le(d + off + 2); nres = (size - rd16le(d + off + 2)) / 4; }
            else if (type == 0x0102 && pool && size >= 36) {
                const uint8_t *el = d + off;
                char name[64] = "";
                pool_str(pool, psz, rd32le(el + 20), name, sizeof(name));
                uint16_t astart = rd16le(el + 24), asize = rd16le(el + 26), acount = rd16le(el + 28);
                for (unsigned i = 0; i < acount; i++) {
                    const uint8_t *at = el + 16 + astart + (size_t)i * asize;
                    char an2[64] = "", av[260] = "";
                    uint32_t ni = rd32le(at + 4);
                    if (!pool_str(pool, psz, ni, an2, sizeof(an2))) continue;
                    /* a shrunk manifest keeps only the attribute's resource id (PayPal's): name and targetActivity by id */
                    if (!an2[0] && resmap && ni < nres) {
                        uint32_t id = rd32le(resmap + 4 * ni);
                        if (id == 0x01010003) snprintf(an2, sizeof(an2), "name"); else if (id == 0x01010202) snprintf(an2, sizeof(an2), "targetActivity");
                    }
                    if (rd32le(at + 8) != 0xFFFFFFFFu) pool_str(pool, psz, rd32le(at + 8), av, sizeof(av));
                    if (!strcmp(name, "manifest") && !strcmp(an2, "package")) snprintf(pkg, pn, "%s", av);
                    if (!strcmp(name, "application") && !strcmp(an2, "name")) snprintf(app, sizeof(app), "%s", av);
                    if ((!strcmp(name, "activity") || !strcmp(name, "activity-alias")) && !strcmp(an2, "name")) snprintf(act, sizeof(act), "%s", av);
                    if (!strcmp(name, "activity-alias") && !strcmp(an2, "targetActivity")) snprintf(act, sizeof(act), "%s", av);
                    if (!strcmp(name, "category") && !strcmp(an2, "name") && !strcmp(av, "android.intent.category.LAUNCHER") && act[0] && !found[0]) snprintf(found, sizeof(found), "%s", act);
                }
            }
            off += size;
        }
    }
    if (found[0]) {
        if (found[0] == '.') snprintf(activity, an, "%s%s", pkg, found);
        else if (!strchr(found, '.')) snprintf(activity, an, "%s.%s", pkg, found);
        else snprintf(activity, an, "%s", found);
        ok = true;
    }
    /* the app's Application subclass, created and its onCreate run before any activity */
    if (app[0] && application && apn) {
        if (app[0] == '.') snprintf(application, apn, "%s%s", pkg, app);
        else if (!strchr(app, '.')) snprintf(application, apn, "%s.%s", pkg, app);
        else snprintf(application, apn, "%s", app);
    }
    if (owned) free((void *)d);
    tl_zip_close(&z);
    return ok;
}

/* The pending exception's stack trace, through android.util.Log, line by line; the exception is cleared. */
static void log_pending_stack(void)
{
    jobj *e = tl_jni_pending_object();
    if (!e) return;
    tl_jni_clear();
    jvalue a[1] = { L(e) }, r;
    if (!tl_dvm_call_static("android/util/Log", "getStackTraceString", "(Ljava/lang/Throwable;)Ljava/lang/String;", a, &r)) { tl_jni_clear(); return; }
    const char *t = tl_jni_string(r.l);
    while (t && *t) {
        const char *nl = strchr(t, '\n');
        int len = nl ? (int)(nl - t) : (int)strlen(t);
        tl_log_line("javaapp:   %.*s", len, t);
        t = nl ? nl + 1 : NULL;
    }
}

static void *app_run(void *arg)
{
    void tl_thread_altstack(void); tl_thread_altstack();
    (void)arg;
    pthread_setname_np("main");
    tl_qos_set_interactive();
    bool startup_trace = getenv("TL_STARTUP_TRACE") != NULL;
    if (startup_trace) tl_log_line("startup: Java Activity runner begin activity=%s", A.activity);
    jvalue r;
    bool ok = false;
    if (tl_dvm_defines_class("husk/AppRunner")) {
        jvalue a[2] = { L(dvm_new_string_utf8(A.activity)), L(A.application[0] ? dvm_new_string_utf8(A.application) : NULL) };
        ok = tl_dvm_call_static("husk/AppRunner", "run", "(Ljava/lang/String;Ljava/lang/String;)V", a, &r);
    } else if (tl_dvm_defines_class("android/app/ActivityThread")) {
        tl_log_line("javaapp: AOSP ActivityThread on the boot path");
        jobj *args = tl_jni_new_obj_array(tl_jni_class("java/lang/String"), 0);
        jvalue a[1] = { L(args) };
        ok = tl_dvm_call_static("android/app/ActivityThread", "main", "([Ljava/lang/String;)V", a, &r);
    } else {
        tl_log_line("javaapp: no AppRunner and no ActivityThread on the boot path");
    }
    if (!ok) {
        char buf[800];
        tl_log_line("javaapp: the app's main thread ended with %s", tl_dvm_describe_pending(buf, sizeof(buf)) ? buf : "?");
        log_pending_stack();
        atomic_store(&A.ended, true);
        tl_javaapp_wake_gl();
    }
    if (startup_trace) tl_log_line("startup: Java Activity runner returned ok=%d", ok);
    return NULL;
}

static void thread_done(void *flag) { atomic_store((atomic_bool *)flag, true); }
static void *app_main(void *arg)
{
    pthread_cleanup_push(thread_done, &A.app_done);
    app_run(arg);
    pthread_cleanup_pop(1);
    return NULL;
}
static void *gl_main(void *arg)
{
    /* This host loop has nested per-frame pools of its own. */
    tl_guest_pool_end(NULL);
    tl_qos_set_interactive();
    pthread_cleanup_push(thread_done, &A.gl_done);
    gl_run(arg);
    pthread_cleanup_pop(1);
    return NULL;
}

/* An empty file in the native library directory for each library the APKs carry: ClassLoader.findLibrary() and File.exists() checks
 * find it (Unity asks the class loader where libil2cpp is), while loading still takes the library from the APK by its name. */
static void stub_lib(const char *name, uint64_t size, void *dir)
{
    (void)size;
    char p[1400]; snprintf(p, sizeof(p), "%s/%s", (const char *)dir, name);
    struct stat st;
    if (stat(p, &st) != 0) { int fd = open(p, O_CREAT | O_WRONLY, 0755); if (fd >= 0) close(fd); }
}

static bool javaapp_begin_session(void)
{
    if (!tl_guest_session_begin()) return false;
    /* The previous session can return to idle only after all guest/GL workers
     * have drained and been reaped. No new workers exist yet, so no UI borrow
     * or late publisher can carry the old game's overlay into this session. */
    tl_ui_frame_reset();
    return true;
}

static void javaapp_reset_subsystems(void)
{
    void tl_dvm_audio_reset(void);
    void tl_opensles_reset_session(void);
    void tl_ndk_reset_session(void);
    void tl_loop_reset_session(void);
    void tl_egl_es3_claim(int version);

    tl_audio_reset();
    tl_dvm_audio_reset();
    tl_opensles_reset_session();
    tl_ndk_reset_session();
    tl_loop_reset_session();
    tl_egl_reset_session();
    tl_vk_reset_session();
    tl_vhtg_quest_reset();
    tl_trutien_text_reset();
    tl_egl_es3_claim(0);
    tl_set_guest_package(NULL);
}

bool tl_javaapp_start(const tl_javaapp_config *cfg)
{
    if (!javaapp_begin_session()) {
        tl_log_line("javaapp: the previous session has not finished closing");
        return false;
    }
    atomic_store(&A.ended, false);
    atomic_store(&A.paused, false);
    atomic_store(&A.frames, 0);
    atomic_store(&A.perf_last_ns, 0);
    atomic_store(&A.perf_since_ns, 0);
    atomic_store(&A.perf_total_ns, 0);
    atomic_store(&A.perf_max_ns, 0);
    atomic_store(&A.perf_frames, 0);
    atomic_store(&A.perf_over50, 0);
    atomic_store(&A.perf_over100, 0);
    atomic_store(&A.render_requested, false);
    atomic_store(&A.defer_until_ns, 0);
    atomic_store(&g_compositor_active, false);
    pthread_mutex_lock(&A.q_lock);
    A.qn = 0; A.q_woken = false;
    memset(A.down_ns, 0, sizeof(A.down_ns));
    memset(A.down_frame, 0, sizeof(A.down_frame));
    pthread_mutex_unlock(&A.q_lock);
    pthread_mutex_lock(&E2.lk);
    E2.ready = false; E2.on = false; E2.opaque = false; E2.tex = 0; E2.seq = 0; E2.drawn_seq = 0;
    pthread_mutex_unlock(&E2.lk);
    g_render_thread = (pthread_t)0;
    A.gl_thread_started = false; A.app_started = false;
    atomic_store(&A.app_done, false);
    atomic_store(&A.gl_done, false);
    A.gl_view = NULL; A.gl_renderer = NULL; A.content = NULL; A.gl_started = false;
    /* stores into a library's executable view (data sharing a page with its code, as Unity's does) go through the writable one */
    { void tl_codewrite_enable(void); tl_codewrite_enable(); }
    A.cfg = *cfg;
    snprintf(A.apk, sizeof(A.apk), "%s", cfg->apk_path);
    /* JIT-less mode: LuaJIT (LÖVE games) runs its interpreter, as the SDL driver already has it do */
    if (getenv("TL_NO_XMEM")) { void tl_luajit_interpreter_only(void); tl_luajit_interpreter_only(); }
    /* an app whose manifest needs OpenGL ES 3.1 or 3.2 is told the driver has it (ANGLE over Metal stops at 3.0; the shim writes
       its 310/320 es shaders down), as it would otherwise refuse to start */
    { int tl_manifest_gles(const char *); void tl_egl_es3_claim(int); int gv = tl_manifest_gles(cfg->apk_path);
      if (gv >= 0x30001) { tl_egl_es3_claim(gv & 0xffff); tl_log_line("egl: the manifest asks for OpenGL ES 3.%d: the driver is described so", gv & 0xffff); } }
    /* the data folder as its canonical path: apps check where a file lands with getCanonicalPath() against getFilesDir() or
       getExternalFilesDir() (Sims FreePlay's zip-slip guard refused all its game data), and on iOS the container is /var/...,
       which is /private/var/... once resolved */
    mkdir(cfg->data_dir, 0755);
    /* ...unless that path has a space, which an Android data path never has: Qt splits its library list on them (a Mac volume
       name has one) */
    { char canon[PATH_MAX]; snprintf(A.data, sizeof(A.data), "%s", realpath(cfg->data_dir, canon) && !strchr(canon, ' ') ? canon : cfg->data_dir); }
    snprintf(A.ext, sizeof(A.ext), "%s/sdcard", A.data);
    snprintf(A.fwres, sizeof(A.fwres), "%s", cfg->framework_res ? cfg->framework_res : "");
    for (int i = 0; i < 4; i++) A.insets[i] = cfg->insets[i];
    mkdir(A.data, 0755); mkdir(A.ext, 0755);
    if (!tl_javaapp_manifest(cfg->apk_path, A.pkg, sizeof(A.pkg), A.activity, sizeof(A.activity), A.application, sizeof(A.application))) {
        /* a feature split can declare the launcher (Chromium's: the "chrome" split has the Main alias) */
        const char *tl_ld_queued_split(int i);
        bool got = false;
        for (int i = 0; !got && tl_ld_queued_split(i); i++) {
            char spkg[260] = "";
            got = tl_javaapp_manifest(tl_ld_queued_split(i), spkg, sizeof(spkg), A.activity, sizeof(A.activity), NULL, 0);
            if (got && !A.pkg[0]) snprintf(A.pkg, sizeof(A.pkg), "%s", spkg);
        }
        if (!got) { tl_log_line("javaapp: the manifest names no launcher activity"); goto fail; }
    }
    if (cfg->package_name && cfg->package_name[0]) snprintf(A.pkg, sizeof(A.pkg), "%s", cfg->package_name);
    husk_aosp_set_session(A.pkg, cfg->width, cfg->height);
    tl_log_line("javaapp: %s, launcher %s, application %s", A.pkg, A.activity, A.application[0] ? A.application : "(none)");
    tl_nwindow_configure(cfg->width, cfg->height, cfg->metal_layer);
    if (cfg->angle_egl && !tl_egl_init(cfg->angle_egl, cfg->angle_gles, cfg->frame_dir, cfg->frame_every)) goto fail;
    { void tl_set_data_dir(const char *dir); tl_set_data_dir(A.data); }
    tl_set_guest_package(A.pkg);
    if (tl_vhtg_quest_start(cfg->apk_path,A.pkg,cfg->quest_only_cutout,cfg->insets,cfg->width,cfg->height))
        for (int i=0;i<4;i++) A.insets[i]=0;
    (void)tl_vhpt_hud_start(cfg->apk_path, A.pkg, cfg->quest_only_cutout, cfg->insets, cfg->width, cfg->height);
    /* the environment every Android process starts with (TrustedCertificateStore builds paths from ANDROID_DATA) */
    setenv("ANDROID_DATA", "/data", 0); setenv("ANDROID_ROOT", "/system", 0); setenv("ANDROID_STORAGE", "/storage", 0);
    setenv("EXTERNAL_STORAGE", "/sdcard", 0); setenv("ANDROID_ASSETS", "/system/app", 0); setenv("BOOTCLASSPATH", "", 0);
    if (!tl_ld_add_apk(cfg->apk_path)) goto fail;
    if (!tl_dvm_add_apk(cfg->apk_path)) goto fail;
    { char lib[1100]; snprintf(lib, sizeof(lib), "%s/lib", A.data); mkdir(lib, 0755); tl_ld_apk_libs(stub_lib, lib); }
    atomic_store(&g_compositor_active, true);
    { pthread_attr_t rt; pthread_attr_init(&rt); pthread_attr_setstacksize(&rt, 64u << 20); A.gl_thread_started = tl_guest_pthread_create(&A.gl_thread, &rt, gl_main, NULL) == 0; pthread_attr_destroy(&rt); }
    if (!A.gl_thread_started) atomic_store(&g_compositor_active, false);
    pthread_attr_t at; pthread_attr_init(&at); pthread_attr_setstacksize(&at, 256u << 20);
    A.app_started = tl_guest_pthread_create(&A.app_thread, &at, app_main, NULL) == 0;
    pthread_attr_destroy(&at);
    if (!A.app_started) goto fail;
    return true;

fail:
    atomic_store(&A.ended, true);
    atomic_store(&g_compositor_active, false);
    pthread_mutex_lock(&A.q_lock);
    A.q_woken = true;
    pthread_cond_broadcast(&A.q_cv);
    pthread_mutex_unlock(&A.q_lock);
    tl_javaapp_wake_gl();
    tl_guest_session_abort();
    (void)tl_guest_session_reap(2000);
    if (A.gl_thread_started) { tl_guest_pthread_join(A.gl_thread, NULL); A.gl_thread_started = false; }
    if (tl_dvm_running()) tl_dvm_drop_app();
    javaapp_reset_subsystems();
    tl_bionic_reset_session();
    tl_ld_reset_session();
    (void)tl_guest_session_finish();
    return false;
}

static void *framework_reset_worker(void *arg)
{
    bool *ok_out = arg;
    jvalue r;
    tl_jni_clear();
    bool called = tl_dvm_call_static("husk/AppRunner", "resetSession", "()V", NULL, &r);
    if (!called || tl_jni_pending()) {
        char buf[800];
        tl_log_line("javaapp: framework reset threw %s", tl_dvm_describe_pending(buf, sizeof(buf)) ? buf : "?");
        log_pending_stack();
        tl_jni_clear();
        *ok_out = false;
    } else {
        *ok_out = true;
    }
    return NULL;
}

static bool call_framework_reset_session(void)
{
    if (!tl_dvm_defines_class("husk/AppRunner")) return true;
    bool reset_ok = false;
    pthread_attr_t ra;
    pthread_t rt;
    bool launched = false;
    if (pthread_attr_init(&ra) == 0) {
        (void)pthread_attr_setstacksize(&ra, 4u << 20);
        if (pthread_create(&rt, &ra, framework_reset_worker, &reset_ok) == 0) {
            pthread_join(rt, NULL);
            launched = true;
        }
        pthread_attr_destroy(&ra);
    }
    if (!launched) (void)framework_reset_worker(&reset_ok);
    return reset_ok;
}

bool tl_javaapp_try_stop(unsigned timeout_ms)
{
    void dvm_monitor_wake_all(void);
    void tl_net_shutdown_all(void);
    void tl_futex_wake_all(void);
    void tl_opensles_begin_close(void);
    void tl_loop_begin_close(void);

    int64_t start = now_ns();
    int64_t deadline = start + (int64_t)timeout_ms * 1000000ll;
    int64_t loop_coop = start + (int64_t)(timeout_ms > 500 ? 500 : timeout_ms / 2) * 1000000ll;
    /* Leaving the view paused writers. They must be able to drain while the
       Activity tears down SDL/AAudio, or its onDestroy joins them forever. */
    tl_audio_begin_close();
    tl_opensles_begin_close();
    tl_loop_begin_close();
    atomic_store(&A.ended, true);
    atomic_store(&g_compositor_active, false);
    pthread_mutex_lock(&A.q_lock);
    A.q_woken = true;
    pthread_cond_broadcast(&A.q_cv);
    pthread_mutex_unlock(&A.q_lock);
    pthread_mutex_lock(&E2.lk);
    pthread_cond_broadcast(&E2.cv);
    pthread_mutex_unlock(&E2.lk);
    tl_javaapp_wake_gl();

    bool close_sent = false;
    bool abort_sent = false;
    while ((A.app_started && !atomic_load(&A.app_done)) ||
           (A.gl_thread_started && !atomic_load(&A.gl_done))) {
        int64_t t = now_ns();
        if (!close_sent && t >= loop_coop) {
            close_sent = true;
            tl_guest_session_close();
            tl_loop_begin_close();
            tl_opensles_begin_close();
            dvm_monitor_wake_all();
            tl_net_shutdown_all();
            tl_futex_wake_all();
        }
        if (!abort_sent && close_sent && t >= loop_coop + 350000000ll) {
            abort_sent = true;
            tl_guest_session_abort();
            tl_loop_begin_close();
            tl_opensles_begin_close();
            dvm_monitor_wake_all();
            tl_net_shutdown_all();
            tl_futex_wake_all();
        }
        if (t >= deadline) {
            tl_log_line("javaapp: close timed out waiting for the Activity/render loop; session retained");
            return false;
        }
        usleep(10000);
    }

    if (!close_sent) {
        tl_guest_session_close();
        tl_loop_begin_close();
        tl_opensles_begin_close();
        dvm_monitor_wake_all();
        tl_net_shutdown_all();
        tl_futex_wake_all();
    }
    int64_t coop_end = now_ns() + 350000000ll;
    if (coop_end > deadline) coop_end = deadline;
    int wake_tick = 0;
    while (tl_guest_session_threads()) {
        int64_t t = now_ns();
        if (!abort_sent && t >= coop_end) {
            abort_sent = true;
            tl_log_line("javaapp: retiring %u lingering guest thread(s)", tl_guest_session_threads());
            tl_guest_session_abort();
            tl_loop_begin_close();
            tl_opensles_begin_close();
            dvm_monitor_wake_all();
            tl_net_shutdown_all();
            tl_futex_wake_all();
        }
        if (t >= deadline) {
            tl_log_line("javaapp: close waiting for %u guest threads; session retained", tl_guest_session_threads());
            tl_guest_session_log_live();
            return false;
        }
        if ((++wake_tick % 10) == 0) {
            tl_loop_begin_close();
            tl_opensles_begin_close();
            dvm_monitor_wake_all();
            tl_net_shutdown_all();
            tl_futex_wake_all();
        }
        usleep(10000);
    }
    int64_t remaining_ns = deadline - now_ns();
    unsigned remaining_ms = remaining_ns > 0 ? (unsigned)(remaining_ns / 1000000) : 0;
    if (!tl_guest_session_reap(remaining_ms)) {
        tl_log_line("javaapp: could not join every guest thread; session retained");
        return false;
    }
    if (!call_framework_reset_session()) {
        tl_log_line("javaapp: framework reset failed; session retained");
        return false;
    }
    A.app_started = false;
    A.gl_thread_started = false;
    /* All contexts are now unbound on their owning threads. Destroy only the
       compositor's own objects here; guest objects follow in reset_subsystems. */
    unsigned (*destroy_surface)(EGLDisplay, EGLSurface) = tl_egl_resolve("eglDestroySurface");
    unsigned (*destroy_context)(EGLDisplay, EGLContext) = tl_egl_resolve("eglDestroyContext");
    if (destroy_surface && E2.screen) destroy_surface(E2.dpy, E2.screen);
    if (destroy_context && E2.renderer) destroy_context(E2.dpy, E2.renderer);
    if (destroy_context && E2.ui) destroy_context(E2.dpy, E2.ui);
    E2.screen = NULL; E2.renderer = NULL; E2.ui = NULL;
    free(E2.px); E2.px = NULL; E2.px_capacity = 0;
    pthread_mutex_lock(&A.t_lock);
    for (int i = 0; i < A.ntext; i++) free(A.text[i]);
    free(A.text); A.text = NULL; A.ntext = 0; A.ctext = 0;
    pthread_mutex_unlock(&A.t_lock);
    g_render_thread = (pthread_t)0;
    A.gl_started = false;
    A.gl_view = NULL; A.gl_renderer = NULL; A.content = NULL;
    atomic_store(&A.defer_until_ns, 0);
    pthread_mutex_lock(&A.q_lock); A.qn = 0; A.q_woken = false; pthread_mutex_unlock(&A.q_lock);
    pthread_mutex_lock(&E2.lk); E2.ready = false; E2.on = false; E2.tex = 0; pthread_mutex_unlock(&E2.lk);
    tl_jni_clear();
    javaapp_reset_subsystems();
    atomic_store(&A.paused, false);
    atomic_store(&A.frames, 0);
    atomic_store(&A.perf_last_ns, 0);
    atomic_store(&A.perf_since_ns, 0);
    atomic_store(&A.perf_total_ns, 0);
    atomic_store(&A.perf_max_ns, 0);
    atomic_store(&A.perf_frames, 0);
    atomic_store(&A.perf_over50, 0);
    atomic_store(&A.perf_over100, 0);
    return tl_guest_session_finish();
}
void tl_javaapp_stop(void) { (void)tl_javaapp_try_stop(8000); }

void tl_javaapp_perf_snapshot(double *fps, double *mean_ms, double *max_ms,
                              unsigned long *over50, unsigned long *over100)
{
    uint64_t now_u = (uint64_t)now_ns();
    uint64_t since = atomic_exchange(&A.perf_since_ns, now_u);
    uint64_t total_ns = atomic_exchange(&A.perf_total_ns, 0);
    uint64_t max_ns = atomic_exchange(&A.perf_max_ns, 0);
    unsigned long n = atomic_exchange(&A.perf_frames, 0);
    unsigned long h50 = atomic_exchange(&A.perf_over50, 0);
    unsigned long h100 = atomic_exchange(&A.perf_over100, 0);
    double elapsed = (since > 0 && now_u > since) ? (double)(now_u - since) / 1e9 : 0.0;
    if (fps) *fps = elapsed > 0.05 ? (double)n / elapsed : 0.0;
    if (mean_ms) *mean_ms = n ? (double)total_ns / (double)n / 1e6 : 0.0;
    if (max_ms) *max_ms = (double)max_ns / 1e6;
    if (over50) *over50 = h50;
    if (over100) *over100 = h100;
}

void tl_javaapp_touch(int phase, int id, float x, float y)
{
    union { float f; int32_t i; } fx, fy; fx.f = x; fy.f = y;
    pthread_mutex_lock(&A.q_lock);
    int before = A.qn;
    tl_input_queue_push(A.q, &A.qn, 4096, phase, id, fx.i, fy.i);
    if (A.qn != before || phase == 1) pthread_cond_broadcast(&A.q_cv);
    pthread_mutex_unlock(&A.q_lock);
}
void tl_javaapp_key(int code, bool down) { tl_javaapp_touch(4, code, down ? 1e-45f : 0.0f, 0); }
void tl_javaapp_back(void) { tl_javaapp_touch(5, 0, 0, 0); }
void tl_javaapp_set_paused(bool p)
{
    if (atomic_exchange(&A.paused, p) != p) {
        tl_javaapp_touch(6, p ? 1 : 0, 0, 0);     /* the activity pauses and resumes with the host */
        tl_javaapp_wake_gl();
    }
}
unsigned long tl_javaapp_frames(void) { return atomic_load(&A.frames); }
bool tl_javaapp_ended(void) { return atomic_load(&A.ended); }
