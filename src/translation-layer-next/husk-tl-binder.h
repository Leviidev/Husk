/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef HUSK_TL_BINDER_H
#define HUSK_TL_BINDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * In-process /dev/binder. Threads in Husk talk Binder without a kernel driver.
 * Handle 0 is the context manager (ServiceManager). Other handles are local C
 * services or sessions that published a binder object.
 */

bool husk_binder_is_path(const char *path);
int husk_binder_open(void);
bool husk_binder_is_fd(int fd);
int husk_binder_ioctl(int fd, unsigned long req, void *arg);
void *husk_binder_mmap(int fd, size_t len);
void husk_binder_close(int fd);

typedef struct husk_binder_reply {
    uint8_t *data;
    size_t data_size;
    uint64_t *offsets;
    size_t n_offsets;
} husk_binder_reply;

typedef void (*husk_binder_local_fn)(uint32_t code, uint32_t flags,
                                     const uint8_t *data, size_t data_size,
                                     const uint64_t *offsets, size_t n_offsets,
                                     husk_binder_reply *reply, void *cookie);

/* Handle 0 if the table is full. */
uint32_t husk_binder_add_local(husk_binder_local_fn fn, void *cookie);
void husk_binder_set_local(uint32_t handle, husk_binder_local_fn fn, void *cookie);
void husk_binder_set_ptr(uint32_t handle, void *ptr);
void *husk_binder_ptr(uint32_t handle);

int husk_binder_transact(uint32_t handle, uint32_t code, uint32_t flags,
                         const uint8_t *data, size_t data_size,
                         const uint64_t *offsets, size_t n_offsets,
                         husk_binder_reply *reply);
void husk_binder_reply_free(husk_binder_reply *r);

#ifdef __cplusplus
}
#endif

#endif
