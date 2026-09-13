#!/bin/bash
# Set LineageOS GRUB boot toggles inside the guest disk image.
#
# LineageOS's bootloader reads a set of switches from a GRUB environment block
# on the `persist` partition and turns them into kernel command-line arguments:
#
#   android_selinux_permissive=1  ->  androidboot.selinux=permissive
#   android_nobootanim=1          ->  androidboot.nobootanim=1
#
# That matters because the kernel command line is the only channel that can
# change Android's behaviour before Android exists, and every remaining blocker
# in Husk is an SELinux denial: `ctl.stop` from the bridge (which is what gates
# saving a GPU machine), init's exec of /system/bin/settings for provisioning,
# and crash_dump's ptrace. This is a supported toggle with its own boot-menu
# entry, not a patched image.
#
# Only the 16 MiB persist partition is rewritten. The rest of the 5 GiB image is
# read, never written -- an earlier attempt converted the whole disk with too
# little free space and took the host down with it.
#
#   usage: set_grub_toggles.sh [image.qcow2]
set -euo pipefail

IMG="${1:-$HOME/husk-snap/vda.qcow2}"
[ -f "$IMG" ] || { echo "no image at $IMG" >&2; exit 1; }

# Byte offset and length of the `persist` partition, from the image's GPT.
# Both land on exact MiB boundaries, which is what lets dd address it cleanly.
PERSIST_OFF=4919918592
PERSIST_LEN=16777216

FREE_GB=$(df -g "$(dirname "$IMG")" | awk 'NR==2{print $4}')
[ "$FREE_GB" -ge 12 ] || { echo "only ${FREE_GB} GB free; need 12+ for the raw copy" >&2; exit 1; }
echo "==> ${FREE_GB} GB free, image $(basename "$IMG")"

WORK="$(cd "$(dirname "$IMG")" && pwd)"
docker run --rm -v "$WORK:/work" -v "$(cd "$(dirname "$0")" && pwd):/scripts:ro" \
    alpine:3.20 sh /scripts/set_grub_toggles_inner.sh "/work/$(basename "$IMG")" \
    "$PERSIST_OFF" "$PERSIST_LEN"
