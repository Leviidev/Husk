#!/bin/bash
# Turn a stock Debian arm64 cloud image into Husk's Waydroid guest.
#
# Runs on the Mac under HVF at near-native speed. This is not a test fixture --
# it produces the artifact that ships. Doing the same provisioning on the phone
# would mean running apt and a Waydroid install under TCG, which is hours of
# emulation for a byte-identical result.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
G="$HUSK_ROOT/guests/phase1"
WORK="$G/work"
LOG="$HUSK_ROOT/build/logs/phase1-image.log"
DISK="$G/husk-guest.qcow2"
DISK_SIZE="${DISK_SIZE:-24G}"
HTTP_PORT=8000

mkdir -p "$WORK" "$(dirname "$LOG")"

command -v qemu-system-aarch64 >/dev/null || { echo "need host qemu (brew install qemu)" >&2; exit 1; }

# ---------------------------------------------------------------- base image
if [ ! -f "$DISK" ]; then
    echo "==> creating working disk from the Debian base ($DISK_SIZE)"
    cp "$G/debian-13-generic-arm64.qcow2" "$DISK"
    qemu-img resize "$DISK" "$DISK_SIZE"
else
    echo "==> reusing existing $DISK (delete it to start clean)"
fi

# NOTE: the LineageOS images are deliberately NOT staged here. `waydroid init`
# runs on the phone at first-run setup instead, which keeps this artifact ~1.2 GB
# rather than 6.05 GB and avoids redistributing LineageOS ourselves.

# ------------------------------------------------------------- cloud-init seed
echo "==> building cloud-init seed"
SEED_DIR="$WORK/seed"
rm -rf "$SEED_DIR" "$WORK/seed.iso"
mkdir -p "$SEED_DIR"
cp "$HUSK_ROOT/scripts/cloud-init/user-data" "$HUSK_ROOT/scripts/cloud-init/meta-data" "$SEED_DIR/"
# cloud-init's NoCloud datasource keys off the volume label 'cidata'.
hdiutil makehybrid -quiet -iso -joliet -default-volume-name cidata \
    -o "$WORK/seed.iso" "$SEED_DIR"

# ------------------------------------------------------------------ firmware
FW="$G/edk2-aarch64-code.fd"
if [ ! -f "$FW" ]; then
    bunzip2 -c "$HUSK_ROOT/third_party/build/qemu-10.0.12-utm/pc-bios/edk2-aarch64-code.fd.bz2" > "$FW"
fi
VARS="$G/edk2-vars.fd"
[ -f "$VARS" ] || dd if=/dev/zero of="$VARS" bs=1m count=64 2>/dev/null

# ----------------------------------------------------------------------- boot
echo "==> booting guest under HVF to provision (log: $LOG)"
echo "    this installs Waydroid + LineageOS; expect several minutes"
: > "$LOG"

qemu-system-aarch64 \
    -M virt,highmem=on \
    -cpu host -accel hvf \
    -smp 4 -m 4096 \
    -drive if=pflash,format=raw,readonly=on,file="$FW" \
    -drive if=pflash,format=raw,file="$VARS" \
    -drive if=virtio,format=qcow2,file="$DISK" \
    -drive if=virtio,format=raw,media=cdrom,readonly=on,file="$WORK/seed.iso" \
    -nic user,model=virtio-net-pci \
    -display none \
    -serial file:"$LOG" \
    -no-reboot

echo "==> guest powered off"
if grep -q "HUSK-PROVISION: done" "$LOG"; then
    echo "==> provisioning SUCCEEDED"
else
    echo "==> provisioning did NOT report success; last 40 lines:" >&2
    tail -40 "$LOG" >&2
    exit 1
fi
qemu-img info "$DISK" | head -5

# ------------------------------------------------------------------ release
# Compressed for distribution. qcow2 compresses its own clusters and QEMU reads
# them transparently, so the app needs no decompressor -- which matters because
# iOS offers LZFSE/LZ4/LZMA/zlib and neither zstd nor bzip2.
RELEASE="$G/husk-guest-release.qcow2"
echo "==> compressing for release"
rm -f "$RELEASE"
qemu-img convert -c -O qcow2 "$DISK" "$RELEASE"
qemu-img info "$RELEASE" | grep "disk size"

# Verify it boots -- with -snapshot, ALWAYS.
#
# Without it QEMU opens the image read-write, the guest's first-boot service runs
# `waydroid init`, and ~800 MB of Android is written into the artifact being
# verified. That is not hypothetical: it turned a 755 MiB release image into
# 3.18 GiB once already. -snapshot sends all writes to a throwaway overlay.
echo "==> verifying the release image boots (read-only via -snapshot)"
cp "$VARS" "$WORK/verify-vars.fd"
( qemu-system-aarch64 -M virt,highmem=on -cpu host -accel hvf -smp 4 -m 4096 -snapshot \
    -drive if=pflash,format=raw,readonly=on,file="$FW" \
    -drive if=pflash,format=raw,file="$WORK/verify-vars.fd" \
    -drive if=virtio,format=qcow2,file="$RELEASE" \
    -nic user,model=virtio-net-pci -display none \
    -serial file:"$WORK/verify.log" -no-reboot >/dev/null 2>&1 & \
  VP=$!; sleep 70; kill $VP 2>/dev/null ) || true

if grep -aq "husk-guest login:" "$WORK/verify.log"; then
    echo "==> release image boots OK"
else
    echo "==> release image did NOT reach a login prompt; see $WORK/verify.log" >&2
    exit 1
fi
qemu-img info "$RELEASE" | grep "disk size"
