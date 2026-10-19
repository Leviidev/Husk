#!/bin/bash
# Download the exact Lineage image used by Husk and build all Android-side app resources.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
REPOSITORY="Leviidev/Husk"
RELEASE_TAG="lineage-v2"
GUEST_VERSION="$(sed -n 's/^[[:space:]]*static let imageVersion = "\([^"]*\)"/\1/p' "$HUSK_ROOT/src/app/Husk/GuestImage.swift" | head -1)"
[ -n "$GUEST_VERSION" ] || { echo "could not read the Lineage image version from GuestImage.swift" >&2; exit 1; }

file_size() {
    if stat -f %z "$1" >/dev/null 2>&1; then
        stat -f %z "$1"
    else
        stat -c %s "$1"
    fi
}

GUEST_DIR="$HUSK_ROOT/build/android-guest"
MANIFEST="$GUEST_DIR/manifest.json"
mkdir -p "$GUEST_DIR"
curl -fsSL "https://github.com/$REPOSITORY/releases/download/$RELEASE_TAG/manifest.json" -o "$MANIFEST"

IMAGE_META="$(python3 - "$MANIFEST" "$GUEST_VERSION" <<'PY'
import json, sys
manifest = json.load(open(sys.argv[1]))
version = sys.argv[2]
if manifest.get("generation") != version:
    raise SystemExit(f"Lineage manifest is {manifest.get('generation')}, app requests {version}")
image = manifest.get("image", {})
if image.get("file") != f"vda-{version}.qcow2":
    raise SystemExit(f"manifest points to unexpected image: {image.get('file')}")
if not image.get("sha256") or not image.get("size"):
    raise SystemExit("Lineage manifest is missing the image size or SHA-256")
print("\t".join((image["file"], image["sha256"], str(image["size"]))))
PY
)"
IFS=$'\t' read -r IMAGE_NAME EXPECTED_SHA EXPECTED_SIZE <<< "$IMAGE_META"
IMAGE="$GUEST_DIR/$IMAGE_NAME"

IMAGE_CACHED=false
if [ -s "$IMAGE" ] && [ "$(file_size "$IMAGE")" = "$EXPECTED_SIZE" ]; then
    CACHED_SHA="$(shasum -a 256 "$IMAGE" | awk '{print $1}')"
    if [ "$CACHED_SHA" = "$EXPECTED_SHA" ]; then IMAGE_CACHED=true; fi
fi

if [ "$IMAGE_CACHED" != true ]; then
    echo "==> downloading Lineage $GUEST_VERSION system image"
    rm -f "$IMAGE.part"
    curl -fL --retry 3 --retry-delay 5 \
        "https://github.com/$REPOSITORY/releases/download/$RELEASE_TAG/$IMAGE_NAME" \
        -o "$IMAGE.part"
    mv "$IMAGE.part" "$IMAGE"
    ACTUAL_SIZE="$(file_size "$IMAGE")"
    ACTUAL_SHA="$(shasum -a 256 "$IMAGE" | awk '{print $1}')"
    if [ "$ACTUAL_SIZE" != "$EXPECTED_SIZE" ] || [ "$ACTUAL_SHA" != "$EXPECTED_SHA" ]; then
        echo "Lineage image failed the release manifest check" >&2
        echo "expected $EXPECTED_SIZE bytes / $EXPECTED_SHA" >&2
        echo "got      $ACTUAL_SIZE bytes / $ACTUAL_SHA" >&2
        exit 1
    fi
fi

SYSTEM_IMG="$GUEST_DIR/system.img"
echo "==> extracting the Lineage /system partition"
python3 "$HUSK_ROOT/scripts/extract_lineage_system_partition.py" "$IMAGE" "$SYSTEM_IMG"

echo "==> building Android system resources"
"$HUSK_ROOT/scripts/build_android_system.sh" --system-image "$SYSTEM_IMG"

echo "==> building Husk's Java runtime"
"$HUSK_ROOT/scripts/build_java_runtime_from_system.sh" "$SYSTEM_IMG"

echo "==> Android resources are ready"
