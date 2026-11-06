#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-web-input.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
python3 "$ROOT/tests/java-framework/run.py"
for fixture in web_hit web_scroll; do
 clang -fobjc-arc -Wno-deprecated-declarations -I"$ROOT/src/translation-layer-next" -I"$ROOT/src/translation-layer" \
  "$ROOT/tests/translation-layer/$fixture.m" -Wl,-undefined,dynamic_lookup \
  -framework Foundation -framework WebKit -framework AppKit -framework QuartzCore -o "$OUT/$fixture"
 "$OUT/$fixture"
done
