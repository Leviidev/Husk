#!/bin/bash
# Build the guest libraries Husk adds next to Android's ART module, with the Android NDK:
#   libunwind.so               the C++ unwinder, which bionic's libc carries on a phone (from the NDK's libunwind.a)
#   libartpalette-system.so    ART's palette, answered for an app process (src/art-support/libartpalette-system.c)
#
#   ANDROID_NDK=/path/to/ndk scripts/build_art_support.sh <out-dir>
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${1:?usage: build_art_support.sh <out-dir>}"
NDK="${ANDROID_NDK:?set ANDROID_NDK to an NDK r27 or later}"
TC="$(echo "$NDK"/toolchains/llvm/prebuilt/*)"
CC="$TC/bin/clang --target=aarch64-linux-android30"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$OUT"

# libunwind: the NDK builds it with hidden symbols, for static linking. Rename each _Unwind_ function and export the real
# name as a single branch to it, so nothing about the unwinder changes and no frame is added.
UNW="$(echo "$TC"/lib/clang/*/lib/linux/aarch64/libunwind.a)"
"$TC/bin/llvm-nm" --defined-only "$UNW" 2>/dev/null | awk '$2=="T" && $3 ~ /^_Unwind_/ {print $3}' | sort -u > "$WORK/syms"
awk '{print $1" __husk"$1}' "$WORK/syms" > "$WORK/redef"
"$TC/bin/llvm-objcopy" --redefine-syms="$WORK/redef" "$UNW" "$WORK/unwind.a"
{
    echo '.text'
    while read -r s; do printf '.globl %s\n.type %s,%%function\n.p2align 2\n%s:\n  b __husk%s\n.size %s, .-%s\n' "$s" "$s" "$s" "$s" "$s" "$s"; done < "$WORK/syms"
} > "$WORK/tramp.S"
$CC -shared -fPIC -Wl,-soname,libunwind.so -o "$OUT/libunwind.so" "$WORK/tramp.S" -Wl,--whole-archive "$WORK/unwind.a" -Wl,--no-whole-archive

$CC -shared -fPIC -O2 -fvisibility=hidden -Wall -Wl,-soname,libartpalette-system.so \
    -o "$OUT/libartpalette-system.so" "$HUSK_ROOT/src/art-support/libartpalette-system.c"

echo "==> $OUT: libunwind.so ($(wc -l < "$WORK/syms" | tr -d ' ') functions), libartpalette-system.so"
