#!/bin/bash
# Publish a pre-booted snapshot to the Dependencies release, split to fit.
#
# GitHub refuses a release asset of 2 GiB or more:
#
#   422 {"field":"size","message":"size must be less than 2147483648"}
#
# A snapshot of a 4 GiB machine compresses to a little over that even at
# gzip -9, so it is uploaded in pieces named .00, .01, ... and the app joins
# them back. The split is on plain byte offsets, so concatenating the pieces
# reproduces the archive byte for byte -- GuestImage.SnapshotFetcher relies on
# exactly that and nothing else.
#
#   usage: publish_snapshot.sh <vdb.qcow2> <version>      e.g. ... vdb.qcow2 v10
set -euo pipefail

SRC="${1:?usage: publish_snapshot.sh <vdb.qcow2> <version>}"
VER="${2:?usage: publish_snapshot.sh <vdb.qcow2> <version>}"
TAG="lineage-v2"           # the single "Dependencies" release
REPO="Leviidev/Husk"
PART_MB=1042               # keeps each piece comfortably under the limit
GH="$(command -v gh || echo /opt/homebrew/bin/gh)"

"$GH" auth status >/dev/null 2>&1 || {
    echo "not signed in to GitHub -- run: gh auth login" >&2; exit 1; }

WORK="$(dirname "$SRC")"
ARCHIVE="$WORK/vdb-snapshot-$VER.qcow2.gz"

# Space is the constraint on the machine that builds these, so the archive is
# split by truncating it rather than copying it: the tail is carved off first,
# then the head is cut back in place. Peak extra usage is one part, not two.
if [ ! -f "$ARCHIVE.00" ]; then
    echo "==> compressing (this takes a while)"
    gzip -9 -c "$SRC" > "$ARCHIVE"
    gzip -t "$ARCHIVE"
    echo "==> splitting"
    dd if="$ARCHIVE" of="$ARCHIVE.01" bs=1m skip="$PART_MB" status=none
    python3 -c "import os,sys; os.truncate(sys.argv[1], $PART_MB*1024*1024)" "$ARCHIVE"
    mv "$ARCHIVE" "$ARCHIVE.00"
fi

for part in "$ARCHIVE".*; do
    size=$(stat -f %z "$part")
    [ "$size" -lt 2147483648 ] || { echo "$part is $size bytes, over the limit" >&2; exit 1; }
    echo "==> uploading $(basename "$part")  ($size bytes)"
    "$GH" release upload "$TAG" "$part" --repo "$REPO" --clobber
done

echo "==> published; set GuestImage.imageVersion to \"$VER\""
