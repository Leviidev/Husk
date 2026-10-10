#!/bin/bash
# build_java_runtime.sh [apex-root] -- the files Husk's Dalvik runtime needs, laid out as Android's APEXes so ANDROID_*_ROOT work:
# libcore's jars and native libraries (ART APEX), ICU (i18n APEX), time zones (tzdata APEX), conscrypt, Husk's Java framework, and
# the platform's resources (framework-res.apk, stripped).
# The APEX root comes from the Lineage system image (see build_android_system.sh); output: build/java-runtime.
set -e
R="$(cd "$(dirname "$0")/.." && pwd)"
APEX="${1:-/Volumes/GTAV/husk2/root/apex}"
OUT="$R/build/java-runtime"
rm -rf "$OUT"; mkdir -p "$OUT"
copy() { mkdir -p "$OUT/$(dirname "$1")"; cp "$APEX/$1" "$OUT/$1"; }
for j in core-oj core-libart okhttp bouncycastle apache-xml; do copy com.android.art/javalib/$j.jar; done
for l in libandroidio libbase libc++ libexpat libjavacore libnativehelper libopenjdk; do copy com.android.art/lib64/$l.so; done
copy com.android.i18n/javalib/core-icu4j.jar
for l in libicu libicui18n libicuuc libicu_jni libandroidicu; do [ -f "$APEX/com.android.i18n/lib64/$l.so" ] && copy com.android.i18n/lib64/$l.so; done
mkdir -p "$OUT/com.android.i18n/etc"; cp -R "$APEX/com.android.i18n/etc/icu" "$OUT/com.android.i18n/etc/"
mkdir -p "$OUT/com.android.tzdata"; cp -R "$APEX/com.android.tzdata/etc" "$OUT/com.android.tzdata/"
copy com.android.conscrypt/javalib/conscrypt.jar
"$R/src/java-framework/build.sh" "$OUT/husk-framework.dex"
# the platform's resources (themes, styles, layouts, drawables), from the same system image, without what an iPhone never picks
python3 "$R/tools/strip_framework_res.py" "${FRAMEWORK_RES:-/Volumes/GTAV/husk2/aosp/fw/framework-res.apk}" "$OUT/framework-res.apk"
du -sh "$OUT"
