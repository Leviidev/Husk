/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "husk-tl-internal.h"
#include "husk-tl-ld.h"
#include "husk-tl-vhtg-quest.h"
#include "husk-tl-quest-layout.h"
#include <CommonCrypto/CommonDigest.h>

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
void *tl_ld_lib_base(const tl_lib *lib) { (void)lib;return (void *)fixture_base; }
void tl_ld_set_plt_filter(void *(*fn)(tl_lib *,const char *,void *)) { filter=fn; }
void tl_log_line(const char *format,...) { (void)format; }
bool tl_zip_open(tl_zip *z,const char *p,char *e,size_t n) { (void)z;(void)p;(void)e;(void)n;return true; }
void tl_zip_close(tl_zip *z) { (void)z; }
const tl_zip_entry *tl_zip_find(const tl_zip *z,const char *name)
{ (void)z;assert(!strcmp(name,"lib/arm64-v8a/libMyGame.so"));return &entry; }
bool tl_zip_data(const tl_zip *z,const tl_zip_entry *e,size_t limit,const uint8_t **d,size_t *n,bool *owned,char *err,size_t cap)
{ (void)z;(void)e;(void)limit;(void)err;(void)cap;*d=fake_zip_data;*n=1;*owned=false;return true; }
unsigned char *CC_SHA256(const void *data,CC_LONG size,unsigned char *out)
{
    assert(data==fake_zip_data&&size==1);
    const char *hex="0c468ae6407377f61d33f1bbe77928b1d5c32cdede98ab163b86a897f0b60dcb";
    for (unsigned i=0;i<32;i++) { unsigned v;assert(sscanf(hex+i*2,"%2x",&v)==1);out[i]=(uint8_t)v; }
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
    assert(shifted==59&&get_double(quest,0x498)==100&&calls==1&&lookups==4&&renders==1);
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
    frame(932,430);assert(get_int(quest,0x28)==59&&get_int(quest,0x2c)==100);
    tl_javaapp_set_insets(0,0,0,0,0);frame(932,430);assert(get_int(quest,0x28)==-149);
    provide_quest=false;frame(932,430);

    // Regression: the Cocos visible size can differ from the pixel framebuffer.
    provide_quest=true;window_at(quest,10,100,200,250);begin(2796,1290,insets);
    frame(932,430);assert(get_double(quest,0x490)==59);
    frame(466,215);assert(get_double(quest,0x490)==59);
    frame(2796,1290);assert(get_double(quest,0x490)==59);
    // A Y-only script update must not make the current X correction permanent.
    put_double(quest,0x498,120);frame(932,430);
    tl_javaapp_set_insets(0,0,0,0,0);frame(932,430);
    assert(get_double(quest,0x490)==10&&get_double(quest,0x498)==120);
    // Native update can create or reposition a window after the safe-area setter.
    tl_javaapp_set_insets(177,0,0,0,0);provide_quest=false;wrapped(932,430,0,0,932,430,932,430);
    provide_quest=true;put_double(quest,0x490,0);render_frame();assert(get_double(quest,0x490)==59);
    // Height overflow alone does not invalidate a horizontal correction.
    put_int(quest,0x34,800);put_double(quest,0x490,10);frame(932,430);assert(get_double(quest,0x490)==59);

    // Only BtnSit and BtnRun move down. The enclosing popbar is never selected.
    provide_movement=true;window_at(sit,10,320,35,40);window_at(run_button,10,365,35,40);
    begin(2796,1290,insets);frame(932,430);
    assert(get_double(sit,0x490)==10&&get_double(sit,0x498)==332);
    assert(get_double(run_button,0x490)==10&&get_double(run_button,0x498)==377);
    for (int i=0;i<10000;i++) frame(932,430);
    assert(get_double(sit,0x498)==332&&get_double(run_button,0x498)==377);
    put_double(sit,0x490,20);frame(932,430); // An X edit must not accumulate the Y adjustment.
    tl_javaapp_set_insets(0,0,0,0,0);frame(932,430);
    assert(get_double(sit,0x490)==20&&get_double(sit,0x498)==320&&get_double(run_button,0x498)==365);
    tl_javaapp_set_insets(0,0,177,0,0);put_double(run_button,0x498,385);frame(932,430);
    assert(get_double(run_button,0x498)==390); // Only five points of room remain.
    put_double(run_button,0x498,400);frame(932,430);assert(get_double(run_button,0x498)==400);
    // Old popbar layouts have a shallow parent: never clip the button or its hit region.
    memset(parent,0,sizeof(parent));put_int(parent,0x40,10);put_int(parent,0x44,300);
    put_int(parent,0x30,120);put_int(parent,0x34,32);put_int(parent,0x438,332);
    parent_ptr=parent;window_at(run_button,0,2,26,29);run_button[0x469]=0;
    memcpy(run_button+0x458,&parent_ptr,sizeof(parent_ptr));frame(932,430);
    assert(get_double(run_button,0x498)==3); // Only one point before the parent would clip.
    wrapped(NAN,430,0,0,932,430,932,430);render_frame();assert(get_double(sit,0x498)==332);
    for (unsigned i=0;i<sizeof(avatar);i++) assert(avatar[i]==0x55&&age_logo[i]==0x66&&movement[i]==0x77);

    // Device's downloaded UI: scene 1665x768, screen-anchored children of a
    // 220x54 popbar. Their screen anchor, not that shallow parent, bounds movement.
    window_at(quest,66,-66,275,204);
    put_double(quest,0x478,0.5);put_double(quest,0x488,0.5);
    window_at(sit,20,170,56,62);window_at(run_button,20,100,58,65);
    put_double(sit,0x478,0.5);put_double(run_button,0x478,0.5);
    memset(parent,0,sizeof(parent));put_int(parent,0x30,220);put_int(parent,0x34,54);
    memcpy(sit+0x458,&parent_ptr,sizeof(parent_ptr));memcpy(run_button+0x458,&parent_ptr,sizeof(parent_ptr));
    begin(2796,1290,insets);
    wrapped(1665,768,0,0,1665,768,1665,768);render_frame();
    assert(get_double(quest,0x490)==106&&get_double(quest,0x498)==-66);
    assert(get_double(sit,0x490)==20&&get_double(sit,0x498)==191);
    assert(get_double(run_button,0x490)==20&&get_double(run_button,0x498)==121);
    for (int i=0;i<10000;i++) render_frame();
    assert(get_double(quest,0x490)==106&&get_double(sit,0x498)==191&&get_double(run_button,0x498)==121);

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
    assert(get_double(quest,0x490)==105); // Old text-only behavior; tabs not yet registered.
    provide_tabs=true;render_frame();
    assert(get_double(quest,0x490)==147); // Common +81 from the leftmost tab.
    assert(get_double(quest_tmp,0x490)==147);
    for (unsigned i=0;i<4;i++) {
        assert(get_double(mission_tabs[i],0x490)==tab_x[i]+81);
        assert(get_double(mission_tabs[i],0x498)==tab_y[i]);
    }
    assert(get_double(mission_tabs[0],0x490)+41==get_double(quest,0x490)); // Same text/tab spacing.
    for (int i=0;i<10000;i++) render_frame();
    assert(get_double(quest,0x490)==147&&get_double(mission_tabs[0],0x490)==106);
    assert(memcmp(tab_parent,saved_parent,sizeof(tab_parent))==0);
    for (unsigned i=0;i<sizeof(avatar);i++) assert(avatar[i]==0x55&&age_logo[i]==0x66&&movement[i]==0x77);
    put_double(mission_tabs[0],0x498,-110);render_frame(); // A Y-only edit must not retain X shift.
    tl_javaapp_set_insets(0,0,0,0,0);render_frame();
    assert(get_double(quest,0x490)==66&&get_double(mission_tabs[0],0x490)==25);
    assert(get_double(mission_tabs[0],0x498)==-110);
    tl_javaapp_set_insets(0,0,177,0,0);render_frame();
    assert(get_double(quest,0x490)==66&&get_double(mission_tabs[0],0x490)==25);
    tl_javaapp_set_insets(177,0,0,0,0);render_frame();
    assert(get_double(quest,0x490)==147&&get_double(mission_tabs[0],0x490)==106);
    assert(get_double(sit,0x498)==191&&get_double(run_button,0x498)==121);
    provide_tabs=provide_tmp=false;
    // Missing groups/exports are inert; later-created script groups are retried.
    window_at(quest,66,-66,275,204);provide_groups=false;begin(2796,1290,insets);
    wrapped(1665,768,0,0,1665,768,1665,768);render_frame();assert(get_double(quest,0x490)==66);
    provide_groups=true;render_frame();assert(get_double(quest,0x490)==106);
    const char *exports[]={"g_UIManager","_ZN9KUIManage10GetUIGroupEPKc","_ZN8KUIGroup9GetWindowEPKc","_ZN10KWndWindow11SetPositionEii"};
    for (unsigned i=0;i<4;i++) {
        window_at(quest,66,-66,275,204);missing_export=exports[i];begin(2796,1290,insets);
        wrapped(1665,768,0,0,1665,768,1665,768);unsigned before=renders;render_frame();
        assert(get_double(quest,0x490)==66&&renders==before+1);
    }
    missing_export=NULL;
    double dx,dy;
    assert(!tl_quest_delta((tl_quest_rect){0,0,932,430},(tl_quest_rect){0,100,900,200},(tl_quest_cutout){59,0,0,0},932,430,&dx,&dy));
    puts("PASS: selected HUD uses the registry/object ABI, moves separate Mission/Team tabs and text together at the observed negative scene origin, preserves spacing/other HUD/IME, handles missing/late groups and exports, and avoids drift over 10000 updates");
    return 0;
}
