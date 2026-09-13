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

# The manifest is what the app actually compares against, so it is written from
# the same bytes that were just uploaded rather than by hand. Getting these out
# of step is exactly the failure this mechanism exists to catch: an asset was
# once published under a new name carrying the previous generation's bytes, and
# nothing noticed until the guest misbehaved for unrelated-looking reasons.
echo "==> building manifest"
IMG="${IMG:-$WORK/vda.qcow2}"
MANIFEST="$WORK/manifest.json"
python3 - "$IMG" "$ARCHIVE" "$VER" "$MANIFEST" <<'MANIFEST_PY'
import hashlib, json, os, sys
img, archive, ver, out = sys.argv[1:5]

def sha256(paths):
    h, n = hashlib.sha256(), 0
    for p in paths:
        with open(p, 'rb') as f:
            for chunk in iter(lambda: f.read(1 << 22), b''):
                h.update(chunk); n += len(chunk)
    return h.hexdigest(), n

parts = sorted(p for p in (archive + '.00', archive + '.01') if os.path.exists(p))
img_sha, img_size = sha256([img])
snap_sha, snap_size = sha256(parts)
json.dump({
    "generation": ver,
    "image": {"file": "vda-%s.qcow2" % ver, "sha256": img_sha, "size": img_size},
    "snapshot": {
        "parts": [os.path.basename(p) for p in parts],
        "sha256": snap_sha, "size": snap_size,
        "guestMiB": 4096, "xres": 360, "yres": 800,
    },
}, open(out, 'w'), indent=2)
print("    image    %s...  %d" % (img_sha[:12], img_size))
print("    snapshot %s...  %d" % (snap_sha[:12], snap_size))
MANIFEST_PY

# Replaced, not added: the name is fixed so the app can find it without knowing
# a version, and the API refuses a second asset with the same name.
OLD_ID=$(curl -fsS -H "Authorization: token $TOKEN" \
    "https://api.github.com/repos/$REPO/releases/$RELEASE/assets" \
    | python3 -c "import json,sys; print(next((a['id'] for a in json.load(sys.stdin) if a['name']=='manifest.json'), ''))")
if [ -n "$OLD_ID" ]; then
    curl -fsS -o /dev/null -X DELETE -H "Authorization: token $TOKEN" \
        "https://api.github.com/repos/$REPO/releases/assets/$OLD_ID"
fi
curl -fsS -X POST -T "$MANIFEST" \
    -H "Authorization: token $TOKEN" -H "Content-Type: application/json" \
    -w '    manifest http %{http_code}\n' -o /dev/null \
    "https://uploads.github.com/repos/$REPO/releases/$RELEASE/assets?name=manifest.json"

echo "==> published $VER"
