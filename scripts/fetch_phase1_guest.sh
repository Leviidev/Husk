#!/bin/bash
# Phase 1 guest: Debian arm64 + Waydroid (LineageOS 20, arm64, no Google apps).
#
# Why Waydroid rather than AOSP: source.android.com states plainly that "Android
# OS development on macOS isn't supported as of June 22, 2021", and requires a
# 64-bit Linux host with 400 GB of disk and 64 GB of RAM. Building AOSP on this
# machine is not a long path, it is a closed one.
#
# Waydroid also fits the product better than a raw Android image would. Its
# multi-window mode renders each Android app as its own chrome-free surface, and
# `waydroid app install` / `waydroid app launch <pkg>` is exactly the APK
# lifecycle Husk needs -- the same illusion, already solved, one layer down.
#
# The VANILLA system image carries no Google apps, so the brief's
# no-Play-Services constraint holds by construction rather than by us stripping
# anything out (and with it the redistribution problem disappears).
set -euo pipefail

HUSK_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$HUSK_ROOT/guests/phase1"
mkdir -p "$OUT"
cd "$OUT"

DEBIAN_URL="https://cloud.debian.org/images/cloud/trixie/latest/debian-13-generic-arm64.qcow2"
WAYDROID_BASE="https://downloads.sourceforge.net/project/waydroid/images"
WD_SYSTEM="lineage-20.0-20260403-VANILLA-waydroid_arm64-system.zip"
WD_VENDOR="lineage-20.0-20260403-MAINLINE-waydroid_arm64-vendor.zip"

get() {
    local url="$1" file="$2"
    if [ -s "$file" ]; then echo "[skip] $file"; return 0; fi
    echo "[get ] $file"
    curl -fL --retry 3 --retry-delay 5 -o "$file.part" "$url"
    mv "$file.part" "$file"
}

# generic, not nocloud: the nocloud variant ships WITHOUT cloud-init (it just
# auto-logs-in root on the console), whereas provisioning Waydroid unattended
# needs cloud-init's NoCloud datasource driven from a seed ISO.
get "$DEBIAN_URL" debian-13-generic-arm64.qcow2
get "$WAYDROID_BASE/system/lineage/waydroid_arm64/$WD_SYSTEM" waydroid-system.zip
get "$WAYDROID_BASE/vendor/waydroid_arm64/$WD_VENDOR"        waydroid-vendor.zip

echo "--- guests/phase1 ---"
ls -lh "$OUT" | grep -v '^total' | awk '{print "  "$5"\t"$9}'
