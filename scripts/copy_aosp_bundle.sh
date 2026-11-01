#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Copy an optional AOSP framework bundle next to build/java-runtime.
# Jars stay out of the git tree (Apache-2.0, loaded at run time).
set -e
R="$(cd "$(dirname "$0")/.." && pwd)"
SYS="${1:-${ANDROID_SYSROOT:-/Volumes/GTAV/husk2/root/system}}"
OUT="${2:-$R/build/java-runtime/aosp}"
mkdir -p "$OUT"
copied=0
for j in framework.jar framework-graphics.jar ext.jar telephony-common.jar voip-common.jar ims-common.jar services.jar framework-connectivity.jar tethering.jar; do
    if [ -f "$SYS/framework/$j" ]; then
        cp "$SYS/framework/$j" "$OUT/$j"
        copied=$((copied + 1))
    fi
done
if [ -d "$SYS/lib64" ]; then
    mkdir -p "$OUT/lib64"
    for l in libandroid_runtime.so libbinder.so libutils.so libcutils.so libui.so libgui.so libhwui.so; do
        [ -f "$SYS/lib64/$l" ] && cp "$SYS/lib64/$l" "$OUT/lib64/$l" || true
    done
fi
if [ "$copied" -eq 0 ]; then
    echo "no AOSP jars under $SYS/framework (Java apps keep using husk-framework.dex)" >&2
    exit 0
fi
echo "copied $copied AOSP jars to $OUT"
du -sh "$OUT"
