#!/bin/bash
# build.sh <tools/x-test.c> <out> [clang flags] -- a Mac harness with every native-runtime source but the app's glue
set -e
R="$(cd "$(dirname "$0")/../.." && pwd)"; N=$R/src/translation-layer-next; T=$R/src/translation-layer
TOOL=$1; OUT=$2; shift 2; rm -f "$OUT"
SRCS="$R/$TOOL"
for f in $N/*.c $N/*.m; do case $f in *-unity-app.c) ;; *) SRCS="$SRCS $f";; esac; done
# husk-console-host.c asks the app glue which game is loaded; the harness has none (-U below)
SRCS="$SRCS $T/husk-tl-elf.c $T/husk-tl-zip.c $T/husk-tl-sound.c"
clang -g -O1 -fobjc-arc -Wall -Wno-unused-function -Wno-deprecated-declarations -I$T -I$N "$@" $SRCS \
    -Wl,-U,_husk_native_loaded_apk -lz -lm -liconv -lresolv -lsqlite3 -framework AudioToolbox -framework Foundation -framework CoreFoundation -framework CoreText \
    -framework CoreGraphics -framework Security -framework QuartzCore -framework Metal -framework IOSurface -framework ImageIO -framework WebKit -framework AppKit -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework Accelerate -o "$OUT"
codesign -s - --force "$OUT" >/dev/null 2>&1
