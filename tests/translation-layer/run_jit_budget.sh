#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macOS: exercise the fixed dual-mapped allocator and manual settings/migration.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-manual-jit.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
clang -Wall -Wextra -Werror -pthread -I"$ROOT/src/translation-layer-next" -I"$ROOT/src/translation-layer" \
    "$ROOT/tests/translation-layer/jit_budget.c" "$ROOT/src/translation-layer-next/husk-tl-xmem.c" -o "$OUT/allocator"
"$OUT/allocator"
swiftc "$ROOT/src/app/Husk/JITMemoryBudget.swift" "$ROOT/tests/translation-layer/jit_budget.swift" -o "$OUT/policy"
"$OUT/policy"
