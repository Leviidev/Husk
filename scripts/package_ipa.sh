#!/bin/bash
# Build Husk and package one IPA for every installer, validating the bundle before shipping it.
#
# No signing team is needed: AltStore / SideStore re-sign at install with the
# user's own profile. The IPA is signed ad hoc with Husk.entitlements embedded
# anyway, because TrollStore keeps the entitlements a binary already carries
# (RootHelper's signApp reads them from the binary) and invents only
# get-task-allow for one that has none. Signed this way, the same file gets the
# memory and dynamic-codesigning entitlements under TrollStore, and a sideloader
# simply replaces the signature.
#
# The validation step exists because a bundle missing CFBundleIdentifier or
# CFBundleExecutable builds and zips perfectly happily, and then fails to install
# with no useful message. Xcode does not inject those keys when a custom
# INFOPLIST_FILE is supplied without GENERATE_INFOPLIST_FILE, which is exactly how
# this project is set up.
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DD="${DD:-/tmp/husk_ipa}"
TROLLSTORE=false
OUT=""
for arg in "$@"; do
    case "$arg" in
        --trollstore|--tipa) TROLLSTORE=true ;;
        *) OUT="${OUT:-$arg}" ;;
    esac
done
[[ "$OUT" == *.tipa ]] && TROLLSTORE=true
if [ -z "$OUT" ]; then
    OUT="$HOME/Desktop/Husk.$([ "$TROLLSTORE" = true ] && echo tipa || echo ipa)"
fi
mkdir -p "$DD"

# The .app is not the only thing that can be stale. The Xcode target links the
# dylib staged in build/ios-arm64/lib, which is filled in by build_ios.sh's qemu
# stage -- so rebuilding QEMU with plain ninja produces a new dylib that never
# reaches the app, and the IPA ships the previous one with no warning at all.
# That happened once and looked exactly like a fix that did not work.
BUILT="$HUSK_ROOT/third_party/build/qemu-10.0.12-utm/_husk_build/libqemu-aarch64-softmmu.dylib"
STAGED="$HUSK_ROOT/build/ios-arm64/lib/libqemu-aarch64-softmmu.dylib"
if [ -f "$BUILT" ] && [ "$BUILT" -nt "$STAGED" ]; then
    echo "==> staged dylib is older than the built one; restaging"
    cp "$BUILT" "$STAGED"
fi

# Regenerate the project first.
#
# project.yml globs src/app/Husk, so adding a source file there is meant to be
# all it takes -- but the checked-in .pbxproj is a build artefact of that glob,
# and nothing was regenerating it. A new file was therefore silently absent from
# the target, and the only symptom was "cannot find X in scope" for a type that
# is plainly right there on disk.
if command -v xcodegen >/dev/null 2>&1; then
    echo "==> regenerating the project from project.yml"
    (cd "$HUSK_ROOT/src/app" && xcodegen generate --quiet)
else
    echo "==> xcodegen not installed; using the checked-in project as-is" >&2
fi

echo "==> building"
xcodebuild -project "$HUSK_ROOT/src/app/Husk.xcodeproj" -scheme Husk \
    -sdk iphoneos -configuration Release -derivedDataPath "$DD" \
    CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
    build 2>&1 | tee "$DD/build.log" | grep -E "error:|BUILD (SUCCEEDED|FAILED)" || true

# A failed build used to sail straight past this: the previous .app is still in
# DerivedData, so validation and packaging both succeed and produce an IPA of
# the LAST build. Shipping a stale binary silently is the worst outcome here --
# it looks exactly like a fix that did not work.
if ! grep -q "BUILD SUCCEEDED" "$DD/build.log"; then
    echo "build failed; refusing to package a stale app" >&2
    grep -E "error:" "$DD/build.log" | head -10 >&2
    exit 1
fi

APP="$DD/Build/Products/Release-iphoneos/Husk.app"
[ -d "$APP" ] || { echo "no app bundle at $APP" >&2; exit 1; }

# Stamp the build's identity into the bundle so its logs can name themselves.
# A log from a stale install is otherwise indistinguishable from a log proving a
# fix did not work.
APP_PLIST="$DD/Build/Products/Release-iphoneos/Husk.app/Info.plist"
if [ -f "$APP_PLIST" ]; then
    COMMIT="$(git -C "$HUSK_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
    git -C "$HUSK_ROOT" diff --quiet 2>/dev/null || COMMIT="$COMMIT-dirty"
    plutil -replace HuskBuildCommit -string "$COMMIT" "$APP_PLIST"
    plutil -replace HuskBuildDate -string "$(date -u '+%Y-%m-%d %H:%M UTC')" "$APP_PLIST"
    echo "==> stamped build $COMMIT"
fi

echo "==> validating bundle"
PLIST="$APP/Info.plist"
rc=0
for key in CFBundleIdentifier CFBundleExecutable CFBundleName \
           CFBundlePackageType CFBundleVersion CFBundleShortVersionString \
           MinimumOSVersion UIDeviceFamily; do
    val="$(/usr/libexec/PlistBuddy -c "Print :$key" "$PLIST" 2>/dev/null || true)"
    if [ -z "$val" ]; then
        echo "  MISSING  $key   <-- the app will not install" >&2
        rc=1
    else
        printf "  ok       %-28s %s\n" "$key" "$(echo "$val" | head -1)"
    fi
done

# The executable named in the plist must actually exist.
EXE="$(/usr/libexec/PlistBuddy -c "Print :CFBundleExecutable" "$PLIST" 2>/dev/null || true)"
if [ -n "$EXE" ] && [ ! -f "$APP/$EXE" ]; then
    echo "  MISSING  executable '$EXE' named by CFBundleExecutable" >&2
    rc=1
elif [ -n "$EXE" ]; then
    printf "  ok       %-28s %s\n" "executable present" "$EXE"
fi

# Both dylibs must be embedded, or the app dies at launch with a dyld error.
#
# ANGLE is checked here because it was not, and an IPA shipped without it: the
# app reached qemu_egl_init_dpy_cocoa and aborted with "Couldn't open
# @rpath/libANGLE-shared.dylib". Everything else in this validation passed. A
# check that covers one of two required libraries is a check that reports
# success on a bundle that cannot launch.
for lib in libqemu-aarch64-softmmu.dylib libANGLE-shared.dylib; do
    if [ ! -f "$APP/Frameworks/$lib" ]; then
        echo "  MISSING  Frameworks/$lib" >&2
        rc=1
    else
        printf "  ok       %-28s %s\n" "${lib%%-*} dylib embedded" \
            "$(du -h "$APP/Frameworks/$lib" | cut -f1)"
    fi
done

# Guest images, firmware, and blank disk seeds required by first launch.
for f in vmlinuz-virt initramfs-virt husk-jit.js \
         edk2-aarch64-code.fd lineage-efi-vars-seed.fd lineage-vdb-seed.qcow2; do
    if [ ! -f "$APP/$f" ]; then
        echo "  MISSING  $f" >&2
        rc=1
    else
        printf "  ok       %-28s %s\n" "$f" "$(du -h "$APP/$f" | cut -f1)"
    fi
done

# Built-in StikJIT's helper (without it the app still installs and runs with
# StikDebug, but its JIT setup would offer a method that can only fail), and the
# widget that draws the download Live Activity.
for f in "$APP/PlugIns/HuskJITHelper.appex/HuskJITHelper" \
         "$APP/PlugIns/HuskDownloadsWidget.appex/HuskDownloadsWidget" \
         "$APP/Frameworks/StikJIT.framework/StikJIT" \
         "$APP/Frameworks/StikJIT.framework/Info.plist"; do
    if [ ! -f "$f" ]; then
        echo "  MISSING  ${f#$APP/}" >&2
        rc=1
    else
        printf "  ok       %-28s %s\n" "$(basename "$f")" "${f#$APP/}"
    fi
done

[ $rc -eq 0 ] || { echo "==> bundle is not installable; refusing to package" >&2; exit 1; }

echo "==> packaging"
STAGE="$(mktemp -d)"
mkdir -p "$STAGE/Payload"
cp -R "$APP" "$STAGE/Payload/"

# Ad hoc, inside out: loose dylibs and frameworks, then app extensions, then the
# app with its entitlements (see the top of this file for why).
ENT_BASE="$HUSK_ROOT/src/app/Husk/Husk.entitlements"
ENT="$STAGE/entitlements.plist"
cp "$ENT_BASE" "$ENT"
BANNED_KEYS=(
    "com.apple.private.cs.debugger"
    "dynamic-codesigning"
    "com.apple.private.skip-library-validation"
)
if [ "$TROLLSTORE" = true ]; then
    for key in "${BANNED_KEYS[@]}"; do
        plutil -remove "$key" "$ENT" 2>/dev/null || true
    done
fi
SAPP="$STAGE/Payload/$(basename "$APP")"
sign() { codesign --force --sign - --timestamp=none "$@"; }
find "$SAPP" -name "*.dylib" -not -path "*/Frameworks/*.framework/*" | while read -r f; do sign "$f"; done
for fw in "$SAPP"/Frameworks/*.framework; do [ -d "$fw" ] && sign "$fw"; done
for ex in "$SAPP"/PlugIns/*.appex; do [ -d "$ex" ] && sign "$ex"; done
sign --entitlements "$ENT" "$SAPP"
echo "==> entitlements in the signed app:"
EMBEDDED_ENTS=$(codesign -d --entitlements - "$SAPP" 2>&1 || true)
REQUIRED_PATTERN="get-task-allow|increased-memory|extended-virtual"
[ "$TROLLSTORE" != true ] && REQUIRED_PATTERN+="|dynamic-codesigning"
if ! grep -qE "$REQUIRED_PATTERN" <<< "$EMBEDDED_ENTS"; then
    echo "==> entitlements did not embed; refusing to package" >&2
    rm -rf "$STAGE"
    exit 1
fi
if [ "$TROLLSTORE" = true ]; then
    BANNED_PATTERN=$(IFS='|'; echo "${BANNED_KEYS[*]}")
    if matched=$(grep -oE "$BANNED_PATTERN" <<< "$EMBEDDED_ENTS" | head -n 1) && [ -n "$matched" ]; then
        echo "==> banned entitlement '$matched' found in trollstore package; refusing to package" >&2
        rm -rf "$STAGE"
        exit 1
    fi
fi
TMP_OUT="$STAGE/package.tmp"
( cd "$STAGE" && zip -qry "$TMP_OUT" Payload )

# Atomic replace so a half-written IPA never sits where the good one was.
mv -f "$TMP_OUT" "$OUT"
rm -rf "$STAGE"
echo "==> $OUT  ($(du -h "$OUT" | cut -f1))"
