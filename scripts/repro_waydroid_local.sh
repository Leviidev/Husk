#!/bin/bash
# Reproduce the phone's Waydroid failure on the Mac, under HVF.
#
# The guest is byte-identical to the one that ships: same Debian, same kernel,
# same Waydroid, same Android images (first-boot pulls them from our mirror, so
# the CPU model does not change which variant is used). Only the accelerator
# differs. That makes this the right place to debug the container, rather than
# rebuilding and reshipping an IPA for every hypothesis.
#
# -snapshot ALWAYS: first boot writes ~2.4 GB of Android into the disk, and
# without it that lands in the artifact being tested.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
G="$HUSK_ROOT/guests/phase1"
DISK="${1:-$G/husk-guest-release.qcow2}"
OUT="$HUSK_ROOT/build/logs/repro-waydroid.log"
SHARE="$G/work/repro-share"
RUNTIME="${REPRO_SECONDS:-900}"

mkdir -p "$SHARE" "$(dirname "$OUT")"
: > "$OUT"
# A BLANK vars store, not a copy of the bake's. UEFI records boot entries as PCI
# device paths, and this run adds a GPU, input devices and a 9p share that the
# bake did not have -- which renumbers the bus, invalidates the recorded entry,
# and drops the firmware into PXE. Starting blank makes it enumerate and find
# GRUB at the removable path (\EFI\BOOT\BOOTAA64.EFI), which survives reordering.
dd if=/dev/zero of="$G/work/repro-vars.fd" bs=1m count=64 2>/dev/null

echo "==> booting $DISK (log: $OUT, ${RUNTIME}s)"

# Device set copied from QemuRunner.phase1Arguments(): the PCI layout affects
# which /dev nodes appear, and /dev/dri/card0 is what cage needs.
qemu-system-aarch64 \
    -M virt,highmem=on \
    -cpu host -accel hvf \
    -smp 4 -m 1571 -snapshot \
    -drive if=pflash,format=raw,readonly=on,file="$G/edk2-aarch64-code.fd" \
    -drive if=pflash,format=raw,file="$G/work/repro-vars.fd" \
    -drive if=virtio,format=qcow2,file="$DISK" \
    -nic user,model=virtio-net-pci \
    -device virtio-gpu-pci \
    -device virtio-tablet-pci \
    -device virtio-keyboard-pci \
    -fsdev local,id=huskfs,path="$SHARE",security_model=none \
    -device virtio-9p-pci,fsdev=huskfs,mount_tag=husk \
    -display none \
    -serial file:"$OUT" \
    -no-reboot & 
VP=$!
trap 'kill $VP 2>/dev/null || true' EXIT

# Stop as soon as the container has either started or failed -- no point holding
# the full timeout once the answer is on disk.
for _ in $(seq 1 "$RUNTIME"); do
    kill -0 $VP 2>/dev/null || break
    if grep -aq "HUSK-UI: --- end ---" "$OUT"; then
        echo "==> failure diagnostics captured"
        break
    fi
    if grep -aq "Android with user 0 is ready" "$OUT"; then
        echo "==> Android booted"
        break
    fi
    sleep 1
done
kill $VP 2>/dev/null || true
wait $VP 2>/dev/null || true
echo "==> done; $(wc -l < "$OUT") lines in $OUT"
