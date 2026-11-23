#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare an unsigned Tru Tien APK with a separate native plain-Jump button.

Only the verified ARM64 library is supported. No build, signing, installation,
device access or original-file overwrite is performed. Khinh cong is preserved.
"""
import argparse
import copy
from dataclasses import dataclass
import hashlib
import json
from pathlib import Path
import re
import struct
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.trutien_jump_elf import append_payload
from scripts.trutien_plain_jump_arm64 import (
    ACTION_VA, CODE_VA, DRAW_VA, INPUT_VA, LABEL_VA, build_payload, unwind_ranges,
)

ORIGINAL_SHA256 = '9a4ef6b46c9cef7f5c109dfd4770518f7f53fc5aab566788c64212811d0f7398'
LIBRARY_ENTRY = 'lib/arm64-v8a/libmain.so'
ORIGINAL_DIAGNOSTIC = (b'[HUD] Khinh cong: jump skill %d (cooling %d), plain jump %d, '
                       b'jumping %d, flying %d, in water %d\0')
DIAGNOSTIC_VA = 0x125B0EE


@dataclass(frozen=True)
class Edit:
    name: str
    va: int
    before: bytes
    after: bytes
    executable: bool = True


def branch_instruction(at, target):
    displacement = target - at
    if at % 4 or target % 4 or not -(1 << 27) <= displacement < (1 << 27):
        raise ValueError('ARM64 hook target is unaligned or out of branch range')
    return struct.pack('<I', 0x14000000 | ((displacement // 4) & 0x3FFFFFF))


def edits():
    return (
        Edit('append separate plain-Jump drawing after native combat HUD', 0x242ECB4,
             bytes.fromhex('6afcff17'), branch_instruction(0x242ECB4, DRAW_VA)),
        Edit('classify separate plain-Jump rectangle after native panel priority', 0x24114A8,
             bytes.fromhex('2041601e'), branch_instruction(0x24114A8, INPUT_VA)),
        Edit('dispatch plain-Jump action 74 after native player checks', 0x23AB870,
             bytes.fromhex('3f1d0171'), branch_instruction(0x23AB870, ACTION_VA)),
    )


def load_segments(data):
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected a little-endian ELF64 library')
    if struct.unpack_from('<H', data, 16)[0] != 3 or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected an AArch64 shared library')
    at = struct.unpack_from('<Q', data, 32)[0]
    size, count = struct.unpack_from('<HH', data, 54)
    if size != 56 or not 0 < count <= 4096 or at + count * size > len(data):
        raise ValueError('Invalid ELF program table')
    segments = []
    for index in range(count):
        kind, flags, offset, va, _, filesz, memsz, _ = struct.unpack_from('<IIQQQQQQ', data, at + index * size)
        if kind == 1:
            if filesz > memsz or offset + filesz > len(data):
                raise ValueError('Invalid ELF load segment')
            segments.append((flags, offset, va, filesz))
    return segments


def file_offset(segments, va, size, executable):
    matches = [(flags, offset + va - start) for flags, offset, start, length in segments
               if size > 0 and start <= va and va + size <= start + length]
    if len(matches) != 1 or bool(matches[0][0] & 1) != executable:
        raise ValueError('Patch is not wholly inside the expected file-backed segment')
    return matches[0][1]


def apply_edits(data, planned):
    segments = load_segments(data)
    located = []
    for edit in planned:
        if not edit.before or len(edit.before) != len(edit.after):
            raise ValueError('Every patch must preserve its original byte length')
        offset = file_offset(segments, edit.va, len(edit.before), edit.executable)
        if data[offset:offset + len(edit.before)] != edit.before:
            raise ValueError('Unexpected original bytes at ' + edit.name)
        if any(offset < other + length and other < offset + len(edit.before) for other, length, _ in located):
            raise ValueError('Overlapping patches are not allowed')
        located.append((offset, len(edit.before), edit))
    result = bytearray(data)
    for offset, size, edit in located:
        result[offset:offset + size] = edit.after
    return bytes(result), [{'name': edit.name, 'virtualAddress': hex(edit.va), 'fileOffset': hex(offset),
                            'bytes': size, 'beforeHex': edit.before.hex(), 'afterHex': edit.after.hex()}
                           for offset, size, edit in located]


def patch_library(data):
    if hashlib.sha256(data).hexdigest() != ORIGINAL_SHA256:
        raise ValueError('Unsupported/already modified libmain.so; original library SHA-256 must match')
    hooked, evidence = apply_edits(data, edits())
    code, label = build_payload()
    result, extension = append_payload(hooked, code, label, code_va=CODE_VA,
                                       unwind_ranges=unwind_ranges())
    # Restore and preserve every old control-16 label/layout edit. The new
    # button never occupies control 16 or changes its Khinh cong action path.
    segments = load_segments(data)
    for start, size, executable in (
        (0x24194EC, 0x241C5FC - 0x24194EC, True),
        (0x242EB2C, 0x242EBB0 - 0x242EB2C, True),
        (DIAGNOSTIC_VA, len(ORIGINAL_DIAGNOSTIC), False),
        (0x23ABA04, 0x23ABA34 - 0x23ABA04, True),
        (0x23ABE90, 160, True), (0x1C5A2A0, 112, True), (0x1C2C6CC, 1048, True),
    ):
        at = file_offset(segments, start, size, executable)
        if result[at:at + size] != data[at:at + size]:
            raise ValueError('Original Khinh cong/control layout or jump physics changed')
    evidence.append({'name': 'separate plain-Jump code, label and unwind metadata', **extension})
    return result, evidence


def is_signature(name):
    upper = name.upper()
    return upper == 'META-INF/MANIFEST.MF' or bool(re.fullmatch(r'META-INF/[^/]+\.(?:SF|RSA|DSA|EC)', upper))


def _write_zip_entry(modified, entry, data):
    """Write a cloned APK zip entry while keeping ZIP_STORED data 4-byte aligned."""
    cloned = copy.copy(entry)
    if cloned.compress_type == zipfile.ZIP_STORED and modified.fp is not None:
        # Android zipalign pads extra with NUL bytes so stored resources stay 4-byte aligned.
        cloned.file_size = len(data)
        cloned.compress_size = len(data)
        cloned.header_offset = modified.fp.tell()
        pad = (4 - ((cloned.header_offset + len(cloned.FileHeader())) % 4)) % 4
        if pad:
            cloned.extra = (cloned.extra or b'') + bytes(pad)
    modified.writestr(cloned, data)


def prepare_apk(source, destination, *, patch_fn=None, report_extra=None):
    source, destination = Path(source).resolve(strict=True), Path(destination).resolve()
    if source == destination or destination.exists():
        raise ValueError('Choose a new output file; existing APKs are never overwritten')
    with zipfile.ZipFile(source) as original:
        entries = original.infolist()
        names = [entry.filename for entry in entries]
        if len(names) != len(set(names)) or names.count(LIBRARY_ENTRY) != 1:
            raise ValueError('Duplicate/missing archive entries are not supported')
        if original.testzip() is not None:
            raise ValueError('The input APK failed its CRC check')
        library, changes = (patch_library if patch_fn is None else patch_fn)(original.read(LIBRARY_ENTRY))
        removed = [name for name in names if is_signature(name)]
        destination.parent.mkdir(parents=True, exist_ok=True)
        created = False
        try:
            with zipfile.ZipFile(destination, 'x') as modified:
                created = True
                modified.comment = original.comment
                for entry in entries:
                    if not is_signature(entry.filename):
                        data = library if entry.filename == LIBRARY_ENTRY else original.read(entry)
                        _write_zip_entry(modified, entry, data)
            with zipfile.ZipFile(destination) as checked:
                if checked.testzip() is not None:
                    raise ValueError('Patched APK failed its CRC check')
                raw = destination.read_bytes()
                for entry in entries:
                    if is_signature(entry.filename):
                        continue
                    if entry.filename != LIBRARY_ENTRY and checked.read(entry.filename) != original.read(entry):
                        raise ValueError('An unrelated APK entry changed')
                    info = checked.getinfo(entry.filename)
                    if info.compress_type == zipfile.ZIP_STORED:
                        nlen, xlen = struct.unpack_from('<HH', raw, info.header_offset + 26)
                        if (info.header_offset + 30 + nlen + xlen) % 4 != 0:
                            raise ValueError(f'Unaligned ZIP_STORED APK entry: {entry.filename}')
        except Exception:
            # Only a new file created by this invocation can be removed here.
            if created and destination.is_file():
                destination.unlink()
            raise
    report = {'sourceAPK': str(source), 'outputAPK': str(destination), 'libraryEntry': LIBRARY_ENTRY,
              'originalLibrarySHA256': ORIGINAL_SHA256, 'patchedLibrarySHA256': hashlib.sha256(library).hexdigest(),
              'patchedAPKSHA256': hashlib.sha256(destination.read_bytes()).hexdigest(),
              'changes': changes, 'removedStaleV1SignatureEntries': removed,
              'patchVersion': 'separate-plain-jump-v3', 'originalUnchanged': True,
              'existingKhinhCongBytesUnchanged': True, 'existingControlLayoutUnchanged': True,
              'plainJumpPhysicsBytesUnchanged': True,
              'huskBuildPerformed': False, 'signed': False, 'installed': False,
              'androidInstallableStatus': 'unsigned; requires Android signing for direct Android installation',
              'deviceVisualAndJumpValidation': 'pending',
              'scope': 'Separate native circular medallion button Nhảy/action74; original Khinh cong/control16/action35, label and geometry preserved.'}
    if report_extra:
        report.update(report_extra)
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--apk', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--report', type=Path, required=True)
    args = ap.parse_args()
    try:
        if args.report.resolve() in (args.apk.resolve(), args.output.resolve()) or args.report.exists():
            raise ValueError('Choose a new separate report path')
        report = prepare_apk(args.apk, args.output)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
        print(json.dumps({'output': str(args.output), 'report': str(args.report), 'signed': False, 'installed': False}))
    except (ValueError, KeyError, zipfile.BadZipFile, OSError) as error:
        ap.exit(1, 'Jump patch refused: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
