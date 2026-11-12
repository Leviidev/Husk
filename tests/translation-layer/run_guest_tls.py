#!/usr/bin/env python3
"""Build tiny arm64 ELF fixtures with Apple's assembler, then execute native loader regressions."""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--out", type=Path, required=True, help="Directory for build output, fixtures and logs")
parser.add_argument("--fixtures-only", action="store_true", help="Generate ELF fixtures without building or running the harness")
args = parser.parse_args()
OUT = args.out.resolve()
OUT.mkdir(parents=True, exist_ok=True)
TASK_TEMP = OUT / "tmp"
TASK_TEMP.mkdir(parents=True, exist_ok=True)
CHILD_ENV = dict(os.environ, TMPDIR=str(TASK_TEMP), TMP=str(TASK_TEMP), TEMP=str(TASK_TEMP), TL_TMP=str(TASK_TEMP))
assembly = [".text", ".p2align 2"]
for register in [0, 16, 17, 18, 28]:
    assembly += [f".globl _tp{register}", f"_tp{register}:", "stp x28, x30, [sp, #-16]!",
                 f"mrs x{register}, tpidr_el0", f"mov x0, x{register}", "ldp x28, x30, [sp], #16", "ret"]
assembly += [".globl _tpflags", "_tpflags:", "cmp x0, x0", "mrs x0, tpidr_el0", "csel x0, x0, xzr, eq", "ret",
             ".globl _write_g", "_write_g:", "mrs x8, tpidr_el0", "str x0, [x8, #16]", "ret",
             ".globl _read_g", "_read_g:", "mrs x0, tpidr_el0", "ldr x0, [x0, #16]", "ret",
             ".globl _read_tlsdesc", "_read_tlsdesc:", "stp x29, x30, [sp, #-16]!", "adr x0, _descriptor_placeholder",
             "ldr x1, [x0]", "blr x1", "mrs x1, tpidr_el0", "add x0, x1, x0", "ldr x0, [x0]", "ldp x29, x30, [sp], #16", "ret",
             ".globl _write_tlsdesc", "_write_tlsdesc:", "stp x19, x30, [sp, #-16]!", "mov x19, x0", "adr x0, _descriptor_placeholder",
             "ldr x1, [x0]", "blr x1", "mrs x1, tpidr_el0", "add x0, x1, x0", "str x19, [x0]", "ldp x19, x30, [sp], #16", "ret",
             ".globl _check_regs", "_check_regs:", "stp x19, x28, [sp, #-16]!", "stp x29, x30, [sp, #-16]!",
             "str q8, [sp, #-16]!", "mov x19, x0"]
assembly += [f"mov x{n}, #{0x100 + n}" for n in range(18)]
assembly += ["mov x28, #0x128", "movi v0.16b, #0x5a", "movi v8.16b, #0xa5", "movi v31.16b, #0x3c",
             "cmp x0, x0", "mrs x17, tpidr_el0"]
assembly += [f"str x{n}, [x19, #{n * 8}]" for n in range(18)]
assembly += ["str x28, [x19, #224]", "mrs x0, nzcv", "str x0, [x19, #240]", "str q0, [x19, #256]",
             "str q8, [x19, #272]", "str q31, [x19, #288]", "ldr q8, [sp], #16", "ldp x29, x30, [sp], #16",
             "ldp x19, x28, [sp], #16", "ret",
             ".globl _small_stack_syscall", "_small_stack_syscall:", "stp x19, x30, [sp, #-16]!", "mov x19, sp",
             "mov sp, x0", "mov x0, #0", "mov x8, #261", "svc #0", "mov sp, x19", "ldp x19, x30, [sp], #16", "ret", "_descriptor_placeholder:", ".space 16"]
(OUT / "fixture.s").write_text("\n".join(assembly) + "\n")
subprocess.run(["clang", "-arch", "arm64", "-c", OUT / "fixture.s", "-o", OUT / "fixture.o"], check=True, env=CHILD_ENV)
obj = (OUT / "fixture.o").read_bytes()
ncommands = struct.unpack_from("<I", obj, 16)[0]
offset, symbols, text = 32, None, None
for _ in range(ncommands):
    command, size = struct.unpack_from("<II", obj, offset)
    if command == 0x19:
        count = struct.unpack_from("<I", obj, offset + 64)[0]
        for n in range(count):
            section = offset + 72 + n * 80
            if obj[section:section + 16].rstrip(b"\0") == b"__text":
                address, length, fileoff = struct.unpack_from("<QQI", obj, section + 32)
                text = obj[fileoff:fileoff + length]
    if command == 2:
        symbols = struct.unpack_from("<IIII", obj, offset + 8)
    offset += size
assert text is not None and symbols is not None
symoff, nsyms, stroff, strsize = symbols
exports = []
for n in range(nsyms):
    string, kind, section, descriptor, value = struct.unpack_from("<IBBHQ", obj, symoff + n * 16)
    name = obj[stroff + string:obj.index(b"\0", stroff + string)].decode()
    if kind & 1 and name.startswith("_"):
        exports.append((name[1:], value))
# ADR sites name the writable descriptor, not its assembler placeholder.
text = bytearray(text)
for name, delta in exports:
    if name in {"read_tlsdesc", "write_tlsdesc"}:
        site = delta + (4 if name == "read_tlsdesc" else 8)
        distance = 0x3000 - (0x1000 + site)
        struct.pack_into("<I", text, site, 0x10000000 | ((distance & 3) << 29) | (((distance >> 2) & 0x7ffff) << 5))
for filename in ["libtls-one.so", "libtls-two.so"]:
    image = bytearray(0x4000)
    image[:64] = struct.pack("<16sHHIQQQIHHHHHH", b"\x7fELF\x02\x01\x01" + b"\0" * 9, 3, 183, 1, 0, 64, 0x2800, 0, 64, 56, 4, 64, 7, 5)
    image[64:120] = struct.pack("<IIQQQQQQ", 1, 5, 0, 0, 0, 0x3000, 0x3000, 0x1000)
    image[120:176] = struct.pack("<IIQQQQQQ", 2, 4, 0x200, 0x200, 0x200, 16 * 9, 16 * 9, 8)
    image[176:232] = struct.pack("<IIQQQQQQ", 1, 6, 0x3000, 0x3000, 0x3000, 0x1000, 0x1000, 0x1000)
    image[232:288] = struct.pack("<IIQQQQQQ", 7, 4, 0x2200, 0x2200, 0x2200, 8, 8, 8)
    strings = b"\0"; symtab = bytes(24)
    for name, value in exports:
        symtab += struct.pack("<IBBHQQ", len(strings), 0x12, 0, 1, 0x1000 + value, 0)
        strings += name.encode() + b"\0"
    image[0x400:0x400 + len(strings)] = strings
    image[0x600:0x600 + len(symtab)] = symtab
    hashes = [1, len(exports) + 1, 1, 0] + list(range(2, len(exports) + 1)) + [0]
    image[0x900:0x900 + 4 * len(hashes)] = struct.pack("<" + "I" * len(hashes), *hashes)
    struct.pack_into("<QQq", image, 0xa00, 0x3000, 1031, 0)
    struct.pack_into("<Q", image, 0x2200, 0x12345678)
    dynamic = [(7, 0xa00), (8, 24), (9, 24), (4, 0x900), (5, 0x400), (6, 0x600), (10, len(strings)), (11, 24), (0, 0)]
    for n, pair in enumerate(dynamic): struct.pack_into("<qQ", image, 0x200 + n * 16, *pair)
    image[0x1000:0x1000 + len(text)] = text
    names = b"\0.text\0.dynstr\0.dynsym\0.hash\0.shstrtab\0.tdata\0"
    image[0x2600:0x2600 + len(names)] = names
    sections = [(0, 0, 0, 0, 0, 0, 0, 0, 0, 0), (1, 1, 6, 0x1000, 0x1000, len(text), 0, 0, 4, 0),
                (7, 3, 2, 0x400, 0x400, len(strings), 0, 0, 1, 0), (15, 11, 2, 0x600, 0x600, len(symtab), 2, 1, 8, 24),
                (23, 5, 2, 0x900, 0x900, 4 * len(hashes), 3, 0, 4, 4), (29, 3, 0, 0, 0x2600, len(names), 0, 0, 1, 0), (39, 1, 0x402, 0x2200, 0x2200, 8, 0, 0, 8, 0)]
    for n, row in enumerate(sections): struct.pack_into("<IIQQQQIIQQ", image, 0x2800 + n * 64, *row)
    (OUT / filename).write_bytes(image)
if not args.fixtures_only:
    with (OUT / "build.log").open("w") as log:
        subprocess.run(["bash", ROOT / "tools/regress/build.sh", "tests/translation-layer/guest_tls.c", OUT / "guest-tls-test"], cwd=ROOT, stdout=log, stderr=subprocess.STDOUT, check=True, env=CHILD_ENV)
    with (OUT / "results.log").open("w") as log:
        result = subprocess.run([OUT / "guest-tls-test", OUT / "libtls-one.so", OUT / "libtls-two.so"],
                                cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=CHILD_ENV)
        log.write(result.stdout)
        print(result.stdout, end="")
        result.check_returncode()
    print(OUT / "results.log")
