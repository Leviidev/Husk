/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Include the production allocator to exercise its reservation/trim seam. */
#include "../../src/translation-layer-next/husk-tl-bionic-io.c"
#include <assert.h>
void tl_log_line(const char *fmt, ...) { (void)fmt; }
int main(int argc, char **argv) {
    bool late = argc > 1 && !strcmp(argv[1], "late");
    size_t len = 2 * PHANTOM_HALF - getpagesize();
    assert(compressed_heap_request(len));
    assert(compressed_heap_request(2 * PHANTOM_HALF));
    assert(!compressed_heap_request(len - getpagesize()));
    if (!late) {
        husk_prepare_compressed_heap(); assert(g_compressed_heap);
        vm_address_t first = g_compressed_heap;
        husk_prepare_compressed_heap(); assert(g_compressed_heap == first);
        husk_release_unused_compressed_heap(); assert(!g_compressed_heap);
        husk_prepare_compressed_heap(); assert(g_compressed_heap);
    }
    vm_address_t alias = 0;
    assert(vm_map(mach_task_self(), &alias, (vm_size_t)512 << 20, 0,
        VM_FLAGS_ANYWHERE, MACH_PORT_NULL, 0, FALSE, VM_PROT_READ | VM_PROT_WRITE,
        VM_PROT_ALL, VM_INHERIT_COPY) == KERN_SUCCESS);
    void *p = phantom_reserve(len);
    if (late) {
        assert(p == MAP_FAILED);
        puts("PASS: late allocation fails in the constrained window");
    } else {
        assert(p != MAP_FAILED && !((uintptr_t)p % PHANTOM_HALF));
        assert(!g_compressed_heap);
        husk_release_unused_compressed_heap(); /* consumed cage must survive */
        assert(phantom_clamp((uintptr_t)p + PHANTOM_HALF, len - PHANTOM_HALF) == 0);
        assert(phantom_clamp((uintptr_t)p, len) == PHANTOM_HALF);
        assert(!mprotect(p, getpagesize(), PROT_READ | PROT_WRITE));
        assert(!mprotect((char *)p + PHANTOM_HALF - getpagesize(), getpagesize(), PROT_READ | PROT_WRITE));
        *(volatile char *)p = 42; *((volatile char *)p + PHANTOM_HALF - 1) = 43;
        assert(!munmap(p, PHANTOM_HALF));
        puts("PASS: early reservation, retry, single transfer, trim, commit at both ends");
    }
    assert(vm_deallocate(mach_task_self(), alias, (vm_size_t)512 << 20) == KERN_SUCCESS);
}
