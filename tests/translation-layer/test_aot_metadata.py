#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check actual AOT metadata placement in split and merged ELF load segments.

bash tools/regress/build.sh tools/elf2macho-test.c /tmp/elf2macho-test
LD_LLD=/path/to/ld.lld python3 tests/translation-layer/test_aot_metadata.py /tmp/elf2macho-test
A workspace Rust toolchain's rust-lld is also supported; no Android sysroot is needed.
"""
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
tool = pathlib.Path(sys.argv[1]).resolve()
linker = os.environ.get("LD_LLD") or shutil.which("ld.lld")
if not linker:
    candidates = list((ROOT / "build/iphone-test/toolchain/rustup/toolchains").glob("*/lib/rustlib/*/bin/rust-lld"))
    if candidates:
        linker = str(candidates[0])
if not linker:
    raise SystemExit("Requires lld (set LD_LLD); skipped without an ELF linker")
with tempfile.TemporaryDirectory(prefix="husk-aot-metadata-") as directory:
    work = pathlib.Path(directory)
    obj = work / "metadata.o"
    subprocess.run(["clang", "-target", "aarch64-linux-android26", "-O1", "-fPIC", "-funwind-tables", "-c",
                    str(ROOT / "tests/translation-layer/fixtures/aot_metadata.c"), "-o", str(obj)], check=True)
    for merged in (False, True):
        elf = work / ("merged.so" if merged else "split.so")
        command = [linker]
        if pathlib.Path(linker).name == "rust-lld":
            command += ["-flavor", "gnu"]
        command += ["-shared", "--eh-frame-hdr", "-z", "max-page-size=4096"]
        if merged:
            command += ["--no-rosegment"]
        subprocess.run([*command, str(obj), "-o", str(elf)], check=True)
        data = elf.read_bytes()
        offset = struct.unpack_from("<Q", data, 32)[0]
        count = struct.unpack_from("<H", data, 56)[0]
        headers = [struct.unpack_from("<II6Q", data, offset + i * 56) for i in range(count)]
        eh = next(p for p in headers if p[0] == 0x6474E550)
        load = next(p for p in headers if p[0] == 1 and p[3] <= eh[3] < p[3] + p[6])
        assert bool(load[1] & 1) == merged, "Fixture did not reproduce the requested unwind-section permissions"
        result = subprocess.run([str(tool), "--check-metadata", str(elf)], text=True, capture_output=True)
        if result.returncode:
            raise SystemExit(result.stdout + result.stderr)
        print(elf.name + ": " + result.stdout.strip())
