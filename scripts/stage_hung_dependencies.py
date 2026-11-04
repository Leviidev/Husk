#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Reuse unchanged, SHA-pinned upstream QEMU/graphics dependencies for an app build.

The Husk app executable is never copied. All modified C/Swift source is compiled
by Xcode; rebuilding every upstream dependency remains a separate workflow mode.
"""
import argparse
import bz2
import hashlib
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import stat
import struct
import zipfile

ROOT = Path(__file__).resolve().parents[1]
NATIVE = {
    "Frameworks/libqemu-aarch64-softmmu.dylib": "build/ios-arm64/lib/libqemu-aarch64-softmmu.dylib",
    "Frameworks/libANGLE-shared.dylib": "build/ios-arm64/sysroot/lib/libANGLE-shared.dylib",
    "Frameworks/MoltenVK.framework/MoltenVK": "build/ios-arm64/lib/MoltenVK.xcframework/ios-arm64/MoltenVK.framework/MoltenVK",
}
OTHER = {
    "Frameworks/MoltenVK.framework/Info.plist": "build/ios-arm64/lib/MoltenVK.xcframework/ios-arm64/MoltenVK.framework/Info.plist",
    "vmlinuz-virt": "guests/phase0/vmlinuz-virt",
    "initramfs-virt": "guests/phase0/initramfs-virt",
}


def stage(ipa, expected, root=ROOT):
    with open(ipa, "rb") as f:
        actual = hashlib.file_digest(f, "sha256").hexdigest()
    if actual != expected.lower():
        raise ValueError("upstream IPA SHA-256 mismatch")
    records = []
    def write(rel, data, source):
        path = root / rel
        if path.exists() and path.read_bytes() != data:
            raise ValueError("refusing to overwrite a different staged dependency: " + rel)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)
        records.append({"source": source, "destination": rel, "sha256": hashlib.sha256(data).hexdigest()})
    with zipfile.ZipFile(ipa) as z:
        names = z.namelist()
        infos = [n for n in names if re.fullmatch(r"Payload/[^/]+\.app/Info\.plist", n)]
        if len(infos) != 1:
            raise ValueError("expected one upstream app")
        meta = plistlib.loads(z.read(infos[0]))
        if meta.get("CFBundleShortVersionString") != "3.3":
            raise ValueError("expected Husk 3.3 dependencies")
        prefix = infos[0].removesuffix("Info.plist")
        def read(rel):
            name = prefix + rel
            if names.count(name) != 1:
                raise ValueError("missing or duplicate dependency: " + rel)
            item = z.getinfo(name)
            if stat.S_ISLNK(item.external_attr >> 16) or item.file_size > 512 << 20:
                raise ValueError("invalid dependency entry: " + rel)
            return z.read(item)
        for src, dest in NATIVE.items():
            data = read(src)
            if len(data) < 32 or struct.unpack_from("<II", data) != (0xFEEDFACF, 0x0100000C):
                raise ValueError("expected arm64 Mach-O dependency: " + src)
            write(dest, data, src)
        for src, dest in OTHER.items():
            write(dest, read(src), src)
        bios = "third_party/build/qemu-10.0.12-utm/pc-bios/"
        write(bios + "edk2-aarch64-code.fd.bz2", bz2.compress(read("edk2-aarch64-code.fd")), "edk2-aarch64-code.fd (lossless bzip2)")
        for name in names:
            if name.startswith(prefix) and re.fullmatch(r"pc-bios/(?:efi|pxe)-[A-Za-z0-9_-]+\.rom", name[len(prefix):]):
                rel = name[len(prefix):]
                write(bios + PurePosixPath(rel).name, read(rel), rel)
    wrapper = {"CFBundlePackageType": "XFWK", "XCFrameworkFormatVersion": "1.0",
               "AvailableLibraries": [{"LibraryIdentifier": "ios-arm64", "LibraryPath": "MoltenVK.framework",
                                       "SupportedArchitectures": ["arm64"], "SupportedPlatform": "ios"}]}
    write("build/ios-arm64/lib/MoltenVK.xcframework/Info.plist", plistlib.dumps(wrapper), "generated XCFramework descriptor")
    provenance = root / "build/support-provenance.json"
    value = json.loads(provenance.read_text()) if provenance.exists() else {"resourceIPASHA256": actual}
    value["unchangedNativeDependencies"] = records
    value["hostAppExecutable"] = "compiled from the current checkout; upstream Husk executable never copied"
    provenance.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    return records


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("ipa", type=Path)
    p.add_argument("sha256")
    args = p.parse_args()
    print(json.dumps(stage(args.ipa, args.sha256), indent=2))
