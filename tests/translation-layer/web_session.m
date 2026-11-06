/* SPDX-License-Identifier: GPL-2.0-or-later */
/* A stopped Java owner must not strand WebKit completions or deliver queued work to a later session. */
#include "../../src/translation-layer-next/husk-tl-dvm-web.m"
#include <assert.h>

static void drain_main(void)
{
    __block bool done = false;
    dispatch_async(dispatch_get_main_queue(), ^{ done = true; });
    while (!done) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, true);
}

int main(void)
{
    @autoreleasepool {
        g_views = [NSMutableDictionary dictionary]; g_pending = [NSMutableDictionary dictionary];
        HuskWeb *first = [HuskWeb new], *second = [HuskWeb new]; first.wid = 1; second.wid = 2;
        g_views[@1] = first; g_views[@2] = second;
        int counts[5] = {0}; int *calls = counts;
        for (int kind = WAIT_NAV; kind <= WAIT_SCHEME; kind++) {
            int captured = kind;
            [first park:[NSObject new] kind:kind cancel:^{ calls[captured]++; }];
        }
        __block int second_calls = 0;
        int old_id = [second park:[NSObject new] kind:WAIT_NAV cancel:^{ second_calls++; }];
        [first teardown]; [first teardown];
        for (int kind = 0; kind < 5; kind++) assert(calls[kind] == 1);
        assert(second_calls == 0 && g_pending.count == 1);
        __block bool stale_work = false;
        on_main(^{ stale_work = true; });
        reset_web_session();
        drain_main();
        assert(!stale_work && second_calls == 1 && g_pending.count == 0);
        HuskWeb *next = [HuskWeb new]; next.wid = 1;
        __block int next_calls = 0;
        int new_id = [next park:[NSObject new] kind:WAIT_NAV cancel:^{ next_calls++; }];
        assert(new_id > old_id); /* late Java replies cannot collide with new requests */
        cancel_pending(0); cancel_pending(0);
        assert(next_calls == 1 && second_calls == 1);
        puts("PASS: every WebKit completion canceled once, per-view isolation, stale queued work discarded, request IDs never reused");
    }
}
