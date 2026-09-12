#!/bin/bash
# Build the host GPU stack for iOS: libepoxy + virglrenderer.
#
# These turn the guest's GL calls into real draw calls on the phone's GPU
# instead of pixels rasterised in software by an emulated CPU. ANGLE supplies
# the EGL/GLES implementation underneath (scripts/build_angle_ios.sh); run that
# first.
#
# Both come from UTM's forks at the commits UTM pins, not from upstream.
# Upstream libepoxy 1.5.10 does not build for iOS at all -- dispatch_egl.c fails
# with "use of undeclared identifier 'EGLDisplay'" -- and the Darwin and Metal
# support in both projects lives in these forks. They are MIT, so linking them
# with Husk's GPLv2+ code is fine; it is only UTM's own Apache-2.0 app code that
# has to be left alone.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GPU="$HUSK_ROOT/third_party/gpu"
PREFIX="$HUSK_ROOT/build/ios-arm64/sysroot"
LOGS="$HUSK_ROOT/build/logs"
MESON=/opt/homebrew/bin/meson
NINJA=/opt/homebrew/bin/ninja
mkdir -p "$GPU" "$LOGS"

EPOXY_COMMIT=bf98587477fe68d07b93319ece7b40a7d0e2eabe
VIRGL_COMMIT=482f9d8b8c2d2288efa10a027116216909d2c226

fetch () {
    local dir="$1"
    local url="$2"
    local commit="$3"
    [ -d "$GPU/$dir" ] || git clone -q "$url" "$GPU/$dir"
    ( cd "$GPU/$dir" && git checkout -q "$commit" )
}

fetch epoxy https://github.com/utmapp/libepoxy.git   "$EPOXY_COMMIT"
fetch virgl https://github.com/utmapp/virglrenderer.git "$VIRGL_COMMIT"

export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig"

echo "==> libepoxy"
( cd "$GPU/epoxy"
  rm -rf _build
  $MESON setup _build --cross-file "$HUSK_ROOT/build/ios-arm64/cross-ios.meson" \
      --prefix "$PREFIX" --default-library=static \
      -Dtests=false -Dglx=no -Degl=yes -Dx11=false
  $NINJA -C _build install ) > "$LOGS/epoxy.log" 2>&1 \
  || { echo "epoxy failed:" >&2; grep -a "error:\|FAILED:" "$LOGS/epoxy.log" | head -10 >&2; exit 1; }
echo "    $(ls -lh "$PREFIX/lib/libepoxy.a" | awk '{print $5}')"

echo "==> virglrenderer (render server in thread mode)"
# Two things this needs that the normal cross file does not give it.
#
# cross-ios-darwin.meson sets host system = darwin rather than ios. virglrenderer
# gates its Metal backend on host_machine.system() == 'darwin', so with 'ios' it
# never calls add_languages('objc') and dies with "No host machine compiler for
# vrend_metal.m". UTM's meson_darwin_build forces the same thing.
#
# render-server-mode=thread, because iOS cannot spawn processes -- the same
# constraint that makes QEMU an in-process library here. UTM uses process mode
# on macOS and thread mode everywhere else for exactly this reason.
( cd "$GPU/virgl"
  rm -rf _build
  $MESON setup _build --cross-file "$HUSK_ROOT/build/ios-arm64/cross-ios-darwin.meson" \
      --prefix "$PREFIX" --default-library=static \
      -Dtests=false -Dcheck-gl-errors=false -Dvenus=false -Dvulkan-dload=false \
      -Drender-server-mode=thread
  $NINJA -C _build install ) > "$LOGS/virgl.log" 2>&1 \
  || { echo "virglrenderer failed:" >&2; grep -a "error:\|FAILED:\|ERROR" "$LOGS/virgl.log" | head -10 >&2; exit 1; }
echo "    $(ls -lh "$PREFIX/lib/libvirglrenderer.a" | awk '{print $5}')"

echo "==> done; QEMU can now be configured with --enable-opengl --enable-virglrenderer"
