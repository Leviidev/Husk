#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Host regression for the shared Vulkan bridge. Needs macOS, Xcode command-line
# tools, Vulkan headers and a macOS MoltenVK library. Does not open a window.
set -eu
if [ "$#" -ne 2 ]; then
    echo "usage: $0 <Vulkan include directory> <macOS MoltenVK library>" >&2
    exit 64
fi
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=${OUT:-"$ROOT/build/macos"}
mkdir -p "$OUT"
xcrun --sdk macosx clang -std=gnu11 -O2 -g -fobjc-arc \
    -I"$1" -I"$ROOT/src/translation-layer-next" \
    "$ROOT/tests/translation-layer/vulkan_compositor.m" \
    "$ROOT/src/translation-layer-next/husk-tl-vulkan.m" \
    -framework Foundation -framework QuartzCore -framework Metal \
    -o "$OUT/vulkan-compositor-test"
"$OUT/vulkan-compositor-test" "$2"
"$OUT/vulkan-compositor-test" "$2" --same-queue
