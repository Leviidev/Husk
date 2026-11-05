/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct { uint64_t *symcache; } tl_lib;
enum { R_ABS64=257, R_GLOB_DAT=1025, R_JUMP_SLOT=1026 };
static unsigned filtered;
static uint64_t resolved=0x1234;
static bool bind_fails;
static void *(*g_plt_filter)(tl_lib *,const char *,void *);
static uint64_t image_addr(tl_lib *l,uint64_t v) { (void)l;return v+0x100000; }
static uint64_t bind_symbol(tl_lib *l,unsigned i,bool *failed) { (void)l;(void)i;*failed=bind_fails;return resolved; }
static const char *sym_at(tl_lib *l,unsigned i) { (void)l;(void)i;return "target"; }
static const char *sym_name(tl_lib *l,const char *s) { (void)l;return s; }
static void *replacement(tl_lib *l,const char *name,void *p)
{ (void)l;(void)name;assert((uintptr_t)p==0x1234);filtered++;return (void *)(uintptr_t)0x5678; }
static bool slot(tl_lib *L,unsigned type,unsigned symidx,int64_t addend,uint64_t *place)
{
    bool failed=false;
    switch (type) {
#include "plt_slot_under_test.h"
    default:return false;
    }
}
int main(void)
{
    uint64_t cache=0, value=0;
    tl_lib lib={.symcache=&cache};
    g_plt_filter=replacement;
    assert(slot(&lib,R_ABS64,1,3,&value)&&value==0x1237&&filtered==0);
    assert(slot(&lib,R_GLOB_DAT,1,3,&value)&&value==0x1234&&filtered==0);
    assert(slot(&lib,R_JUMP_SLOT,1,0,&value)&&value==0x5678&&filtered==1);
    assert(slot(&lib,R_JUMP_SLOT,0,7,&value)&&value==0x100007&&filtered==1);
    resolved=1;assert(slot(&lib,R_JUMP_SLOT,1,0,&value)&&value==0&&filtered==1);
    resolved=0;assert(slot(&lib,R_JUMP_SLOT,1,0,&value)&&value==0&&filtered==1);
    resolved=0x1234;bind_fails=true;value=0xabcd;
    assert(!slot(&lib,R_JUMP_SLOT,1,0,&value)&&value==0xabcd&&filtered==1);
    bind_fails=false;g_plt_filter=NULL;
    assert(slot(&lib,R_JUMP_SLOT,1,0,&value)&&value==0x1234&&filtered==1);
    puts("PASS: production PLT slot filter preserves data relocations, addends, null/weak slots, failure paths and unfiltered sessions");
    return 0;
}
