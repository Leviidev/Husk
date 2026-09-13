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
REPO="Leviidev/Husk"
RELEASE=387759263          # the single "Dependencies" release, tag lineage-v2
PART_MB=1042               # keeps each piece comfortably under the limit

# The token git already has. The remote is plain HTTPS with credential.helper =
# osxkeychain, so pushes authenticate without setup and so does this -- no CLI
# to install and nothing extra to log into. It is never echoed.
TOKEN="$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill \
         | sed -n 's/^password=//p')"
[ -n "$TOKEN" ] || { echo "no GitHub credential in the keychain" >&2; exit 1; }

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
    name="$(basename "$part")"
    size=$(stat -f %z "$part")
    [ "$size" -lt 2147483648 ] || { echo "$name is $size bytes, over the limit" >&2; exit 1; }
    echo "==> uploading $name  ($size bytes)"
    # -T, not --data-binary: the latter reads the whole file into memory first
    # and simply dies on a gigabyte ("option --data-binary: out of memory").
    curl -fsS -X POST -T "$part" \
        -H "Authorization: token $TOKEN" \
        -H "Content-Type: application/octet-stream" \
        -w '    http %{http_code}, %{size_upload} bytes in %{time_total}s\n' -o /dev/null \
        "https://uploads.github.com/repos/$REPO/releases/$RELEASE/assets?name=$name"
done

echo "==> published; set GuestImage.imageVersion to \"$VER\""
