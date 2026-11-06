#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# macOS; requires the libcore boot jars from an existing build/java-runtime.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-string-oom.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
RUNTIME=${HUSK_JAVA_RUNTIME:-"$ROOT/build/java-runtime"}
test -f "$RUNTIME/com.android.art/javalib/core-oj.jar"
JDK=${HUSK_JDK:-${JAVA_HOME:-}}
: "${JDK:?Set HUSK_JDK or JAVA_HOME to a JDK with javac}"
D8=${HUSK_D8:-"$ROOT/build/tools/r8.jar"}
mkdir -p "$OUT/classes" "$OUT/dex"
"$JDK/bin/javac" --release 8 -d "$OUT/classes" "$ROOT/tests/translation-layer/fixtures/StringAllocation.java"
"$JDK/bin/java" -cp "$D8" com.android.tools.r8.D8 --min-api 26 --output "$OUT/dex" "$OUT/classes/StringAllocation.class"
clang -dynamiclib "$ROOT/tests/translation-layer/string_oom_allocator.c" -o "$OUT/allocator.dylib"
"$ROOT/tools/regress/build.sh" tests/translation-layer/string_oom.c "$OUT/string-oom"
DYLD_INSERT_LIBRARIES="$OUT/allocator.dylib" "$OUT/string-oom" "$RUNTIME" "$OUT/dex/classes.dex"
