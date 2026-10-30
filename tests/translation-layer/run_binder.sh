#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
SRC="$ROOT/src/translation-layer-next"
CC="clang -O1 -pthread -I$SRC"

$CC "$ROOT/tools/binder-test.c" "$SRC/husk-tl-binder.c" -o "$OUT/binder-test"
"$OUT/binder-test"

$CC "$ROOT/tools/servicemanager-test.c" "$SRC/husk-tl-binder.c" "$SRC/husk-tl-servicemanager.c" \
  -o "$OUT/sm-test"
"$OUT/sm-test"

$CC "$ROOT/tools/aosp-boot-test.c" "$SRC/husk-tl-binder.c" "$SRC/husk-tl-servicemanager.c" \
  "$SRC/husk-tl-aosp.c" -o "$OUT/aosp-boot-test"
"$OUT/aosp-boot-test"
