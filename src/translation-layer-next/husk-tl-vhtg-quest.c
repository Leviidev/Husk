/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VHTG's selected HUD layout. No game assets or executable instructions are rewritten.
 * The object layout below is used ONLY for the SHA-256-matched arm64 library. */
#include "husk-tl-vhtg-quest.h"
#include "husk-tl-quest-layout.h"
#include "husk-tl-vhpt-layout.h"
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-bionic.h"
#include <CommonCrypto/CommonDigest.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t size;
    const char *hash;
    uintptr_t anchor_offset;
    uintptr_t render_offset;
} vhtg_profile;

static const vhtg_profile vhtg_profiles[] = {
    { 14643976, "0c468ae6407377f61d33f1bbe77928b1d5c32cdede98ab163b86a897f0b60dcb", 0x7cd9d0, 0x7cdbdc },
    { 14882616, "8cc2b4e59fd6087d726445526eb04fc39f1a7262be383fb52ef78533e95ab780", 0x7e2abc, 0x7e2cc8 },
};

typedef void (*anchor_fn)(double,double,double,double,double,double,double,double);
typedef struct {
    void *window;
    double base_x, base_y, applied_x, applied_y;
    bool anchored, missing_reported;
} quest_slot;
enum { QUEST_TARGETS=6, HUD_TARGETS=8 };
static const struct { const char *group, *name; } targets[HUD_TARGETS] = {
    {"UI_TASKTRACK","Main"}, {"UI_TASKTRACKTMP","Main"},
    {"UI_TABMISSIONTEAM","BtnTab1"}, {"UI_TABMISSIONTEAM","BtnTab2"},
    {"UI_TABMISSIONTEAM","BtnTab3"}, {"UI_TABMISSIONTEAM","BtnReportBattle"},
    {"UI_POPBAR","BtnSit"}, {"UI_POPBAR","BtnRun"}
};
typedef struct {
    tl_quest_rect rect;
    void *parent;
    double scale;
    bool new_window;
} window_layout;
static struct {
    tl_lib *lib;
    anchor_fn original;
    void (*render)(void);
    uintptr_t anchor_offset, render_offset;
    void *manager;
    void *(*get_group)(void *, const char *);
    void *(*get_window)(void *, const char *);
    void (*set_position)(void *, int, int);
    bool resolved, geometry_valid;
    double pixel_w, pixel_h;
    tl_quest_rect scene, area;
    unsigned reports;
    quest_slot slots[HUD_TARGETS];
} Q;
static pthread_mutex_t q_lock = PTHREAD_MUTEX_INITIALIZER;
static tl_quest_cutout q_cut;
static atomic_bool q_enabled;

static double read_double(const void *p, size_t offset) { double v; memcpy(&v,(const uint8_t *)p+offset,8); return v; }
static int read_int(const void *p, size_t offset) { int v; memcpy(&v,(const uint8_t *)p+offset,4); return v; }
static void write_double(void *p, size_t offset, double v) { memcpy((uint8_t *)p+offset,&v,8); }

static bool measure_window(unsigned index, tl_quest_rect anchor_area, window_layout *layout)
{
    const char *group=targets[index].group, *name=targets[index].name;
    // UI_* identifies a script group, not a native root window name. Main is
    // itself the group's root, so Wnd_FindWindow(group, name) cannot select it.
    void *ui = Q.get_group(Q.manager,group);
    void *w = ui ? Q.get_window(ui,name) : NULL;
    quest_slot *s = &Q.slots[index];
    if (!w) {
        if (!s->missing_reported && Q.reports) {
            --Q.reports; tl_log_line("vhtg-quest: waiting for %s/%s (%s missing)",group,name,ui?"window":"UI group");
        }
        *s=(quest_slot){.missing_reported=true}; return false;
    }
    const uint8_t *bytes = w;
    bool anchored = bytes[0x468] != 0;
    double current_x = anchored ? read_double(w,0x490) : read_int(w,0x28);
    double current_y = anchored ? read_double(w,0x498) : read_int(w,0x2c);
    if (!isfinite(current_x) || !isfinite(current_y) || fabs(current_x)>100000 || fabs(current_y)>100000) return false;
    bool new_window=s->window != w || s->anchored != anchored;
    if (new_window) {
        *s = (quest_slot){ .window=w, .base_x=current_x, .base_y=current_y,
                          .applied_x=current_x, .applied_y=current_y, .anchored=anchored };
    } else {
        // Updating one axis must not bake our adjustment on the other into its baseline.
        if (current_x != s->applied_x) s->base_x=s->applied_x=current_x;
        if (current_y != s->applied_y) s->base_y=s->applied_y=current_y;
    }
    tl_quest_rect rect = { .w=read_int(w,0x30), .h=read_int(w,0x34) };
    void *parent; memcpy(&parent,bytes+0x458,sizeof(parent));
    double scale = 1;
    if (anchored) {
        tl_quest_rect container = anchor_area;
        if (parent && !bytes[0x469])
            container = (tl_quest_rect){read_int(parent,0x40),read_int(parent,0x44),read_int(parent,0x30),read_int(parent,0x34)};
        // Native applyAnchorLayoutTree ignores parent scale for screen anchors.
        if (parent && !bytes[0x469] && read_int(w,0x4b0)) {
            float value; memcpy(&value,(uint8_t *)parent+0x4ac,4); scale=value;
        }
        if (!isfinite(scale) || scale<=0 || scale>64) return false;
        rect.x = tl_quest_anchor(container.x,container.w,read_double(w,0x470),rect.w,read_double(w,0x480),s->base_x,scale);
        rect.y = tl_quest_anchor(container.y,container.h,read_double(w,0x478),rect.h,read_double(w,0x488),s->base_y,scale);
    } else {
        rect.x=s->base_x+(parent?read_int(parent,0x40):0);
        rect.y=s->base_y+(parent?read_int(parent,0x44):0);
    }
    if (!isfinite(rect.x) || !isfinite(rect.y) || rect.w<=0 || rect.h<=0) return false;
    *layout=(window_layout){.rect=rect,.parent=parent,.scale=scale,.new_window=new_window};
    return true;
}

static void apply_window(unsigned index, const window_layout *layout, double dx, double dy)
{
    const char *group=targets[index].group, *name=targets[index].name;
    quest_slot *s=&Q.slots[index];
    void *w=s->window;
    double x=s->base_x+dx/layout->scale, y=s->base_y+dy/layout->scale;
    if (layout->new_window && Q.reports) {
        --Q.reports; tl_log_line("vhtg-quest: %s/%s baseline %.1f,%.1f %.1fx%.1f; delta %.1f,%.1f",
                                group,name,layout->rect.x,layout->rect.y,layout->rect.w,layout->rect.h,dx,dy);
    }
    if (x==s->applied_x && y==s->applied_y) return;
    if (s->anchored) { write_double(w,0x490,x); write_double(w,0x498,y); }
    else Q.set_position(w,(int)floor(x+0.5),(int)floor(y+0.5));
    s->applied_x=s->anchored?x:floor(x+0.5); s->applied_y=s->anchored?y:floor(y+0.5);
    if (Q.reports) { --Q.reports; tl_log_line("vhtg-quest: %s/%s offset %.1f,%.1f",group,name,dx,dy); }
}

static void quest_anchor(double fw,double fh,double sx,double sy,double sw,double sh,double dw,double dh)
{
    Q.original(fw,fh,sx,sy,sw,sh,dw,dh);
    if (!atomic_load(&q_enabled)) return;
    Q.geometry_valid=false;
    double values[]={fw,fh,sx,sy,sw,sh,dw,dh};
    for (unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++) if (!isfinite(values[i])) return;
    if (fw<=0 || fh<=0 || dw<=0 || dh<=0) return;
    double k=fmin(fw/dw,fh/dh);
    if (!isfinite(k) || k<=0) return;
    tl_quest_rect scene={(dw-fw/k)*0.5,(dh-fh/k)*0.5,fw/k,fh/k};
    // Same conversion as KT_SetUiAnchorSafeArea, retaining the game's own top/bottom margins.
    double x=fmax(0,sx),y=fmax(0,sy),r=fmin(fw,sx+sw),b=fmin(fh,sy+sh);
    tl_quest_rect area=scene;
    if (isfinite(x)&&isfinite(y)&&isfinite(r)&&isfinite(b)&&r>x&&b>y)
        area=(tl_quest_rect){scene.x+x/k,scene.y+y/k,(r-x)/k,(b-y)/k};
    Q.scene=scene; Q.area=area; Q.geometry_valid=true;
}

static void quest_render(void)
{
    // GameClientScene::update continues running scripts after setting the safe area.
    // Select the live windows here, immediately before native anchor layout/drawing.
    if (atomic_load(&q_enabled) && Q.geometry_valid) {
        if (!Q.resolved) {
            // g_UIManager is the object, not a pointer variable. Resolve it via
            // the loader to get the same writable data view as the game's GOT.
            Q.manager=tl_ld_sym(Q.lib,"g_UIManager");
            Q.get_group=(void *(*)(void *,const char *))tl_ld_sym(Q.lib,"_ZN9KUIManage10GetUIGroupEPKc");
            Q.get_window=(void *(*)(void *,const char *))tl_ld_sym(Q.lib,"_ZN8KUIGroup9GetWindowEPKc");
            Q.set_position=(void (*)(void *,int,int))tl_ld_sym(Q.lib,"_ZN10KWndWindow11SetPositionEii");
            Q.resolved=true;
            tl_log_line("vhtg-quest: UI registry pre-render layout; framebuffer %.0fx%.0f, scene %.1f,%.1f %.1fx%.1f; exports %s",
                        Q.pixel_w,Q.pixel_h,Q.scene.x,Q.scene.y,Q.scene.w,Q.scene.h,
                        Q.manager&&Q.get_group&&Q.get_window&&Q.set_position?"ready":"missing");
        }
        if (Q.manager && Q.get_group && Q.get_window && Q.set_position) {
            pthread_mutex_lock(&q_lock); tl_quest_cutout cut=q_cut; pthread_mutex_unlock(&q_lock);
            window_layout layout[HUD_TARGETS];
            bool found[HUD_TARGETS], have_bounds=false;
            tl_quest_rect bounds={0};
            for (unsigned i=0;i<HUD_TARGETS;i++) {
                found[i]=measure_window(i,Q.area,&layout[i]);
                if (!found[i] || i>=QUEST_TARGETS) continue;
                tl_quest_rect rect=layout[i].rect;
                if (!have_bounds) { bounds=rect; have_bounds=true; continue; }
                double right=fmax(bounds.x+bounds.w,rect.x+rect.w);
                double bottom=fmax(bounds.y+bounds.h,rect.y+rect.h);
                bounds.x=fmin(bounds.x,rect.x); bounds.y=fmin(bounds.y,rect.y);
                bounds.w=right-bounds.x; bounds.h=bottom-bounds.y;
            }
            // The quest text and its screen-anchored Mission/Team tabs are separate
            // groups. Translate them together so the tabs clear the island and do
            // not overlap the text. Moving their unanchored Main would not move
            // these screen-anchored buttons.
            double dx=0,dy=0;
            if (have_bounds && tl_quest_delta(Q.scene,bounds,cut,Q.pixel_w,Q.pixel_h,&dx,&dy))
                for (unsigned i=0;i<QUEST_TARGETS;i++) if (found[i]) apply_window(i,&layout[i],dx,dy);
            for (unsigned i=QUEST_TARGETS;i<HUD_TARGETS;i++) if (found[i]) {
                double down=0, dx=0;
                if (Q.scene.w>Q.scene.h && (cut.left>0 || cut.right>0)) {
                    down=tl_quest_movement_down(Q.scene,layout[i].rect);
                    const uint8_t *bytes=Q.slots[i].window;
                    if (layout[i].parent && (!Q.slots[i].anchored || !bytes[0x469])) {
                        double room=read_int(layout[i].parent,0x438)-layout[i].rect.y-layout[i].rect.h;
                        down=fmin(down,floor(fmax(0,room)));
                    }
                    double inward = floor(fmin(20.0, fmax(12.0, Q.scene.w * (18.0 / 932.0))));
                    double dir = (layout[i].rect.x < Q.scene.x + Q.scene.w * 0.5) ? 1.0 : -1.0;
                    dx = dir * inward;
                }
                apply_window(i,&layout[i],dx,down);
            }
        }
    }
    Q.render();
}

typedef struct {
    tl_lib *lib;
    bool enabled;
    bool rodata_ok;
    bool rodata_checked;
    int pixel_w, pixel_h;
    void (*orig_gl_clear)(unsigned int mask);
    void *last_scene;
    uint32_t last_gen;
    int last_safe_l;
    struct {
        void *root;
        float applied_x;
        bool logged;
    } slots[12];
} vhpt_hud_state;

static atomic_bool v_enabled;
static vhpt_hud_state V;

static uintptr_t vhpt_read_uptr(const void *p)
{
    uintptr_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static int32_t vhpt_read_i32(const void *p)
{
    int32_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static uint32_t vhpt_read_u32(const void *p)
{
    uint32_t v = 0;
    memcpy(&v, p, sizeof(v));
    return v;
}

static float vhpt_read_f32(const void *p)
{
    float v = 0.0f;
    memcpy(&v, p, sizeof(v));
    return v;
}

static const char *vhpt_read_std_string(const uint8_t *str_obj, char *scratch, size_t cap)
{
    if (!str_obj || (uintptr_t)str_obj < 0x10000ULL) return "";
    uint8_t tag = str_obj[0];
    if ((tag & 1u) == 0u) {
        size_t len = (size_t)(tag >> 1);
        if (len >= cap) len = cap - 1;
        memcpy(scratch, str_obj + 1, len);
        scratch[len] = '\0';
        return scratch;
    }
    const char *ptr = (const char *)vhpt_read_uptr(str_obj + 16);
    if (!ptr || (uintptr_t)ptr < 0x10000ULL) return "";
    return ptr;
}

static bool vhpt_verify_rodata(const uint8_t *base)
{
    if (!base) return false;
    if (V.rodata_checked) return V.rodata_ok;
    V.rodata_checked = true;
    V.rodata_ok =
        !memcmp(base + VHPT_STR_ANCHOR_OFF, "Anchor", 7) &&
        !memcmp(base + VHPT_STR_TOPLEFT_OFF, "TopLeft", 8) &&
        !memcmp(base + VHPT_STR_CHARINFO_OFF, "ui_mobile/charinfo.ini", 23);
    if (!V.rodata_ok) {
        tl_log_line("vhpt-hud: runtime rodata signature mismatch; disabling HUD realignment");
    }
    return V.rodata_ok;
}

static bool vhpt_node_y_span(const uint8_t *root, float root_y, float root_scale, float *min_y, float *max_y)
{
    if (!root || root_scale <= 0.0f) return false;
    uintptr_t begin = vhpt_read_uptr(root + VHPT_NODE_CHILDREN_VEC);
    uintptr_t end = vhpt_read_uptr(root + VHPT_NODE_CHILDREN_VEC + sizeof(uintptr_t));
    if (!begin || end <= begin || begin < 0x10000ULL) return false;
    size_t count = (end - begin) / sizeof(uintptr_t);
    if (count == 0 || count > 256) return false;
    const uint8_t *children_raw = (const uint8_t *)begin;
    float lo = 1e9f, hi = -1e9f;
    bool any = false;
    for (size_t i = 0; i < count; i++) {
        const uint8_t *ch = (const uint8_t *)vhpt_read_uptr(children_raw + i * sizeof(uintptr_t));
        if (!ch || (uintptr_t)ch < 0x10000ULL) continue;
        float cy = vhpt_read_f32(ch + VHPT_NODE_POSY_OFF);
        if (isnan(cy) || cy == 0.0f) continue;
        float sy = root_y + cy * root_scale;
        if (sy < lo) lo = sy;
        if (sy > hi) hi = sy;
        any = true;
    }
    if (!any || hi <= lo) return false;
    *min_y = lo;
    *max_y = hi;
    return true;
}

static void vhpt_align_hud(void)
{
    if (!atomic_load(&v_enabled)) return;
    pthread_mutex_lock(&q_lock);
    tl_lib *lib = V.lib ? V.lib : tl_ld_find_lib("libMyGame.so");
    if (!lib) { pthread_mutex_unlock(&q_lock); return; }
    V.lib = lib;
    const uint8_t *base = (const uint8_t *)tl_ld_lib_base(lib);
    if (!vhpt_verify_rodata(base)) {
        atomic_store(&v_enabled, false);
        pthread_mutex_unlock(&q_lock);
        return;
    }
    uintptr_t scene = vhpt_read_uptr(base + VHPT_BSS_GAMESCENE_OFF);
    if (!scene || scene < 0x10000ULL) {
        pthread_mutex_unlock(&q_lock);
        return;
    }
    int safe_l = vhpt_read_i32(base + VHPT_BSS_SAFE_L_OFF);
    uint32_t safe_gen = vhpt_read_u32(base + VHPT_BSS_SAFE_GEN_OFF);
    if (safe_l <= 0) safe_l = q_cut.left;
    float vis_h = (float)(V.pixel_h > 0 ? V.pixel_h : q_cut.top + q_cut.bottom);
    if (vis_h <= 0.0f) { pthread_mutex_unlock(&q_lock); return; }

    if ((void *)scene != V.last_scene || safe_gen != V.last_gen || safe_l != V.last_safe_l) {
        V.last_scene = (void *)scene;
        V.last_gen = safe_gen;
        V.last_safe_l = safe_l;
        memset(V.slots, 0, sizeof(V.slots));
    }

    size_t nslots = 0;
    const tl_vhpt_panel_slot *slots = tl_vhpt_panel_slots(&nslots);
    for (size_t i = 0; i < nslots && i < sizeof(V.slots) / sizeof(V.slots[0]); i++) {
        uintptr_t panel = vhpt_read_uptr((const uint8_t *)scene + slots[i].scene_off);
        if (!panel || panel < 0x10000ULL) continue;
        uintptr_t ini = vhpt_read_uptr((const uint8_t *)panel + slots[i].ini_off);
        void *m_root = (void *)vhpt_read_uptr((const uint8_t *)panel + slots[i].root_off);
        if (!ini || ini < 0x10000ULL || !m_root || (uintptr_t)m_root < 0x10000ULL) continue;

        char anchor_buf[64];
        const char *anchor = vhpt_read_std_string((const uint8_t *)(ini + VHPT_INI_ANCHOR_STR_OFF), anchor_buf, sizeof(anchor_buf));
        float cur_x = vhpt_read_f32((const uint8_t *)m_root + VHPT_NODE_POSX_OFF);
        float cur_y = vhpt_read_f32((const uint8_t *)m_root + VHPT_NODE_POSY_OFF);
        float scale = vhpt_read_f32((const uint8_t *)m_root + VHPT_NODE_SCALEX_OFF);
        float min_y = 0.0f, max_y = 0.0f;
        bool has_y = vhpt_node_y_span((const uint8_t *)m_root, cur_y, scale, &min_y, &max_y);

        if (slots[i].keep_unchanged) {
            if (!V.slots[i].logged || V.slots[i].root != m_root) {
                V.slots[i].root = m_root;
                V.slots[i].logged = true;
                tl_log_line("vhpt-hud: %s anchor=%s pos=(%.1f,%.1f) scale=%.3f unchanged (Nhiệm vụ)",
                            slots[i].name, anchor[0] ? anchor : "TopLeft", cur_x, cur_y, scale);
            }
            continue;
        }

        if (V.slots[i].root == m_root && fabsf(cur_x - V.slots[i].applied_x) < 0.5f) continue;

        float target_x = tl_vhpt_panel_target_x(false, anchor, cur_x, (float)safe_l, vis_h, has_y, min_y, max_y);
        if (fabsf(target_x - cur_x) >= 0.5f) {
            void *const *vtable = (void *const *)vhpt_read_uptr(m_root);
            if (vtable && (uintptr_t)vtable >= 0x10000ULL) {
                typedef void (*set_pos_fn)(void *, const float *);
                set_pos_fn set_pos = (set_pos_fn)vhpt_read_uptr((const uint8_t *)vtable + VHPT_NODE_VTBL_SETPOS);
                if (set_pos) {
                    float pos[2] = { target_x, cur_y };
                    set_pos(m_root, pos);
                }
            }
        }
        V.slots[i].root = m_root;
        V.slots[i].applied_x = target_x;
        if (!V.slots[i].logged) {
            V.slots[i].logged = true;
            tl_log_line("vhpt-hud: %s anchor=%s pos=(%.1f,%.1f)->(%.1f,%.1f) safeL=%d scale=%.3f",
                        slots[i].name, anchor[0] ? anchor : "TopLeft",
                        cur_x, cur_y, target_x, cur_y, safe_l, scale);
        }
    }
    pthread_mutex_unlock(&q_lock);
}

static void vhpt_gl_clear(unsigned int mask)
{
    vhpt_align_hud();
    if (V.orig_gl_clear) V.orig_gl_clear(mask);
}

static bool verify_mygame_profile(const char *apk, size_t expected_size, const char *expected_hash)
{
    tl_zip zip;
    char error[160];
    if (!tl_zip_open(&zip, apk, error, sizeof(error))) return false;
    const tl_zip_entry *entry = tl_zip_find(&zip, "lib/arm64-v8a/libMyGame.so");
    bool ok = entry && entry->usize == expected_size;
    const uint8_t *data = NULL;
    size_t size = 0;
    bool owned = false;
    if (ok) ok = tl_zip_data(&zip, entry, 24u << 20, &data, &size, &owned, error, sizeof(error));
    if (ok && size > 0) {
        uint8_t hash[32];
        char hex[65];
        CC_SHA256(data, (CC_LONG)size, hash);
        for (unsigned i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", hash[i]);
        ok = !strcmp(hex, expected_hash);
    } else {
        ok = false;
    }
    if (owned) free((void *)data);
    tl_zip_close(&zip);
    return ok;
}

static void *quest_plt(tl_lib *lib, const char *symbol, void *original)
{
    if (atomic_load(&v_enabled) && !strcmp(symbol, "glClear")) {
        const char *name = tl_ld_lib_name(lib);
        const char *leaf = name ? strrchr(name, '/') : NULL;
        if (name && !strcmp(leaf ? leaf + 1 : name, "libMyGame.so")) {
            V.lib = lib;
            V.orig_gl_clear = (void (*)(unsigned int))original;
            tl_log_line("vhpt-hud: bound glClear");
            return (void *)vhpt_gl_clear;
        }
    }
    if (!atomic_load(&q_enabled)) return original;
    bool anchor=!strcmp(symbol,"KT_SetUiAnchorSafeArea"), render=!strcmp(symbol,"_Z17Wnd_RenderWindowsv");
    if (!anchor && !render) return original;
    const char *name=tl_ld_lib_name(lib); const char *leaf=name?strrchr(name,'/'):NULL;
    if (!name || strcmp(leaf?leaf+1:name,"libMyGame.so")) return original;
    if ((uintptr_t)original != (uintptr_t)tl_ld_lib_base(lib)+(anchor?Q.anchor_offset:Q.render_offset)) return original;
    Q.lib=lib;
    tl_log_line("vhtg-quest: bound %s",symbol);
    if (anchor) { Q.original=(anchor_fn)original; return (void *)quest_anchor; }
    Q.render=(void (*)(void))original; return (void *)quest_render;
}

bool tl_vhtg_quest_active(void) { return atomic_load(&q_enabled); }
void tl_vhtg_quest_insets(int l,int t,int r,int b)
{
    pthread_mutex_lock(&q_lock); q_cut=(tl_quest_cutout){l,t,r,b}; pthread_mutex_unlock(&q_lock);
}
void tl_vhtg_quest_reset(void)
{
    atomic_store(&q_enabled,false);
    atomic_store(&v_enabled,false);
    pthread_mutex_lock(&q_lock);
    memset(&Q,0,sizeof(Q));
    memset(&V,0,sizeof(V));
    memset(&q_cut,0,sizeof(q_cut));
    pthread_mutex_unlock(&q_lock);
}
bool tl_vhpt_hud_start(const char *apk,const char *package,bool requested,const int insets[4],int pixel_width,int pixel_height)
{
    atomic_store(&v_enabled,false);
    pthread_mutex_lock(&q_lock); memset(&V,0,sizeof(V)); pthread_mutex_unlock(&q_lock);
    if (!requested || !package || strcmp(package,"com.tech3cgame.vhpt1") ||
        !insets || pixel_width<=0 || pixel_height<=0) return false;
    if (!verify_mygame_profile(apk, VHPT_LIB_SIZE,
            "c04629c87573f99234f54e768e121c09b0ddcef3ac531e1facc82738ea967b44")) {
        tl_log_line("vhpt-hud: unsupported library; Vo Hiep HUD alignment disabled");
        return false;
    }
    V.pixel_w = pixel_width;
    V.pixel_h = pixel_height;
    V.enabled = true;
    tl_vhtg_quest_insets(insets[0],insets[1],insets[2],insets[3]);
    atomic_store(&v_enabled,true);
    tl_ld_set_plt_filter(quest_plt);
    tl_log_line("vhpt-hud: verified library (%u bytes); aligning Character & below-left HUD like Vo Hiep (Quest unchanged)", VHPT_LIB_SIZE);
    return true;
}
bool tl_vhtg_quest_start(const char *apk,const char *package,bool requested,const int insets[4],int pixel_width,int pixel_height)
{
    atomic_store(&q_enabled,false); memset(&Q,0,sizeof(Q));
    if (!requested || !package ||
        (strcmp(package,"review.com.game3c.vhtgmobile") && strcmp(package,"com.game3c.vhtgmobile")) ||
        !insets || pixel_width<=0 || pixel_height<=0) return false;
    const vhtg_profile *matched = NULL;
    for (size_t i = 0; i < sizeof(vhtg_profiles)/sizeof(vhtg_profiles[0]); i++) {
        if (verify_mygame_profile(apk, vhtg_profiles[i].size, vhtg_profiles[i].hash)) {
            matched = &vhtg_profiles[i];
            break;
        }
    }
    if (!matched) { tl_log_line("vhtg-quest: unsupported library; quest-only adjustment disabled"); return false; }
    Q.anchor_offset = matched->anchor_offset;
    Q.render_offset = matched->render_offset;
    Q.pixel_w=pixel_width; Q.pixel_h=pixel_height; Q.reports=48;
    tl_vhtg_quest_insets(insets[0],insets[1],insets[2],insets[3]);
    atomic_store(&q_enabled,true);
    tl_ld_set_plt_filter(quest_plt);
    tl_log_line("vhtg-quest: verified library (%zu bytes); quest avoidance and separate walk/sit adjustment", matched->size);
    return true;
}
