#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macOS host test. No speaker, game, or third-party build is needed.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
xcrun --sdk macosx clang ${CFLAGS:-} -std=gnu11 -O1 -ffunction-sections -fdata-sections \
  -I"$ROOT/src/translation-layer-next" -I"$ROOT/src/translation-layer" \
  "$ROOT/tests/translation-layer/aaudio_rate.c" \
  "$ROOT/src/translation-layer-next/husk-tl-bionic-ndk.c" \
  "$ROOT/src/translation-layer-next/husk-tl-audio.c" \
  -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -lz -o "$OUT/aaudio_rate"
if [ "$#" -gt 0 ]; then
  "$OUT/aaudio_rate" "$@"
  exit
fi
"$OUT/aaudio_rate" 24000 write i16 2
"$OUT/aaudio_rate" 24000 callback float 2
"$OUT/aaudio_rate" 48000 callback float 2
"$OUT/aaudio_rate" 48000 write i16 1
"$OUT/aaudio_rate" 0 callback float 2
"$OUT/aaudio_rate" 44100 write i16 2
"$OUT/aaudio_rate" rate-selection
"$OUT/aaudio_rate" pacing
