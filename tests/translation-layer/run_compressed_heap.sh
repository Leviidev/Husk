#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macOS only. Unrelated runtime entry points are not invoked.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
clang -dynamiclib "$ROOT/tests/translation-layer/compressed_heap_window.c" -o "$OUT/window.dylib"
clang -O1 -ffunction-sections -fdata-sections -I"$ROOT/src/translation-layer-next" -I"$ROOT/src/translation-layer" \
  "$ROOT/tests/translation-layer/compressed_heap.c" -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$OUT/test"
DYLD_INSERT_LIBRARIES="$OUT/window.dylib" "$OUT/test" late
DYLD_INSERT_LIBRARIES="$OUT/window.dylib" "$OUT/test"
