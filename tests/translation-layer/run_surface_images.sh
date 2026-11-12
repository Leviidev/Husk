#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# Run software image fixtures in the real DVM with existing libcore boot jars.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/../.." && pwd)
: "${TMPDIR:?Set TMPDIR to a writable test directory}"
OUT=$(mktemp -d "$TMPDIR/husk-image-frames.XXXXXX")
trap 'rm -rf "$OUT"' EXIT
JDK=${HUSK_JDK:-${JAVA_HOME:-}}
: "${JDK:?Set HUSK_JDK or JAVA_HOME}"
D8=${HUSK_D8:-"$ROOT/build/tools/r8.jar"}
RUNTIME=${HUSK_JAVA_RUNTIME:-"$ROOT/build/java-runtime"}
mkdir -p "$OUT/stubs" "$OUT/framework" "$OUT/classes" "$OUT/framework-dex" "$OUT/fixture-dex"
rg --files "$ROOT/src/java-framework/stubs" -g '*.java' > "$OUT/stubs.list"
"$JDK/bin/javac" -J-Djava.io.tmpdir="$OUT" -nowarn --release 8 -d "$OUT/stubs" @"$OUT/stubs.list"
rg --files "$ROOT/src/java-framework/src" "$ROOT/src/java-framework/gen-src" -g '*.java' > "$OUT/framework.list"
"$JDK/bin/javac" -J-Djava.io.tmpdir="$OUT" -nowarn --release 8 -cp "$OUT/stubs" -d "$OUT/framework" @"$OUT/framework.list"
(cd "$OUT/framework" && "$JDK/bin/jar" -J-Djava.io.tmpdir="$OUT" cf "$OUT/framework.jar" .)
"$JDK/bin/javac" -J-Djava.io.tmpdir="$OUT" --release 8 -cp "$OUT/framework" -d "$OUT/classes" \
 "$ROOT/tests/translation-layer/fixtures/ImageReaderBufferTest.java" \
 "$ROOT/tests/translation-layer/fixtures/NativeSurfaceLayerTest.java"
(cd "$OUT/classes" && "$JDK/bin/jar" -J-Djava.io.tmpdir="$OUT" cf "$OUT/fixtures.jar" .)
"$JDK/bin/java" -Djava.io.tmpdir="$OUT" -cp "$D8" com.android.tools.r8.D8 --min-api 26 --output "$OUT/framework-dex" "$OUT/framework.jar"
"$JDK/bin/java" -Djava.io.tmpdir="$OUT" -cp "$D8" com.android.tools.r8.D8 --min-api 26 --output "$OUT/fixture-dex" "$OUT/fixtures.jar"
"$ROOT/tools/regress/build.sh" tools/java-test.c "$OUT/java-images"
export TL_ART="$RUNTIME/com.android.art" TL_FRAMEWORK="$OUT/framework-dex/classes.dex"
export ANDROID_ART_ROOT="$TL_ART" ANDROID_I18N_ROOT="$RUNTIME/com.android.i18n" ANDROID_TZDATA_ROOT="$RUNTIME/com.android.tzdata"
for fixture in ImageReaderBufferTest NativeSurfaceLayerTest; do
 "$OUT/java-images" "$OUT/fixture-dex/classes.dex" "$fixture" run '()Ljava/lang/String;' > "$OUT/result.log" 2>&1
 cat "$OUT/result.log"
 rg -q 'RESULT "PASS:' "$OUT/result.log"
done
