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

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location('patch_trutien_sit', ROOT / 'scripts/patch_trutien_sit.py')
sit = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = sit
SPEC.loader.exec_module(sit)

JUMP_SPEC = importlib.util.spec_from_file_location('patch_trutien_jump', ROOT / 'scripts/patch_trutien_jump.py')
jump = importlib.util.module_from_spec(JUMP_SPEC)
sys.modules[JUMP_SPEC.name] = jump
JUMP_SPEC.loader.exec_module(jump)


class SitPatchTests(unittest.TestCase):
    def test_sit_and_jump_payload_preserves_layout_and_uses_png_atlas(self):
        from scripts.trutien_sit_arm64 import (
            ACTION_VA, ATLAS_PNG_NAME, ATLAS_PNG_SIZE, ATLAS_PNG_VA,
            CAP45_CEILING_VA, DRAW_VA,
            FILL_VA, FRAGMENTS, INPUT_VA, LABEL_JUMP_VA, LABEL_VA,
            LABEL_WHEN_SITTING_VA, LABEL_WHEN_STANDING_VA, build_icon_atlas_png,
            build_payload, unwind_ranges,
        )
        code, rodata = build_payload()
        self.assertEqual(len(code), 0x1000)
        stand_expected = 'Ng\u1ed3i\0'.encode('utf-32le').ljust(0x20, b'\0')
        sit_expected = '\u0110\u1ee9ng\0'.encode('utf-32le').ljust(0x20, b'\0')
        jump_expected = 'Nh\u1ea3y\0'.encode('utf-32le').ljust(0x40, b'\0')
        atlas_expected = (ROOT / 'scripts' / ATLAS_PNG_NAME).read_bytes()
        self.assertEqual(len(atlas_expected), ATLAS_PNG_SIZE)
        self.assertTrue(atlas_expected.startswith(b'\x89PNG\r\n\x1a\n'))
        self.assertEqual(rodata, stand_expected + sit_expected + jump_expected + atlas_expected)
        self.assertEqual(LABEL_WHEN_SITTING_VA - LABEL_WHEN_STANDING_VA, 0x20)
        self.assertEqual(LABEL_JUMP_VA - LABEL_WHEN_STANDING_VA, 0x40)
        self.assertEqual(ATLAS_PNG_VA - LABEL_VA, 0x80)
        if all((ROOT / 'file' / n).exists() for n in ('Nut-Ngoi.png', 'Nut-Dung.png', 'Nut-Nhay.png')):
            try:
                from PIL import Image  # noqa: F401
            except ImportError:
                pass
            else:
                self.assertEqual(build_icon_atlas_png(ROOT / 'file'), atlas_expected)
        hud_vas = (0x242ECB4, 0x24114A8, 0x23AB870, 0x2402090)
        all_edits = {edit.va: edit for edit in sit.edits()}
        for va in hud_vas:
            self.assertIn(va, all_edits)
        fps_expected_vas = {
            0x132662C, 0x1326704, 0x1326770, 0x13267DC,
            0x1326660, 0x13266CC, 0x1326738, 0x13267A4, 0x1326810,
            0x31EC188, 0x1B86964, 0x1B86968,
            0x1B86B68, 0x1B86B7C, 0x1B86B94, 0x1B86BA0, 0x1B86BAC, 0x1B86BBC, 0x1B86BD4, 0x1B86BD8, 0x1B86BF4,
            0x1B8756C, 0x1B87578, 0x1B8758C, 0x1B87598, 0x1B875A0, 0x1B875A8, 0x1B875BC, 0x1B875C8, 0x1B875E0,
            0x1B87748, 0x1B885A4, 0x1B88DD8,
            0x1E05350, 0x1B86C98, 0x1B86DD8, 0x1B871B4, 0x2447318,
            0x1B87D4C, 0x1B89B00, 0x1B88164, 0x1B89BE4, 0x1B89C34, 0x1B89CAC,
            0x1B89E30, 0x1B89F1C, 0x1B8A02C,
            0x1B889BC, 0x1B889FC, 0x1B88D88, 0x1E052C0,
            0x1E0721C, 0x1E07240, 0x1E072F8, 0x1E07340, 0x1E0740C, 0x1E07A38, 0x1E07AE8, 0x1E07C20, 0x1E07CD0,
            0x27EF300, 0x27F6260, 0x27F01E0, 0x281DAA0, 0x281DB4C, 0x29651D4, 0x2965230,
            0x1252F95, 0x1B8FFCC, 0x1B8FFD4, 0x1B90198, 0x1B901B0, 0x1D08300,
            0x249CC94, 0x249DE50,
            0x24C01B4,
        } | {
            base + offset
            for base in (0x1326600, 0x132666C, 0x13266D8, 0x1326744)
            for offset in (0x4C, 0x50, 0x54, 0x58, 0x5C)
        } | {
            base + offset
            for base in (0x1326600, 0x132666C, 0x13266D8, 0x1326744, 0x13267B0)
            for offset in (0x64, 0x68)
        }
        self.assertEqual(set(all_edits), set(hud_vas) | fps_expected_vas)
        self.assertEqual(all_edits[0x1252F95].after, b'86,91')
        self.assertEqual(all_edits[0x1B889BC].after, bytes.fromhex('1f2003d5'))
        self.assertEqual(all_edits[0x2447318].after, bytes.fromhex('e2031f2a'))
        self.assertEqual(all_edits[0x249CC94].after, bytes.fromhex('e1031f2a'))
        self.assertEqual(all_edits[0x249DE50].after, bytes.fromhex('e1031f2a'))
        self.assertEqual([(r['offset'], r['size']) for r in unwind_ranges()],
                         [(offset, len(bytes.fromhex(encoded))) for _, offset, _, encoded in FRAGMENTS])
        branch_targets = {
            0x242ECB4: DRAW_VA,
            0x24114A8: INPUT_VA,
            0x23AB870: ACTION_VA,
            0x2402090: FILL_VA,
            0x1B86968: CAP45_CEILING_VA,
            0x1B86B7C: 0x1B86BC0,
            0x1B86BA0: 0x1B86BC0,
            0x1B87578: 0x1B875AC,
            0x1B87598: 0x1B875AC,
            0x1B889FC: 0x1B88A4C,
            0x1B88D88: 0x1B88DAC,
            0x1B89B00: 0x1B89BB4,
            0x1B89BE4: 0x1B89CAC,
            0x1E052C0: 0x1E052F4,
        }
        for va, expected in branch_targets.items():
            word = struct.unpack('<I', all_edits[va].after)[0]
            self.assertEqual(word >> 26, 5)
            disp = word & 0x3FFFFFF
            if disp & 0x2000000:
                disp -= 0x4000000
            self.assertEqual(va + disp * 4, expected)
        for va in (0x1B87748, 0x1B885A4, 0x1B88DD8):
            word = struct.unpack('<I', all_edits[va].after)[0]
            self.assertEqual(word >> 26, 0x25)
            disp = word & 0x3FFFFFF
            if disp & 0x2000000:
                disp -= 0x4000000
            self.assertEqual(va + disp * 4, 0x2442F98)

    def test_sit_and_jump_payload_v2503_profile(self):
        from scripts.trutien_sit_arm64 import (
            ATLAS_PNG_NAME, ATLAS_PNG_SIZE, CODE_VA_2503, PROFILES,
            build_payload, unwind_ranges,
        )
        arm = PROFILES['2503']
        code, rodata = build_payload('2503')
        self.assertEqual(len(code), 0x1000)
        self.assertEqual(arm['code_va'], CODE_VA_2503)
        self.assertEqual(arm['atlas_png_va'] - arm['label_va'], 0x80)
        atlas_expected = (ROOT / 'scripts' / ATLAS_PNG_NAME).read_bytes()
        self.assertEqual(len(atlas_expected), ATLAS_PNG_SIZE)
        self.assertTrue(rodata.endswith(atlas_expected))
        all_edits = {edit.va: edit for edit in sit.edits('2503')}
        self.assertEqual(len(all_edits), 110)
        self.assertEqual(all_edits[0x1255A28].after, b'86,91')
        self.assertEqual(all_edits[0x1B8E2BC].after, bytes.fromhex('1f2003d5'))
        self.assertEqual(all_edits[0x246057C].after, bytes.fromhex('e2031f2a'))
        self.assertEqual(all_edits[0x24B5EF8].after, bytes.fromhex('e1031f2a'))
        self.assertEqual(all_edits[0x24B70B4].after, bytes.fromhex('e1031f2a'))
        self.assertEqual([(r['offset'], r['size']) for r in unwind_ranges('2503')],
                         [(offset, len(bytes.fromhex(encoded))) for _, offset, _, encoded in arm['fragments']])
        branch_targets = {
            0x2447D84: arm['draw_va'],
            0x242A55C: arm['input_va'],
            0x23B1B00: arm['action_va'],
            0x241AFD0: arm['fill_va'],
            0x1B8C268: arm['cap45_ceiling_va'],
            0x1B8C47C: 0x1B8C4C0,
            0x1B8C4A0: 0x1B8C4C0,
            0x1B8CE78: 0x1B8CEAC,
            0x1B8CE98: 0x1B8CEAC,
            0x1B8E2FC: 0x1B8E34C,
            0x1B8E688: 0x1B8E6AC,
            0x1B8F400: 0x1B8F4B4,
            0x1B8F4E4: 0x1B8F5AC,
            0x1E0ABF4: 0x1E0AC28,
        }
        for va, expected in branch_targets.items():
            word = struct.unpack('<I', all_edits[va].after)[0]
            self.assertEqual(word >> 26, 5)
            disp = word & 0x3FFFFFF
            if disp & 0x2000000:
                disp -= 0x4000000
            self.assertEqual(va + disp * 4, expected)
        for va in (0x1B8D048, 0x1B8DEA4, 0x1B8E6D8):
            word = struct.unpack('<I', all_edits[va].after)[0]
            self.assertEqual(word >> 26, 0x25)
            disp = word & 0x3FFFFFF
            if disp & 0x2000000:
                disp -= 0x4000000
            self.assertEqual(va + disp * 4, 0x245C1FC)

    def test_sit_and_jump_payload_v2685_profile(self):
        from scripts.trutien_sit_arm64 import (
            ATLAS_PNG_NAME, ATLAS_PNG_SIZE, CODE_VA_2685, PROFILES,
            build_payload, unwind_ranges,
        )
        arm = PROFILES['2685']
        code, rodata = build_payload('2685')
        self.assertEqual(len(code), 0x1000)
        self.assertEqual(arm['code_va'], CODE_VA_2685)
        self.assertEqual(arm['atlas_png_va'] - arm['label_va'], 0x80)
        atlas_expected = (ROOT / 'scripts' / ATLAS_PNG_NAME).read_bytes()
        self.assertEqual(len(atlas_expected), ATLAS_PNG_SIZE)
        self.assertTrue(rodata.endswith(atlas_expected))
        all_edits = {edit.va: edit for edit in sit.edits('2685')}
        self.assertEqual(len(all_edits), 111)
        self.assertEqual(all_edits[0x125AA07].after, b'86,91')
        self.assertEqual(all_edits[0x1B98788].after, bytes.fromhex('1f2003d5'))
        self.assertEqual(all_edits[0x2483AD0].after, bytes.fromhex('e2031f2a'))
        self.assertEqual(all_edits[0x24D97DC].after, bytes.fromhex('e1031f2a'))
        self.assertEqual(all_edits[0x24DA998].after, bytes.fromhex('e1031f2a'))
        self.assertEqual([(r['offset'], r['size']) for r in unwind_ranges('2685')],
                         [(offset, len(bytes.fromhex(encoded))) for _, offset, _, encoded in arm['fragments']])
        branch_targets = {
            0x246B030: arm['draw_va'],
            0x244D768: arm['input_va'],
            0x23C7FB0: arm['action_va'],
            0x243E1DC: arm['fill_va'],
            0x1B96578: arm['cap45_ceiling_va'],
            0x1B9678C: 0x1B967D0,
            0x1B967B0: 0x1B967D0,
            0x1B972AC: 0x1B972E0,
            0x1B972CC: 0x1B972E0,
            0x1B987C8: 0x1B98818,
            0x1B98B54: 0x1B98B78,
            0x1B99914: 0x1B999C8,
            0x1B999F8: 0x1B99AC0,
            0x1E16D8C: 0x1E16DA0,
            0x1E1A450: 0x1E1A470,
        }
        for va, expected in branch_targets.items():
            word = struct.unpack('<I', all_edits[va].after)[0]
            self.assertEqual(word >> 26, 5)
            disp = word & 0x3FFFFFF
            if disp & 0x2000000:
                disp -= 0x4000000
            self.assertEqual(va + disp * 4, expected)
        for va in (0x1B9747C, 0x1B982DC, 0x1B98BA4):
            word = struct.unpack('<I', all_edits[va].after)[0]
            self.assertEqual(word >> 26, 0x25)
            disp = word & 0x3FFFFFF
            if disp & 0x2000000:
                disp -= 0x4000000
            self.assertEqual(va + disp * 4, 0x247F644)

    def test_apk_copy_preserves_entries_and_removes_only_stale_signatures(self):
        with tempfile.TemporaryDirectory() as temp:
            source, dest = Path(temp) / 'original.apk', Path(temp) / 'sit_jump.apk'
            with zipfile.ZipFile(source, 'w') as z:
                z.writestr(jump.LIBRARY_ENTRY, b'original native', compress_type=zipfile.ZIP_DEFLATED)
                z.writestr('classes.dex', b'unchanged code', compress_type=zipfile.ZIP_STORED)
                z.writestr('resources.arsc', b'1234567', compress_type=zipfile.ZIP_STORED)
                z.writestr('META-INF/MANIFEST.MF', b'stale')
                z.writestr('META-INF/CERT.RSA', b'stale')
                z.writestr('META-INF/services/native-provider', b'keep')
            before = source.read_bytes()
            with patch.object(sit, 'patch_library', return_value=(b'modified paired native', [])):
                report = sit.prepare_apk(source, dest)
            self.assertEqual(source.read_bytes(), before)
            self.assertEqual(report['patchVersion'], 'separate-jump-and-sit-v9-45fps-turbo')
            self.assertEqual(report['maxFpsTarget'], 45)
            self.assertFalse(report['signed'])
            self.assertFalse(report['installed'])
            raw = dest.read_bytes()
            with zipfile.ZipFile(dest) as z:
                self.assertEqual(z.read(jump.LIBRARY_ENTRY), b'modified paired native')
                self.assertEqual(z.read('classes.dex'), b'unchanged code')
                self.assertEqual(z.read('META-INF/services/native-provider'), b'keep')
                self.assertNotIn('META-INF/CERT.RSA', z.namelist())
                for info in z.infolist():
                    if info.compress_type == zipfile.ZIP_STORED:
                        nlen, xlen = struct.unpack_from('<HH', raw, info.header_offset + 26)
                        self.assertEqual((info.header_offset + 30 + nlen + xlen) % 4, 0)

    def test_refuses_malformed_or_unsupported_elf(self):
        with tempfile.TemporaryDirectory() as temp:
            source, dest = Path(temp) / 'bad.apk', Path(temp) / 'out.apk'
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                with zipfile.ZipFile(source, 'w') as z:
                    z.writestr(jump.LIBRARY_ENTRY, b'unsupported')
            with self.assertRaises(ValueError):
                sit.prepare_apk(source, dest)
            self.assertFalse(dest.exists())

    def test_auto_discovery_parse_axml_and_rejects_non_trutien_elf(self):
        from scripts.trutien_sit_auto import discover_trutien_profile, parse_axml_version_code

        # Minimal valid little-endian AArch64 ELF64 missing section headers -> ValueError
        elf = bytearray(120)
        elf[0:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<HH', elf, 16, 3, 183)
        struct.pack_into('<Q', elf, 32, 64)
        struct.pack_into('<HH', elf, 54, 56, 1)
        struct.pack_into('<IIQQQQQQ', elf, 64, 1, 5, 0, 0, 0, len(elf), len(elf), 0x1000)
        with self.assertRaises(ValueError):
            discover_trutien_profile(bytes(elf))
        with self.assertRaises(ValueError):
            sit.patch_library(bytes(elf))

        # Minimal binary AXML with resource map [0x0101021b] and versionCode=2685 attribute
        res_map = struct.pack('<HHII', 0x0180, 8, 12, 0x0101021B)
        attr = struct.pack('<IIIHBBI', 0xFFFFFFFF, 0, 0xFFFFFFFF, 8, 0, 0x10, 2685)
        elem = (struct.pack('<HHIII', 0x0102, 16, 16 + 20 + 20, 1, 0xFFFFFFFF)
                + struct.pack('<IIHHHHHH', 0xFFFFFFFF, 0, 20, 20, 1, 0, 0, 0)
                + attr)
        axml = struct.pack('<HHI', 3, 8, 8 + len(res_map) + len(elem)) + res_map + elem
        self.assertEqual(parse_axml_version_code(axml), 2685)
        self.assertIsNone(parse_axml_version_code(b'not-axml'))

    def test_auto_discovery_matches_static_profiles_when_apks_present(self):
        local_apks = [
            ('2411', ROOT / 'APK/TruTien-Goc.apk'),
            ('2503', ROOT / 'APK/TruTien-Goc-2503.apk'),
            ('2685', ROOT / 'APK/TruTien-Goc-2685.apk'),
        ]
        for ver, apk_path in local_apks:
            if not apk_path.is_file():
                continue
            with zipfile.ZipFile(apk_path) as zf:
                raw = zf.read(jump.LIBRARY_ENTRY)
                axml_vc = sit.parse_axml_version_code(zf.read('AndroidManifest.xml'))
            self.assertEqual(axml_vc, sit.VERSION_CONFIGS[ver]['version_code'])
            static_patched, _ = sit.patch_library(raw)
            auto_patched, auto_ev = sit.patch_library(raw, force_auto=True)
            self.assertEqual(auto_patched, static_patched)
            self.assertTrue(auto_ev[-1]['autoDiscovered'])


if __name__ == '__main__':
    unittest.main()
