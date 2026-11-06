/* SPDX-License-Identifier: GPL-2.0-or-later */
/* VHTG's selected HUD layout. No game assets or executable instructions are rewritten.
 * The object layout below is used ONLY for the SHA-256-matched arm64 library. */
#include "husk-tl-vhtg-quest.h"
#include "husk-tl-quest-layout.h"
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
                double down=0;
                if (Q.scene.w>Q.scene.h && (cut.left>0 || cut.right>0)) {
                    down=tl_quest_movement_down(Q.scene,layout[i].rect);
                    const uint8_t *bytes=Q.slots[i].window;
                    if (layout[i].parent && (!Q.slots[i].anchored || !bytes[0x469])) {
                        double room=read_int(layout[i].parent,0x438)-layout[i].rect.y-layout[i].rect.h;
                        down=fmin(down,floor(fmax(0,room)));
                    }
                }
                apply_window(i,&layout[i],0,down);
            }
        }
    }
    Q.render();
}

static void *quest_plt(tl_lib *lib, const char *symbol, void *original)
{
    if (!atomic_load(&q_enabled)) return original;
    bool anchor=!strcmp(symbol,"KT_SetUiAnchorSafeArea"), render=!strcmp(symbol,"_Z17Wnd_RenderWindowsv");
    if (!anchor && !render) return original;
    const char *name=tl_ld_lib_name(lib); const char *leaf=name?strrchr(name,'/'):NULL;
    if (!name || strcmp(leaf?leaf+1:name,"libMyGame.so")) return original;
    if ((uintptr_t)original != (uintptr_t)tl_ld_lib_base(lib)+(anchor?0x7cd9d0:0x7cdbdc)) return original;
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
bool tl_vhtg_quest_start(const char *apk,const char *package,bool requested,const int insets[4],int pixel_width,int pixel_height)
{
    atomic_store(&q_enabled,false); memset(&Q,0,sizeof(Q));
    if (!requested || !package || strcmp(package,"review.com.game3c.vhtgmobile") ||
        !insets || pixel_width<=0 || pixel_height<=0) return false;
    tl_zip zip; char error[160];
    if (!tl_zip_open(&zip,apk,error,sizeof(error))) return false;
    const tl_zip_entry *entry=tl_zip_find(&zip,"lib/arm64-v8a/libMyGame.so");
    const uint8_t *data=NULL; size_t size=0; bool owned=false;
    bool ok=entry && entry->usize==14643976 && tl_zip_data(&zip,entry,16u<<20,&data,&size,&owned,error,sizeof(error));
    uint8_t hash[32]; char hex[65];
    if (ok) {
        CC_SHA256(data,(CC_LONG)size,hash);
        for (unsigned i=0;i<32;i++) snprintf(hex+i*2,3,"%02x",hash[i]);
        ok=!strcmp(hex,"0c468ae6407377f61d33f1bbe77928b1d5c32cdede98ab163b86a897f0b60dcb");
    }
    if (owned) free((void *)data);
    tl_zip_close(&zip);
    if (!ok) { tl_log_line("vhtg-quest: unsupported library; quest-only adjustment disabled"); return false; }
    Q.pixel_w=pixel_width; Q.pixel_h=pixel_height; Q.reports=48;
    tl_vhtg_quest_insets(insets[0],insets[1],insets[2],insets[3]);
    atomic_store(&q_enabled,true);
    tl_ld_set_plt_filter(quest_plt);
    tl_log_line("vhtg-quest: verified library; quest avoidance and separate walk/sit adjustment");
    return true;
}
