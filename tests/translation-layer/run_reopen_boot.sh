#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-session-close.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
JDK=${HUSK_JDK:-${JAVA_HOME:-}}
: "${JDK:?Set HUSK_JDK or JAVA_HOME}"
D8=${HUSK_D8:-"$ROOT/build/tools/r8.jar"}
mkdir -p "$OUT/classes" "$OUT/dex" "$OUT/framework-dex"
rg --files "$ROOT/src/java-framework/stubs" -g '*.java' > "$OUT/stubs.list"
"$JDK/bin/javac" -nowarn --release 8 -encoding UTF-8 -d "$OUT/stubs" @"$OUT/stubs.list"
rg --files "$ROOT/src/java-framework/src" "$ROOT/src/java-framework/gen-src" -g '*.java' > "$OUT/framework.list"
"$JDK/bin/javac" -nowarn --release 8 -encoding UTF-8 -cp "$OUT/stubs" -d "$OUT/framework" @"$OUT/framework.list" 2> "$OUT/javac.log" || { cat "$OUT/javac.log"; exit 1; }
(cd "$OUT/framework" && "$JDK/bin/jar" cf "$OUT/framework.jar" .)
"$JDK/bin/java" -cp "$D8" com.android.tools.r8.D8 --min-api 26 --output "$OUT/framework-dex" "$OUT/framework.jar"
"$JDK/bin/javac" --release 8 -cp "$OUT/framework" -d "$OUT/classes" "$ROOT/tests/translation-layer/fixtures/ReopenBootProbe.java"
(cd "$OUT/classes" && "$JDK/bin/jar" cf "$OUT/probe.jar" .)
"$JDK/bin/java" -cp "$D8" com.android.tools.r8.D8 --min-api 26 --output "$OUT/dex" "$OUT/probe.jar"
"$ROOT/tools/regress/build.sh" tests/translation-layer/reopen_boot.c "$OUT/reopen-boot"
"$OUT/reopen-boot" "${HUSK_JAVA_RUNTIME:-$ROOT/build/java-runtime}" "$OUT/dex/classes.dex" "$OUT/framework-dex/classes.dex"
