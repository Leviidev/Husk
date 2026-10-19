#!/bin/bash
# Build Husk's bundled Android Java runtime from the /system partition used by
# the Lineage guest. Usage: build_java_runtime_from_system.sh <system.img>
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SYSTEM_IMG="${1:?usage: build_java_runtime_from_system.sh <system.img>}"
DEBUGFS="${DEBUGFS:-$(command -v debugfs || echo /opt/homebrew/opt/e2fsprogs/sbin/debugfs)}"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

[ -f "$SYSTEM_IMG" ] || { echo "system image not found: $SYSTEM_IMG" >&2; exit 1; }
[ -x "$DEBUGFS" ] || { echo "debugfs not found: $DEBUGFS" >&2; exit 1; }

dump_file() {
    local source="$1" destination="$2"
    "$DEBUGFS" -R "dump $source $destination" "$SYSTEM_IMG" >/dev/null 2>&1
    [ -s "$destination" ] || { echo "failed to extract $source from Lineage /system" >&2; exit 1; }
}

extract_apex() {
    local module="$1" archive="" suffix apex_name listing
    for suffix in capex apex; do
        rm -f "$WORK/$module.$suffix"
        "$DEBUGFS" -R "dump /system/apex/$module.$suffix $WORK/$module.$suffix" \
            "$SYSTEM_IMG" >/dev/null 2>&1 || true
        if [ -s "$WORK/$module.$suffix" ]; then
            archive="$WORK/$module.$suffix"
            break
        fi
    done
    if [ -z "$archive" ]; then
        listing="$("$DEBUGFS" -R "ls /system/apex" "$SYSTEM_IMG" 2>/dev/null || true)"
        apex_name="$(printf '%s\n' "$listing" | grep -oE "${module//./\\.}(@[^[:space:]()/]+)?\\.(capex|apex)" | head -1 || true)"
        if [ -n "$apex_name" ]; then
            archive="$WORK/$apex_name"
            "$DEBUGFS" -R "dump /system/apex/$apex_name $archive" "$SYSTEM_IMG" >/dev/null 2>&1 || true
            [ -s "$archive" ] || archive=""
        fi
    fi
    [ -n "$archive" ] || { echo "could not find $module APEX in Lineage /system" >&2; exit 1; }

    python3 - "$archive" "$WORK/$module.img" <<'PY'
import io, sys, zipfile
source, output = sys.argv[1:]
with zipfile.ZipFile(source) as outer:
    if "original_apex" in outer.namelist():
        content = outer.read("original_apex")
        apex = zipfile.ZipFile(io.BytesIO(content))
    else:
        apex = outer
    with open(output, "wb") as target:
        target.write(apex.read("apex_payload.img"))
PY
    [ -s "$WORK/$module.img" ] || { echo "empty payload for $module" >&2; exit 1; }
}

extract_apex_tree() {
    local module="$1"
    local image="$WORK/$module.img" root="$WORK/apex/$module"
    mkdir -p "$root"
    case "$module" in
        com.android.art)
            "$DEBUGFS" -R "rdump /javalib $root" "$image" >/dev/null 2>&1
            "$DEBUGFS" -R "rdump /lib64 $root" "$image" >/dev/null 2>&1
            ;;
        com.android.i18n|com.android.conscrypt)
            "$DEBUGFS" -R "rdump /javalib $root" "$image" >/dev/null 2>&1
            "$DEBUGFS" -R "rdump /lib64 $root" "$image" >/dev/null 2>&1
            "$DEBUGFS" -R "rdump /etc $root" "$image" >/dev/null 2>&1 || true
            ;;
        com.android.tzdata)
            "$DEBUGFS" -R "rdump /etc $root" "$image" >/dev/null 2>&1
            ;;
    esac
}

mkdir -p "$WORK/apex"
for module in com.android.art com.android.i18n com.android.tzdata com.android.conscrypt; do
    echo "==> extracting $module"
    extract_apex "$module"
    extract_apex_tree "$module"
done

HTTP_LEGACY="$WORK/org.apache.http.legacy.jar"
FRAMEWORK_RES="$WORK/framework-res.apk"
dump_file /system/framework/org.apache.http.legacy.jar "$HTTP_LEGACY"
dump_file /system/framework/framework-res.apk "$FRAMEWORK_RES"

echo "==> compiling Husk's Java framework"
HTTP_LEGACY="$HTTP_LEGACY" \
FRAMEWORK_RES="$FRAMEWORK_RES" \
    "$HUSK_ROOT/scripts/build_java_runtime.sh" "$WORK/apex"

JRT="$HUSK_ROOT/build/java-runtime"
for required in \
    com.android.art/javalib/core-oj.jar \
    com.android.art/javalib/core-libart.jar \
    com.android.i18n/javalib/core-icu4j.jar \
    com.android.conscrypt/javalib/conscrypt.jar \
    org.apache.http.legacy.jar framework-res.apk husk-framework.dex; do
    [ -s "$JRT/$required" ] || { echo "required Java runtime file missing: $JRT/$required" >&2; exit 1; }
done
echo "==> Java runtime ready: $(du -sh "$JRT" | cut -f1)"
