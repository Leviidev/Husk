#!/bin/bash
# build.sh <out.dex> -- Husk's Java framework (android.* for apps run on Husk's Dalvik runtime): javac, then d8.
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
JDK="${HUSK_JDK:-/Volumes/GTAV/husk2/tools-dl/jdk/Contents/Home}"
D8="${HUSK_D8:-/Volumes/GTAV/husk2/tools-dl/r8.jar}"   # R8 8.5+: the d8 in build-tools 34 crashes on these classes
OUT="${1:-$HERE/../../build/husk-framework.dex}"
TMP="$(mktemp -d)"
python3 "$HERE/gen/gles20.py" "$HERE/src/android/opengl" "$HERE/../translation-layer-next/husk-tl-dvm-gles20.c" >/dev/null
# libcore's own classes that javac's JDK does not have (org.xmlpull): compiled to build against, not put in the dex
"$JDK/bin/javac" -nowarn --release 8 -encoding UTF-8 -d "$TMP/stubs" $(find "$HERE/stubs" -name '*.java')
if ! "$JDK/bin/javac" -nowarn --release 8 -encoding UTF-8 -Xmaxerrs 100000 -cp "$TMP/stubs" -d "$TMP/classes" $(find "$HERE/src" "$HERE/gen-src" -name '*.java') 2> "$TMP/javac.log"; then
    grep -v "^Note:" "$TMP/javac.log" > "${JAVAC_LOG:-/dev/stderr}"; echo "javac failed" >&2; exit 1
fi
(cd "$TMP/classes" && "$JDK/bin/jar" cf "$TMP/classes.jar" .)
"$JDK/bin/java" -cp "$D8" com.android.tools.r8.D8 --min-api 26 --output "$TMP" "$TMP/classes.jar"
mkdir -p "$(dirname "$OUT")"
mv "$TMP/classes.dex" "$OUT"
rm -rf "$TMP"
echo "built $OUT"
