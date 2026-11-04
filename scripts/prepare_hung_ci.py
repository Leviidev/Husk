#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Stage Android resources from a hash-pinned upstream IPA, never its app executable.

The original Java/runtime packaging scripts depend on paths on the author's Mac.
This bootstrap reuses only java-runtime and android-system resources. All host
runtime C/Swift code, including iconv, is built from this checkout.
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import shutil
import stat
import zipfile

ROOT = Path(__file__).resolve().parents[1]
PS4_DEPENDENCY = '''      - framework: ../../build/ios-arm64/lib/libhuskps4.dylib
        embed: true
        codeSign: true
        link: false
'''
REQUIRED = (
    "java-runtime/husk-framework.dex",
    "java-runtime/com.android.art/javalib/core-oj.jar",
    "java-runtime/com.android.art/javalib/core-libart.jar",
    "java-runtime/com.android.art/lib64/libc++.so",
    "java-runtime/com.android.art/lib64/libjavacore.so",
    "java-runtime/com.android.art/lib64/libopenjdk.so",
    "java-runtime/com.android.i18n/javalib/core-icu4j.jar",
    "java-runtime/com.android.conscrypt/javalib/conscrypt.jar",
    "android-system/etc/fonts.xml",
)


def prepare(ipa, expected_sha256, root=ROOT, source_url="https://github.com/Leviidev/Husk/tree/3.3"):
    with open(ipa, "rb") as source:
        digest = hashlib.file_digest(source, "sha256").hexdigest()
    if digest.lower() != expected_sha256.lower():
        raise ValueError(f"upstream IPA SHA-256 mismatch: {digest}")
    build = root / "build"
    build.mkdir(exist_ok=True)
    with zipfile.ZipFile(ipa) as z:
        infos = [i for i in z.infolist() if len(PurePosixPath(i.filename).parts) == 3
                 and i.filename.startswith("Payload/") and i.filename.endswith(".app/Info.plist")]
        if len(infos) != 1:
            raise ValueError("expected one Payload/*.app/Info.plist")
        info = plistlib.loads(z.read(infos[0]))
        if info.get("CFBundleShortVersionString") != "3.3":
            raise ValueError("resource bootstrap expects the audited Husk 3.3 runtime layout")
        prefix = infos[0].filename.removesuffix("Info.plist")
        entries = []
        seen = set()
        total = 0
        for item in z.infolist():
            if not item.filename.startswith(prefix):
                continue
            rel = item.filename[len(prefix):]
            if not rel.startswith(("java-runtime/", "android-system/")):
                continue
            parts = PurePosixPath(rel).parts
            if ".." in parts or "\\" in rel or ":" in rel or stat.S_ISLNK(item.external_attr >> 16):
                raise ValueError(f"unsafe resource entry: {rel}")
            if item.is_dir():
                continue
            if rel in seen:
                raise ValueError(f"duplicate resource entry: {rel}")
            seen.add(rel)
            total += item.file_size
            if item.file_size > 512 << 20 or total > 2 << 30:
                raise ValueError("resource archive exceeds extraction limits")
            entries.append((item, rel))
        missing = set(REQUIRED) - seen
        if missing:
            raise ValueError("upstream IPA lacks required resources: " + ", ".join(sorted(missing)))
        # Refuse a dirty destination instead of silently mixing runtime versions.
        for dirname in ("java-runtime", "android-system"):
            dest = build / dirname
            if dest.exists() and any(dest.iterdir()):
                raise ValueError(f"use a clean build directory; destination already populated: {dest}")
        for item, rel in entries:
            dest = build.joinpath(*PurePosixPath(rel).parts)
            dest.parent.mkdir(parents=True, exist_ok=True)
            with z.open(item) as src, dest.open("wb") as dst:
                shutil.copyfileobj(src, dst)
    spec = (root / "src/app/project.yml").read_text(encoding="utf-8")
    if spec.count(PS4_DEPENDENCY) != 1:
        raise ValueError("PS4 dependency layout changed: review Android-only CI spec generation")
    spec = spec.replace(PS4_DEPENDENCY, "", 1)
    (root / "src/app/project.hung.generated.yml").write_text(spec, encoding="utf-8")
    provenance = {"resourceIPASHA256": digest, "upstreamVersion": info["CFBundleShortVersionString"],
                  "upstreamBuild": info.get("CFBundleVersion"), "correspondingSource": source_url,
                  "copied": ["java-runtime", "android-system"], "ps4Included": False,
                  "hostRuntime": "built from the current checkout; no upstream app executable copied"}
    (build / "support-provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    return provenance


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("ipa", type=Path)
    ap.add_argument("sha256")
    args = ap.parse_args()
    print(json.dumps(prepare(args.ipa, args.sha256), indent=2))
