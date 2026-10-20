#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Verify that every translated format preserves its dynamic data arguments."""
import json
import re
import subprocess
import sys
from pathlib import Path

root = Path(sys.argv[1])


def catalog(language):
    path = root / f"{language}.lproj/Localizable.strings"
    return json.loads(subprocess.check_output(["plutil", "-convert", "json", "-o", "-", str(path)]))


def arguments(text):
    found, automatic, position = set(), 1, 0
    for match in re.finditer(r"%(?:%|(?:(\d+)\$)?@)", text):
        assert "%" not in text[position:match.start()], f"Unsupported format: {text!r}"
        position = match.end()
        if match[0] != "%%":
            found.add(int(match[1]) if match[1] else automatic)
            automatic += 1
    assert "%" not in text[position:], f"Unsupported format: {text!r}"
    return found


english, chinese = catalog("en"), catalog("zh-Hans")
assert english.keys() == chinese.keys(), "Languages have different keys"
# Parameter 2 contains only English's plural suffix, never user data.
plural_suffixes = {
    "Restored %@ file%@ from %@.",
    "Metadata for %@ of your app%@ has been updated. Would you like to download the updates?",
    "%@ DEX file%@, %@",
}
for key, translation in chinese.items():
    assert translation, f"Empty translation: {key}"
    assert english[key] == key, f"English fallback differs from the source: {key}"
    expected = arguments(key) - ({2} if key in plural_suffixes else set())
    assert arguments(translation) == expected, f"Changed interpolation arguments: {key}"
print(f"Catalog checks passed: {len(chinese)} Chinese translations, matching English keys and valid placeholders.")
