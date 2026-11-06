#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-session-close.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
cc -Wall -Wextra -Werror -pthread -I"$ROOT/src/translation-layer-next" \
    "$ROOT/tests/translation-layer/session_test.c" \
    "$ROOT/src/translation-layer-next/husk-tl-session.c" -o "$OUT/session-test"
"$OUT/session-test"
cc -Wall -Wextra -Werror -pthread -I"$ROOT/src/translation-layer-next" \
    "$ROOT/tests/translation-layer/session_events.c" -o "$OUT/session-events"
"$OUT/session-events"
