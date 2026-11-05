/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Windows lock model for the synchronous profile contracts; not a race detector. */
#ifndef QUEST_TEST_PTHREAD_H
#define QUEST_TEST_PTHREAD_H
#ifndef _WIN32
#include_next <pthread.h>
#else
#include <assert.h>
typedef struct { int held; } pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
static inline int pthread_mutex_lock(pthread_mutex_t *m) { assert(!m->held); m->held=1; return 0; }
static inline int pthread_mutex_unlock(pthread_mutex_t *m) { assert(m->held); m->held=0; return 0; }
#endif
#endif
