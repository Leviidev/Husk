#!/bin/bash
# Build ANGLE (EGL + GLES over Metal) for iOS arm64.
#
# This is the host-side GPU stack Husk needs. virglrenderer turns the guest's
# GL calls into real draw calls, but it needs an EGL/GLES implementation to make
# them against -- and iOS has no EGL at all. ANGLE supplies one on top of Metal.
# UTM does the same thing, which is what makes GPU acceleration inside a QEMU
# guest on iOS a solved problem rather than a hope.
#
# ANGLE is taken from WebKit rather than upstream because WebKit carries an
# Xcode project for it, which avoids needing depot_tools and gn. A blob-filtered
# sparse checkout keeps that to ~260 MB instead of cloning all of WebKit.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GPU="$HUSK_ROOT/third_party/gpu"
WK="$GPU/webkit"
PREFIX="$HUSK_ROOT/build/ios-arm64/sysroot"
LOG="$HUSK_ROOT/build/logs/angle-build.log"
mkdir -p "$GPU" "$(dirname "$LOG")"

if [ ! -d "$WK" ]; then
    echo "==> sparse-cloning WebKit for its ANGLE tree"
    git clone --depth 1 --filter=blob:none --sparse \
        https://github.com/WebKit/WebKit.git "$WK"
fi

# Configurations and Tools/ccache are not optional: the ANGLE xcconfigs include
# ../../../../Tools/ccache/ccache.xcconfig, and the build fails to even start
# without it.
( cd "$WK" && git sparse-checkout set \
    Source/ThirdParty/ANGLE Configurations Tools/Scripts Tools/ccache )

cd "$WK/Source/ThirdParty/ANGLE"

# Two overrides worth explaining.
#
# WK_AVAILABILITY_OVERLAY_FLAGS points at a clang VFS overlay that WebKit
# generates to defuse availability annotations. Nothing in this subset of the
# tree generates it, so the build dies looking for availability-overlay.yaml.
# Clearing the flag drops the overlay -- which then surfaces the errors it was
# hiding, all of the form "MTLPixelFormatBC1_RGBA is only available on iOS
# 16.4". Hence the deployment target: 17.0 is what Husk requires anyway, so the
# annotations are satisfied honestly rather than suppressed.
ALIASES="$GPU/angle-aliases.txt"

angle_build () {
    env -i PATH="$PATH" HOME="$HOME" xcodebuild archive \
        -archivePath "ANGLE" -scheme "ANGLE" \
        -sdk iphoneos -arch arm64 -configuration Release \
        WEBCORE_LIBRARY_DIR="/usr/local/lib" NORMAL_UMBRELLA_FRAMEWORKS_DIR="" \
        CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO \
        WK_AVAILABILITY_OVERLAY_FLAGS="" WK_AVAILABILITY_OVERLAY_SWIFT_FLAGS="" \
        IPHONEOS_DEPLOYMENT_TARGET="17.0" \
        ${1:+OTHER_LDFLAGS=$(printf '$(inherited) -Wl,-alias_list,%s' "$1")} \
        > "$LOG" 2>&1 \
      || { echo "ANGLE build failed; last errors:" >&2
           grep -a "error:\|^ld: " "$LOG" | grep -v xcodebuild | head -10 >&2; exit 1; }
}

# Two passes, because of a name mismatch.
#
# WebKit builds ANGLE with its entry points renamed -- the dylib exports
# EGL_ChooseConfig, not eglChooseConfig -- since WebCore reaches them through
# ANGLE's own loader. libepoxy and virglrenderer expect the standard names.
# Rather than hand-write ~115 forwarding functions, the second pass relinks with
# ld's -alias_list, which adds the standard name as an alias of each renamed
# symbol. No wrappers, no extra indirection at call time.
#
# EGL only, deliberately. Aliasing the 828 GL_* exports the same way fails with
# "ld: 828 duplicate symbols" -- ANGLE already carries internal gl* symbols that
# the aliases collide with. It is also unnecessary: epoxy resolves GL entry
# points through eglGetProcAddress, so EGL is the only surface that must be
# reachable by its standard name.
echo "==> building ANGLE for iOS arm64, pass 1 (log: $LOG)"
angle_build ""

echo "==> generating EGL symbol aliases"
nm -g "ANGLE.xcarchive/Products/usr/local/lib/libANGLE-shared.dylib" \
  | awk '$2=="T"{print $3}' | grep -E '^_EGL_' \
  | awk '{o=$1; a=$1; sub(/^_EGL_/,"_egl",a); print o, a}' > "$ALIASES"
echo "    $(wc -l < "$ALIASES") aliases"

echo "==> pass 2, relinking with standard EGL names"
angle_build "$ALIASES"

DYLIB="ANGLE.xcarchive/Products/usr/local/lib/libANGLE-shared.dylib"
[ -f "$DYLIB" ] || { echo "no dylib at $DYLIB" >&2; exit 1; }

install -d "$PREFIX/lib" "$PREFIX/include"
cp "$DYLIB" "$PREFIX/lib/"
rsync -a "include/" "$PREFIX/include/"

# WebKit ships it for loading out of WebCore.framework; ours is embedded in the
# app, so the install name has to say so or dyld will not find it at runtime.
install_name_tool -id "@rpath/libANGLE-shared.dylib" "$PREFIX/lib/libANGLE-shared.dylib"

echo "==> staged $(ls -lh "$PREFIX/lib/libANGLE-shared.dylib" | awk '{print $5}') to $PREFIX/lib"
otool -D "$PREFIX/lib/libANGLE-shared.dylib" | tail -1
echo "==> standard EGL entry points exported: $(nm -g "$PREFIX/lib/libANGLE-shared.dylib" | grep -c ' T _egl')"
