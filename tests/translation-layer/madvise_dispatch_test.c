/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Real syscall dispatch, madvise validation and anon_zap control flow. The host
 * VM calls below model one tracked anonymous region without allocating pages on
 * the iPhone; no guest object lifetime or collector behavior is changed. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef PAGE
#define PAGE 16384u
#endif
#define MAX_ANON 8
#define PROT_READ 1
#define PROT_WRITE 2
#define MAP_FIXED 16
#define MAP_PRIVATE 2
#define MAP_ANON 4096
#define KERN_SUCCESS 0
#define VM_REGION_BASIC_INFO_64 9
#define VM_REGION_BASIC_INFO_COUNT_64 10
#define VM_PROT_READ 1
#define VM_PROT_WRITE 2
typedef uintptr_t vm_address_t;
typedef size_t vm_size_t;
typedef unsigned mach_msg_type_number_t;
typedef unsigned mach_port_t;
typedef void *vm_region_info_t;
typedef struct { int protection; } vm_region_basic_info_data_64_t;
static struct { uintptr_t addr; size_t len; } g_anon[MAX_ANON];
static int g_nanon, g_anon_lock;
/* COFF caps static alignment at8192. Align within backing storage so the same
 *16KiB page layout is exercised on Windows and Apple hosts. */
static unsigned char backing[PAGE * 4];
static unsigned char *pages;
static int guest_errno, maps, acquired, released;
static bool image_protected;
static int pthread_mutex_lock(int *lock) { (void)lock; return 0; }
static int pthread_mutex_unlock(int *lock) { (void)lock; return 0; }
static int mach_task_self(void) { return 1; }
static int vm_region_64(int task, vm_address_t *a, vm_size_t *s, int flavor,
    vm_region_info_t info, mach_msg_type_number_t *n, mach_port_t *object)
{
    (void)task; (void)flavor; (void)n;
    *a = (uintptr_t)pages; *s = PAGE * 3;
    ((vm_region_basic_info_data_64_t *)info)->protection = VM_PROT_READ | VM_PROT_WRITE;
    *object = 42; acquired++;
    return KERN_SUCCESS;
}
static __attribute__((unused)) int mach_port_deallocate(int task, mach_port_t object)
{ (void)task; assert(object == 42); released++; return 0; }
static bool tl_ld_covers(const void *a, size_t n) { (void)a; (void)n; return image_protected; }
static bool tl_xmem_contains(const void *a) { (void)a; return false; }
static void *mmap(void *a, size_t n, int prot, int flags, int fd, long off)
{
    (void)prot; (void)flags; (void)fd; (void)off;
    assert((uintptr_t)a >= (uintptr_t)pages + PAGE);
    assert((uintptr_t)a + n <= (uintptr_t)pages + PAGE * 2);
    memset(a, 0, n); maps++; return a;
}
static __attribute__((unused)) int getpagesize(void) { return PAGE; }
static __attribute__((unused)) int *tl_guest_errno_ptr(void) { return &guest_errno; }
static __attribute__((unused)) void tl_set_guest_errno(int value) { guest_errno = value; }
static void mm_trace(const char *what, void *a, size_t n, long x, long y)
{ (void)what; (void)a; (void)n; (void)x; (void)y; }

/* Generated directly from the current production function bodies and case233. */
#include "madvise_under_test.h"

static void check_neighbors(void)
{
    for (size_t i = 0; i < PAGE; i++) {
        assert(pages[i] == 0x5a);
        assert(pages[PAGE * 2 + i] == 0x5a);
    }
}

int main(void)
{
    pages = (unsigned char *)(((uintptr_t)backing + PAGE - 1) & ~(uintptr_t)(PAGE - 1));
    unsigned char *middle = pages + PAGE;
    g_anon[0].addr = (uintptr_t)middle; g_anon[0].len = PAGE; g_nanon = 1;
    memset(pages, 0x5a, PAGE * 3); guest_errno = 7;
    assert(syscall_madvise_under_test((intptr_t)middle, PAGE, 4, 233) == 0);
    assert(maps == 1); /* old raw dispatch falsely succeeded without discard */
    assert(acquired == released); /* vm_region's object port must be released */
    for (size_t i = 0; i < PAGE; i++) assert(middle[i] == 0);
    check_neighbors();
    assert(guest_errno == 7); /* successful syscalls preserve guest errno */

    memset(middle, 0x5a, PAGE);
    assert(syscall_madvise_under_test((intptr_t)middle + 1, PAGE - 1, 4, 233) == -22);
    assert(guest_errno == 22 && maps == 1);
    assert(middle[0] == 0x5a); check_neighbors();
    guest_errno = 9;
    assert(syscall_madvise_under_test((intptr_t)middle, 0, 4, 233) == 0);
    assert(syscall_madvise_under_test(0, 0, 4, 233) == 0);
    assert(maps == 1 && guest_errno == 9);
    assert(syscall_madvise_under_test((intptr_t)(UINTPTR_MAX & ~(uintptr_t)(PAGE - 1)), PAGE * 2, 4, 233) == -22);
    assert(syscall_madvise_under_test((intptr_t)middle, -1, 4, 233) == -22);
    assert(maps == 1);

    assert(syscall_madvise_under_test((intptr_t)middle, 1, 4, 233) == 0);
    assert(maps == 2); /* Linux rounds length up; neighbors remain untouched. */
    for (size_t i = 0; i < PAGE; i++) assert(middle[i] == 0);
    check_neighbors();
    memset(middle, 0x5a, PAGE); image_protected = true;
    assert(syscall_madvise_under_test((intptr_t)middle, PAGE, 4, 233) == 0);
    assert(maps == 2 && middle[0] == 0x5a && acquired == released);
    image_protected = false; guest_errno = 17;
    assert(b_madvise(middle, PAGE, 4) == 0);
    assert(maps == 3 && middle[0] == 0 && guest_errno == 17 && acquired == released);
    puts("PASS: raw madvise discards tracked pages, validates ranges and releases VM query ports");
    return 0;
}
