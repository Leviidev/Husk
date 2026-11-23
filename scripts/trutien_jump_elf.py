#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Append one RX payload to the narrowly supported Tru Tien ELF layout.

This helper does not assemble code, validate game-version hashes, sign, write
files, or install anything. The caller must guard the original library version.
Existing code/data addresses and section indices remain stable. Only ELF header
metadata and verified PT_NOTE/PT_GNU_EH_FRAME headers change in the old prefix.
"""
import struct


PT_LOAD = 1
PT_PHDR = 6
PT_NOTE = 4
PT_GNU_EH_FRAME = 0x6474E550
SHT_PROGBITS = 1
SHT_STRTAB = 3
SHT_NOBITS = 8
SHF_ALLOC = 2
SHF_EXECINSTR = 4
CODE_NAME = b'.husk_plain_jump'
RODATA_NAME = b'.husk_plain_jump_rodata'
CFI_NAME = b'.husk_plain_jump_eh_frame'
EH_HEADER_NAME = b'.eh_frame_hdr'
PHDR = struct.Struct('<IIQQQQQQ')
SHDR = struct.Struct('<IIQQQQIIQQ')
UINT64_MAX = (1 << 64) - 1


def _align(value, alignment):
    return (value + alignment - 1) & -alignment


def _file_range(offset, size, length):
    return 0 <= offset <= length and 0 <= size <= length - offset


def _memory_range(start, size):
    return 0 <= start <= UINT64_MAX and 0 <= size <= UINT64_MAX - start


def _overlap(a, size_a, b, size_b):
    return size_a > 0 and size_b > 0 and a < b + size_b and b < a + size_a


def _virtual_offset(loads, va, size):
    found = [h[2] + va - h[3] for h in loads
             if h[3] <= va and va + size <= h[3] + h[5]]
    if len(found) != 1:
        raise ValueError('Unwind pointer is not inside one file-backed load')
    return found[0]


def _uleb(value):
    result = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        result.append(byte | (0x80 if value else 0))
        if not value:
            return bytes(result)


def _signed32(value):
    if not -(1 << 31) <= value < (1 << 31):
        raise ValueError('Unwind PC-relative displacement exceeds signed 32 bits')
    return struct.pack('<i', value)


def _normalise_unwind(ranges, code_size):
    if not isinstance(ranges, (tuple, list)) or not ranges:
        raise ValueError('Unwind ranges must be a nonempty list/tuple')
    result = []
    for item in ranges:
        if not isinstance(item, dict) or set(item) != {
                'offset', 'size', 'cfaRegister', 'cfaOffset', 'savedRegisters'}:
            raise ValueError('Unsupported unwind-range fields')
        offset, size, register, cfa_offset = (item[k] for k in
            ('offset', 'size', 'cfaRegister', 'cfaOffset'))
        if not all(type(x) is int for x in (offset, size, register, cfa_offset)):
            raise ValueError('Unwind fields must be integers')
        if offset < 0 or size <= 0 or offset % 4 or size % 4 or offset + size > code_size:
            raise ValueError('Unwind range is outside the code or not ARM64-aligned')
        if not 0 <= register <= 31 or not 0 <= cfa_offset <= 0x100000 or cfa_offset % 8:
            raise ValueError('Unsupported CFA register/offset')
        saved = item['savedRegisters']
        if not isinstance(saved, dict):
            raise ValueError('Saved registers must be a DWARF-register map')
        for reg, value in saved.items():
            if type(reg) is not int or not 0 <= reg <= 95 or type(value) is not int:
                raise ValueError('Unsupported saved-register rule')
            if not -0x100000 <= value < 0 or value % 8:
                raise ValueError('Saved-register offsets must be negative multiples of eight')
        result.append({'offset': offset, 'size': size, 'cfaRegister': register,
                       'cfaOffset': cfa_offset, 'savedRegisters': dict(saved)})
    result.sort(key=lambda x: x['offset'])
    if any(a['offset'] + a['size'] > b['offset'] for a, b in zip(result, result[1:])):
        raise ValueError('Overlapping unwind ranges')
    return result


def _old_unwind(original, headers, loads, sections, names):
    header = headers[7]
    if header[0] != PT_GNU_EH_FRAME or header[1] != 4 or header[5] < 12 or header[5] != header[6] or header[7] != 4:
        raise ValueError('Expected the original PT_GNU_EH_FRAME in slot 7')
    at, va, length = header[2], header[3], header[5]
    if _virtual_offset(loads, va, length) != at:
        raise ValueError('Unwind-header file/VA mapping differs')
    if original[at:at + 4] != bytes.fromhex('011b033b'):
        raise ValueError('Unsupported .eh_frame_hdr encoding')
    frame_va = va + 4 + struct.unpack_from('<i', original, at + 4)[0]
    _virtual_offset(loads, frame_va, 4)
    count = struct.unpack_from('<I', original, at + 8)[0]
    table_end = 12 + count * 8
    if count == 0 or not table_end <= length <= table_end + 16:
        raise ValueError('Invalid .eh_frame_hdr table size')
    if any(original[at + table_end:at + length]):
        raise ValueError('Unexpected nonzero unwind-header padding')
    pairs = []
    for index in range(count):
        pc, fde = struct.unpack_from('<ii', original, at + 12 + index * 8)
        pc, fde = va + pc, va + fde
        if not any(h[1] & 1 and h[3] <= pc < h[3] + h[5] for h in loads):
            raise ValueError('Unwind table names a non-executable start address')
        _virtual_offset(loads, fde, 8)
        if pairs and pairs[-1][0] >= pc:
            raise ValueError('Original unwind table is not strictly sorted')
        pairs.append((pc, fde))
    section_index = None
    for index, section in enumerate(sections):
        start = section[0]
        if names[start:names.index(b'\0', start)] == EH_HEADER_NAME:
            if section_index is not None or section[1:6] != (SHT_PROGBITS, SHF_ALLOC, va, at, length):
                raise ValueError('Unexpected original .eh_frame_hdr section')
            section_index = index
    return frame_va, pairs, section_index


def _new_frames(frame_va, code_va, ranges):
    # One CIE: version 1, zR, code alignment 1, data alignment -8,
    # return register x30, PC-relative signed-32 FDE encoding. The initial
    # frame is a leaf (SP+0, x30 still in its register); FDEs override it.
    body = b'\0' * 4 + b'\x01zR\0\x01\x78\x1e\x01\x1b\x0c\x1f\0\x08\x1e'
    body += bytes((-len(body) - 4) % 8)
    result = bytearray(struct.pack('<I', len(body)) + body)
    pairs = []
    for item in ranges:
        fde_va = frame_va + len(result)
        pc = code_va + item['offset']
        instructions = b'\x0c' + _uleb(item['cfaRegister']) + _uleb(item['cfaOffset'])
        for register, offset in sorted(item['savedRegisters'].items()):
            instructions += (bytes([0x80 | register]) if register < 64
                             else b'\x05' + _uleb(register)) + _uleb(-offset // 8)
        body = (struct.pack('<I', fde_va + 4 - frame_va)
                + _signed32(pc - (fde_va + 8)) + _signed32(item['size'])
                + b'\0' + instructions)
        body += bytes((-len(body) - 4) % 8)
        result.extend(struct.pack('<I', len(body)) + body)
        pairs.append((pc, fde_va))
    result.extend(bytes(4))
    return bytes(result), pairs


def _new_unwind_header(header_va, original_frame_va, pairs):
    result = bytearray(bytes.fromhex('011b033b') + _signed32(original_frame_va - (header_va + 4))
                       + struct.pack('<I', len(pairs)))
    for pc, fde in pairs:
        result.extend(_signed32(pc - header_va) + _signed32(fde - header_va))
    return bytes(result)


def _parse_original(original):
    if not isinstance(original, bytes) or len(original) < 64:
        raise ValueError('Expected immutable ELF64 bytes')
    if original[:7] != b'\x7fELF\x02\x01\x01':
        raise ValueError('Expected ELF64, little-endian, version 1')
    kind, machine, version = struct.unpack_from('<HHI', original, 16)
    if (kind, machine, version) != (3, 183, 1):
        raise ValueError('Expected an AArch64 ET_DYN library')
    phoff, shoff = struct.unpack_from('<QQ', original, 32)
    flags, ehsize, phentsize, phnum, shentsize, shnum, shstrndx = struct.unpack_from('<IHHHHHH', original, 48)
    if (flags, ehsize, phoff, phentsize, phnum, shentsize) != (0, 64, 64, 56, 10, 64):
        raise ValueError('Unsupported ELF/program-header structure')
    if not (1 <= shnum < 0xFF00 - 2 and 0 < shstrndx < shnum):
        raise ValueError('Extended/missing section tables are not supported')
    if not _file_range(phoff, phnum * PHDR.size, len(original)):
        raise ValueError('Program headers run outside the file')
    if shoff < phoff + phnum * PHDR.size or not _file_range(shoff, shnum * SHDR.size, len(original)):
        raise ValueError('Section headers run outside the file')
    headers = [PHDR.unpack_from(original, phoff + i * PHDR.size) for i in range(phnum)]
    first = headers[0]
    if first != (PT_PHDR, 4, phoff, phoff, phoff, phnum * PHDR.size, phnum * PHDR.size, 8):
        raise ValueError('Unexpected PT_PHDR description')
    note = headers[9]
    if note[0] != PT_NOTE or note[1] != 4 or note[5] == 0 or note[5] != note[6] or note[7] != 4:
        raise ValueError('Final slot is not the expected read-only PT_NOTE')
    if not _file_range(note[2], note[5], len(original)) or note[2] < phoff + phnum * PHDR.size:
        raise ValueError('Invalid PT_NOTE payload')
    loads = []
    for header in headers:
        kind, prot, offset, va, _, filesz, memsz, alignment = header
        if filesz and not _file_range(offset, filesz, len(original)):
            raise ValueError('A program header names bytes outside the file')
        if not _memory_range(va, memsz):
            raise ValueError('Program-header address overflow')
        if kind != PT_LOAD:
            continue
        if not memsz or filesz > memsz:
            raise ValueError('Invalid PT_LOAD size')
        if alignment > 1 and (alignment & (alignment - 1) or offset % alignment != va % alignment):
            raise ValueError('Invalid PT_LOAD alignment/congruence')
        if prot & ~7 or not prot & 4:
            raise ValueError('Unexpected PT_LOAD permissions')
        if loads and va < loads[-1][3]:
            raise ValueError('PT_LOAD virtual addresses are not ordered')
        if any(_overlap(va, memsz, old[3], old[6]) for old in loads):
            raise ValueError('Overlapping existing PT_LOAD memory ranges')
        if any(_overlap(offset, filesz, old[2], old[5]) for old in loads):
            raise ValueError('Overlapping existing PT_LOAD file ranges')
        loads.append(header)
    if not loads or len(loads) + 1 >= 16:
        raise ValueError('No room for one more supported load segment')
    if not any(h[2] <= note[2] and note[2] + note[5] <= h[2] + h[5]
               and h[3] + note[2] - h[2] == note[3] and h[1] == 4 for h in loads):
        raise ValueError('PT_NOTE is not inside an existing read-only load')

    sections = [SHDR.unpack_from(original, shoff + i * SHDR.size) for i in range(shnum)]
    if sections[0] != (0,) * 10:
        raise ValueError('Unexpected null section header')
    strings = sections[shstrndx]
    if strings[1] != SHT_STRTAB or strings[2] != 0 or strings[3] != 0 or strings[8] != 1:
        raise ValueError('Expected a non-allocated section-name string table')
    if not _file_range(strings[4], strings[5], len(original)):
        raise ValueError('Section-name table runs outside the file')
    names = original[strings[4]:strings[4] + strings[5]]
    if not names or names[0] != 0 or names[-1] != 0:
        raise ValueError('Invalid section-name table termination')
    for section in sections:
        name, kind, prot, va, offset, size, _, _, alignment, _ = section
        if name >= len(names) or names.find(b'\0', name) == -1:
            raise ValueError('Invalid section-name offset')
        end = names.index(b'\0', name)
        if names[name:end] in (CODE_NAME, RODATA_NAME):
            raise ValueError('Payload sections already exist')
        if kind != SHT_NOBITS and not _file_range(offset, size, len(original)):
            raise ValueError('A section names bytes outside the file')
        if alignment > 1 and alignment & (alignment - 1):
            raise ValueError('Invalid section alignment')
        if prot & SHF_ALLOC and size:
            if not _memory_range(va, size):
                raise ValueError('Section address overflow')
            if not any(h[3] <= va and va + size <= h[3] + h[6] for h in loads):
                raise ValueError('Allocated section is not inside an existing load')
            if kind != SHT_NOBITS and not any(
                    h[3] <= va and va + size <= h[3] + h[5]
                    and h[2] + va - h[3] == offset for h in loads):
                raise ValueError('Allocated section does not match its file-backed load mapping')
    return headers, loads, sections, names, shstrndx


def append_payload(original: bytes, code: bytes, rodata: bytes,
                   code_va=0x36B8000, alignment=0x4000, unwind_ranges=None):
    """Return modified bytes/evidence; never mutate inputs or perform file I/O.

    The default virtual address is specific to the caller's verified library.
    Synthetic callers may choose a different page-aligned, non-overlapping VA.
    Old section indices survive. The old .shstrtab index points to an appended
    name table; the old table and old section-header bytes remain in the file.
    """
    headers, loads, sections, names, shstrndx = _parse_original(original)
    if not isinstance(code, bytes) or not code or len(code) % 4:
        raise ValueError('Code must be nonempty, immutable, whole ARM64 words')
    if not isinstance(rodata, bytes) or not rodata:
        raise ValueError('Read-only payload must be nonempty immutable bytes')
    if not isinstance(alignment, int) or not 0x1000 <= alignment <= 0x100000 or alignment & (alignment - 1):
        raise ValueError('Alignment must be a supported page-size power of two')
    if not isinstance(code_va, int) or code_va < 0 or code_va % alignment:
        raise ValueError('Payload VA must be nonnegative and page-aligned')
    ro_offset = _align(len(code), 16)
    # The replacement is the last program-header slot, so the new load must
    # follow every existing load, including BSS, in virtual-address order.
    if code_va < max(h[3] + h[6] for h in loads):
        raise ValueError('Payload overlaps or precedes existing load memory')
    file_at = _align(len(original), alignment)
    if file_at % alignment != code_va % alignment:
        raise ValueError('Payload file/virtual addresses are incongruent')

    payload = bytearray(code)
    payload.extend(bytes(ro_offset - len(code)))
    payload.extend(rodata)
    ranges = None if unwind_ranges is None else _normalise_unwind(unwind_ranges, len(code))
    exec_size = ro_offset + len(rodata)
    unwind_at = cfi_at = header_at = header_section_index = None
    cfi = new_header = b''
    old_pairs, new_pairs = [], []
    if ranges is not None:
        note = headers[9]
        if note[2] != 64 + len(headers) * PHDR.size or note[5] < PHDR.size:
            raise ValueError('Final note payload cannot host a read-only unwind load header')
        original_frame_va, old_pairs, header_section_index = _old_unwind(original, headers, loads, sections, names)
        unwind_at = _align(exec_size, alignment)
        payload.extend(bytes(unwind_at - len(payload)))
        cfi_at = _align(len(payload), 8)
        payload.extend(bytes(cfi_at - len(payload)))
        cfi, new_pairs = _new_frames(code_va + cfi_at, code_va, ranges)
        payload.extend(cfi)
        header_at = _align(len(payload), 4)
        payload.extend(bytes(header_at - len(payload)))
        combined = sorted(old_pairs + new_pairs)
        if any(a[0] >= b[0] for a, b in zip(combined, combined[1:])):
            raise ValueError('New and original unwind starts conflict')
        new_header = _new_unwind_header(code_va + header_at, original_frame_va, combined)
        payload.extend(new_header)
    load_size = len(payload)
    if not _memory_range(code_va, load_size):
        raise ValueError('Payload address overflow')

    new_names = names + CODE_NAME + b'\0' + RODATA_NAME + b'\0'
    code_name_at = len(names)
    ro_name_at = len(names) + len(CODE_NAME) + 1
    cfi_name_at = header_name_at = None
    if ranges is not None:
        cfi_name_at = len(new_names)
        new_names += CFI_NAME + b'\0'
        if header_section_index is None:
            header_name_at = len(new_names)
            new_names += EH_HEADER_NAME + b'\0'
    output = bytearray(original)
    output.extend(bytes(file_at - len(output)))
    output.extend(payload)
    names_at = len(output)
    output.extend(new_names)
    table_at = _align(len(output), 8)
    output.extend(bytes(table_at - len(output)))
    updated_sections = list(sections)
    old_strings = list(updated_sections[shstrndx])
    old_strings[4], old_strings[5] = names_at, len(new_names)
    updated_sections[shstrndx] = tuple(old_strings)
    updated_sections.extend((
        (code_name_at, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR,
         code_va, file_at, len(code), 0, 0, 4, 0),
        (ro_name_at, SHT_PROGBITS, SHF_ALLOC, code_va + ro_offset,
         file_at + ro_offset, len(rodata), 0, 0, 16, 0),
    ))
    if ranges is not None:
        updated_sections.append((cfi_name_at, SHT_PROGBITS, SHF_ALLOC, code_va + cfi_at,
                                 file_at + cfi_at, len(cfi), 0, 0, 8, 0))
        if header_section_index is None:
            updated_sections.append((header_name_at, SHT_PROGBITS, SHF_ALLOC, code_va + header_at,
                                     file_at + header_at, len(new_header), 0, 0, 4, 0))
        else:
            original_header_section = list(sections[header_section_index])
            original_header_section[3:6] = [code_va + header_at, file_at + header_at, len(new_header)]
            updated_sections[header_section_index] = tuple(original_header_section)
    if len(updated_sections) >= 0xFF00:
        raise ValueError('Appended section count requires unsupported extended numbering')
    for section in updated_sections:
        output.extend(SHDR.pack(*section))
    struct.pack_into('<Q', output, 40, table_at)
    struct.pack_into('<H', output, 60, len(updated_sections))
    note_header_at = 64 + 9 * PHDR.size
    if ranges is None:
        PHDR.pack_into(output, note_header_at, PT_LOAD, 5, file_at, code_va,
                       code_va, exec_size, exec_size, alignment)
        metadata = [(40, 48), (60, 62), (note_header_at, note_header_at + PHDR.size)]
    else:
        # Keep unwind tables in a separate non-executable PT_LOAD so Husk AOT maps them
        # into the writable data copy and slides FDEs cleanly in aot_fix_cxx_unwind.
        unwind_size = load_size - unwind_at
        PHDR.pack_into(output, note_header_at, PT_LOAD, 5, file_at, code_va,
                       code_va, exec_size, exec_size, alignment)
        PHDR.pack_into(output, note_header_at + PHDR.size, PT_LOAD, 4,
                       file_at + unwind_at, code_va + unwind_at, code_va + unwind_at,
                       unwind_size, unwind_size, alignment)
        struct.pack_into('<H', output, 56, len(headers) + 1)
        phdr_size = (len(headers) + 1) * PHDR.size
        struct.pack_into('<QQ', output, 64 + 32, phdr_size, phdr_size)
        eh_header_at = 64 + 7 * PHDR.size
        PHDR.pack_into(output, eh_header_at, PT_GNU_EH_FRAME, 4, file_at + header_at,
                       code_va + header_at, code_va + header_at, len(new_header), len(new_header), 4)
        metadata = sorted((
            (40, 48), (56, 58), (60, 62), (64 + 32, 64 + 48),
            (eh_header_at, eh_header_at + PHDR.size),
            (note_header_at, note_header_at + 2 * PHDR.size),
        ))
    previous = 0
    for start, end in metadata + [(len(original), len(original))]:
        if output[previous:start] != original[previous:start]:
            raise AssertionError('An original byte outside declared ELF metadata changed')
        previous = end
    for index, old in enumerate(sections):
        if index not in (shstrndx, header_section_index) and updated_sections[index] != old:
            raise AssertionError('An existing section index or description changed')
    evidence = {
        'loadSegmentIndex': 9, 'loadFileOffset': file_at,
        'loadVA': code_va, 'loadSize': exec_size, 'loadAlignment': alignment,
        'codeSectionIndex': len(sections), 'codeVA': code_va,
        'codeFileOffset': file_at, 'codeSize': len(code),
        'rodataSectionIndex': len(sections) + 1, 'rodataVA': code_va + ro_offset,
        'rodataFileOffset': file_at + ro_offset, 'rodataSize': len(rodata),
        'sectionNamesIndex': shstrndx, 'sectionNamesFileOffset': names_at,
        'sectionTableFileOffset': table_at, 'sectionCount': len(updated_sections),
        'changedOriginalMetadataRanges': [[start, end] for start, end in metadata],
        'originalPrefixPreservedExceptMetadata': True,
        'existingLoadAddressesAndSectionIndicesPreserved': True,
        'oldNotePayloadPreserved': ranges is None,
    }
    if ranges is not None:
        evidence['unwind'] = {
            'unwindLoadSegmentIndex': 10,
            'unwindLoadFileOffset': file_at + unwind_at,
            'unwindLoadVA': code_va + unwind_at,
            'unwindLoadSize': load_size - unwind_at,
            'originalTableEntries': len(old_pairs), 'addedTableEntries': len(new_pairs),
            'tableEntries': len(old_pairs) + len(new_pairs),
            'frameVA': code_va + cfi_at, 'frameFileOffset': file_at + cfi_at,
            'frameSize': len(cfi), 'headerVA': code_va + header_at,
            'headerFileOffset': file_at + header_at, 'headerSize': len(new_header),
            'headerSectionIndex': header_section_index if header_section_index is not None else len(updated_sections) - 1,
            'frameSectionIndex': len(sections) + 2,
            'originalFrameBytesAndTablePairsPreserved': True,
            'newFDEs': [{'pc': pc, 'fdeVA': fde} for pc, fde in new_pairs],
            'ranges': ranges,
        }
    return bytes(output), evidence
