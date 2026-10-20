#!/bin/bash
# Host-side checks for the app's Foundation-based localization path.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TMP="$(mktemp -d "${TMPDIR:-/tmp}/husk-localization.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT
plutil -lint "$ROOT/src/app/Shared/"{en,zh-Hans}.lproj/Localizable.strings \
    "$ROOT/src/app/Husk/"{en,zh-Hans}.lproj/InfoPlist.strings
python3 "$ROOT/tests/check_localization_catalog.py" "$ROOT/src/app/Shared"
xcrun swiftc "$ROOT/src/app/Shared/Localization.swift" \
    "$ROOT/tests/LocalizationChecks.swift" -o "$TMP/check"
"$TMP/check" "$ROOT/src/app/Shared"
