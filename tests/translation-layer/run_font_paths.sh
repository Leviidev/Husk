#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macOS host test; unrelated runtime entry points are not invoked.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
clang -O1 -ffunction-sections -fdata-sections -I"$ROOT/src/translation-layer-next" -I"$ROOT/src/translation-layer" \
  "$ROOT/tests/translation-layer/font_paths.c" "$ROOT/src/translation-layer-next/husk-tl-bionic-io.c" \
  -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$OUT/font_paths"
"$OUT/font_paths"
