# SPDX-License-Identifier: GPL-2.0-or-later
import importlib.util
import io
from pathlib import Path
import struct
import unittest


SPEC = importlib.util.spec_from_file_location('trutien_jump_elf',
    Path(__file__).resolve().parents[1] / 'scripts/trutien_jump_elf.py')
elf = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(elf)


def fixture():
    """Synthetic ELF with BSS, unequal file/VA offsets and a final PT_NOTE."""
    data = bytearray(0x4000)
    data[:7] = b'\x7fELF\x02\x01\x01'
    struct.pack_into('<HHI', data, 16, 3, 183, 1)
    struct.pack_into('<QQ', data, 32, 64, 0x3A00)
    struct.pack_into('<IHHHHHH', data, 48, 0, 64, 56, 10, 64, 6, 5)
    headers = [
        (6, 4, 64, 64, 64, 560, 560, 8),
        (1, 4, 0, 0, 0, 0x1000, 0x1000, 0x1000),
        (1, 5, 0x1000, 0x4000, 0x4000, 0x1000, 0x1000, 0x1000),
        (1, 6, 0x2000, 0x8000, 0x8000, 0x1000, 0x1800, 0x1000),
        (1, 6, 0x3000, 0xC000, 0xC000, 0x800, 0x1000, 0x1000),
        (2, 6, 0x2200, 0x8200, 0x8200, 32, 32, 8),
        (0x6474E552, 4, 0x2000, 0x8000, 0x8000, 0x100, 0x100, 1),
        (0x6474E550, 4, 0x900, 0x900, 0x900, 32, 32, 4),
        (0x6474E551, 6, 0, 0, 0, 0, 0, 0),
        (4, 4, 0x270, 0x270, 0x270, 188, 188, 4),
    ]
    for index, header in enumerate(headers):
        elf.PHDR.pack_into(data, 64 + 56 * index, *header)
    data[0x270:0x270 + 188] = b'ORIGINAL_NOTE' + bytes(175)
    data[0x1000:0x1100] = bytes(range(256))
    data[0x2000:0x2100] = b'D' * 256
    names = b'\0.note\0.text\0.data\0.bss\0.shstrtab\0'
    data[0x3900:0x3900 + len(names)] = names
    sections = [
        (0,) * 10,
        (1, 7, 2, 0x270, 0x270, 188, 0, 0, 4, 0),
        (7, 1, 6, 0x4000, 0x1000, 0x100, 0, 0, 16, 0),
        (13, 1, 3, 0x8000, 0x2000, 0x100, 0, 0, 8, 0),
        (19, 8, 3, 0xC800, 0x3800, 0x800, 0, 0, 8, 0),
        (24, 3, 0, 0, 0x3900, len(names), 0, 0, 1, 0),
    ]
    for index, section in enumerate(sections):
        elf.SHDR.pack_into(data, 0x3A00 + 64 * index, *section)
    return bytes(data)


CODE = bytes.fromhex('1f2003d5' * 4 + 'c0035fd6')
RODATA = 'Nh\u1ea3y\0'.encode('utf-32le')


def unwind_fixture():
    data = bytearray(fixture())
    cie = bytes.fromhex('14000000 00000000 01 7a5200 01 78 1e 01 1b 0c1f00 081e 0000')
    frames = bytearray(cie)
    pairs = []
    for pc in (0x4000, 0x4080):
        va = 0xA00 + len(frames)
        body = struct.pack('<IiiB', va + 4 - 0xA00, pc - (va + 8), 0x40, 0)
        body += b'\x0c\x1f\x10\x9e\x01\0\0'
        frames.extend(struct.pack('<I', len(body)) + body)
        pairs.append((pc, va))
    frames.extend(bytes(4))
    data[0xA00:0xA00 + len(frames)] = frames
    header = bytearray(bytes.fromhex('011b033b') + struct.pack('<iI', 0xA00 - 0x904, len(pairs)))
    for pc, fde in pairs:
        header.extend(struct.pack('<ii', pc - 0x900, fde - 0x900))
    header.extend(bytes(8))
    data[0x900:0x900 + len(header)] = header
    elf.PHDR.pack_into(data, 64 + 7 * 56, 0x6474E550, 4,
                      0x900, 0x900, 0x900, len(header), len(header), 4)
    struct.pack_into('<H', data, 60, 8)
    old_strings = elf.SHDR.unpack_from(data, 0x3A00 + 5 * 64)
    names = bytes(data[old_strings[4]:old_strings[4] + old_strings[5]])
    header_name, frame_name = len(names), len(names) + len(b'.eh_frame_hdr\0')
    names += b'.eh_frame_hdr\0.eh_frame\0'
    data[0x3900:0x3900 + len(names)] = names
    strings_header = list(old_strings)
    strings_header[5] = len(names)
    elf.SHDR.pack_into(data, 0x3A00 + 5 * 64, *strings_header)
    elf.SHDR.pack_into(data, 0x3A00 + 6 * 64,
                      header_name, 1, 2, 0x900, 0x900, len(header), 0, 0, 4, 0)
    elf.SHDR.pack_into(data, 0x3A00 + 7 * 64,
                      frame_name, 1, 2, 0xA00, 0xA00, len(frames), 0, 0, 8, 0)
    return bytes(data), pairs


def unwind_table(binary):
    header = elf.PHDR.unpack_from(binary, 64 + 7 * 56)
    offset, va = header[2:4]
    count = struct.unpack_from('<I', binary, offset + 8)[0]
    pairs = [(va + pc, va + fde) for pc, fde in
             struct.iter_unpack('<ii', binary[offset + 12:offset + 12 + count * 8])]
    return header, pairs


def read_uleb(binary, position):
    value, shift = 0, 0
    while True:
        byte = binary[position]
        position += 1
        value |= (byte & 127) << shift
        if not byte & 128:
            return value, position
        shift += 7


def cfi_rules(instructions, cfa=(31, 0), rules=None):
    rules = {30: ('SAME_VALUE', None)} if rules is None else dict(rules)
    position = 0
    while position < len(instructions):
        opcode = instructions[position]
        position += 1
        if opcode == 0:
            continue
        if opcode == 0x0C:
            register, position = read_uleb(instructions, position)
            offset, position = read_uleb(instructions, position)
            cfa = (register, offset)
        elif opcode == 0x08:
            register, position = read_uleb(instructions, position)
            rules[register] = ('SAME_VALUE', None)
        elif opcode == 0x05 or opcode & 0xC0 == 0x80:
            if opcode == 0x05:
                register, position = read_uleb(instructions, position)
            else:
                register = opcode & 0x3F
            factor, position = read_uleb(instructions, position)
            rules[register] = ('OFFSET', -8 * factor)
        else:
            raise AssertionError('Unknown CFI opcode: ' + hex(opcode))
    return cfa, rules


def new_fde_rows(binary, evidence):
    unwind = evidence['unwind']
    start, va, size = unwind['frameFileOffset'], unwind['frameVA'], unwind['frameSize']
    block = binary[start:start + size]
    cie_length = struct.unpack_from('<I', block)[0]
    if block[4:17] != bytes.fromhex('00000000 01 7a5200 01 78 1e 01 1b'):
        raise AssertionError('Incorrect zR CIE/header encoding')
    initial_cfa, initial_rules = cfi_rules(block[17:4 + cie_length])
    result, position = [], 4 + cie_length
    while position + 4 < len(block):
        length, cie_back, pc_delta, extent = struct.unpack_from('<IIii', block, position)
        if position + 4 - cie_back != 0 or block[position + 16] != 0:
            raise AssertionError('FDE CIE pointer or augmentation length incorrect')
        cfa, rules = cfi_rules(block[position + 17:position + 4 + length], initial_cfa, initial_rules)
        result.append({'pc': va + position + 8 + pc_delta, 'size': extent,
                       'cfaRegister': cfa[0], 'cfaOffset': cfa[1],
                       'savedRegisters': {reg: arg for reg, (kind, arg) in rules.items() if kind == 'OFFSET'},
                       'returnRule': rules[30]})
        position += length + 4
    if block[position:] != bytes(4):
        raise AssertionError('CFI block lacks exactly one terminator')
    return result


class AppendPayloadTests(unittest.TestCase):
    def test_generated_frames_decode_with_independent_pyelftools_when_available(self):
        try:
            from elftools.dwarf.callframe import CallFrameInfo, FDE
            from elftools.dwarf.structs import DWARFStructs
        except ImportError:
            self.skipTest('Optional independent decoder: install pyelftools or supply it on PYTHONPATH')
        original, _ = unwind_fixture()
        ranges = [
            {'offset': 0, 'size': 8, 'cfaRegister': 31, 'cfaOffset': 0, 'savedRegisters': {}},
            {'offset': 16, 'size': 4, 'cfaRegister': 29, 'cfaOffset': 80,
             'savedRegisters': {29: -80, 30: -72, 19: -8, 79: -144}},
        ]
        output, evidence = elf.append_payload(original, CODE, RODATA, code_va=0x10000, unwind_ranges=ranges)
        info = evidence['unwind']
        block = output[info['frameFileOffset']:info['frameFileOffset'] + info['frameSize']]
        parser = CallFrameInfo(io.BytesIO(block), len(block), info['frameVA'],
                               DWARFStructs(little_endian=True, dwarf_format=32, address_size=8), True)
        entries = [entry for entry in parser.get_entries() if isinstance(entry, FDE)]
        self.assertEqual(len(entries), len(ranges))
        for entry, expected in zip(entries, ranges):
            row = entry.get_decoded().table[-1]
            self.assertEqual(entry.header.initial_location, 0x10000 + expected['offset'])
            self.assertEqual(entry.header.address_range, expected['size'])
            self.assertEqual((row['cfa'].reg, row['cfa'].offset),
                             (expected['cfaRegister'], expected['cfaOffset']))
            saved = {reg: rule.arg for reg, rule in row.items()
                     if isinstance(reg, int) and rule.type == 'OFFSET'}
            self.assertEqual(saved, expected['savedRegisters'])

    def test_relocated_unwind_retains_all_old_entries_and_describes_new_frames(self):
        original, old_pairs = unwind_fixture()
        ranges = [
            {'offset': 0, 'size': 8, 'cfaRegister': 31, 'cfaOffset': 0, 'savedRegisters': {}},
            {'offset': 16, 'size': 4, 'cfaRegister': 29, 'cfaOffset': 80,
             'savedRegisters': {29: -80, 30: -72, 19: -8, 79: -144}},
        ]
        output, evidence = elf.append_payload(original, CODE, RODATA, code_va=0x10000, unwind_ranges=ranges)
        header, pairs = unwind_table(output)
        self.assertEqual(header[0:2], (0x6474E550, 4))
        self.assertEqual(header[5], 12 + 4 * 8)
        self.assertEqual(pairs[:2], old_pairs)
        self.assertEqual([pc for pc, _ in pairs[2:]], [0x10000, 0x10010])
        self.assertEqual(output[header[2]:header[2] + 4], bytes.fromhex('011b033b'))
        self.assertEqual(header[3] + 4 + struct.unpack_from('<i', output, header[2] + 4)[0], 0xA00)
        self.assertEqual(output[0xA00:0xA4C], original[0xA00:0xA4C])
        rows = new_fde_rows(output, evidence)
        self.assertEqual(rows[0], {'pc': 0x10000, 'size': 8, 'cfaRegister': 31,
                                 'cfaOffset': 0, 'savedRegisters': {}, 'returnRule': ('SAME_VALUE', None)})
        self.assertEqual(rows[1], {'pc': 0x10010, 'size': 4, 'cfaRegister': 29,
                                 'cfaOffset': 80, 'savedRegisters': ranges[1]['savedRegisters'],
                                 'returnRule': ('OFFSET', -72)})
        table_at = struct.unpack_from('<Q', output, 40)[0]
        relocated_hdr_section = elf.SHDR.unpack_from(output, table_at + 6 * 64)
        self.assertEqual(relocated_hdr_section[3:6], (header[3], header[2], header[5]))
        self.assertEqual(elf.SHDR.unpack_from(output, table_at + 7 * 64),
                         elf.SHDR.unpack_from(original, 0x3A00 + 7 * 64))
        self.assertEqual(struct.unpack_from('<H', output, 56)[0], 11)
        phdr_self = elf.PHDR.unpack_from(output, 64)
        self.assertEqual(phdr_self, (6, 4, 64, 64, 64, 616, 616, 8))
        exec_seg = elf.PHDR.unpack_from(output, 64 + 9 * 56)
        ro_seg = elf.PHDR.unpack_from(output, 64 + 10 * 56)
        self.assertEqual(exec_seg, (1, 5, 0x4000, 0x10000, 0x10000, 52, 52, 0x4000))
        self.assertEqual(ro_seg[0:4], (1, 4, 0x8000, 0x14000))
        self.assertGreaterEqual(header[3], ro_seg[3])
        self.assertLessEqual(header[3] + header[5], ro_seg[3] + ro_seg[6])
        self.assertGreaterEqual(evidence['unwind']['frameVA'], ro_seg[3])
        self.assertLessEqual(evidence['unwind']['frameVA'] + evidence['unwind']['frameSize'], ro_seg[3] + ro_seg[6])
        changed = {i for i, (a, b) in enumerate(zip(original, output)) if a != b}
        allowed = (
            set(range(40, 48)) | set(range(56, 58)) | set(range(60, 62))
            | set(range(0x60, 0x70)) | set(range(0x1C8, 0x200)) | set(range(0x238, 0x2A8))
        )
        self.assertTrue(changed <= allowed)
        self.assertEqual(evidence['unwind']['originalTableEntries'], 2)
        self.assertEqual(evidence['unwind']['addedTableEntries'], 2)

    def test_rejects_short_note_when_extra_unwind_load_is_needed(self):
        original, _ = unwind_fixture()
        modified = bytearray(original)
        elf.PHDR.pack_into(modified, 64 + 9 * 56, 4, 4, 0x270, 0x270, 0x270, 40, 40, 4)
        good = {'offset': 0, 'size': 4, 'cfaRegister': 31, 'cfaOffset': 0, 'savedRegisters': {}}
        with self.assertRaises(ValueError):
            elf.append_payload(bytes(modified), CODE, RODATA, code_va=0x10000, unwind_ranges=[good])

    def test_unsupported_unwind_headers_and_invalid_rules_are_rejected(self):
        original, _ = unwind_fixture()
        good = {'offset': 0, 'size': 4, 'cfaRegister': 31, 'cfaOffset': 0, 'savedRegisters': {}}
        for offset, fmt, value in ((0x900, '<B', 2), (0x901, '<B', 3), (0x902, '<B', 0x1B),
                                   (0x903, '<B', 0x1B), (0x908, '<I', 999),
                                   (0x914, '<i', 0x4000 - 0x900),
                                   (0x910, '<i', 0x80000), (0x91C, '<B', 1)):
            modified = bytearray(original)
            struct.pack_into(fmt, modified, offset, value)
            with self.subTest(header_offset=hex(offset)), self.assertRaises(ValueError):
                elf.append_payload(bytes(modified), CODE, RODATA, code_va=0x10000, unwind_ranges=[good])
        invalid = [[], [dict(good, size=0)], [dict(good, offset=2)], [dict(good, size=24)],
                   [dict(good, cfaRegister=64)], [dict(good, cfaOffset=-8)],
                   [dict(good, savedRegisters={30: -7})], [dict(good, savedRegisters={96: -8})],
                   [dict(good, unknown=1)], [good, good]]
        for ranges in invalid:
            with self.subTest(ranges=ranges), self.assertRaises(ValueError):
                elf.append_payload(original, CODE, RODATA, code_va=0x10000, unwind_ranges=ranges)
        with self.assertRaises(ValueError):
            elf.append_payload(original, CODE, RODATA, code_va=0x100000000, unwind_ranges=[good])

    def test_rejects_malformed_headers_note_slot_sections_and_existing_loads(self):
        source = fixture()
        mutations = [
            ('architecture', 18, '<H', 62),
            ('ELF version', 20, '<I', 0),
            ('program table', 32, '<Q', 0x100),
            ('section table bounds', 40, '<Q', len(source) + 64),
            ('program entry size', 54, '<H', 55),
            ('program count', 56, '<H', 9),
            ('section entry size', 58, '<H', 63),
            ('extended section count', 60, '<H', 0),
            ('extended name index', 62, '<H', 0xFFFF),
            ('PHDR payload size', 64 + 32, '<Q', 559),
            ('reused note slot', 64 + 9 * 56, '<I', 1),
            ('writable note', 64 + 9 * 56 + 4, '<I', 6),
            ('note inside headers', 64 + 9 * 56 + 8, '<Q', 0),
            ('note file bounds', 64 + 9 * 56 + 8, '<Q', len(source)),
            ('note VA mapping', 64 + 9 * 56 + 16, '<Q', 0x370),
            ('load file bounds', 64 + 2 * 56 + 32, '<Q', len(source)),
            ('load overlap', 64 + 3 * 56 + 16, '<Q', 0x4000),
            ('load congruence', 64 + 3 * 56 + 8, '<Q', 0x2001),
            ('section name bounds', 0x3A00 + 2 * 64, '<I', 1000),
            ('section file bounds', 0x3A00 + 2 * 64 + 24, '<Q', len(source)),
            ('section load mapping', 0x3A00 + 2 * 64 + 24, '<Q', 0x1100),
            ('section unmapped VA', 0x3A00 + 2 * 64 + 16, '<Q', 0x50000),
            ('allocated name table', 0x3A00 + 5 * 64 + 8, '<Q', 2),
        ]
        for name, offset, fmt, value in mutations:
            modified = bytearray(source)
            struct.pack_into(fmt, modified, offset, value)
            with self.subTest(case=name), self.assertRaises(ValueError):
                elf.append_payload(bytes(modified), CODE, RODATA, code_va=0x10000)
        for malformed in (b'', source[:32], b'BAD!' + source[4:]):
            with self.subTest(length=len(malformed)), self.assertRaises(ValueError):
                elf.append_payload(malformed, CODE, RODATA, code_va=0x10000)
        with self.assertRaises(ValueError):
            elf.append_payload(source, CODE, RODATA, code_va=0xC000)

    def test_maps_payload_and_preserves_original_prefix_outside_metadata(self):
        original = fixture()
        snapshot = bytes(original)
        output, evidence = elf.append_payload(original, CODE, RODATA, code_va=0x10000)
        self.assertEqual(original, snapshot)
        self.assertEqual(evidence['loadFileOffset'], 0x4000)
        self.assertEqual(evidence['loadVA'], 0x10000)
        self.assertEqual(evidence['rodataVA'], 0x10020)
        segment = elf.PHDR.unpack_from(output, 64 + 9 * 56)
        self.assertEqual(segment, (1, 5, 0x4000, 0x10000, 0x10000, 52, 52, 0x4000))
        self.assertEqual(output[0x4000:0x4000 + len(CODE)], CODE)
        self.assertEqual(output[0x4014:0x4020], bytes(12))
        self.assertEqual(output[0x4020:0x4034], RODATA)
        self.assertEqual(output[0x270:0x32C], original[0x270:0x32C])
        changed = {index for index, (old, new) in enumerate(zip(original, output)) if old != new}
        allowed = set(range(40, 48)) | set(range(60, 62)) | set(range(0x238, 0x270))
        self.assertTrue(changed <= allowed)
        for index in range(9):
            self.assertEqual(output[64 + 56 * index:64 + 56 * (index + 1)],
                             original[64 + 56 * index:64 + 56 * (index + 1)])

    def test_keeps_section_indices_and_separates_code_from_rodata(self):
        original = fixture()
        output, evidence = elf.append_payload(original, CODE, RODATA, code_va=0x10000)
        shoff = struct.unpack_from('<Q', output, 40)[0]
        self.assertEqual(struct.unpack_from('<HH', output, 60), (8, 5))
        self.assertEqual(shoff % 8, 0)
        sections = [elf.SHDR.unpack_from(output, shoff + i * 64) for i in range(8)]
        for index in range(5):
            self.assertEqual(sections[index], elf.SHDR.unpack_from(original, 0x3A00 + index * 64))
        strings = sections[5]
        names = output[strings[4]:strings[4] + strings[5]]
        self.assertEqual(names[sections[6][0]:].split(b'\0', 1)[0], b'.husk_plain_jump')
        self.assertEqual(names[sections[7][0]:].split(b'\0', 1)[0], b'.husk_plain_jump_rodata')
        self.assertEqual(sections[6][1:6], (1, 6, 0x10000, 0x4000, len(CODE)))
        self.assertEqual(sections[7][1:6], (1, 2, 0x10020, 0x4020, len(RODATA)))
        self.assertEqual(sections[6][8], 4)
        self.assertEqual(sections[7][8], 16)
        self.assertEqual(evidence['codeSectionIndex'], 6)
        self.assertEqual(evidence['rodataSectionIndex'], 7)
        self.assertEqual(output[0x3A00:0x3B80], original[0x3A00:0x3B80])

    def test_page_padding_and_bss_overlap_are_checked(self):
        original = fixture() + b'tail'
        output, evidence = elf.append_payload(original, CODE, RODATA, code_va=0x10000)
        self.assertEqual(evidence['loadFileOffset'], 0x8000)
        self.assertEqual(output[len(original):0x8000], bytes(0x8000 - len(original)))
        for va in (0, 0x4000, 0x8000, 0xC000):
            with self.subTest(va=va), self.assertRaises(ValueError):
                elf.append_payload(fixture(), CODE, RODATA, code_va=va)

    def test_rejects_unexpected_or_malformed_headers_before_appending(self):
        cases = []
        for offset, fmt, value in ((4, '<B', 1), (5, '<B', 2), (16, '<H', 2),
                                   (18, '<H', 62), (20, '<I', 2), (32, '<Q', 65),
                                   (40, '<Q', 0x4000), (48, '<I', 1), (52, '<H', 63),
                                   (54, '<H', 55), (56, '<H', 9), (58, '<H', 63),
                                   (60, '<H', 0), (62, '<H', 6),
                                   (64 + 9 * 56, '<I', 1),
                                   (64 + 9 * 56 + 4, '<I', 5),
                                   (64 + 9 * 56 + 8, '<Q', 0x4000),
                                   (64 + 2 * 56 + 16, '<Q', 0x4001),
                                   (64 + 2 * 56 + 40, '<Q', 0x100),
                                   (0x3A00 + 2 * 64 + 24, '<Q', 0x4000),
                                   (0x3A00 + 2 * 64, '<I', 0xFFFFFFFF)):
            data = bytearray(fixture())
            struct.pack_into(fmt, data, offset, value)
            cases.append(bytes(data))
        cases.extend((b'', b'\x7fELF', fixture()[:623]))
        for index, data in enumerate(cases):
            with self.subTest(index=index), self.assertRaises(ValueError):
                elf.append_payload(data, CODE, RODATA, code_va=0x10000)

    def test_rejects_payload_alignment_mutable_inputs_and_reused_slot(self):
        for code, rodata, va, alignment in ((b'', RODATA, 0x10000, 0x4000),
                                          (CODE + b'x', RODATA, 0x10000, 0x4000),
                                          (CODE, b'', 0x10000, 0x4000),
                                          (bytearray(CODE), RODATA, 0x10000, 0x4000),
                                          (CODE, RODATA, 0x10001, 0x4000),
                                          (CODE, RODATA, 0x10000, 3),
                                          (CODE, RODATA, 0x10000, 0x800),
                                          (CODE, RODATA, 1 << 64, 0x4000)):
            with self.subTest(va=va, alignment=alignment), self.assertRaises(ValueError):
                elf.append_payload(fixture(), code, rodata, va, alignment)
        with self.assertRaises(ValueError):
            elf.append_payload(bytearray(fixture()), CODE, RODATA, code_va=0x10000)
        output, _ = elf.append_payload(fixture(), CODE, RODATA, code_va=0x10000)
        with self.assertRaises(ValueError):
            elf.append_payload(output, CODE, RODATA, code_va=0x20000)


if __name__ == '__main__':
    unittest.main()
