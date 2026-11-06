#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macOS integration test against the complete native runtime.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-session-close.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
"$ROOT/tools/regress/build.sh" tests/translation-layer/session_io.c "$OUT/session-io"
"$OUT/session-io"
