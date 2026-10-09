#!/bin/bash
# Build MoltenVK for iOS (arm64) and stage it where the app project expects it.
#
# Unreal Engine 4 games (Minecraft Dungeons) draw with Vulkan, and on Apple hardware Vulkan is MoltenVK. The Mac one from Homebrew is no use on a phone, so this
# builds KhronosGroup/MoltenVK from source: `fetchDependencies --ios` clones and builds SPIRV-Cross and SPIRV-Tools, `make ios` produces a dynamic MoltenVK.framework
# in an xcframework. project.yml embeds build/ios-arm64/lib/MoltenVK.xcframework; the native runtime opens it by path (husk_ue4_set_vulkan).
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${MOLTENVK_VERSION:-v1.4.2}"
SRC="$HUSK_ROOT/third_party/build/MoltenVK-${VERSION#v}"
OUT="$HUSK_ROOT/build/ios-arm64/lib"

if [ ! -d "$SRC" ]; then
    echo "==> cloning MoltenVK $VERSION"
    git clone --depth 1 --branch "$VERSION" https://github.com/KhronosGroup/MoltenVK "$SRC"
fi
cd "$SRC"
# SPIRV-Cross at MoltenVK's pinned revision with Husk's patches, kept outside External/ (fetchDependencies force-checks-out what is there).
# The dependencies are rebuilt whenever the patches change.
SPVC_REV="$(cat ExternalRevisions/SPIRV-Cross_repo_revision)"
SPVC="$HUSK_ROOT/third_party/build/SPIRV-Cross-$SPVC_REV"
if [ ! -d "$SPVC/.git" ]; then
    echo "==> cloning SPIRV-Cross $SPVC_REV"
    git clone https://github.com/KhronosGroup/SPIRV-Cross "$SPVC"
    git -C "$SPVC" checkout -q "$SPVC_REV"
fi
for p in "$HUSK_ROOT"/patches/husk-spirv-cross-*.patch; do
    git -C "$SPVC" apply --reverse --check "$p" 2>/dev/null || { echo "==> applying $(basename "$p")"; git -C "$SPVC" apply "$p"; }
done
STAMP=External/build/husk-patches.stamp
WANT="$(cat "$HUSK_ROOT"/patches/husk-spirv-cross-*.patch | shasum | cut -c1-40)"
if [ ! -d External/build/Release/SPIRVCross.xcframework ] || [ "$(cat "$STAMP" 2>/dev/null)" != "$WANT" ]; then
    echo "==> fetching and building dependencies"
    ./fetchDependencies --ios --spirv-cross-root "$SPVC"
    echo "$WANT" > "$STAMP"
fi
echo "==> building"
make ios
mkdir -p "$OUT"
rm -rf "$OUT/MoltenVK.xcframework"
cp -R Package/Release/MoltenVK/dynamic/MoltenVK.xcframework "$OUT/"
echo "==> $OUT/MoltenVK.xcframework"
