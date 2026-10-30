/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_SERVICEMANAGER_H
#define HUSK_TL_SERVICEMANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Handle 0: android.os.IServiceManager (addService / getService / checkService / list). */
void husk_sm_start(void);
bool husk_sm_add(const char *name, uint32_t handle);
uint32_t husk_sm_get(const char *name);
int husk_sm_list(char *buf, int cap); /* names, one per line; returns count */

#ifdef __cplusplus
}
#endif

#endif
