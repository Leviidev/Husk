/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Run on macOS from the repository root:
 * clang -fobjc-arc -Wno-deprecated-declarations -Isrc/translation-layer-next \
 *   -Isrc/translation-layer tests/translation-layer/web_scroll.m \
 *   -Wl,-undefined,dynamic_lookup -framework Foundation -framework WebKit \
 *   -framework AppKit -framework QuartzCore -o build/web-scroll-test
 * build/web-scroll-test
 */
#include "../../src/translation-layer-next/husk-tl-dvm-web.m"
#include <assert.h>

int main(void)
{
    g_scale = 2;
    jvalue a[8] = {0}, ret = {0};
    a[0].i = 1; a[1].i = 0; a[2].i = 120;
    a[3].i = 804; a[4].i = 1600; a[5].i = 1; a[6].i = 1;
    assert(W_frame(NULL, a, &ret));
    assert(tl_web_hit(150, 80));
    assert(tl_web_parent_scroll_axes(150, 80) == 0);  /* standalone page */
    a[7].i = 2;
    assert(W_frame(NULL, a, &ret));
    assert(tl_web_parent_scroll_axes(150, 80) == 2);  /* vertical Android list */
    assert(tl_web_parent_scroll_axes(20, 59) == 0);   /* before the page */
    assert(tl_web_parent_scroll_axes(402, 80) == 0);  /* half-open bounds */
    a[7].i = 3;
    assert(W_frame(NULL, a, &ret));
    assert(tl_web_parent_scroll_axes(150, 80) == 3);  /* nested directions */
    a[6].i = 0;
    assert(W_frame(NULL, a, &ret));
    assert(!tl_web_hit(150, 80));
    assert(tl_web_parent_scroll_axes(150, 80) == 0);  /* another window owns input */
    a[6].i = 1; a[5].i = 0;
    assert(W_frame(NULL, a, &ret));
    assert(tl_web_parent_scroll_axes(150, 80) == 0);  /* hidden page */
    a[5].i = 1;
    assert(W_frame(NULL, a, &ret));
    forget_hits(1);
    assert(g_nhits == 0);
    assert(tl_web_parent_scroll_axes(150, 80) == 0);  /* removed page */
    puts("PASS: parent scroll axes, standalone pages, scaling, bounds, input ownership, visibility and removal");
    return 0;
}
