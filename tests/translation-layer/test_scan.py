#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
The APK scanner against real arm64 Android libraries.

Every library here is built by clang for an Android target and linked by lld,
so the layouts, relocation encodings and instruction patterns are the ones a
real APK carries. Where a number can be checked against LLVM's own tools --
relocation counts, imported symbols, stack-guard reads -- it is, rather than
against a number written down by hand.

    test_scan.py OUT_DIR SCAN_CLI
"""
import json
import os
import re
import struct
import subprocess
import sys
import zipfile
import zlib

OUT, CLI = sys.argv[1], sys.argv[2]
FIX = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fixtures")
failures = 0


def sh(*args):
    return subprocess.run(args, check=True, capture_output=True, text=True).stdout


def check(what, cond, detail=""):
    global failures
    if cond:
        print(f"ok   {what}")
    else:
        failures += 1
        print(f"FAIL {what} {detail}")


# ------------------------------------------------------------------ libraries

STUBS = os.path.join(OUT, "stubs")
os.makedirs(STUBS, exist_ok=True)


def build(name, src, target="aarch64-linux-android29", extra=(), outdir=OUT):
    out = os.path.join(outdir, name)
    sh("clang", f"--target={target}", "-O2", "-fPIC", "-shared", "-nostdlib",
       "-fuse-ld=lld", f"-Wl,-soname,{name}", "-o", out, os.path.join(FIX, src), *extra)
    return out


for stub in ("libc.so", "liblog.so", "libm.so"):
    build(stub, "empty.c", outdir=STUBS)
LINK = ("-L" + STUBS, "-lc", "-llog", "-lm")
P16 = "-Wl,-z,max-page-size=16384"

good = build("libgood.so", "plain.c", extra=(P16, "-Wl,--pack-dyn-relocs=android", *LINK))
flat = build("libflat.so", "plain.c", extra=(P16, "-Wl,--pack-dyn-relocs=none", *LINK))
relr = build("librelr.so", "plain.c", extra=(P16, "-Wl,--pack-dyn-relocs=relr", *LINK))
old = build("libold.so", "plain.c", extra=("-Wl,-z,max-page-size=4096", *LINK))
svc = build("libsvc.so", "syscall.c", extra=(P16, "-fstack-protector-all", *LINK))
v7 = build("libv7.so", "plain.c", target="armv7a-linux-androideabi29")
x86 = build("libx86.so", "plain.c", target="x86_64-linux-android29")


def readelf_relocs(lib):
    return len(re.findall(r"R_AARCH64_\w+", sh("llvm-readelf", "-r", lib)))


def readelf_imports(lib):
    count = 0
    for line in sh("llvm-readelf", "--dyn-syms", lib).splitlines():
        cols = line.split()
        # Num: Value Size Type Bind Vis Ndx Name -- undefined and named
        if len(cols) >= 8 and cols[0].rstrip(":").isdigit() and cols[6] == "UND":
            count += 1
    return count


def objdump_count(lib, pattern):
    return len(re.findall(pattern, sh("llvm-objdump", "-d", lib)))


# ---------------------------------------------------------------------- APKs

MANIFEST = b"\x03\x00\x08\x00" + bytes(range(256)) * 4
DEX = b"dex\n035\x00" + b"\x00" * 120


def apk(name, entries):
    path = os.path.join(OUT, name)
    with zipfile.ZipFile(path, "w") as z:
        for arc, data, deflate in entries:
            if isinstance(data, str):
                with open(data, "rb") as f:
                    data = f.read()
            z.writestr(zipfile.ZipInfo(arc), data,
                       compress_type=zipfile.ZIP_DEFLATED if deflate else zipfile.ZIP_STORED)
    return path


def zip64_apk(name, arcname, data_path):
    """One stored entry, every size and offset moved into ZIP64 fields."""
    with open(data_path, "rb") as f:
        data = f.read()
    arc = arcname.encode()
    crc = zlib.crc32(data)
    local = struct.pack("<IHHHHHIIIHH", 0x04034B50, 45, 0, 0, 0, 0, crc,
                        0xFFFFFFFF, 0xFFFFFFFF, len(arc), 20) + arc \
        + struct.pack("<HHQQ", 1, 16, len(data), len(data))
    body = local + data
    extra = struct.pack("<HHQQQ", 1, 24, len(data), len(data), 0)
    central = struct.pack("<IHHHHHHIIIHHHHHII", 0x02014B50, 45, 45, 0, 0, 0, 0, crc,
                          0xFFFFFFFF, 0xFFFFFFFF, len(arc), len(extra), 0, 0, 0, 0,
                          0xFFFFFFFF) + arc + extra
    cd_off, cd_size = len(body), len(central)
    end64 = struct.pack("<IQHHIIQQQQ", 0x06064B50, 44, 45, 45, 0, 0, 1, 1, cd_size, cd_off)
    locator = struct.pack("<IIQI", 0x07064B50, 0, cd_off + cd_size, 1)
    end = struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 0xFFFF, 0xFFFF, 0xFFFFFFFF,
                      0xFFFFFFFF, 0)
    path = os.path.join(OUT, name)
    with open(path, "wb") as f:
        f.write(body + central + end64 + locator + end)
    return path


java_apk = apk("java.apk", [("AndroidManifest.xml", MANIFEST, True),
                            ("classes.dex", DEX, True), ("classes2.dex", DEX, True),
                            ("res/raw/not-code.so", b"x", False)])
good_apk = apk("good.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True),
                            ("lib/arm64-v8a/libgood.so", good, False),
                            ("lib/arm64-v8a/librelr.so", relr, True),
                            ("lib/armeabi-v7a/libv7.so", v7, True)])
old_apk = apk("old.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True),
                          ("lib/arm64-v8a/libold.so", old, True),
                          ("lib/arm64-v8a/libsvc.so", svc, False)])
v7_apk = apk("v7.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True),
                        ("lib/armeabi-v7a/libv7.so", v7, True)])
x86_apk = apk("x86.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True),
                          ("lib/x86_64/libx86.so", x86, True)])
base_apk = apk("base.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True)])
split_apk = apk("split_config.arm64_v8a.apk", [("AndroidManifest.xml", MANIFEST, True),
                                               ("lib/arm64-v8a/libgood.so", good, False)])
unity_apk = apk("unity.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True),
                              ("lib/arm64-v8a/libil2cpp.so", good, False),
                              ("lib/arm64-v8a/libunity.so", flat, False)])
fake_apk = apk("fake.apk", [("AndroidManifest.xml", MANIFEST, True), ("classes.dex", DEX, True),
                            ("lib/arm64-v8a/libfake.so", b"not an ELF file at all", False),
                            ("lib/arm64-v8a/libx86inside.so", x86, False)])
zip64 = zip64_apk("zip64.apk", "lib/arm64-v8a/libgood.so", good)

truncated = os.path.join(OUT, "truncated.apk")
with open(good_apk, "rb") as f, open(truncated, "wb") as g:
    g.write(f.read()[: os.path.getsize(good_apk) // 2])
notzip = os.path.join(OUT, "notzip.apk")
with open(notzip, "wb") as f:
    f.write(os.urandom(4096))


def scan(*paths):
    raw = sh(CLI, "scan", *paths)
    return json.loads(raw)     # also proves the output is valid JSON


def lib(report, name):
    return next(l for l in report["libraries"] if l["name"] == name)


# --------------------------------------------------------------------- checks

r = scan(java_apk)
check("java: verdict", r["verdict"] == "java", r["verdict"])
check("java: dex counted", r["dexCount"] == 2 and r["dexBytes"] == 2 * len(DEX), r)
check("java: .so outside lib/ ignored", r["libraries"] == [] and r["abis"] == [], r)

r = scan(good_apk)
check("good: verdict", r["verdict"] == "native", r["summary"])
check("good: abis", r["abis"] == ["arm64-v8a", "armeabi-v7a"], r["abis"])
check("good: only arm64 analysed", {l["name"] for l in r["libraries"]} == {"libgood.so", "librelr.so"})
g = lib(r, "libgood.so")
check("good: status ok", g["status"] == "ok", g)
check("good: 16k aligned, no conflict", g["maxAlign"] == 16384 and g["conflictPages"] == 0, g)
check("good: APS2 recognised", g["packing"] == "android", g["packing"])
check("good: APS2 decode matches llvm-readelf",
      g["relocations"] == readelf_relocs(good), (g["relocations"], readelf_relocs(good)))
check("good: APS2 decode matches the unpacked build",
      g["relocations"] == json.loads(sh(CLI, "scan", apk("flat.apk", [
          ("AndroidManifest.xml", MANIFEST, True),
          ("lib/arm64-v8a/libflat.so", flat, False)])))["libraries"][0]["relocations"])
check("good: TLS seen", g["tls"] and g["tlsRelocations"] >= 1, g)
check("good: imports match llvm-readelf", g["imports"] == readelf_imports(good),
      (g["imports"], readelf_imports(good)))
check("good: needed", g["needed"] == ["libc.so", "liblog.so", "libm.so"], g["needed"])
check("good: soname", g["soname"] == "libgood.so", g["soname"])
check("good: system libraries", r["systemLibraries"] == ["libc.so", "liblog.so", "libm.so"],
      r["systemLibraries"])
rr = lib(r, "librelr.so")
check("relr: recognised", rr["packing"] == "relr" and rr["compressed"], rr)
check("relr: count matches llvm-readelf", rr["relocations"] == readelf_relocs(relr),
      (rr["relocations"], readelf_relocs(relr)))

r = scan(old_apk)
check("old: verdict", r["verdict"] == "nativeWithWork", r["summary"])
o = lib(r, "libold.so")
check("old: 4k page conflict found", o["maxAlign"] == 4096 and o["conflictPages"] >= 1, o)
check("old: status work", o["status"] == "work", o["status"])
check("old: layout marks the conflict", "!" in o["layout"], o["layout"])
s = lib(r, "libsvc.so")
check("svc: one system call", s["svc"] == 1, s["svc"])
check("svc: thread-register write", s["tpidrWrites"] == 1, s["tpidrWrites"])
want_reads = objdump_count(svc, r"mrs\s+x\d+, TPIDR_EL0")
check("svc: stack-guard reads match llvm-objdump",
      s["tpidrReads"] == want_reads and want_reads >= 3, (s["tpidrReads"], want_reads))
check("svc: notes explain", any("system call" in n for n in s["notes"]), s["notes"])

r = scan(v7_apk)
check("v7: 32-bit ARM only", r["verdict"] == "noArm64" and "32-bit ARM" in r["summary"], r)
r = scan(x86_apk)
check("x86: needs a translator", r["verdict"] == "noArm64" and "FEXCore" in r["summary"], r)

r = scan(base_apk, split_apk)
check("split: base and config merged", r["verdict"] == "native" and r["dexCount"] == 1
      and [l["apk"] for l in r["libraries"]] == ["split_config.arm64_v8a.apk"], r)

r = scan(unity_apk)
check("engine: Unity IL2CPP", r["engine"] == "Unity (IL2CPP)", r["engine"])

r = scan(fake_apk)
f1, f2 = lib(r, "libfake.so"), lib(r, "libx86inside.so")
check("fake: not ELF is blocked", f1["status"] == "blocked" and "not an ELF" in f1["notes"][0], f1)
check("fake: wrong machine is blocked", f2["status"] == "blocked" and "machine 62" in f2["notes"][0], f2)
check("fake: verdict", r["verdict"] == "nativeWithWork", r["verdict"])

r = scan(zip64)
check("zip64: read", r["ok"] and lib(r, "libgood.so")["status"] == "ok", r)

for bad in (truncated, notzip, os.path.join(OUT, "missing.apk")):
    r = scan(bad)
    check(f"unreadable: {os.path.basename(bad)}", not r["ok"] and r["verdict"] == "unreadable", r)

# Entries, as the app reads the manifest for the name and icon.
got = subprocess.run([CLI, "entry", good_apk, "AndroidManifest.xml", "1000000"],
                     capture_output=True).stdout
check("entry: deflated manifest round-trips", got == MANIFEST, len(got))
got = subprocess.run([CLI, "entry", good_apk, "lib/arm64-v8a/libgood.so", "100000000"],
                     capture_output=True).stdout
check("entry: stored library round-trips", got == open(good, "rb").read(), len(got))
rc = subprocess.run([CLI, "entry", good_apk, "AndroidManifest.xml", "10"]).returncode
check("entry: limit enforced", rc != 0)
rc = subprocess.run([CLI, "entry", good_apk, "nope", "1000"]).returncode
check("entry: missing entry", rc != 0)

print("all passed" if failures == 0 else f"{failures} FAILED")
sys.exit(1 if failures else 0)
