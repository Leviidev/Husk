#!/bin/bash
# Build the SAME patched QEMU for macOS, purely to validate the display bridge.
#
# husk-display.c is platform-neutral, and husk-ios-jit.c compiles to stubs off
# iOS (region.c's iOS branch is guarded on TARGET_OS_IPHONE, so macOS keeps
# QEMU's normal vmremap splitwx). That means a macOS build exercises the exact
# DisplayChangeListener code the iPhone will run, and can be driven headless
# here instead of waiting for hardware.
#
# Uses Homebrew's glib/pixman rather than our iOS sysroot -- this build is a test
# fixture, not a shipping artifact.
set -euo pipefail
HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
Q="$HUSK_ROOT/third_party/build/qemu-10.0.12-utm"
BUILD="$Q/_husk_macos"
LOG="$HUSK_ROOT/build/logs/qemu-macos.log"
mkdir -p "$HUSK_ROOT/build/logs"

"$HUSK_ROOT/scripts/integrate_husk.sh" >/dev/null

if [ ! -f "$BUILD/build.ninja" ]; then
    echo "configuring macOS validation build..."
    rm -rf "$BUILD"; mkdir -p "$BUILD"
    ( cd "$BUILD" && ../configure \
        --target-list=aarch64-softmmu \
        --enable-shared-lib \
        --disable-cocoa --disable-sdl --disable-gtk --disable-vnc --disable-spice \
        --disable-opengl --disable-virglrenderer --disable-curses --disable-curl \
        --disable-libusb --disable-usb-redir --disable-tpm --disable-docs \
        --disable-guest-agent --disable-tools --disable-hvf --disable-png --disable-pvg \
        --disable-debug-info ) > "$LOG" 2>&1 \
      || { echo "configure failed; see $LOG" >&2; tail -20 "$LOG" >&2; exit 1; }
fi

echo "building..."
( cd "$BUILD" && ninja libqemu-aarch64-softmmu.dylib ) >> "$LOG" 2>&1 \
  || { echo "build failed; see $LOG" >&2; tail -30 "$LOG" >&2; exit 1; }

echo "ok: $BUILD/libqemu-aarch64-softmmu.dylib"
