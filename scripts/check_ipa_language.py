#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Exercise the IPA's actual language metadata/resources via Bundle.main."""
import argparse
import plistlib
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path

root = Path(__file__).resolve().parent.parent
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("ipa", type=Path)
parser.add_argument("--chinese-only", action="store_true", help="Validate a dedicated Chinese-only distribution.")
args = parser.parse_args()
ipa = args.ipa
expected_languages = ["zh-Hans"] if args.chinese_only else ["en", "zh-Hans"]
development_language = "zh-Hans" if args.chinese_only else "en"
bundles = ["Payload/Husk.app/"]
with tempfile.TemporaryDirectory(prefix="husk-language-") as temp, zipfile.ZipFile(ipa) as archive:
    temp = Path(temp)
    executable = temp / "LanguageCheck"
    subprocess.run(["xcrun", "swiftc", str(root / "src/app/Shared/Localization.swift"),
                    str(root / "tests/PackagedLanguageChecks.swift"), "-o", str(executable)], check=True)
    bundles += sorted({name.rsplit("Info.plist", 1)[0] for name in archive.namelist()
                       if name.startswith("Payload/Husk.app/PlugIns/") and name.endswith(".appex/Info.plist")})
    for index, prefix in enumerate(bundles):
        info = plistlib.loads(archive.read(prefix + "Info.plist"))
        assert info["CFBundleDevelopmentRegion"] == development_language, prefix
        languages = {name[len(prefix):].split("/", 1)[0]
                     for name in archive.namelist() if name.startswith(prefix)
                     and ".lproj/" in name[len(prefix):]
                     and "/" not in name[len(prefix):].split(".lproj/")[0]}
        assert languages == {f"{language}.lproj" for language in expected_languages}, (prefix, languages)
        contents = temp / f"Check{index}.app/Contents"
        (contents / "MacOS").mkdir(parents=True)
        for language in expected_languages:
            resources = contents / "Resources" / f"{language}.lproj"
            resources.mkdir(parents=True)
            (resources / "Localizable.strings").write_bytes(
                archive.read(prefix + language + ".lproj/Localizable.strings"))
        host_info = {
            "CFBundleIdentifier": f"com.husk.languagecheck{index}",
            "CFBundleExecutable": "LanguageCheck",
            "CFBundlePackageType": "APPL",
            "CFBundleDevelopmentRegion": info["CFBundleDevelopmentRegion"],
            "CFBundleLocalizations": info.get("CFBundleLocalizations", expected_languages),
        }
        (contents / "Info.plist").write_bytes(plistlib.dumps(host_info))
        target = contents / "MacOS/LanguageCheck"
        shutil.copy2(executable, target)
        for preference in ["en-US", "zh-Hans-CN", "ja-JP"]:
            expected = "zh-Hans" if args.chinese_only or preference.startswith("zh") else "en"
            result = subprocess.run([str(target), expected, "-AppleLanguages", f"({preference})"],
                                    text=True, capture_output=True, check=True)
            print(f"{prefix} [{preference}] {result.stdout.strip()}")
