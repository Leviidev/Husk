#!/bin/bash
# The few files of Android's /system that apps on the translation layer read directly, taken from the Lineage image the Android
# guest boots, into build/android-system (bundled as the app's android-system resource folder):
#
#   cacerts/      the trusted root certificates (one PEM per CA, named by subject hash), from the Conscrypt module
#   fonts/        the system fonts, without the Chinese/Japanese/Korean ones (58 MB of the 82)
#   etc/          fonts.xml and font_fallback.xml, the list text engines read to find a typeface
#
#   scripts/build_android_system.sh <lineage vda.qcow2 | vda.raw>
#
# Needs qemu-img and e2fsprogs' debugfs (Homebrew). The image is the one GuestImage.swift downloads (releases/lineage-v2).
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
IMG="${1:?usage: build_android_system.sh <vda.qcow2|vda.raw>}"
OUT="$HUSK_ROOT/build/android-system"
DEBUGFS="$(command -v debugfs || echo /opt/homebrew/opt/e2fsprogs/sbin/debugfs)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

RAW="$IMG"
case "$IMG" in *.qcow2) echo "==> converting the image"; qemu-img convert -O raw "$IMG" "$WORK/vda.raw"; RAW="$WORK/vda.raw" ;; esac

# The super partition (the second one) holds system: unpack its logical partitions' extents.
echo "==> unpacking the super partition"
python3 - "$RAW" "$WORK" <<'EOF'
import struct, subprocess, sys
raw, work = sys.argv[1], sys.argv[2]
out = subprocess.run(['gpt', '-r', 'show', raw], capture_output=True, text=True).stdout
start = [int(l.split()[0]) for l in out.splitlines() if l.split()[2:4] == ['2', 'GPT']][0]
base = start * 512
f = open(raw, 'rb')
f.seek(base + 12288); h = f.read(256)
magic, _, _, hsize = struct.unpack_from('<IHHI', h, 0)
assert magic == 0x414C5030, 'no logical partition metadata'
o = 12 + 32; tsize = struct.unpack_from('<I', h, o)[0]; o += 4 + 32
descs = [struct.unpack_from('<III', h, o + 12 * i) for i in range(4)]
f.seek(base + 12288 + hsize); T = f.read(tsize)
(po, pn, ps), (eo, en, es) = descs[0], descs[1]
ext = [struct.unpack_from('<QIQI', T, eo + es * i) for i in range(en)]
for i in range(pn):
    e = T[po + ps * i:po + ps * (i + 1)]
    name = e[:36].split(b'\0')[0].decode(); _, fi, ne, _ = struct.unpack_from('<IIII', e, 36)
    if name != 'system': continue
    with open(work + '/system.img', 'wb') as w:
        for j in range(fi, fi + ne):
            nsec, _, data, _ = ext[j]
            f.seek(base + data * 512); left = nsec * 512
            while left:
                b = f.read(min(left, 64 << 20)); w.write(b); left -= len(b)
EOF

rm -rf "$OUT"; mkdir -p "$OUT/etc" "$WORK/dump"
echo "==> fonts"
"$DEBUGFS" -R "rdump /system/fonts $WORK/dump" "$WORK/system.img" 2>/dev/null || true
mkdir -p "$OUT/fonts"
find "$WORK/dump/fonts" -type f ! -name '*CJK*' -size +100c -exec cp {} "$OUT/fonts/" \;
for x in fonts.xml font_fallback.xml; do "$DEBUGFS" -R "dump /system/etc/$x $OUT/etc/$x" "$WORK/system.img" 2>/dev/null; done

echo "==> certificates"
"$DEBUGFS" -R "dump /system/apex/com.android.conscrypt.capex $WORK/conscrypt.capex" "$WORK/system.img" 2>/dev/null
python3 - "$WORK" <<'EOF'
import sys, zipfile
w = sys.argv[1]
z = zipfile.ZipFile(w + '/conscrypt.capex')
if 'original_apex' in z.namelist(): z = zipfile.ZipFile(z.open('original_apex'))
open(w + '/conscrypt.img', 'wb').write(z.read('apex_payload.img'))
EOF
mkdir -p "$WORK/apex"
"$DEBUGFS" -R "rdump /cacerts $WORK/apex" "$WORK/conscrypt.img" 2>/dev/null || true
mkdir -p "$OUT/cacerts" && cp "$WORK/apex/cacerts/"* "$OUT/cacerts/"

echo "==> $OUT: $(ls "$OUT/fonts" | wc -l | tr -d ' ') fonts, $(ls "$OUT/cacerts" | wc -l | tr -d ' ') certificates ($(du -sh "$OUT" | cut -f1))"
