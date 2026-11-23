/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-vhtg-quest.h"
#include "husk-tl-vhtg-elf.h"
#include "husk-tl-quest-layout.h"
#include "husk-tl-vhpt-layout.h"
#include <CommonCrypto/CommonDigest.h>

static unsigned vhpt_clears;
static void fake_gl_clear(unsigned int mask) { (void)mask; vhpt_clears++; }
static void fake_setpos(void *node, const float *xy)
{
    *(float *)((uint8_t *)node + VHPT_NODE_POSX_OFF) = xy[0];
    *(float *)((uint8_t *)node + VHPT_NODE_POSY_OFF) = xy[1];
}

static unsigned char quest[0x4b8], avatar[0x4b8], age_logo[0x4b8], movement[0x4b8];
static unsigned char sit[0x4b8], run_button[0x4b8], quest_tmp[0x4b8];
static unsigned char mission_tabs[4][0x4b8];
static const char *tab_names[]={"BtnTab1","BtnTab2","BtnTab3","BtnReportBattle"};
static bool hash_match=true, provide_quest=true, provide_movement=false;
static bool provide_groups=true, provide_tabs=false, provide_tmp=false;
static const char *missing_export;
static unsigned char manager[48];
static struct { const char *name; } groups[]={{"UI_TASKTRACK"},{"UI_TASKTRACKTMP"},{"UI_POPBAR"},{"UI_TABMISSIONTEAM"}};
static unsigned calls, lookups, renders;
static uintptr_t fixture_base;
static const char *library="libMyGame.so";
static void *(*filter)(tl_lib *,const char *,void *);
static tl_zip_entry entry={.usize=14643976};
static const uint8_t fake_zip_data[1]={0};
static const uint8_t *override_zip_data;
static size_t override_zip_size;
static void put_double(void *p,unsigned at,double v) { memcpy((uint8_t *)p+at,&v,8); }
static double get_double(const void *p,unsigned at) { double v;memcpy(&v,(const uint8_t *)p+at,8);return v; }
static void put_int(void *p,unsigned at,int v) { memcpy((uint8_t *)p+at,&v,4); }
static int get_int(const void *p,unsigned at) { int v;memcpy(&v,(const uint8_t *)p+at,4);return v; }
static void original(double a,double b,double c,double d,double e,double f,double g,double h)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;calls++; }
static void original_render(void) { renders++; }
static void *get_group(void *self,const char *name)
{
    assert(self==manager); // g_UIManager is an object, never dereference it as a pointer.
    for (unsigned i=0;i<4;i++) {
        if (!strcmp(name,groups[i].name)) return provide_groups&&(i!=3||provide_tabs)?&groups[i]:NULL;
    }
    assert(false);return NULL;
}
static void *get_window(void *self,const char *name)
{
    assert(self==&groups[0]||self==&groups[1]||self==&groups[2]||self==&groups[3]);
    if (self==&groups[3]) {
        for (unsigned i=0;i<4;i++) if (!strcmp(name,tab_names[i])) return mission_tabs[i];
        assert(false);return NULL; // Moving Main would miss these screen-anchored buttons.
    }
    if (self==&groups[2]) {
        assert(!strcmp(name,"BtnSit")||!strcmp(name,"BtnRun"));
        return provide_movement?(!strcmp(name,"BtnSit")?sit:run_button):NULL;
    }
    assert(!strcmp(name,"Main"));
    return provide_quest&&self==&groups[0]?quest:provide_tmp&&self==&groups[1]?quest_tmp:NULL;
}
static void set_position(void *window,int x,int y)
{
    bool selected=window==quest||window==quest_tmp||window==sit||window==run_button;
    for (unsigned i=0;i<4;i++) selected|=window==mission_tabs[i];
    assert(selected);put_int(window,0x28,x);put_int(window,0x2c,y);
}
void *tl_ld_sym(tl_lib *lib,const char *symbol)
{
    (void)lib;lookups++;
    if (missing_export&&!strcmp(symbol,missing_export)) return NULL;
    if (!strcmp(symbol,"g_UIManager")) return manager;
    if (!strcmp(symbol,"_ZN9KUIManage10GetUIGroupEPKc")) return (void *)get_group;
    if (!strcmp(symbol,"_ZN8KUIGroup9GetWindowEPKc")) return (void *)get_window;
    if (!strcmp(symbol,"_ZN10KWndWindow11SetPositionEii")) return (void *)set_position;
    assert(false);return NULL;
}
const char *tl_ld_lib_name(const tl_lib *lib) { (void)lib;return library; }
static tl_lib *found_lib = (tl_lib *)1;
tl_lib *tl_ld_find_lib(const char *name) { return name && !strcmp(name, "libMyGame.so") ? found_lib : NULL; }
void *tl_ld_lib_base(const tl_lib *lib) { (void)lib;return (void *)fixture_base; }
static uintptr_t dup_rx_lo, dup_rx_hi, dup_rw_base;
void *tl_ld_aot_writable(const void *addr, size_t bytes)
{
    uintptr_t a = (uintptr_t)addr;
    if (dup_rw_base && a >= dup_rx_lo && a + bytes <= dup_rx_hi)
        return (void *)(dup_rw_base + (a - dup_rx_lo));
    return NULL;
}
void tl_ld_set_plt_filter(void *(*fn)(tl_lib *,const char *,void *)) { filter=fn; }
void tl_log_line(const char *format,...) { (void)format; }
bool tl_zip_open(tl_zip *z,const char *p,char *e,size_t n) { (void)z;(void)p;(void)e;(void)n;return true; }
void tl_zip_close(tl_zip *z) { (void)z; }
const tl_zip_entry *tl_zip_find(const tl_zip *z,const char *name)
{ (void)z;assert(!strcmp(name,"lib/arm64-v8a/libMyGame.so"));return &entry; }
bool tl_zip_data(const tl_zip *z,const tl_zip_entry *e,size_t limit,const uint8_t **d,size_t *n,bool *owned,char *err,size_t cap)
{
    (void)z;(void)e;(void)limit;(void)err;(void)cap;
    *d = override_zip_data ? override_zip_data : fake_zip_data;
    *n = override_zip_data ? override_zip_size : 1u;
    *owned = false;
    return true;
}
static const char *mock_sha = "0c468ae6407377f61d33f1bbe77928b1d5c32cdede98ab163b86a897f0b60dcb";
unsigned char *CC_SHA256(const void *data,CC_LONG size,unsigned char *out)
{
    if (override_zip_data) {
        (void)data;(void)size;
        memset(out, 0xab, 32);
        return out;
    }
    assert(data==fake_zip_data&&size==1);
    for (unsigned i=0;i<32;i++) { unsigned v;assert(sscanf(mock_sha+i*2,"%2x",&v)==1);out[i]=(uint8_t)v; }
    if (!hash_match) out[0]^=1;
    return out;
}
static struct { int insets[5]; } A;
static void tl_javaapp_touch(int phase,int id,float x,float y) { assert(phase==8);(void)id;(void)x;(void)y; }
#include "quest_insets_under_test.h"

typedef void (*anchor_call)(double,double,double,double,double,double,double,double);
static anchor_call wrapped;
static void (*render_frame)(void);
static void bind_profile(void)
{
    fixture_base=(uintptr_t)original-0x7cd9d0;
    wrapped=(anchor_call)filter((tl_lib *)1,"KT_SetUiAnchorSafeArea",(void *)original);
    fixture_base=(uintptr_t)original_render-0x7cdbdc;
    assert(filter((tl_lib *)1,"_Z17Wnd_RenderWindowsv",(void *)get_window)==(void *)get_window);
    render_frame=(void (*)(void))filter((tl_lib *)1,"_Z17Wnd_RenderWindowsv",(void *)original_render);
    assert(wrapped!=original && render_frame!=original_render);
}
static void frame(double fw,double fh)
{
    wrapped(fw,fh,0,0,fw,fh,932,430);
    render_frame();
}
static void window_at(void *w,int x,int y,int width,int height)
{
    memset(w,0,0x4b8); ((uint8_t *)w)[0x468]=((uint8_t *)w)[0x469]=1;
    put_int(w,0x30,width);put_int(w,0x34,height);
    put_double(w,0x490,x);put_double(w,0x498,y);
}
static void begin(int pixel_w,int pixel_h,const int *clip)
{
    assert(tl_vhtg_quest_start("x","review.com.game3c.vhtgmobile",true,clip,pixel_w,pixel_h));
    bind_profile();
}

static void test_dynamic_elf_discovery(void);

int main(void)
{
    const int insets[]={177,0,0,0};
    assert(!tl_vhtg_quest_start("x","another.game",true,insets,2796,1290));
    assert(!tl_vhtg_quest_start("x","review.com.game3c.vhtgmobile",false,insets,2796,1290));
    assert(!tl_vhtg_quest_start("x","review.com.game3c.vhtgmobile",true,insets,0,1290));
    assert(!tl_vhtg_quest_start("x","review.com.game3c.vhtgmobile",true,NULL,2796,1290));
    hash_match=false;assert(!tl_vhtg_quest_start("x","review.com.game3c.vhtgmobile",true,insets,2796,1290));
    hash_match=true;entry.usize=1;assert(!tl_vhtg_quest_start("x","review.com.game3c.vhtgmobile",true,insets,2796,1290));
    entry.usize=14643976;begin(2796,1290,insets);
    assert(filter&&tl_vhtg_quest_active());
    library="other.so";assert(filter((tl_lib *)1,"KT_SetUiAnchorSafeArea",(void *)original)==(void *)original);
    library="libMyGame.so";assert(filter((tl_lib *)1,"another_symbol",(void *)original)==(void *)original);
    fixture_base=(uintptr_t)original-0x7cd9d0;
    assert(filter((tl_lib *)1,"KT_SetUiAnchorSafeArea",(void *)get_window)==(void *)get_window);
    memset(avatar,0x55,sizeof(avatar));memset(age_logo,0x66,sizeof(age_logo));memset(movement,0x77,sizeof(movement));
    quest[0x468]=quest[0x469]=1;put_int(quest,0x30,200);put_int(quest,0x34,250);
    put_double(quest,0x490,10);put_double(quest,0x498,100);
    tl_javaapp_set_insets(177,0,0,0,123);
    assert(A.insets[0]==0&&A.insets[1]==0&&A.insets[2]==0&&A.insets[3]==0&&A.insets[4]==123);
    frame(2796,1290);
    double shifted=get_double(quest,0x490);
    assert(shifted==75&&get_double(quest,0x498)==100&&calls==1&&lookups==4&&renders==1);
    for (int i=0;i<10000;i++) frame(2796,1290);
    assert(get_double(quest,0x490)==shifted&&lookups==4);
    for (unsigned i=0;i<sizeof(avatar);i++) assert(avatar[i]==0x55&&age_logo[i]==0x66&&movement[i]==0x77);
    // Removing the cutout restores the original quest offset without accumulating drift.
    tl_javaapp_set_insets(0,0,0,0,0);frame(2796,1290);
    assert(get_double(quest,0x490)==10);
    // A user/game layout change becomes a fresh baseline rather than being overwritten.
    put_double(quest,0x490,200);tl_javaapp_set_insets(177,0,0,0,0);
    frame(2796,1290);assert(get_double(quest,0x490)==200);
    // Right-anchored quest windows move left only when the camera is at that edge.
    put_double(quest,0x470,1);put_double(quest,0x480,1);put_double(quest,0x490,-10);
    tl_javaapp_set_insets(0,0,177,0,0);frame(2796,1290);
    assert(get_double(quest,0x490)<-10);
    tl_javaapp_set_insets(177,0,0,0,0);frame(2796,1290);
    assert(get_double(quest,0x490)==-10);
    // Parent scaling affects the offset, not the final safe screen position.
    unsigned char parent[0x4b8]={0};void *parent_ptr=parent;float scale=2;
    memcpy(quest+0x458,&parent_ptr,sizeof(parent_ptr));memcpy(parent+0x4ac,&scale,4);put_int(quest,0x4b0,1);
    put_double(quest,0x470,0);put_double(quest,0x480,0);put_double(quest,0x490,10);
    frame(2796,1290);assert(get_double(quest,0x490)>10);
    tl_javaapp_set_insets(0,0,0,0,0);frame(2796,1290);
    assert(get_double(quest,0x490)==10);
    parent_ptr=NULL;memcpy(quest+0x458,&parent_ptr,sizeof(parent_ptr));put_int(quest,0x4b0,0);
    // A render-resolution change preserves the same HUD geometry for equal point-space cutouts.
    const int high[]={118,0,0,0};begin(1864,860,high);frame(1864,860);
    assert(get_double(quest,0x490)==shifted);
    put_double(quest,0x490,10);
    const int low[]={59,0,0,0};begin(932,430,low);frame(932,430);
    assert(get_double(quest,0x490)==shifted);
    // Legacy unanchored windows use the game's position API.
    quest[0x468]=quest[0x469]=0;put_int(quest,0x28,-149);put_int(quest,0x2c,100);
    frame(932,430);assert(get_int(quest,0x28)==75&&get_int(quest,0x2c)==100);
    tl_javaapp_set_insets(0,0,0,0,0);frame(932,430);assert(get_int(quest,0x28)==-149);
    provide_quest=false;frame(932,430);

    // Regression: the Cocos visible size can differ from the pixel framebuffer.
    provide_quest=true;window_at(quest,10,100,200,250);begin(2796,1290,insets);
    frame(932,430);assert(get_double(quest,0x490)==75);
    frame(466,215);assert(get_double(quest,0x490)==75);
    frame(2796,1290);assert(get_double(quest,0x490)==75);
    // A Y-only script update must not make the current X correction permanent.
    put_double(quest,0x498,120);frame(932,430);
    tl_javaapp_set_insets(0,0,0,0,0);frame(932,430);
    assert(get_double(quest,0x490)==10&&get_double(quest,0x498)==120);
    // Native update can create or reposition a window after the safe-area setter.
    tl_javaapp_set_insets(177,0,0,0,0);provide_quest=false;wrapped(932,430,0,0,932,430,932,430);
    provide_quest=true;put_double(quest,0x490,0);render_frame();assert(get_double(quest,0x490)==75);
    // Height overflow alone does not invalidate a horizontal correction.
    put_int(quest,0x34,800);put_double(quest,0x490,10);frame(932,430);assert(get_double(quest,0x490)==75);

    // BtnSit and BtnRun move outward toward the outer bezel AND down clear of the cutout.
    provide_movement=true;window_at(sit,10,320,35,40);window_at(run_button,10,365,35,40);
    begin(2796,1290,insets);frame(932,430);
    assert(get_double(sit,0x490)==9&&get_double(sit,0x498)==340);
    assert(get_double(run_button,0x490)==9&&get_double(run_button,0x498)==385);
    for (int i=0;i<10000;i++) frame(932,430);
    assert(get_double(sit,0x490)==9&&get_double(sit,0x498)==340&&get_double(run_button,0x498)==385);
    put_double(sit,0x490,20);frame(932,430); // Game edit re-aligns outward; Y stays below cutout without drift.
    assert(get_double(sit,0x490)==9&&get_double(sit,0x498)==340);
    tl_javaapp_set_insets(0,0,0,0,0);frame(932,430);
    assert(get_double(sit,0x490)==20&&get_double(sit,0x498)==320&&get_double(run_button,0x498)==365);
    tl_javaapp_set_insets(0,0,177,0,0);put_double(run_button,0x498,385);frame(932,430);
    assert(get_double(run_button,0x498)==390); // Only five points of room remain.
    put_double(run_button,0x498,400);frame(932,430);assert(get_double(run_button,0x498)==400);
    // Movement controls inside the Dynamic Island bottom band push down past it AND stay outward.
    window_at(run_button,60,260,35,40);window_at(sit,60,305,35,40);
    tl_javaapp_set_insets(177,0,0,0,0);frame(932,430);
    assert(get_double(run_button,0x490)==9&&get_double(sit,0x490)==9);
    assert(get_double(run_button,0x498)>=294&&get_double(sit,0x498)==get_double(run_button,0x498)+45);
    // Old popbar layouts have a shallow parent: never clip the button or its hit region.
    memset(parent,0,sizeof(parent));put_int(parent,0x40,10);put_int(parent,0x44,300);
    put_int(parent,0x30,120);put_int(parent,0x34,32);put_int(parent,0x438,332);
    parent_ptr=parent;window_at(run_button,0,2,26,29);run_button[0x469]=0;
    memcpy(run_button+0x458,&parent_ptr,sizeof(parent_ptr));frame(932,430);
    assert(get_double(run_button,0x498)==3); // Only one point before the parent would clip.
    wrapped(NAN,430,0,0,932,430,932,430);render_frame();
    for (unsigned i=0;i<sizeof(avatar);i++) assert(avatar[i]==0x55&&age_logo[i]==0x66&&movement[i]==0x77);

    // Device's downloaded UI: scene 1665x768, screen-anchored children of a
    // 220x54 popbar. They align to the outer edge (~8-9pt) and drop clear below tai tho.
    window_at(quest,66,-66,275,204);
    put_double(quest,0x478,0.5);put_double(quest,0x488,0.5);
    window_at(sit,20,170,56,62);window_at(run_button,20,100,58,65);
    put_double(sit,0x478,0.5);put_double(run_button,0x478,0.5);
    memset(parent,0,sizeof(parent));put_int(parent,0x30,220);put_int(parent,0x34,54);
    memcpy(sit+0x458,&parent_ptr,sizeof(parent_ptr));memcpy(run_button+0x458,&parent_ptr,sizeof(parent_ptr));
    begin(2796,1290,insets);
    // Calling tl_vhpt_hud_start with a VHTG package must NOT disarm VHTG quest mode.
    assert(!tl_vhpt_hud_start("x","com.game3c.vhtgmobile",true,insets,2796,1290));
    assert(tl_vhtg_quest_active());
    wrapped(1665,768,0,0,1665,768,1665,768);render_frame();
    assert(get_double(quest,0x490)==135&&get_double(quest,0x498)==-66);
    assert(get_double(run_button,0x490)==15&&get_double(run_button,0x498)==142);
    assert(get_double(sit,0x490)==15&&get_double(sit,0x498)==212);
    for (int i=0;i<10000;i++) render_frame();
    assert(get_double(quest,0x490)==135&&get_double(sit,0x498)==212&&get_double(run_button,0x498)==142);

    // Device report: text is adjusted, but its separate Mission/Team tabs are
    // still behind the left island. Reproduce the negative scene origin from
    // the live log, plus the downloaded tabs' screen-anchor geometry.
    unsigned char tab_parent[0x4b8]={0}, saved_parent[0x4b8];void *tab_parent_ptr=tab_parent;
    put_int(tab_parent,0x40,-90);put_int(tab_parent,0x44,-45);
    put_int(tab_parent,0x30,281);put_int(tab_parent,0x34,532);
    scale=2;memcpy(tab_parent+0x4ac,&scale,4);memcpy(saved_parent,tab_parent,sizeof(tab_parent));
    const int tab_x[]={25,25,24,24}, tab_w[]={41,41,57,105}, tab_h[]={107,107,57,53};
    const double tab_y[]={-116,-15,71.5,261.5};
    for (unsigned i=0;i<4;i++) {
        window_at(mission_tabs[i],tab_x[i],0,tab_w[i],tab_h[i]);
        put_double(mission_tabs[i],0x498,tab_y[i]);put_double(mission_tabs[i],0x478,0.5);
        memcpy(mission_tabs[i]+0x458,&tab_parent_ptr,sizeof(tab_parent_ptr));put_int(mission_tabs[i],0x4b0,1);
    }
    window_at(quest,66,-66,275,204);put_double(quest,0x478,0.5);put_double(quest,0x488,0.5);
    memcpy(quest_tmp,quest,sizeof(quest));provide_tmp=true;
    put_double(sit,0x498,170);put_double(run_button,0x498,100);
    begin(2796,1290,insets);wrapped(1665,768,0,0,1665,768,1366,768);render_frame();
    assert(get_double(quest,0x490)==134); // Old text-only behavior; tabs not yet registered.
    provide_tabs=true;render_frame();
    assert(get_double(quest,0x490)==176); // Common inward shift with tabs.
    assert(get_double(quest_tmp,0x490)==176);
    for (unsigned i=0;i<4;i++) {
        assert(get_double(mission_tabs[i],0x490)==tab_x[i]+110);
        assert(get_double(mission_tabs[i],0x498)==tab_y[i]);
    }
    assert(get_double(mission_tabs[0],0x490)+41==get_double(quest,0x490)); // Same text/tab spacing.
    for (int i=0;i<10000;i++) render_frame();
    assert(get_double(quest,0x490)==176&&get_double(mission_tabs[0],0x490)==135);
    assert(memcmp(tab_parent,saved_parent,sizeof(tab_parent))==0);
    for (unsigned i=0;i<sizeof(avatar);i++) assert(avatar[i]==0x55&&age_logo[i]==0x66&&movement[i]==0x77);
    put_double(mission_tabs[0],0x498,-110);render_frame(); // A Y-only edit must not retain X shift.
    tl_javaapp_set_insets(0,0,0,0,0);render_frame();
    assert(get_double(quest,0x490)==66&&get_double(mission_tabs[0],0x490)==25);
    assert(get_double(mission_tabs[0],0x498)==-110);
    tl_javaapp_set_insets(0,0,177,0,0);render_frame();
    assert(get_double(quest,0x490)==66&&get_double(mission_tabs[0],0x490)==25);
    tl_javaapp_set_insets(177,0,0,0,0);render_frame();
    assert(get_double(quest,0x490)==176&&get_double(mission_tabs[0],0x490)==135);
    assert(get_double(sit,0x498)==212&&get_double(run_button,0x498)==142);
    provide_tabs=provide_tmp=false;
    // Missing groups/exports are inert; later-created script groups are retried.
    window_at(quest,66,-66,275,204);provide_groups=false;begin(2796,1290,insets);
    wrapped(1665,768,0,0,1665,768,1665,768);render_frame();assert(get_double(quest,0x490)==66);
    provide_groups=true;render_frame();assert(get_double(quest,0x490)==135);
    const char *exports[]={"g_UIManager","_ZN9KUIManage10GetUIGroupEPKc","_ZN8KUIGroup9GetWindowEPKc","_ZN10KWndWindow11SetPositionEii"};
    for (unsigned i=0;i<4;i++) {
        window_at(quest,66,-66,275,204);missing_export=exports[i];begin(2796,1290,insets);
        wrapped(1665,768,0,0,1665,768,1665,768);unsigned before=renders;render_frame();
        assert(get_double(quest,0x490)==66&&renders==before+1);
    }
    missing_export=NULL;

    // Profile 2: dev/release build (com.game3c.vhtgmobile)
    entry.usize = 14882616;
    mock_sha = "8cc2b4e59fd6087d726445526eb04fc39f1a7262be383fb52ef78533e95ab780";
    assert(tl_vhtg_quest_start("x","com.game3c.vhtgmobile",true,insets,2796,1290));
    fixture_base = (uintptr_t)original - 0x7e2abc;
    wrapped = (anchor_call)filter((tl_lib *)1, "KT_SetUiAnchorSafeArea", (void *)original);
    fixture_base = (uintptr_t)original_render - 0x7e2cc8;
    render_frame = (void (*)(void))filter((tl_lib *)1, "_Z17Wnd_RenderWindowsv", (void *)original_render);
    assert(wrapped != original && render_frame != original_render);
    window_at(quest, 10, 100, 200, 250);
    frame(2796, 1290);
    assert(get_double(quest, 0x490) == 75);
    double dx,dy;
    assert(!tl_quest_delta((tl_quest_rect){0,0,932,430},(tl_quest_rect){0,100,900,200},(tl_quest_cutout){59,0,0,0},932,430,&dx,&dy));

    // Profile 3: VHPT (com.tech3cgame.vhpt1) HUD alignment matching VHTG outer edge, Quest unchanged
    // 3a) Pure layout math across supported iPhone screen sizes (iPhone 8 -> iPhone 18)
    assert(tl_vhpt_outer_left_px(0.0f, 750.0f) == 0.0f);                 /* iPhone 8 / SE: flat bezel */
    assert(tl_vhpt_outer_left_px(94.0f, 780.0f) == 16.0f);               /* iPhone 12/13/14 (notch @2x): 8pt */
    assert(tl_vhpt_outer_left_px(118.0f, 860.0f) == 17.0f);              /* iPhone 15/16 Pro Max (Standard @2x): 8.5pt */
    assert(tl_vhpt_outer_left_px(177.0f, 1290.0f) == 26.0f);             /* iPhone 15/16 Pro Max (Native @3x): 8.7pt */
    assert(tl_vhpt_outer_left_px(186.0f, 1320.0f) == 26.0f);             /* iPhone 16 Pro Max / 17 Pro Max (@3x) */
    assert(tl_vhpt_is_left_anchor("TopLeft") && tl_vhpt_is_left_anchor("BottomLeft") && tl_vhpt_is_left_anchor(""));
    assert(!tl_vhpt_is_left_anchor("TopRight") && !tl_vhpt_is_left_anchor("TopCenter") && !tl_vhpt_is_left_anchor("BottomCenter"));

    /* Top-left Character box & below-left Pet box move outward (~101px closer at Standard) while clearing corner curve;
     * Quest list (Nhiệm vụ) ALWAYS stays unchanged at original position, even if anchored TopLeft! */
    assert(tl_vhpt_panel_target_x(false, "TopLeft", 118.0f, 118.0f, 860.0f, true, 720.0f, 845.0f) == 17.0f);
    assert(tl_vhpt_panel_target_x(false, "TopLeft", 118.0f, 118.0f, 860.0f, true, 610.0f, 710.0f) == 17.0f);
    assert(tl_vhpt_panel_target_x(false, "BottomLeft", 118.0f, 118.0f, 860.0f, true, 16.0f, 160.0f) == 17.0f);
    /* Quest panel (Nhiệm vụ) NEVER moves */
    assert(tl_vhpt_panel_target_x(true, "TopLeft", 118.0f, 118.0f, 860.0f, true, 300.0f, 520.0f) == 118.0f);
    assert(tl_vhpt_panel_target_x(true, "TopRight", 118.0f, 118.0f, 860.0f, true, 300.0f, 520.0f) == 118.0f);
    /* Middle-left panel overlapping hardware Dynamic Island / Notch band stays clear at safe_l */
    assert(tl_vhpt_panel_target_x(false, "MiddleLeft", 118.0f, 118.0f, 860.0f, true, 330.0f, 530.0f) == 118.0f);
    /* Right/Center panels stay untouched */
    assert(tl_vhpt_panel_target_x(false, "TopRight", 118.0f, 118.0f, 860.0f, false, 0.0f, 0.0f) == 118.0f);
    /* Notch on right (180-deg flip, safe_l == 0) keeps left-edge origin at 0 */
    assert(tl_vhpt_panel_target_x(false, "TopLeft", 0.0f, 0.0f, 860.0f, true, 720.0f, 845.0f) == 0.0f);

    /* QuickMenu BottomLeft bucket (Emote/Ride/Sit) moves X outward AND safely down without clipping Sit */
    float bl_x = 0.0f, bl_y = 0.0f;
    tl_vhpt_bottom_left_target(177.0f, 0.0f, 177.0f, 1290.0f, true, 61.3f, 385.0f, &bl_x, &bl_y);
    assert(bl_x == 26.0f && bl_y == -6.0f);
    tl_vhpt_bottom_left_target(177.0f, 0.0f, 177.0f, 1290.0f, true, 260.0f, 460.0f, &bl_x, &bl_y);
    assert(bl_x == 26.0f && bl_y <= -52.0f);
    /* Real device 768p PTUi container space (cur_x = 105.6 = 177 / 1.676259): moves outward to 15px (~8.4pt). */
    assert(tl_vhpt_panel_target_x(false, "TopLeft", 105.6f, 177.0f, 1290.0f, true, 650.0f, 748.0f) == 15.0f);
    tl_vhpt_bottom_left_target(105.6f, 0.0f, 177.0f, 1290.0f, true, 36.5f, 201.0f, &bl_x, &bl_y);
    assert(bl_x == 15.0f && bl_y == -3.0f);

    // 3b) Start/reset and glClear runtime alignment against fake VHPT AOT ELF+dup+scene fixture
    assert(tl_vhtg_quest_active()); /* switching from VHTG -> VHPT must disarm VHTG quest hook */
    entry.usize = VHPT_LIB_SIZE;
    mock_sha = "c04629c87573f99234f54e768e121c09b0ddcef3ac531e1facc82738ea967b44";
    int vhpt_insets[4] = {177, 0, 0, 0};
    assert(tl_vhpt_hud_start("x", "com.tech3cgame.vhpt1", true, vhpt_insets, 2796, 1290));
    assert(!tl_vhtg_quest_active()); /* native insets preserved for Quest & non-left panels */
    void (*wrapped_clear)(unsigned int) = (void (*)(unsigned int))filter((tl_lib *)1, "glClear", (void *)fake_gl_clear);
    assert(wrapped_clear != fake_gl_clear);
    uint8_t *vhpt_img = calloc(1, 0x1000000);
    uint8_t *vhpt_dup = calloc(1, 0x1000000);
    assert(vhpt_img != NULL && vhpt_dup != NULL);
    memcpy(vhpt_img + VHPT_STR_ANCHOR_OFF, "Anchor", 7);
    memcpy(vhpt_img + VHPT_STR_TOPLEFT_OFF, "TopLeft", 8);
    memcpy(vhpt_img + VHPT_STR_CHARINFO_OFF, "ui_mobile/charinfo.ini", 23);
    /* Put live BSS ONLY in vhpt_dup (simulating AOT mixed/copied data pages while rx remains zeroed) */
    *(int32_t *)(vhpt_dup + VHPT_BSS_SAFE_L_OFF) = 177;
    *(uint32_t *)(vhpt_dup + VHPT_BSS_SAFE_GEN_OFF) = 1;
    _Alignas(16) uint8_t scene_mem[0x700] = {0};
    *(uintptr_t *)(vhpt_dup + VHPT_BSS_GAMESCENE_OFF) = (uintptr_t)scene_mem;
    dup_rx_lo = (uintptr_t)vhpt_img + 0xe80000u;
    dup_rx_hi = (uintptr_t)vhpt_img + 0x1000000u;
    dup_rw_base = (uintptr_t)vhpt_dup + 0xe80000u;
    void *fake_vtable[0x30] = {0};
    fake_vtable[VHPT_NODE_VTBL_SETPOS / sizeof(void *)] = (void *)fake_setpos;
    _Alignas(16) uint8_t char_panel[0x400] = {0}, pet_panel[0x400] = {0}, quest_panel[0x400] = {0}, qm_panel[0x400] = {0};
    _Alignas(16) uint8_t ini_tl[0x80] = {0};
    ini_tl[VHPT_INI_ANCHOR_STR_OFF] = (uint8_t)(7 << 1);
    memcpy(ini_tl + VHPT_INI_ANCHOR_STR_OFF + 1, "TopLeft", 8);
    _Alignas(16) uint8_t char_root[0x200] = {0}, pet_root[0x200] = {0}, quest_root[0x200] = {0}, bl_bucket[0x200] = {0};
    _Alignas(16) uint8_t bl_ch_sit[0x100] = {0}, bl_ch_emote[0x100] = {0};
    *(void **)char_root = fake_vtable;
    *(void **)pet_root = fake_vtable;
    *(void **)quest_root = fake_vtable;
    *(void **)bl_bucket = fake_vtable;
    float sc = 1290.0f / 768.0f;
    *(float *)(char_root + VHPT_NODE_SCALEX_OFF) = sc;
    *(float *)(pet_root + VHPT_NODE_SCALEX_OFF) = sc;
    *(float *)(quest_root + VHPT_NODE_SCALEX_OFF) = sc;
    *(float *)(bl_bucket + VHPT_NODE_SCALEX_OFF) = sc;
    *(float *)(char_root + VHPT_NODE_POSX_OFF) = 177.0f;
    *(float *)(pet_root + VHPT_NODE_POSX_OFF) = 177.0f;
    *(float *)(quest_root + VHPT_NODE_POSX_OFF) = 177.0f;
    *(float *)(bl_bucket + VHPT_NODE_POSX_OFF) = 177.0f;
    *(float *)(bl_bucket + VHPT_NODE_POSY_OFF) = 0.0f;
    *(float *)(bl_ch_sit + VHPT_NODE_POSY_OFF) = 36.5f;
    *(float *)(bl_ch_emote + VHPT_NODE_POSY_OFF) = 201.0f;
    uintptr_t bl_children[2] = { (uintptr_t)bl_ch_sit, (uintptr_t)bl_ch_emote };
    *(uintptr_t *)(bl_bucket + VHPT_NODE_CHILDREN_VEC) = (uintptr_t)bl_children;
    *(uintptr_t *)(bl_bucket + VHPT_NODE_CHILDREN_VEC + sizeof(uintptr_t)) = (uintptr_t)(bl_children + 2);
    _Alignas(16) uint8_t qm_buckets[VHPT_QM_BUCKET_STRIDE * 2] = {0};
    qm_buckets[0] = (uint8_t)(8 << 1);
    memcpy(qm_buckets + 1, "TopRight", 9);
    qm_buckets[VHPT_QM_BUCKET_STRIDE] = (uint8_t)(10 << 1);
    memcpy(qm_buckets + VHPT_QM_BUCKET_STRIDE + 1, "BottomLeft", 11);
    *(uintptr_t *)(qm_buckets + VHPT_QM_BUCKET_STRIDE + VHPT_QM_BUCKET_NODE_OFF) = (uintptr_t)bl_bucket;
    *(uintptr_t *)(qm_panel + VHPT_QM_BUCKETS_VEC_OFF) = (uintptr_t)qm_buckets;
    *(uintptr_t *)(qm_panel + VHPT_QM_BUCKETS_VEC_OFF + sizeof(uintptr_t)) = (uintptr_t)(qm_buckets + sizeof(qm_buckets));
    *(uintptr_t *)(char_panel + 0x300) = (uintptr_t)ini_tl;
    *(uintptr_t *)(char_panel + 0x328) = (uintptr_t)char_root;
    *(uintptr_t *)(pet_panel + 0x300) = (uintptr_t)ini_tl;
    *(uintptr_t *)(pet_panel + 0x328) = (uintptr_t)pet_root;
    *(uintptr_t *)(quest_panel + 0x308) = (uintptr_t)ini_tl;
    *(uintptr_t *)(quest_panel + 0x368) = (uintptr_t)quest_root;
    *(uintptr_t *)(scene_mem + 0x3b0) = (uintptr_t)char_panel;
    *(uintptr_t *)(scene_mem + 0x3b8) = (uintptr_t)pet_panel;
    *(uintptr_t *)(scene_mem + 0x5f0) = (uintptr_t)quest_panel;
    *(uintptr_t *)(scene_mem + VHPT_QM_SCENE_OFF) = (uintptr_t)qm_panel;
    fixture_base = (uintptr_t)vhpt_img;
    library = "libMyGame.so";
    wrapped_clear(0x4000u);
    assert(vhpt_clears == 1);
    assert(*(float *)(char_root + VHPT_NODE_POSX_OFF) == 26.0f);
    assert(*(float *)(pet_root + VHPT_NODE_POSX_OFF) == 26.0f);
    assert(*(float *)(bl_bucket + VHPT_NODE_POSX_OFF) == 26.0f);
    assert(*(float *)(bl_bucket + VHPT_NODE_POSY_OFF) == -6.0f);
    assert(*(float *)(quest_root + VHPT_NODE_POSX_OFF) == 177.0f); /* Quest unchanged */
    assert(*(float *)(quest_root + VHPT_NODE_SCALEX_OFF) == sc);
    for (int i = 0; i < 1000; i++) wrapped_clear(0x4000u);
    assert(*(float *)(bl_bucket + VHPT_NODE_POSX_OFF) == 26.0f && *(float *)(bl_bucket + VHPT_NODE_POSY_OFF) == -6.0f);
    dup_rw_base = 0;
    tl_vhtg_quest_reset();
    free(vhpt_dup);
    free(vhpt_img);
    test_dynamic_elf_discovery();
    puts("PASS: selected HUD uses the registry/object ABI, moves Character & BottomLeft outward (+downward clear of cutout) in both VHTG & VHPT, handles AOT dup BSS, keeps Nhiệm vụ clear/unchanged, avoids drift, and auto-discovers shifted VHTG/VHPT ELFs");
    return 0;
}

static void w16(uint8_t *p, uint16_t v) { memcpy(p, &v, sizeof(v)); }
static void w32(uint8_t *p, uint32_t v) { memcpy(p, &v, sizeof(v)); }
static void w64(uint8_t *p, uint64_t v) { memcpy(p, &v, sizeof(v)); }

static uint32_t enc_adrp(uint32_t rd, uint64_t pc, uint64_t target)
{
    int64_t pages = (int64_t)(target >> 12) - (int64_t)(pc >> 12);
    uint32_t imm21 = (uint32_t)(pages & 0x1fffff);
    return 0x90000000u | ((imm21 & 3u) << 29) | (((imm21 >> 2) & 0x7ffffu) << 5) | (rd & 0x1fu);
}

static uint32_t enc_add_imm(uint32_t rd, uint32_t rn, uint32_t imm12)
{
    return 0x91000000u | ((imm12 & 0xfffu) << 10) | ((rn & 0x1fu) << 5) | (rd & 0x1fu);
}

static uint32_t enc_ldr64_uoff(uint32_t rt, uint32_t rn, uint32_t off)
{
    assert((off % 8u) == 0u);
    return 0xf9400000u | (((off / 8u) & 0xfffu) << 10) | ((rn & 0x1fu) << 5) | (rt & 0x1fu);
}

typedef struct {
    const char *name;
    uint64_t va;
} test_dyn_sym;

static uint8_t *build_test_elf(size_t *out_len, const test_dyn_sym *syms, size_t nsyms)
{
    enum {
        TEXT_OFF = 0x4000u, TEXT_SZ = 0x1000u,
        RO_OFF = 0x2000u, RO_SZ = 0x1000u,
        DYNSTR_OFF = 0x1000u, DYNSTR_SZ = 0x400u,
        DYNSYM_OFF = 0x1400u, SHSTR_OFF = 0x6000u, SHDR_OFF = 0x7000u,
        ELF_SZ = 0x10000u
    };
    uint8_t *elf = calloc(1, ELF_SZ);
    assert(elf != NULL);
    elf[0] = 0x7fu; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
    elf[4] = 2u; elf[5] = 1u; elf[6] = 1u;
    w16(elf + 16, 3u);   /* ET_DYN */
    w16(elf + 18, 183u); /* EM_AARCH64 */
    w32(elf + 20, 1u);
    w64(elf + 32, 64u);       /* e_phoff */
    w64(elf + 40, SHDR_OFF);  /* e_shoff */
    w16(elf + 52, 64u);
    w16(elf + 54, 56u);
    w16(elf + 56, 2u);
    w16(elf + 58, 64u);
    w16(elf + 60, 6u);
    w16(elf + 62, 5u);

    /* PT_LOAD[0]: R|X file-backed 0x0..0x10000 */
    w32(elf + 64, 1u); w32(elf + 68, 5u);
    w64(elf + 72, 0u); w64(elf + 80, 0u);
    w64(elf + 96, ELF_SZ); w64(elf + 104, ELF_SZ);
    /* PT_LOAD[1]: R|W BSS 0x10000..0x20000 */
    w32(elf + 120, 1u); w32(elf + 124, 6u);
    w64(elf + 128, 0u); w64(elf + 136, 0x10000u);
    w64(elf + 152, 0u); w64(elf + 160, 0x10000u);

    /* Populate .dynstr and .dynsym */
    size_t str_cur = 1u;
    for (size_t i = 0; i < nsyms; i++) {
        size_t slen = strlen(syms[i].name) + 1u;
        assert(str_cur + slen <= DYNSTR_SZ);
        memcpy(elf + DYNSTR_OFF + str_cur, syms[i].name, slen);
        uint8_t *ent = elf + DYNSYM_OFF + (i + 1u) * 24u;
        w32(ent + 0, (uint32_t)str_cur);
        ent[4] = 0x12u;
        w64(ent + 8, syms[i].va);
        w64(ent + 16, 64u);
        str_cur += slen;
    }

    /* Section names in .shstrtab */
    static const char shnames[] = "\0.dynsym\0.dynstr\0.rodata\0.text\0.shstrtab\0";
    memcpy(elf + SHSTR_OFF, shnames, sizeof(shnames));
    uint8_t *sh = elf + SHDR_OFF;
    /* [1] .dynsym */
    w32(sh + 64 * 1 + 0, 1u); w32(sh + 64 * 1 + 4, 11u);
    w64(sh + 64 * 1 + 16, DYNSYM_OFF); w64(sh + 64 * 1 + 24, DYNSYM_OFF);
    w64(sh + 64 * 1 + 32, (nsyms + 1u) * 24u); w32(sh + 64 * 1 + 40, 2u);
    w64(sh + 64 * 1 + 56, 24u);
    /* [2] .dynstr */
    w32(sh + 64 * 2 + 0, 9u); w32(sh + 64 * 2 + 4, 3u);
    w64(sh + 64 * 2 + 16, DYNSTR_OFF); w64(sh + 64 * 2 + 24, DYNSTR_OFF);
    w64(sh + 64 * 2 + 32, DYNSTR_SZ);
    /* [3] .rodata */
    w32(sh + 64 * 3 + 0, 17u); w32(sh + 64 * 3 + 4, 1u);
    w64(sh + 64 * 3 + 16, RO_OFF); w64(sh + 64 * 3 + 24, RO_OFF);
    w64(sh + 64 * 3 + 32, RO_SZ);
    /* [4] .text */
    w32(sh + 64 * 4 + 0, 25u); w32(sh + 64 * 4 + 4, 1u);
    w64(sh + 64 * 4 + 16, TEXT_OFF); w64(sh + 64 * 4 + 24, TEXT_OFF);
    w64(sh + 64 * 4 + 32, TEXT_SZ);
    /* [5] .shstrtab */
    w32(sh + 64 * 5 + 0, 31u); w32(sh + 64 * 5 + 4, 3u);
    w64(sh + 64 * 5 + 24, SHSTR_OFF); w64(sh + 64 * 5 + 32, sizeof(shnames));

    *out_len = ELF_SZ;
    return elf;
}

static void test_dynamic_elf_discovery(void)
{
    /* 1. VHTG: shifted .dynsym offsets resolved dynamically despite unknown size & SHA-256 */
    static const test_dyn_sym vhtg_syms[6] = {
        {"KT_SetUiAnchorSafeArea", 0x912340u},
        {"_Z17Wnd_RenderWindowsv", 0x912580u},
        {"g_UIManager", 0x1f00000u},
        {"_ZN9KUIManage10GetUIGroupEPKc", 0x920000u},
        {"_ZN8KUIGroup9GetWindowEPKc", 0x920100u},
        {"_ZN10KWndWindow11SetPositionEii", 0x920200u},
    };
    size_t elf_sz = 0;
    uint8_t *vhtg_elf = build_test_elf(&elf_sz, vhtg_syms, 6);
    override_zip_data = vhtg_elf;
    override_zip_size = elf_sz;
    entry.usize = elf_sz;
    const int insets[4] = {177, 0, 177, 63};
    /* Package-agnostic ELF capability activation for renamed/cloned VHTG APKs */
    assert(tl_apk_cutout_profile_kind("future.apk", "vn.vhtg.repack") == 1);
    assert(tl_vhtg_quest_start("future.apk", "vn.vhtg.repack", true, insets, 2556, 1179));
    fixture_base = 0x1000000ULL;
    library = "libMyGame.so";
    assert(filter(found_lib, "KT_SetUiAnchorSafeArea", (void *)(fixture_base + 0x912340u)) != (void *)(fixture_base + 0x912340u));
    assert(filter(found_lib, "_Z17Wnd_RenderWindowsv", (void *)(fixture_base + 0x912580u)) != (void *)(fixture_base + 0x912580u));
    assert(filter(found_lib, "KT_SetUiAnchorSafeArea", (void *)(fixture_base + 0x7cd9d0u)) == (void *)(fixture_base + 0x7cd9d0u));
    tl_vhtg_quest_reset();

    /* Reject ELF missing a required VHTG UI symbol */
    uint8_t *bad_vhtg = build_test_elf(&elf_sz, vhtg_syms, 5);
    override_zip_data = bad_vhtg;
    override_zip_size = elf_sz;
    assert(!tl_vhtg_quest_start("future.apk", "com.game3c.vhtgmobile", true, insets, 2556, 1179));
    free(bad_vhtg);
    free(vhtg_elf);

    /* 2. VHPT: shifted .rodata strings + ADRP+ADD/LDR .bss offsets resolved dynamically */
    static const test_dyn_sym vhpt_syms[1] = {
        {"Java_org_cocos2dx_cpp_AppActivity_nativeSetSafeArea", 0x4200u},
    };
    uint8_t *vhpt_elf = build_test_elf(&elf_sz, vhpt_syms, 1);
    memcpy(vhpt_elf + 0x2111u, "Anchor", 7);
    memcpy(vhpt_elf + 0x2222u, "TopLeft", 8);
    memcpy(vhpt_elf + 0x2333u, "ui_mobile/charinfo.ini", 23);
    memcpy(vhpt_elf + 0x2444u, "scene.update.head", 18);
    /* nativeSetSafeArea @ 0x4200 -> 0x11140 (.bss SAFE_L, SAFE_GEN=0x11150) */
    w32(vhpt_elf + 0x4200u, enc_adrp(12u, 0x4200u, 0x11140u));
    w32(vhpt_elf + 0x4204u, enc_add_imm(12u, 12u, 0x140u));
    /* scene.update.head xref @ 0x4400 -> 0x2444, followed by s_gameScene @ 0x11880 */
    w32(vhpt_elf + 0x4400u, enc_adrp(0u, 0x4400u, 0x2444u));
    w32(vhpt_elf + 0x4404u, enc_add_imm(0u, 0u, 0x444u));
    w32(vhpt_elf + 0x4420u, enc_adrp(20u, 0x4420u, 0x11880u));
    w32(vhpt_elf + 0x4424u, enc_ldr64_uoff(8u, 20u, 0x880u));

    override_zip_data = vhpt_elf;
    override_zip_size = elf_sz;
    entry.usize = elf_sz;
    /* Package-agnostic ELF capability activation for renamed/cloned VHPT APKs */
    assert(tl_apk_cutout_profile_kind("future-vhpt.apk", "com.vhpt.clone") == 2);
    assert(tl_vhpt_hud_start("future-vhpt.apk", "com.vhpt.clone", true, insets, 2556, 1179));

    uint8_t *vhpt_dyn_img = calloc(1, 0x20000u);
    assert(vhpt_dyn_img != NULL);
    memcpy(vhpt_dyn_img + 0x2111u, "Anchor", 7);
    memcpy(vhpt_dyn_img + 0x2222u, "TopLeft", 8);
    memcpy(vhpt_dyn_img + 0x2333u, "ui_mobile/charinfo.ini", 23);

    _Alignas(16) uint8_t scene_mem[0x700] = {0};
    _Alignas(16) uint8_t char_panel[0x400] = {0};
    _Alignas(16) uint8_t char_root[0x200] = {0};
    _Alignas(16) uint8_t ini_tl[0x80] = {0};
    ini_tl[VHPT_INI_ANCHOR_STR_OFF] = (uint8_t)(7 << 1);
    memcpy(ini_tl + VHPT_INI_ANCHOR_STR_OFF + 1, "TopLeft", 8);
    void *dyn_vtable[0x30] = {0};
    dyn_vtable[VHPT_NODE_VTBL_SETPOS / sizeof(void *)] = (void *)fake_setpos;
    *(void **)(char_root) = dyn_vtable;
    *(float *)(char_root + VHPT_NODE_SCALEX_OFF) = 1.0f;
    *(float *)(char_root + VHPT_NODE_POSX_OFF) = 177.0f;
    *(uintptr_t *)(char_panel + 0x300) = (uintptr_t)ini_tl;
    *(uintptr_t *)(char_panel + 0x328) = (uintptr_t)char_root;
    *(uintptr_t *)(scene_mem + 0x3b0) = (uintptr_t)char_panel;
    *(uintptr_t *)(vhpt_dyn_img + 0x11880u) = (uintptr_t)scene_mem;
    int32_t safe_l_val = 177;
    uint32_t safe_gen_val = 2u;
    memcpy(vhpt_dyn_img + 0x11140u, &safe_l_val, sizeof(safe_l_val));
    memcpy(vhpt_dyn_img + 0x11150u, &safe_gen_val, sizeof(safe_gen_val));

    void (*dyn_clear)(unsigned int) = (void (*)(unsigned int))filter(found_lib, "glClear", (void *)fake_gl_clear);
    assert(dyn_clear != fake_gl_clear);
    fixture_base = (uintptr_t)vhpt_dyn_img;
    library = "libMyGame.so";
    vhpt_clears = 0;
    dyn_clear(0x4000u);
    assert(vhpt_clears == 1);
    assert(*(float *)(char_root + VHPT_NODE_POSX_OFF) == 24.0f || *(float *)(char_root + VHPT_NODE_POSX_OFF) == 26.0f);

    tl_vhtg_quest_reset();
    free(vhpt_dyn_img);
    free(vhpt_elf);
    override_zip_data = NULL;
    override_zip_size = 0;
    entry.usize = 14643976;
}
