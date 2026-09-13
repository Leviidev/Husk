#!/bin/bash
# Declare android.hardware.ethernet in the guest image, so EthernetService runs.
#
# Android's connectivity module gates EthernetService on FEATURE_ETHERNET. The
# LineageOS image does not declare it, so nothing ever claims eth0 -- no network
# is registered, and netd's per-uid routing then has no default network to point
# at. That is what silences the command bridge about a minute after every boot.
#
# /system is a LOGICAL partition inside `super` (see inspect_partitions.py), and
# it is ext4, so one file can be written into it with debugfs -- no mounting, no
# repacking. Only that partition is read and written; the rest of the 5 GiB image
# is untouched. An earlier attempt converted the whole disk with too little free
# space and took the host down, hence the headroom check below.
set -euo pipefail

IMG="${1:-$HOME/husk-snap/vda.qcow2}"
[ -f "$IMG" ] || { echo "no image at $IMG" >&2; exit 1; }

# From scripts/inspect_partitions.py against this image.
SYS_OFF=1837105152
SYS_LEN=1080315904

FREE_GB=$(df -g "$(dirname "$IMG")" | awk 'NR==2{print $4}')
[ "$FREE_GB" -ge 4 ] || { echo "only ${FREE_GB} GB free; need 4+ for the partition copy" >&2; exit 1; }
echo "==> ${FREE_GB} GB free, image $(basename "$IMG")"

WORK="$(cd "$(dirname "$IMG")" && pwd)"
docker run --rm -v "$WORK:/work" -v "$(cd "$(dirname "$0")" && pwd):/scripts:ro" \
    alpine:3.20 sh /scripts/add_ethernet_feature_inner.sh \
    "/work/$(basename "$IMG")" "$SYS_OFF" "$SYS_LEN"
