#!/bin/bash
# Copy Husk's own sources into the QEMU tree and wire them into its meson build.
# Idempotent: safe to re-run after editing anything in src/ios-jit/.
set -euo pipefail
HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="$HUSK_ROOT/third_party/build/qemu-10.0.12-utm"

[ -d "$Q" ] || { echo "QEMU source tree missing; run fetch_sources.sh first" >&2; exit 1; }

echo "[cp  ] JIT substrate -> tcg/"
cp "$HUSK_ROOT/src/ios-jit/husk-ios-jit.c" \
   "$HUSK_ROOT/src/ios-jit/husk-ios-jit.h" \
   "$HUSK_ROOT/src/ios-jit/husk-brk.S" "$Q/tcg/"

# region.c is patched in place rather than copied, so a re-extracted QEMU tree
# would silently lose it -- and the symptom (QEMU falling back to an RWX mmap and
# failing with EPERM) does not obviously point back here. Apply it idempotently.
if grep -q "alloc_code_gen_buffer_splitwx_husk_ios" "$Q/tcg/region.c"; then
    echo "[skip] tcg/region.c already patched"
else
    echo "[patch] tcg/region.c <- husk-qemu-ios-jit.patch"
    # The file is named rather than taken from the patch's headers, which
    # carry paths from the machine the patch was made on (/tmp/region.c.orig).
    # No -p level finds those anywhere else, so a clean tree could never be
    # patched -- only a tree that already had been, which this step skips.
    patch --silent "$Q/tcg/region.c" < "$HUSK_ROOT/patches/husk-qemu-ios-jit.patch" \
      || { echo "  FAILED to apply region.c patch" >&2; exit 1; }
fi

echo "[cp  ] display bridge -> ui/"
cp "$HUSK_ROOT/src/ios-jit/husk-display.c" \
   "$HUSK_ROOT/src/ios-jit/husk-display.h" "$Q/ui/"

echo "[cp  ] GL display bridge -> ui/"
cp "$HUSK_ROOT/src/ios-jit/husk-display-gl.c" \
   "$HUSK_ROOT/src/ios-jit/husk-display-gl.h" "$Q/ui/"

echo "[cp  ] audio backend -> audio/"
cp "$HUSK_ROOT/src/ios-jit/husk-audio.c" \
   "$HUSK_ROOT/src/ios-jit/husk-audio.h" "$Q/audio/"

echo "[cp  ] balloon control -> system/"
cp "$HUSK_ROOT/src/ios-jit/husk-balloon.c" \
   "$HUSK_ROOT/src/ios-jit/husk-balloon.h" "$Q/system/"

echo "[cp  ] snapshot control -> system/"
cp "$HUSK_ROOT/src/ios-jit/husk-snapshot.c" \
   "$HUSK_ROOT/src/ios-jit/husk-snapshot.h" "$Q/system/"

python3 - "$Q" <<'PY'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

# tcg/meson.build: husk-ios-jit.c + husk-brk.S alongside region.c
p = q / "tcg/meson.build"
s = p.read_text()
if "husk-brk.S" not in s:
    old = "tcg_ss.add(files(\n"
    assert old in s
    # husk-ios-jit.c may already be present from an earlier run
    s = s.replace(old, old + "  'husk-brk.S',\n", 1)
    if "husk-ios-jit.c" not in s:
        s = s.replace(old, old + "  'husk-ios-jit.c',\n", 1)
    p.write_text(s)
    print("  tcg/meson.build: added husk sources")
else:
    print("  tcg/meson.build: already wired")

# ui/meson.build: the display bridge
p = q / "ui/meson.build"
s = p.read_text()
if "husk-display.c" not in s:
    old = "system_ss.add(files(\n"
    assert old in s, "ui/meson.build shape changed"
    s = s.replace(old, old + "  'husk-display.c',\n", 1)
    p.write_text(s)
    print("  ui/meson.build: added husk-display.c")
else:
    print("  ui/meson.build: already wired")

# husk-display-gl.c only builds with CONFIG_OPENGL, and it goes in the same
# block as QEMU's own shader.c/console-gl.c so it inherits that condition.
s = p.read_text()
if "husk-display-gl.c" not in s:
    old_gl = "if_true: files('shader.c', 'console-gl.c'))"
    assert old_gl in s, "ui/meson.build GL block shape changed"
    s = s.replace(old_gl, "if_true: files('shader.c', 'console-gl.c', 'husk-display-gl.c'))", 1)
    p.write_text(s)
    print("  ui/meson.build: added husk-display-gl.c (CONFIG_OPENGL)")
else:
    print("  ui/meson.build: GL bridge already wired")

# system/meson.build: balloon control. It lives here rather than in ui/ because
# it calls qmp_balloon(), which system/balloon.c defines.
p = q / "system/meson.build"
s = p.read_text()
if "husk-balloon.c" not in s:
    old = "system_ss.add(files(\n"
    assert old in s, "system/meson.build shape changed"
    s = s.replace(old, old + "  'husk-balloon.c',\n", 1)
    p.write_text(s)
    print("  system/meson.build: added husk-balloon.c")
else:
    print("  system/meson.build: already wired")

s = p.read_text()
if "husk-snapshot.c" not in s:
    old_s = "system_ss.add(files(\n"
    assert old_s in s
    s = s.replace(old_s, old_s + "  'husk-snapshot.c',\n", 1)
    p.write_text(s)
    print("  system/meson.build: added husk-snapshot.c")
else:
    print("  system/meson.build: snapshot already wired")
PY

# tcg/region.c: a second way to get executable memory.
#
# Patched here rather than in husk-qemu-ios-jit.patch because that patch is
# applied with `patch` and skipped once it has been, so an addition to it would
# never reach a tree that was already patched.
python3 - "$Q" <<'PY_JITFALLBACK'
import pathlib, sys
q = pathlib.Path(sys.argv[1])
p = q / "tcg/region.c"
s = p.read_text()

old = """        /*
         * If splitwx force-on (1), fail;
         * if splitwx default-on (-1), fall through to splitwx off.
         */
        if (splitwx > 0) {
            return -1;
        }
        error_free_or_abort(errp);"""
new = """        /*
         * If splitwx force-on (1), fail;
         * if splitwx default-on (-1), fall through to splitwx off.
         *
         * Husk: on iOS, fall through either way.
         *
         * The dual RW/RX mapping above needs StikDebug attached and actively
         * servicing brk traps. That is required on a device with TXM, and it is
         * merely one of two options everywhere else: with CS_DEBUGGED set, iOS
         * still honours a plain MAP_JIT mapping toggled with
         * pthread_jit_write_protect_np, which is the path below and the one UTM
         * and every other iOS emulator uses.
         *
         * Husk forces splitwx on because without it TCG goes straight to an RWX
         * mmap that iOS refuses -- but forcing it also threw away the MAP_JIT
         * route entirely. A build running inside a container app gets
         * CS_DEBUGGED without a trap servicer, so the first route fails and the
         * second, which would have worked, was never tried.
         */
#if defined(__APPLE__) && TARGET_OS_IPHONE
        fprintf(stderr, "[husk-jit] dual mapping unavailable; trying MAP_JIT "
                        "instead (works without a trap servicer, but not "
                        "on a device with TXM)\\n");
        fflush(stderr);
        error_free_or_abort(errp);
        splitwx = 0;
#else
        if (splitwx > 0) {
            return -1;
        }
        error_free_or_abort(errp);
#endif"""
if old in s:
    s = s.replace(old, new, 1)
    # The MAP_JIT flag is gated on the ORIGINAL splitwx argument, so clearing the
    # local is only half of it if that test reads the parameter -- it does not,
    # it reads the same local, so this is complete.
    p.write_text(s)
    print("  tcg/region.c: iOS falls back to MAP_JIT when the dual mapping fails")
elif "trying MAP_JIT" in s:
    print("  tcg/region.c: MAP_JIT fallback already present")
else:
    raise SystemExit("tcg/region.c: splitwx fallback shape changed")
PY_JITFALLBACK

# Audio: a host backend for iOS, and the QAPI name that lets -audiodev pick it.
#
# QEMU's audio subsystem is already compiled in -- mixer, voices, devices. The
# only thing missing was a backend: the tree ships coreaudio for macOS and its
# AudioUnit subtype does not exist on iOS. Registering a driver is not enough on
# its own, because -audiodev is parsed through a QAPI union whose discriminator
# is a closed enum, so an unknown name is rejected before any driver is
# consulted.
python3 - "$Q" <<'PY_AUDIO'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

p = q / "audio/meson.build"
s = p.read_text()
old = """  'noaudio.c',"""
new = """  'husk-audio.c',
  'noaudio.c',"""
if old in s and "husk-audio.c" not in s:
    p.write_text(s.replace(old, new, 1))
    print("  audio/meson.build: added husk-audio.c")
elif "husk-audio.c" in s:
    print("  audio/meson.build: audio backend already wired")
else:
    raise SystemExit("audio/meson.build: shape changed")

# Adding a driver to the enum is not enough: two switches in the audio core
# enumerate every driver by hand, and neither has a default. audio_get_pdo_in()
# and audio_get_pdo_out() fall off the end of theirs into a bare abort() -- no
# message, no assertion text, nothing on stderr. That is what a new backend gets
# for existing in the enum and nowhere else, and it cost six builds to find
# because there was no error to read.
p = q / "audio/audio_template.h"
s = p.read_text()
old = """    case AUDIODEV_DRIVER_WAV:
        return dev->u.wav.TYPE;"""
new = """    case AUDIODEV_DRIVER_HUSK:
        return dev->u.husk.TYPE;

    case AUDIODEV_DRIVER_WAV:
        return dev->u.wav.TYPE;"""
if "AUDIODEV_DRIVER_HUSK" in s:
    print("  audio/audio_template.h: already knows the husk driver")
elif old in s:
    p.write_text(s.replace(old, new, 1))
    print("  audio/audio_template.h: audio_get_pdo_* knows the husk driver")
else:
    raise SystemExit("audio/audio_template.h: switch shape changed")

# The other switch allocates the per-direction options. Without a case here the
# in/out pointers stay NULL, and the abort above is reached before anything
# dereferences them -- but fixing only one of the two would move the crash
# rather than remove it.
p = q / "audio/audio.c"
s = p.read_text()
old = """        CASE(NONE, none, );"""
new = """        CASE(NONE, none, );
        CASE(HUSK, husk, );"""
if old in s and "CASE(HUSK" not in s:
    p.write_text(s.replace(old, new, 1))
    print("  audio/audio.c: husk gets per-direction options allocated")
elif "CASE(HUSK" in s:
    print("  audio/audio.c: husk options already allocated")
else:
    raise SystemExit("audio/audio.c: driver switch shape changed")

# virtio-sound declares itself unmigratable, which makes every snapshot fail:
#
#   save FAILED: State blocked by non-migratable device 'virtio-sound-device'
#
# Nothing about the device is unsaveable in Husk's sense. It has no host-side
# state that a restore cannot rebuild -- unlike virgl, where textures live in
# the GPU. What it has is streams the guest opened, and on a restore Android
# reopens them the same way it does after any suspend. The blocker is there
# because upstream has not done the work to serialise stream position, not
# because the result is unsound for a machine that is frozen and thawed whole.
p = q / "hw/audio/virtio-snd.c"
s = p.read_text()
old = """static const VMStateDescription vmstate_virtio_snd = {
    .name = TYPE_VIRTIO_SND,
    .unmigratable = 1,"""
new = """static const VMStateDescription vmstate_virtio_snd = {
    .name = TYPE_VIRTIO_SND,
    /*
     * Husk: left ALONE. This was set to 0 to stop the device blocking
     * snapshots, and the result was a SIGSEGV inside virtio_save() on the
     * first save:
     *
     *   virtio_save + 480
     *   vmstate_save_state_v / vmstate_save
     *   qemu_savevm_state_complete_precopy_non_iterable
     *   save_snapshot / husk_save_bh
     *
     * The blocker is not bureaucracy. virtio-snd holds in-flight buffers that
     * reference guest elements, and its vmstate has no fields describing them,
     * so serialising it walks into memory nothing has written. Refusing the
     * save is the correct behaviour; crashing is what removing the refusal
     * bought. Sound and snapshots stay mutually exclusive until someone does
     * the work upstream.
     */
    .unmigratable = 1,"""
if old in s:
    p.write_text(s.replace(old, new, 1))
    print("  hw/audio/virtio-snd.c: blocker left in place, with the reason recorded")
elif "left ALONE" in s:
    print("  hw/audio/virtio-snd.c: already annotated")
else:
    raise SystemExit("hw/audio/virtio-snd.c: vmstate shape changed")

p = q / "qapi/audio.json"
s = p.read_text()
if "'husk'" not in s:
    old = """            'spice': { 'type': 'AudiodevGenericOptions',
                   'if': 'CONFIG_SPICE' },"""
    # enum first
    e_old = """{ 'enum': 'AudiodevDriver',
  'data': [ 'none',"""
    e_new = """{ 'enum': 'AudiodevDriver',
  'data': [ 'none',
            'husk',"""
    if e_old not in s:
        raise SystemExit("qapi/audio.json: enum shape changed")
    s = s.replace(e_old, e_new, 1)

    u_old = """  'data': {
    'none':      'AudiodevGenericOptions',"""
    u_new = """  'data': {
    'none':      'AudiodevGenericOptions',
    'husk':      'AudiodevGenericOptions',"""
    if u_old not in s:
        raise SystemExit("qapi/audio.json: union shape changed")
    s = s.replace(u_old, u_new, 1)
    p.write_text(s)
    print("  qapi/audio.json: -audiodev husk is now a valid driver")
else:
    print("  qapi/audio.json: husk driver already declared")
PY_AUDIO

# Trace the virtio-sound TX path.
#
# The guest opens a stream, QEMU enables the voice, and not one byte of PCM
# reaches the backend -- "audio 0 frames in" in every window. The gap is between
# the guest queueing data and the audio core asking for it, and neither log
# could say which side is silent. These counters can.
python3 - "$Q" <<'PY_SNDTRACE'
import pathlib, sys
q = pathlib.Path(sys.argv[1])
p = q / "hw/audio/virtio-snd.c"
s = p.read_text()

# Where the guest hands us PCM, and where we hand it to the audio core. Between
# those two the data either arrives or it does not, and nothing in the log has
# been able to say which.
old = """static void virtio_snd_pcm_out_cb(void *data, int available)
{
    VirtIOSoundPCMStream *stream = data;
    VirtIOSoundPCMBuffer *buffer;
    size_t size;
"""
new = """static void virtio_snd_pcm_out_cb(void *data, int available)
{
    VirtIOSoundPCMStream *stream = data;
    VirtIOSoundPCMBuffer *buffer;
    size_t size;
    static uint64_t cb_calls, cb_written, cb_empty;

    cb_calls++;
    if (QSIMPLEQ_EMPTY(&stream->queue)) {
        cb_empty++;
    }
    if (cb_calls <= 3 || (cb_calls % 500) == 0) {
        fprintf(stderr, "[husk-snd] out_cb #%llu available=%d queue=%s "
                        "active=%d (empty %llu times, %llu bytes written)\\n",
                (unsigned long long)cb_calls, available,
                QSIMPLEQ_EMPTY(&stream->queue) ? "EMPTY" : "has buffers",
                stream->active ? 1 : 0,
                (unsigned long long)cb_empty,
                (unsigned long long)cb_written);
        fflush(stderr);
    }
"""
if old in s and "[husk-snd] out_cb" not in s:
    s = s.replace(old, new, 1)
elif "[husk-snd] out_cb" in s:
    print("  hw/audio/virtio-snd.c: already traced")
    raise SystemExit
else:
    raise SystemExit("hw/audio/virtio-snd.c: out_cb shape changed")

old = """                buffer->size -= size;
                buffer->offset += size;"""
new = """                cb_written += size;
                buffer->size -= size;
                buffer->offset += size;"""
assert old in s
s = s.replace(old, new, 1)

old = """static inline void return_tx_buffer(VirtIOSoundPCMStream *stream,
                                    VirtIOSoundPCMBuffer *buffer)
{"""
new = """static inline void return_tx_buffer(VirtIOSoundPCMStream *stream,
                                    VirtIOSoundPCMBuffer *buffer)
{
    {
        static uint64_t returns;
        if (++returns <= 3 || (returns % 200) == 0) {
            fprintf(stderr, "[husk-snd] returned %llu tx buffer(s) to the "
                            "guest\\n", (unsigned long long)returns);
            fflush(stderr);
        }
    }
"""
if old in s:
    s = s.replace(old, new, 1)
else:
    raise SystemExit("hw/audio/virtio-snd.c: return_tx_buffer shape changed")

old = """static void virtio_snd_handle_tx_xfer(VirtIODevice *vdev, VirtQueue *vq)
{"""
new = """static void virtio_snd_handle_tx_xfer(VirtIODevice *vdev, VirtQueue *vq)
{
    {
        static uint64_t tx_calls;
        if (++tx_calls <= 3 || (tx_calls % 500) == 0) {
            fprintf(stderr, "[husk-snd] tx_xfer #%llu (the guest is queueing "
                            "PCM)\\n", (unsigned long long)tx_calls);
            fflush(stderr);
        }
    }
"""
assert old in s
p.write_text(s.replace(old, new, 1))
print("  hw/audio/virtio-snd.c: TX and out_cb traced")
PY_SNDTRACE

# virtio-gpu: let a virgl-enabled machine be snapshotted, opt-in.
#
# Patched in place rather than shipped as a .patch because a re-extracted QEMU
# tree would silently lose it, and the symptom -- "Migration is disabled when
# virgl is enabled" from save_snapshot -- points at migration rather than here.
python3 - "$Q" <<'PY_VIRGL'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

p = q / "hw/display/virtio-gpu-base.c"
s = p.read_text()
old = """    if (virtio_gpu_virgl_enabled(g->conf)) {
        error_setg(&g->migration_blocker, "virgl is not yet migratable");
        if (migrate_add_blocker(&g->migration_blocker, errp) < 0) {
            return false;
        }
    }"""
new = """    /*
     * Husk: the blocker is opt-out, via HUSK_VIRGL_SNAPSHOT.
     *
     * Upstream refuses to migrate a virgl-enabled GPU because virglrenderer
     * holds the 3D state -- contexts, shaders, textures -- and none of it is
     * serialisable. virtio_gpu_save() only ever walks g->reslist and writes out
     * 2D resources, so a machine saved while the guest holds 3D resources comes
     * back with the guest believing in resource ids the fresh virglrenderer has
     * never heard of. That is not a theoretical hazard; it is exactly the
     * "virgl_cmd_set_scanout: illegal resource specified" failure Husk hit.
     *
     * The blocker is therefore correct in general and unnecessary in the one
     * case Husk cares about: a guest holding NO 3D resources. Husk snapshots
     * with the Android framework stopped, which closes every DRM file and frees
     * every 3D context with it, leaving only the kernel's own 2D framebuffer --
     * precisely what virtio_gpu_save() already handles. Restoring then starts
     * the framework again and it builds its context against a fresh renderer.
     *
     * Whoever sets this variable is promising that invariant holds. Nothing
     * here can check it, so it stays opt-in rather than becoming the default.
     */
    if (virtio_gpu_virgl_enabled(g->conf) && !getenv("HUSK_VIRGL_SNAPSHOT")) {
        error_setg(&g->migration_blocker, "virgl is not yet migratable");
        if (migrate_add_blocker(&g->migration_blocker, errp) < 0) {
            return false;
        }
    }"""
if old in s:
    p.write_text(s.replace(old, new, 1))
    print("  virtio-gpu-base.c: virgl migration blocker is now opt-out")
elif "HUSK_VIRGL_SNAPSHOT" in s:
    print("  virtio-gpu-base.c: already patched")
else:
    raise SystemExit("virtio-gpu-base.c: blocker shape changed")

# A save must never abort the process. Husk's one hard rule is no crashes, and
# assert() in a save path turns "this snapshot cannot be taken" into a dead app.
p = q / "hw/display/virtio-gpu.c"
s = p.read_text()
old = """    /* in 2d mode we should never find unprocessed commands here */
    assert(QTAILQ_EMPTY(&g->cmdq));"""
new = """    /*
     * Husk: refuse the save rather than abort the process.
     *
     * Upstream asserts because in 2d mode a pending command is impossible. With
     * virgl enabled it is merely unlikely, and an assertion that fires here
     * kills the app outright -- the one failure mode Husk will not ship. Failing
     * the save leaves the previous snapshot intact and says so in the log.
     */
    if (!QTAILQ_EMPTY(&g->cmdq)) {
        qemu_log_mask(LOG_GUEST_ERROR,
                      "virtio-gpu: refusing to save with commands still queued\\n");
        return -EBUSY;
    }"""
# Every occurrence, not the first.
#
# virtio_gpu_save() and virtio_gpu_blob_save() open with the same assert, so
# replacing once patched a different function on each run -- the tree depended
# on how many times this script had been run, which is the one thing an
# idempotent script must not do. Both are save paths and neither may abort.
if old in s:
    n = s.count(old)
    p.write_text(s.replace(old, new))
    print(f"  virtio-gpu.c: {n} save path(s) refuse instead of asserting")
elif "refusing to save with commands still queued" in s:
    print("  virtio-gpu.c: already patched")
else:
    raise SystemExit("virtio-gpu.c: save shape changed")

# Re-read: the edit above went straight to disk, so `s` no longer matches the
# file and writing it back at the end would undo it.
s = p.read_text()

# --- 1. save: never dereference a virgl resource's absent pixman image -------
old = """    QTAILQ_FOREACH(res, &g->reslist, next) {
        if (res->blob_size) {
            continue;
        }
        qemu_put_be32(f, res->resource_id);"""
new = """    QTAILQ_FOREACH(res, &g->reslist, next) {
        if (res->blob_size) {
            continue;
        }
        if (!res->image) {
            /*
             * Husk: a virgl-backed resource, so skip it.
             *
             * virtio_gpu_save() was written for the 2d device, where every
             * non-blob resource owns a pixman image holding its pixels. Under
             * virgl the pixels live in the host renderer instead and ->image
             * stays NULL -- including for plain CREATE_RESOURCE_2D, which
             * virtio-gpu-virgl.c still routes to virgl_renderer_resource_create.
             * The two pixman calls at the end of this loop then dereference
             * NULL and take the whole process down.
             *
             * There is nothing to write: the contents are host GPU state, which
             * is the same reason virgl carries a migration blocker upstream.
             * Skipping keeps the stream well formed, and the resource simply
             * does not exist in the restored machine. virtio_gpu_post_load()
             * below clears any scanout left pointing at one.
             */
            qemu_log_mask(LOG_GUEST_ERROR,
                          "virtio-gpu: resource %u (%ux%u fmt %u) not saved; "
                          "its pixels live in the host renderer\\n",
                          res->resource_id, res->width, res->height,
                          res->format);
            continue;
        }
        qemu_put_be32(f, res->resource_id);"""
if old in s:
    s = s.replace(old, new, 1)
    print("  virtio-gpu.c: save skips virgl-backed resources")
elif "its pixels live in the host renderer" in s:
    print("  virtio-gpu.c: save already skips virgl-backed resources")
else:
    raise SystemExit("virtio-gpu.c: save loop shape changed")

# --- 2. post_load: a skipped resource must not fail the whole restore -------
old = """        res = virtio_gpu_find_resource(g, scanout->resource_id);
        if (!res) {
            return -EINVAL;
        }"""
new = """        res = virtio_gpu_find_resource(g, scanout->resource_id);
        if (!res) {
            /*
             * Husk: the scanout pointed at a resource the save skipped above.
             * Refusing the restore over it would strand the machine; blanking
             * the scanout costs one frame, because the guest issues SET_SCANOUT
             * again as soon as its compositor starts.
             */
            qemu_log_mask(LOG_GUEST_ERROR,
                          "virtio-gpu: scanout %d had resource %u, which was "
                          "not saved; starting it blank\\n",
                          i, scanout->resource_id);
            scanout->resource_id = 0;
            scanout->cursor.resource_id = 0;
            continue;
        }"""
if old in s:
    s = s.replace(old, new, 1)
    print("  virtio-gpu.c: post_load survives a skipped scanout resource")
elif "starting it blank" in s:
    print("  virtio-gpu.c: post_load already survives a skipped scanout resource")
else:
    raise SystemExit("virtio-gpu.c: post_load shape changed")

p.write_text(s)
PY_VIRGL

python3 - "$Q" <<'PY2'
import pathlib, sys
q = pathlib.Path(sys.argv[1])

# system/qemu.symbols is the authoritative export list for the shared-lib build.
# On darwin meson seds it into an ld64 -exported_symbols_list, so a symbol absent
# from here stays local no matter what visibility attribute it carries.
p = q / "system/qemu.symbols"
s = p.read_text()

# Drop husk symbols that no longer exist. Adding without pruning leaves stale
# names in the export list, and the link then fails with "Undefined symbols"
# for a function that was simply renamed.
import re as _re
_live = None
wanted = [
    "husk_display_init",
    "husk_display_lock_frame",
    "husk_display_unlock_frame",
    "husk_display_sequence",
    "husk_display_set_ui_size",
    "husk_display_guest_size",
    "husk_audio_pull",
    "husk_audio_active",
    "husk_audio_frames_in",
    "husk_audio_underruns",
    "husk_display_send_pointer",
    "husk_display_request_update",
    "husk_display_send_key",
    "husk_display_gl_create",
    "husk_display_gl_bind",
    "husk_display_gl_probe",
    "husk_display_gl_early",
    "husk_display_gl_frames",
    "husk_display_gl_set_metal_presenter",
    "husk_snapshot_save",
    "husk_snapshot_load_at_startup",
    "husk_balloon_set_bytes",
    "husk_ios_jit_prewarm",
    "husk_ios_jit_install_trap_handler",
    "husk_ios_jit_is_available",
    "husk_ios_jit_mapjit_works",
    "husk_ios_jit_detach",
    "husk_ios_jit_log_footprint",
    "husk_ios_available_memory",
]
_present = _re.findall(r"^\s*(husk_\w+);", s, _re.M)
for _stale in [n for n in _present if n not in wanted]:
    s = _re.sub(r"^\s*" + _stale + r";\n", "", s, flags=_re.M)
    print("  system/qemu.symbols: pruned stale " + _stale)

missing = [w for w in wanted if f"  {w};" not in s]
if missing:
    assert s.lstrip().startswith("{"), "qemu.symbols shape changed"
    idx = s.index("{") + 1
    s = s[:idx] + "\n" + "".join(f"  {w};\n" for w in missing) + s[idx:].lstrip("\n")
    p.write_text(s)
    print(f"  system/qemu.symbols: exported {len(missing)} husk symbols")
else:
    print("  system/qemu.symbols: already exported")
PY2

echo "[ok  ] integrated"
