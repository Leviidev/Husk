#!/bin/bash
# Phase 0 guest: a minimal aarch64 Linux that proves JIT -> TCG -> display -> input.
#
# Alpine, not Debian. The brief suggested a Debian cloud image, but for a first
# bring-up Alpine's netboot flavour is strictly better: a 15 MB kernel + initramfs
# that boots directly via -kernel/-initrd with no UEFI firmware, no disk image, no
# bootloader and no installer in the way. Under TCG on a phone, every second of
# guest boot costs real time, and every extra moving part is one more thing that
# can fail ambiguously on the first run.
#
# The edk2 firmware is also unpacked, because the ISO path is what Phase 1 will
# need once we boot a real Android image.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HUSK_ROOT/guests/phase0"
mkdir -p "$OUT"

ALPINE_VER="3.24.1"
ALPINE_BRANCH="v3.24"
BASE="https://dl-cdn.alpinelinux.org/alpine/$ALPINE_BRANCH/releases/aarch64"
NETBOOT="alpine-netboot-$ALPINE_VER-aarch64.tar.gz"

cd "$OUT"

if [ ! -s "$NETBOOT" ]; then
    echo "[get ] $NETBOOT"
    curl -fL --retry 3 -o "$NETBOOT.part" "$BASE/$NETBOOT"
    mv "$NETBOOT.part" "$NETBOOT"
else
    echo "[skip] $NETBOOT"
fi

echo "[sha ] verifying"
curl -fsL "$BASE/$NETBOOT.sha256" -o "$NETBOOT.sha256"
shasum -a 256 -c "$NETBOOT.sha256"

if [ ! -f vmlinuz-virt ]; then
    echo "[tar ] unpacking kernel + initramfs"
    tar -xf "$NETBOOT"
    find . -name 'vmlinuz-virt' -exec cp {} ./vmlinuz-virt \; 2>/dev/null || true
    find . -name 'initramfs-virt' -exec cp {} ./initramfs-virt \; 2>/dev/null || true
fi

# edk2 firmware, for the Phase 1 ISO/disk path.
FW_SRC="$HUSK_ROOT/third_party/build/qemu-10.0.12-utm/pc-bios/edk2-aarch64-code.fd.bz2"
if [ -f "$FW_SRC" ] && [ ! -f edk2-aarch64-code.fd ]; then
    echo "[bz2 ] unpacking edk2-aarch64-code.fd"
    bunzip2 -c "$FW_SRC" > edk2-aarch64-code.fd
fi

echo "--- guests/phase0 ---"
ls -la "$OUT" | grep -vE '^total|\.part$'
