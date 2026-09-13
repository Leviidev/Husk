#!/bin/bash
# Publish a guest system image to the Dependencies release and update the manifest.
#
# The snapshot entry is carried over from the manifest already published rather
# than rebuilt: a new system image does not necessarily invalidate the snapshot
# taken against the previous one. v11 differs from v10 only in the GRUB
# environment block on the persist partition, which the bootloader reads before
# Android exists and a restored machine never reads at all -- so the same
# snapshot is still valid, and forcing everyone to re-download two gigabytes of
# it would be wrong. When a change does invalidate the snapshot, publish a new
# snapshot too and its digest will differ, which is what the app compares.
#
#   usage: publish_image.sh <image.qcow2> <version>     e.g. ... vda.qcow2 v11
set -euo pipefail

IMG="${1:?usage: publish_image.sh <image.qcow2> <version>}"
VER="${2:?usage: publish_image.sh <image.qcow2> <version>}"
REPO="Leviidev/Husk"
RELEASE=387759263
TAG="lineage-v2"

TOKEN="$(printf 'protocol=https\nhost=github.com\n\n' | git credential fill \
         | sed -n 's/^password=//p')"
[ -n "$TOKEN" ] || { echo "no GitHub credential in the keychain" >&2; exit 1; }

SIZE=$(stat -f %z "$IMG")
SHA=$(shasum -a 256 "$IMG" | cut -d' ' -f1)
NAME="vda-$VER.qcow2"
echo "==> $NAME  $SIZE bytes  ${SHA:0:12}…"

# Replaced, not added: the API refuses a second asset with the same name.
drop_asset() {
    local id
    id=$(curl -fsS -H "Authorization: token $TOKEN" \
        "https://api.github.com/repos/$REPO/releases/$RELEASE/assets" \
        | python3 -c "import json,sys; print(next((a['id'] for a in json.load(sys.stdin) if a['name']=='$1'), ''))")
    [ -n "$id" ] && curl -fsS -o /dev/null -X DELETE \
        -H "Authorization: token $TOKEN" \
        "https://api.github.com/repos/$REPO/releases/assets/$id" || true
}

drop_asset "$NAME"
echo "==> uploading (this takes a couple of minutes)"
# -T, not --data-binary: the latter reads the whole file into memory and dies on
# a gigabyte with "option --data-binary: out of memory".
curl -fsS -X POST -T "$IMG" \
    -H "Authorization: token $TOKEN" \
    -H "Content-Type: application/octet-stream" \
    -w '    http %{http_code}, %{size_upload} bytes in %{time_total}s\n' -o /dev/null \
    "https://uploads.github.com/repos/$REPO/releases/$RELEASE/assets?name=$NAME"

echo "==> rebuilding the manifest around the published snapshot"
curl -fsSL "https://github.com/$REPO/releases/download/$TAG/manifest.json" -o /tmp/old-manifest.json
NAME="$NAME" VER="$VER" SHA="$SHA" SIZE="$SIZE" python3 - <<'PY'
import json, os
m = json.load(open("/tmp/old-manifest.json"))
m["generation"] = os.environ["VER"]
m["image"] = {"file": os.environ["NAME"],
              "sha256": os.environ["SHA"],
              "size": int(os.environ["SIZE"])}
json.dump(m, open("/tmp/manifest.json", "w"), indent=2)
print("    snapshot kept:", m["snapshot"]["sha256"][:12] + "…")
PY

drop_asset manifest.json
curl -fsS -X POST -T /tmp/manifest.json \
    -H "Authorization: token $TOKEN" -H "Content-Type: application/json" \
    -w '    manifest http %{http_code}\n' -o /dev/null \
    "https://uploads.github.com/repos/$REPO/releases/$RELEASE/assets?name=manifest.json"

echo "==> published $VER"
