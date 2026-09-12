# Patches

## `husk-qemu-ios-jit.patch`

Husk's own. Diverts `tcg/region.c`'s split-W^X allocator to
`alloc_code_gen_buffer_splitwx_husk_ios()` on iOS, which sources the executable
half from StikDebug rather than allocating it locally (TXM forbids the latter).
Applied by `scripts/integrate_husk.sh` along with copying `src/ios-jit/*` into the
QEMU tree.

GPL-2.0-or-later, same as the file it patches.

## `qemu-10.0.12-utm.patch`, `pixman-0.38.0.patch`, `libslirp-v4.9.1.patch`

Not ours. Taken verbatim from UTM's `patches/` directory
(https://github.com/utmapp/UTM), vendored so the build is reproducible without
cloning UTM.

These are patches against QEMU, pixman and libslirp, so they carry those
projects' licences rather than UTM's own Apache-2.0 — the QEMU patch is
GPL-2.0. This distinction matters: UTM's *application* code is Apache-2.0, which
is incompatible with GPLv2 and must not be copied into Husk. See
`docs/01-licensing.md`.

UTM's build script (`scripts/build_dependencies.sh`), which Husk's
`scripts/build_ios.sh` borrows its toolchain setup from, is ISC-licensed
(credited to Angelo Haller, 2014) and therefore permissive.
