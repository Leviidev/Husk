#!/bin/sh
# The container half of add_ethernet_feature.sh. Alpine, image at $1.
set -e
IMG="$1"; OFF="$2"; LEN="$3"
SYS=/work/system-ethedit.img
XML=/work/android.hardware.ethernet.xml
cleanup() { rm -f "$SYS" "$XML" /work/vda-ethedit.raw; }
trap cleanup EXIT

apk add --no-cache qemu-img e2fsprogs e2fsprogs-extra >/dev/null 2>&1

cat > "$XML" <<'XMLEOF'
<permissions>
    <feature name="android.hardware.ethernet" />
</permissions>
XMLEOF

# Via a sparse raw copy, the way set_grub_toggles_inner.sh does it.
#
# `qemu-img dd` accepts skip= and ignores it -- the first attempt at this
# produced a zero-length file and only the fsck guard below caught it before the
# image was written back. dd(1) honours skip, so the qcow2 is expanded to a
# sparse raw file once and the partition cut out of that.
RAW=/work/vda-ethedit.raw
echo "==> expanding to a sparse raw copy (read-only pass over the image)"
qemu-img convert -O raw -S 4k "$IMG" "$RAW"

echo "==> cutting out the system partition ($((LEN / 1048576)) MiB)"
dd if="$RAW" of="$SYS" bs=512 skip=$((OFF / 512)) count=$((LEN / 512)) status=none

echo "==> checking it really is ext4"
dumpe2fs -h "$SYS" 2>/dev/null | sed -n 's/^Filesystem features:/  features:/p;s/^Block count:/  blocks  :/p'

echo "==> writing the feature declaration"
debugfs -w -R "rm /etc/permissions/android.hardware.ethernet.xml" "$SYS" 2>/dev/null || true
debugfs -w -R "write $XML etc/permissions/android.hardware.ethernet.xml" "$SYS" 2>&1 | grep -v '^debugfs' || true
debugfs -R "stat /etc/permissions/android.hardware.ethernet.xml" "$SYS" 2>/dev/null \
    | sed -n 's/^Size:/  size:/p' | head -1

echo "==> fsck before writing it back (a corrupt /system is an unbootable guest)"
e2fsck -fp "$SYS" || { echo "fsck was not clean; NOT writing back" >&2; exit 1; }

echo "==> writing the partition back into the qcow2"
qemu-io -f qcow2 -c "write -s $SYS $OFF $LEN" "$IMG"

echo "==> verifying, by re-reading from the qcow2 itself"
rm -f "$SYS" "$RAW"
qemu-img convert -O raw -S 4k "$IMG" "$RAW"
dd if="$RAW" of="$SYS" bs=512 skip=$((OFF / 512)) count=$((LEN / 512)) status=none
debugfs -R "cat /etc/permissions/android.hardware.ethernet.xml" "$SYS" 2>/dev/null
