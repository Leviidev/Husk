#!/bin/bash
# The few files of Android's /system that apps on the translation layer read directly, taken from the Lineage image the Android
# guest boots, into build/android-system (bundled as the app's android-system resource folder):
#
#   cacerts/      the trusted root certificates (one PEM per CA, named by subject hash), from the Conscrypt module
#   fonts/        the system fonts, without the Chinese/Japanese/Korean ones (58 MB of the 82)
#   etc/          fonts.xml and font_fallback.xml, the list text engines read to find a typeface
#
#   scripts/build_android_system.sh <lineage vda.qcow2 | vda.raw>
#   scripts/build_android_system.sh --system-image <extracted system.img>
#
# Needs qemu-img and e2fsprogs' debugfs (Homebrew). The image is the one GuestImage.swift downloads (releases/lineage-v2).
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
if [ "${1:-}" = "--system-image" ]; then
    SYSTEM_IMG="${2:?usage: build_android_system.sh --system-image <system.img>}"
    EXTRACT_SYSTEM=false
else
    IMG="${1:?usage: build_android_system.sh <vda.qcow2|vda.raw>}"
    EXTRACT_SYSTEM=true
fi
OUT="$HUSK_ROOT/build/android-system"
DEBUGFS="${DEBUGFS:-$(command -v debugfs || echo /opt/homebrew/opt/e2fsprogs/sbin/debugfs)}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

if [ "$EXTRACT_SYSTEM" = true ]; then
    SYSTEM_IMG="$WORK/system.img"
    echo "==> unpacking the super partition"
    python3 "$HUSK_ROOT/scripts/extract_lineage_system_partition.py" "$IMG" "$SYSTEM_IMG"
fi
[ -f "$SYSTEM_IMG" ] || { echo "system partition not found: $SYSTEM_IMG" >&2; exit 1; }

rm -rf "$OUT"; mkdir -p "$OUT/etc" "$WORK/dump"
echo "==> fonts"
"$DEBUGFS" -R "rdump /system/fonts $WORK/dump" "$SYSTEM_IMG" 2>/dev/null || true
mkdir -p "$OUT/fonts"
find "$WORK/dump/fonts" -type f ! -name '*CJK*' -size +100c -exec cp {} "$OUT/fonts/" \;
for x in fonts.xml font_fallback.xml; do "$DEBUGFS" -R "dump /system/etc/$x $OUT/etc/$x" "$SYSTEM_IMG" 2>/dev/null; done

echo "==> certificates"
CONSCRYPT_APEX="$WORK/conscrypt.apex"
"$DEBUGFS" -R "dump /system/apex/com.android.conscrypt.capex $CONSCRYPT_APEX" "$SYSTEM_IMG" >/dev/null 2>&1 || true
if [ ! -s "$CONSCRYPT_APEX" ]; then
    rm -f "$CONSCRYPT_APEX"
    APEX_LIST="$("$DEBUGFS" -R "ls /system/apex" "$SYSTEM_IMG" 2>/dev/null || true)"
    CONSCRYPT_NAME="$(printf '%s\n' "$APEX_LIST" | grep -oE 'com\.android\.conscrypt(@[^[:space:]()/]+)?\.(capex|apex)' | head -1 || true)"
    [ -n "$CONSCRYPT_NAME" ] || { echo "Conscrypt APEX not found in Lineage /system" >&2; exit 1; }
    "$DEBUGFS" -R "dump /system/apex/$CONSCRYPT_NAME $CONSCRYPT_APEX" "$SYSTEM_IMG" >/dev/null 2>&1
fi
python3 - "$WORK" <<'EOF'
import sys, zipfile
w = sys.argv[1]
z = zipfile.ZipFile(w + '/conscrypt.apex')
if 'original_apex' in z.namelist(): z = zipfile.ZipFile(z.open('original_apex'))
open(w + '/conscrypt.img', 'wb').write(z.read('apex_payload.img'))
EOF
mkdir -p "$WORK/apex"
"$DEBUGFS" -R "rdump /cacerts $WORK/apex" "$WORK/conscrypt.img" 2>/dev/null || true
mkdir -p "$OUT/cacerts" && cp "$WORK/apex/cacerts/"* "$OUT/cacerts/"

echo "==> $OUT: $(ls "$OUT/fonts" | wc -l | tr -d ' ') fonts, $(ls "$OUT/cacerts" | wc -l | tr -d ' ') certificates ($(du -sh "$OUT" | cut -f1))"
