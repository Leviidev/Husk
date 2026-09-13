#!/bin/sh
# The container half of set_grub_toggles.sh. Alpine, with the image at $1.
set -e

IMG="$1"
OFF="$2"
LEN="$3"
RAW=/work/vda-grubedit.raw
SLICE=/work/persist-grubedit.img

cleanup() { rm -f "$RAW" "$SLICE" /work/grubenv.orig /work/grubenv.new /work/grubenv.check; }
trap cleanup EXIT

apk add --no-cache qemu-img mtools python3 >/dev/null 2>&1

echo "==> converting to raw (read-only pass over the image)"
qemu-img convert -O raw -S 4k "$IMG" "$RAW"

echo "==> reading the GRUB environment block"
mcopy -n -i "$RAW@@$OFF" ::/grubenv /work/grubenv.orig

python3 /scripts/edit_grubenv.py /work/grubenv.orig /work/grubenv.new

echo "==> writing it back into the raw copy"
mcopy -o -i "$RAW@@$OFF" /work/grubenv.new ::/grubenv

echo "==> carving out the 16 MiB persist partition"
dd if="$RAW" of="$SLICE" bs=1M skip=$((OFF / 1048576)) count=$((LEN / 1048576)) status=none

echo "==> writing that slice back into the qcow2"
qemu-io -f qcow2 -c "write -s $SLICE $OFF $LEN" "$IMG"

echo "==> verifying, by re-reading from the qcow2 itself"
rm -f "$RAW"
qemu-img convert -O raw -S 4k "$IMG" "$RAW"
mcopy -n -i "$RAW@@$OFF" ::/grubenv /work/grubenv.check
python3 /scripts/edit_grubenv.py --verify /work/grubenv.check
