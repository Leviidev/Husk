/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Compile without ARC to exercise actual Foundation autorelease/dealloc. */
#import <Foundation/Foundation.h>
#include "husk-tl-autorelease.h"
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
static unsigned destroyed;
@interface TLPoolProbe : NSObject
@end
@implementation TLPoolProbe
- (void)dealloc { destroyed++; [super dealloc]; }
@end
static void early_return(void) { TL_AUTORELEASE_SCOPE(pool); [[[TLPoolProbe alloc] init] autorelease]; }
static void *worker(void *unused)
{
    (void)unused;
    {
        TL_AUTORELEASE_SCOPE(thread_pool);
        TLPoolProbe *retained=[[TLPoolProbe alloc] init];
        [[[TLPoolProbe alloc] init] autorelease]; // Initialization lives until thread scope ends.
        for (unsigned i=0;i<10000;i++) {
            {
                TL_AUTORELEASE_SCOPE(frame_pool);
                [[[TLPoolProbe alloc] init] autorelease];
                if (i%2) continue;
            }
            assert(destroyed==i+1);
        }
        assert(destroyed==10000);
        early_return();assert(destroyed==10001);
        assert([retained isKindOfClass:[TLPoolProbe class]]);
        [retained release];assert(destroyed==10002);
    }
    assert(destroyed==10003);
    return NULL;
}
int main(void)
{
    pthread_t thread;
    assert(pthread_create(&thread,NULL,worker,NULL)==0);
    assert(pthread_join(thread,NULL)==0);
    assert(destroyed==10003);
    puts("PASS: actual Apple runtime releases frame temporaries on a POSIX thread, retains owned objects and drains initialization/early-return pools");
    return 0;
}
