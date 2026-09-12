#!/bin/bash
# Build and run the display-bridge probe against the macOS validation QEMU.
set -euo pipefail
HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$HUSK_ROOT/third_party/build/qemu-10.0.12-utm/_husk_macos/libqemu-aarch64-softmmu.dylib"
GUEST="$HUSK_ROOT/guests/phase0"
BIN="$HUSK_ROOT/build/husk_display_probe"
SECONDS_TO_RUN="${1:-45}"

[ -f "$DYLIB" ] || { echo "missing $DYLIB -- run scripts/build_macos_validation.sh" >&2; exit 1; }

mkdir -p "$HUSK_ROOT/build"
echo "compiling probe..."
clang -O1 -Wall -Wextra -o "$BIN" "$HUSK_ROOT/tests/husk_display_probe.c"

echo "running probe for ${SECONDS_TO_RUN}s..."
# Same guest and the same device set the iOS app uses -- see
# QemuRunner.phase0Arguments(). Kept in step by hand; if one changes, change both.
exec "$BIN" "$DYLIB" "$SECONDS_TO_RUN" \
    -M virt \
    -cpu cortex-a72 \
    -smp 4 \
    -m 1024 \
    -accel tcg,tb-size=256,thread=multi \
    -kernel "$GUEST/vmlinuz-virt" \
    -initrd "$GUEST/initramfs-virt" \
    -append "console=tty0 console=ttyAMA0" \
    -device virtio-gpu-pci \
    -device virtio-tablet-pci \
    -device virtio-keyboard-pci \
    -display none \
    -monitor none \
    -serial none \
    -no-reboot
