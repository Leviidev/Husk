/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_GUEST_THREAD_H
#define HUSK_TL_GUEST_THREAD_H
#include <pthread.h>
/* Host-owned pumps which call guest function pointers participate in the same
   shutdown barrier as pthread_create and java.lang.Thread. */
int tl_guest_pthread_create(pthread_t *out, const pthread_attr_t *attr, void *(*fn)(void *), void *arg);
int tl_guest_pthread_join(pthread_t thread, void **result);
#endif
