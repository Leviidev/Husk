# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import warnings
from unittest.mock import patch
import zipfile

SPEC = importlib.util.spec_from_file_location('patch_trutien_jump', Path(__file__).resolve().parents[1] / 'scripts/patch_trutien_jump.py')
game = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = game
SPEC.loader.exec_module(game)


def elf_fixture():
    data = bytearray(0x400)
    data[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<HH', data, 16, 3, 183)
    struct.pack_into('<Q', data, 32, 64)
    struct.pack_into('<HH', data, 54, 56, 2)
    # Deliberately different VA/file offsets; an offset-as-VA patch must fail.
    struct.pack_into('<IIQQQQQQ', data, 64, 1, 5, 0x200, 0x1000, 0, 0x80, 0x100, 0x1000)
    struct.pack_into('<IIQQQQQQ', data, 120, 1, 4, 0x300, 0x2000, 0, 0x80, 0x80, 0x1000)
    data[0x210:0x214] = b'ABCD'
    data[0x310:0x314] = b'abcd'
    return bytes(data)


class JumpPatchTests(unittest.TestCase):
    def test_maps_virtual_address_and_changes_only_declared_bytes(self):
        data = elf_fixture()
        output, report = game.apply_edits(data, [game.Edit('code', 0x1010, b'ABCD', b'WXYZ')])
        self.assertEqual(output[0x210:0x214], b'WXYZ')
        self.assertEqual(data[0x210:0x214], b'ABCD')
        self.assertEqual(output[:0x210], data[:0x210])
        self.assertEqual(output[0x214:], data[0x214:])
        self.assertEqual(report[0]['fileOffset'], '0x210')

    def test_wrong_bytes_overlaps_growth_permissions_and_unbacked_memory_rejected(self):
        bad = [
            [game.Edit('wrong', 0x1010, b'EEEE', b'FFFF')],
            [game.Edit('growth', 0x1010, b'ABCD', b'longer')],
            [game.Edit('not code', 0x2010, b'abcd', b'wxyz')],
            [game.Edit('BSS', 0x1090, b'ABCD', b'WXYZ')],
            [game.Edit('cross end', 0x107e, b'ABCD', b'WXYZ')],
            [game.Edit('one', 0x1010, b'ABCD', b'WXYZ'), game.Edit('two', 0x1011, b'BC', b'YZ')],
        ]
        for edits in bad:
            with self.subTest(edits=edits), self.assertRaises(ValueError):
                game.apply_edits(elf_fixture(), edits)
        with self.assertRaises(ValueError):
            game.patch_library(elf_fixture())

    def test_unknown_or_malformed_elf_cannot_be_written(self):
        for data in (b'', b'\x7fELF', bytes(64), elf_fixture()[:100]):
            with self.subTest(length=len(data)), self.assertRaises(ValueError):
                game.load_segments(data)

    def test_separate_payload_preserves_old_control_and_uses_own_utf32_label(self):
        from scripts.trutien_plain_jump_arm64 import FRAGMENTS, build_payload, unwind_ranges
        code, label = build_payload()
        self.assertEqual(len(code), 0x800)
        self.assertEqual(label, bytes.fromhex('4e00000068000000a31e00007900000000000000'))
        self.assertEqual(game.ORIGINAL_DIAGNOSTIC.count(b'%d'), 6)
        old_edits = {0x241BA08, 0x241BA48, 0x242EB80, 0x242EB8C, game.DIAGNOSTIC_VA}
        self.assertFalse(old_edits.intersection(edit.va for edit in game.edits()))
        self.assertEqual({edit.va for edit in game.edits()}, {0x242ECB4, 0x24114A8, 0x23AB870})
        self.assertEqual([(r['offset'], r['size']) for r in unwind_ranges()],
                         [(offset, len(bytes.fromhex(encoded))) for _, offset, _, encoded in FRAGMENTS])

    def test_branch_hooks_are_bounded_and_reach_their_own_payload_entries(self):
        targets = (game.DRAW_VA, game.INPUT_VA, game.ACTION_VA)
        for edit, expected in zip(game.edits(), targets):
            word = struct.unpack('<I', edit.after)[0]
            self.assertEqual(word >> 26, 5)  # B, not BL or an unrelated instruction.
            displacement = word & 0x3FFFFFF
            if displacement & 0x2000000:
                displacement -= 0x4000000
            self.assertEqual(edit.va + displacement * 4, expected)
        for at, target in ((1, 4), (4, 5), (0, 1 << 27), (0, -(1 << 27) - 4)):
            with self.subTest(at=at, target=target), self.assertRaises(ValueError):
                game.branch_instruction(at, target)

    def test_apk_copy_preserves_entries_and_removes_only_stale_signatures(self):
        with tempfile.TemporaryDirectory() as temp:
            source, dest = Path(temp) / 'original.apk', Path(temp) / 'jump.apk'
            with zipfile.ZipFile(source, 'w') as z:
                z.writestr(game.LIBRARY_ENTRY, b'original native', compress_type=zipfile.ZIP_DEFLATED)
                z.writestr('classes.dex', b'unchanged code', compress_type=zipfile.ZIP_STORED)
                z.writestr('resources.arsc', b'1234567', compress_type=zipfile.ZIP_STORED)
                z.writestr('AndroidManifest.xml', b'unchanged package', compress_type=zipfile.ZIP_STORED)
                z.writestr('assets/config.txt', b'unchanged config')
                z.writestr('META-INF/MANIFEST.MF', b'stale')
                z.writestr('META-INF/CERT.RSA', b'stale')
                z.writestr('META-INF/CERT.SF', b'stale')
                z.writestr('META-INF/services/native-provider', b'keep')
            before = source.read_bytes()
            with patch.object(game, 'patch_library', return_value=(b'modified native', [])):
                report = game.prepare_apk(source, dest)
            self.assertEqual(source.read_bytes(), before)
            self.assertFalse(report['signed'])
            self.assertFalse(report['installed'])
            raw = dest.read_bytes()
            with zipfile.ZipFile(dest) as z:
                self.assertEqual(z.read(game.LIBRARY_ENTRY), b'modified native')
                self.assertEqual(z.read('classes.dex'), b'unchanged code')
                self.assertEqual(z.read('resources.arsc'), b'1234567')
                self.assertEqual(z.read('AndroidManifest.xml'), b'unchanged package')
                self.assertEqual(z.read('assets/config.txt'), b'unchanged config')
                self.assertEqual(z.read('META-INF/services/native-provider'), b'keep')
                self.assertNotIn('META-INF/CERT.RSA', z.namelist())
                for info in z.infolist():
                    if info.compress_type == zipfile.ZIP_STORED:
                        nlen, xlen = struct.unpack_from('<HH', raw, info.header_offset + 26)
                        self.assertEqual((info.header_offset + 30 + nlen + xlen) % 4, 0)

    def test_refuses_original_output_existing_output_and_duplicate_entries(self):
        with tempfile.TemporaryDirectory() as temp:
            source, dest = Path(temp) / 'source.apk', Path(temp) / 'output.apk'
            source.write_bytes(b'original')
            dest.write_bytes(b'existing')
            for target in (source, dest):
                with self.assertRaises(ValueError):
                    game.prepare_apk(source, target)
            self.assertEqual(source.read_bytes(), b'original')
            self.assertEqual(dest.read_bytes(), b'existing')
            source.unlink()
            with zipfile.ZipFile(source, 'w') as z:
                z.writestr('classes.dex', b'no native library')
            with self.assertRaises(ValueError):
                game.prepare_apk(source, Path(temp) / 'new.apk')
            self.assertFalse((Path(temp) / 'new.apk').exists())
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                with zipfile.ZipFile(source, 'w') as z:
                    z.writestr(game.LIBRARY_ENTRY, b'first')
                    z.writestr(game.LIBRARY_ENTRY, b'duplicate')
            with self.assertRaises(ValueError):
                game.prepare_apk(source, Path(temp) / 'duplicate.apk')
            self.assertFalse((Path(temp) / 'duplicate.apk').exists())

    def test_failed_preflight_never_creates_output(self):
        with tempfile.TemporaryDirectory() as temp:
            source, dest = Path(temp) / 'source.apk', Path(temp) / 'output.apk'
            with zipfile.ZipFile(source, 'w') as z:
                z.writestr(game.LIBRARY_ENTRY, b'unsupported')
            before = source.read_bytes()
            with self.assertRaises(ValueError):
                game.prepare_apk(source, dest)
            self.assertFalse(dest.exists())
            self.assertEqual(source.read_bytes(), before)


if __name__ == '__main__':
    unittest.main()
