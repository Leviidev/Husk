#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Dynamic ARM64 ELF auto-scanner and payload builder for TruTien libmain.so.

Discovers every HUD hook address, struct offset, mobile profile table slot,
45 FPS hard-lock site, anti-hitch CVar default, and swap/memory threshold directly
from exported `.dynsym` symbols, `.rodata` string anchors, and AArch64 instruction
patterns so new TruTien APK updates patch cleanly without hardcoded VAs.
"""
import bisect
from collections import namedtuple
import hashlib
from pathlib import Path
import struct
import sys

if __package__ is None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from scripts.patch_trutien_jump import ORIGINAL_DIAGNOSTIC, branch_instruction
from scripts.trutien_sit_arm64 import (
    ATLAS_PNG_NAME, ATLAS_PNG_SHA256, ATLAS_PNG_SIZE, CODE_SIZE, FRAGMENTS,
)

_PtLoad = namedtuple('_PtLoad', ('flags', 'offset', 'vaddr', 'file_size', 'mem_size', 'executable'))


def _load_pt_segments(data):
    if len(data) < 64 or data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected a little-endian ELF64 library')
    if struct.unpack_from('<H', data, 16)[0] != 3 or struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected an AArch64 shared library')
    phoff = struct.unpack_from('<Q', data, 32)[0]
    phentsize, phnum = struct.unpack_from('<HH', data, 54)
    if phentsize != 56 or not 0 < phnum <= 4096 or phoff + phnum * phentsize > len(data):
        raise ValueError('Invalid ELF program table')
    loads = []
    for index in range(phnum):
        kind, flags, offset, va, _, filesz, memsz, _ = struct.unpack_from('<IIQQQQQQ', data, phoff + index * phentsize)
        if kind == 1:
            if filesz > memsz or offset + filesz > len(data):
                raise ValueError('Invalid ELF load segment')
            loads.append(_PtLoad(flags, offset, va, filesz, memsz, bool(flags & 1)))
    if not loads:
        raise ValueError('No PT_LOAD segments in ELF')
    return loads


def _align_up(val, align):
    return (val + align - 1) & -align


def _word(data, loads, va):
    off = _va_to_offset(loads, va, 4)
    if off is None:
        raise ValueError(f'Unmapped word VA {hex(va)}')
    return struct.unpack_from('<I', data, off)[0]


def _va_to_offset(loads, va, size=4):
    for seg in loads:
        if seg.vaddr <= va and va + size <= seg.vaddr + seg.file_size:
            return seg.offset + (va - seg.vaddr)
    return None


def _offset_to_va(loads, off):
    for seg in loads:
        if seg.offset <= off < seg.offset + seg.file_size:
            return seg.vaddr + (off - seg.offset)
    return None


def parse_elf_symbols(data):
    """Return (loads, sym_map, ordered_func_vas) for an AArch64 ELF64 binary."""
    loads = _load_pt_segments(data)
    e_shoff, = struct.unpack_from('<Q', data, 40)
    e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', data, 58)
    if not e_shnum or e_shstrndx >= e_shnum or e_shoff + e_shnum * e_shentsize > len(data):
        raise ValueError('Missing ELF section table')
    sections = {}
    sh_hdrs = [struct.unpack_from('<IIQQQQIIQQ', data, e_shoff + i * e_shentsize)
               for i in range(e_shnum)]
    shstr_off = sh_hdrs[e_shstrndx][4]
    for sh in sh_hdrs:
        end = data.find(b'\0', shstr_off + sh[0])
        if end < 0:
            end = shstr_off + sh[0] + 64
        sections[data[shstr_off + sh[0]:end].decode('ascii', 'replace')] = sh
    if '.dynsym' not in sections or '.dynstr' not in sections:
        raise ValueError('Missing .dynsym/.dynstr in ELF')
    ds, dst = sections['.dynsym'], sections['.dynstr']
    syms = {}
    vas = set()
    for off in range(ds[4], ds[4] + ds[5], ds[9]):
        st_name, st_info, _, _, st_value, st_size = struct.unpack_from('<IBBHQQ', data, off)
        if st_value:
            end = data.find(b'\0', dst[4] + st_name)
            if end < 0:
                end = dst[4] + st_name
            name = data[dst[4] + st_name:end].decode('ascii', 'replace')
            syms[name] = (st_value, st_size)
            if (st_info & 0xF) in (2, 10):
                vas.add(st_value)
    return loads, syms, sorted(vas)


def _sym_span(syms, ordered_vas, name, min_span=0):
    if name not in syms:
        raise ValueError(f'Missing required symbol in .dynsym: {name}')
    va, sz = syms[name]
    idx = bisect.bisect_right(ordered_vas, va)
    end = ordered_vas[idx] if idx < len(ordered_vas) else va + max(sz, 0x4000)
    span = max(sz, end - va, min_span)
    return va, min(span, 0x20000)


def _find_in_span(data, loads, va, span, pattern_bytes):
    fo = _va_to_offset(loads, va, span)
    if fo is None:
        raise ValueError(f'Span {hex(va)}..{hex(va + span)} is not file-backed')
    chunk = data[fo:fo + span]
    pos = chunk.find(pattern_bytes)
    if pos < 0:
        raise ValueError(f'Pattern {pattern_bytes.hex()} not found in {hex(va)}..{hex(va + span)}')
    if chunk.find(pattern_bytes, pos + 4) >= 0:
        raise ValueError(f'Ambiguous pattern {pattern_bytes.hex()} in {hex(va)}..{hex(va + span)}')
    return va + pos


def _decode_branch(at_va, word):
    top = word >> 26
    if top in (0x05, 0x25):
        imm = word & 0x3FFFFFF
        if imm & 0x2000000:
            imm -= 0x4000000
        return at_va + imm * 4
    if (word & 0x7E000000) == 0x34000000 or (word & 0xFF000010) == 0x54000000:
        imm19 = (word >> 5) & 0x7FFFF
        if imm19 & 0x40000:
            imm19 -= 0x80000
        return at_va + imm19 * 4
    if (word & 0x7E000000) == 0x36000000:
        imm14 = (word >> 5) & 0x3FFF
        if imm14 & 0x2000:
            imm14 -= 0x4000
        return at_va + imm14 * 4
    raise ValueError(f'Unsupported branch encoding at {hex(at_va)}: {hex(word)}')


def _decode_adrp_pair(data, loads, pc, max_gap=4):
    w0 = _word(data, loads, pc)
    if (w0 & 0x9F000000) != 0x90000000:
        return None
    rd = w0 & 0x1F
    imm21 = (((w0 >> 5) & 0x7FFFF) << 2) | ((w0 >> 29) & 3)
    if imm21 & (1 << 20):
        imm21 -= (1 << 21)
    page = (pc & ~0xFFF) + (imm21 << 12)
    for j in range(1, max_gap + 1):
        w1 = _word(data, loads, pc + j * 4)
        rn = (w1 >> 5) & 0x1F
        if rn == rd:
            if (w1 & 0xFF800000) == 0x91000000:
                return (page + ((w1 >> 10) & 0xFFF), 'add', w1 & 0x1F, j)
            if (w1 & 0xFFC00000) == 0xF9400000:
                return (page + ((w1 >> 10) & 0xFFF) * 8, 'ldr64', w1 & 0x1F, j)
            if (w1 & 0xFFC00000) == 0xB9400000:
                return (page + ((w1 >> 10) & 0xFFF) * 4, 'ldr32', w1 & 0x1F, j)
            if (w1 & 0xFFC00000) == 0xB9000000:
                return (page + ((w1 >> 10) & 0xFFF) * 4, 'str32', w1 & 0x1F, j)
    return None


def _decode_movz_movk_32(w0, w1):
    # movz wd, #imm16 ; movk wd, #hi, lsl #16
    if (w0 & 0xFFE00000) != 0x52800000 or (w1 & 0xFFE00000) != 0x72A00000:
        return None
    if (w0 & 0x1F) != (w1 & 0x1F):
        return None
    return ((w0 >> 5) & 0xFFFF) | (((w1 >> 5) & 0xFFFF) << 16)


def _find_all_cstr_vas(data, loads, key_bytes):
    vas = set()
    needle = b'\0' + key_bytes
    idx = 0
    while True:
        idx = data.find(needle, idx)
        if idx < 0:
            break
        va = _offset_to_va(loads, idx + 1)
        if va is not None:
            vas.add(va)
        idx += 1
    return vas


def _find_cvar_default_insn(data, loads, fn_va, fn_span, key_bytes, target_reg=2):
    """Locate the instruction setting w<target_reg> default near the unique key xref."""
    str_vas = _find_all_cstr_vas(data, loads, key_bytes)
    if not str_vas:
        raise ValueError(f'Missing string {key_bytes!r}')
    hits = []
    for off in range(0, fn_span, 4):
        pc = fn_va + off
        res = _decode_adrp_pair(data, loads, pc, max_gap=5)
        if res and res[1] == 'add' and res[0] in str_vas:
            hits.append(pc)
    if len(hits) != 1:
        raise ValueError(f'Expected 1 xref to {key_bytes!r} in {hex(fn_va)}, got {[hex(h) for h in hits]}')
    xref_pc = hits[0]
    # Search forward up to +28B (or -8..+28B) for the last instruction setting w<target_reg> before the getter BL.
    cand = None
    for delta in range(-8, 32, 4):
        pc = xref_pc + delta
        w = _word(data, loads, pc)
        if (w >> 26) == 0x25 and delta > 0:
            break
        rd = w & 0x1F
        if rd == target_reg and ((w & 0x7F800000) in (0x52800000, 0x12800000)  # movz/movn
                                 or (w & 0x7FE00000) == 0x2A000000               # orr (mov w2, wzr / cset fallback)
                                 or (w & 0x7FE00C00) == 0x1A800400               # csinc / cset
                                 or (w & 0x7FC00000) == 0x53000000):             # ubfx / sbfx
            cand = pc
    if cand is None:
        raise ValueError(f'No default register w{target_reg} assignment near {key_bytes!r} at {hex(xref_pc)}')
    return cand


def _encode_movz(rd, imm16):
    return struct.pack('<I', 0x52800000 | ((imm16 & 0xFFFF) << 5) | (rd & 0x1F))


def _encode_movk(rd, imm16, shift=16):
    hw = shift // 16
    return struct.pack('<I', 0x72800000 | (hw << 21) | ((imm16 & 0xFFFF) << 5) | (rd & 0x1F))


def _encode_adrp(rd, from_va, target_va):
    pages = (target_va >> 12) - (from_va >> 12)
    immhi = (pages >> 2) & 0x7FFFF
    immlo = pages & 3
    return struct.pack('<I', 0x90000000 | (immlo << 29) | (immhi << 5) | (rd & 0x1F))


def _encode_ldr64_uoff(rt, rn, byte_offset):
    if byte_offset % 8 or not 0 <= byte_offset // 8 < 0x1000:
        raise ValueError(f'Out-of-range ldr64 offset: {hex(byte_offset)}')
    return struct.pack('<I', 0xF9400000 | ((byte_offset // 8) << 10) | ((rn & 0x1F) << 5) | (rt & 0x1F))


def _encode_ldr32_uoff(rt, rn, byte_offset):
    if byte_offset % 4 or not 0 <= byte_offset // 4 < 0x1000:
        raise ValueError(f'Out-of-range ldr32 offset: {hex(byte_offset)}')
    return struct.pack('<I', 0xB9400000 | ((byte_offset // 4) << 10) | ((rn & 0x1F) << 5) | (rt & 0x1F))


def _encode_ldrb_uoff(rt, rn, byte_offset):
    if not 0 <= byte_offset < 0x1000:
        raise ValueError(f'Out-of-range ldrb offset: {hex(byte_offset)}')
    return struct.pack('<I', 0x39400000 | (byte_offset << 10) | ((rn & 0x1F) << 5) | (rt & 0x1F))


def _encode_stp_w_off(rt1, rt2, rn, byte_offset):
    if byte_offset % 4 or not -256 <= byte_offset <= 252:
        raise ValueError(f'Out-of-range stp w offset: {byte_offset}')
    imm7 = (byte_offset // 4) & 0x7F
    return struct.pack('<I', 0x29000000 | (imm7 << 15) | ((rt2 & 0x1F) << 10) | ((rn & 0x1F) << 5) | (rt1 & 0x1F))


def _encode_bl(at, target):
    disp = target - at
    if at % 4 or target % 4 or not -(1 << 27) <= disp < (1 << 27):
        raise ValueError(f'ARM64 BL target out of range: {hex(at)} -> {hex(target)}')
    return struct.pack('<I', 0x94000000 | ((disp // 4) & 0x3FFFFFF))


def _find_sym_prefix(syms, prefix):
    hits = [k for k in syms if k.startswith(prefix)]
    if len(hits) != 1:
        raise ValueError(f'Expected 1 symbol starting with {prefix!r}, got {hits}')
    return hits[0]


def discover_trutien_profile(data):
    """Discover all patch configs and relocatable payload parameters from any TruTien libmain.so."""
    loads, syms, ordered_vas = parse_elf_symbols(data)
    exec_loads = [s for s in loads if s.executable]
    if not exec_loads:
        raise ValueError('No executable PT_LOAD segment in ELF')
    end_bss = max(s.vaddr + s.mem_size for s in loads)
    code_va = _align_up(end_bss, 0x4000)

    # 1. HUD hooks & fragment targets
    hvc_va, hvc_span = _sym_span(syms, ordered_vas, '_Z13HudViewCombatRK10HUDVIEWCTX')
    hvc_fo = _va_to_offset(loads, hvc_va, hvc_span)
    hvc_chunk = data[hvc_fo:hvc_fo + hvc_span]
    draw_pat = bytes.fromhex('21d4e04e62cc614ea2033cad')
    draw_cands = []
    pos = 0
    while True:
        pos = hvc_chunk.find(draw_pat, pos)
        if pos < 0:
            break
        cand_va = hvc_va + pos
        w_bl = _word(data, loads, cand_va + 0x0C)
        w_b = _word(data, loads, cand_va + 0x10)
        if (w_bl >> 26) == 0x25 and (w_b >> 26) == 0x05:
            draw_cands.append(cand_va)
        pos += 4
    if len(draw_cands) != 1:
        raise ValueError(f'Expected 1 draw_hook candidate in HudViewCombat, got {[hex(x) for x in draw_cands]}')
    draw_hook = draw_cands[0] + 0x10
    draw_exit = _decode_branch(draw_hook, _word(data, loads, draw_hook))

    hic_va, hic_span = _sym_span(syms, ordered_vas, '_Z16HudInputClassifyRK11HUDTOUCHCTXddP12HUDFINGERHIT')
    in_sig = _find_in_span(data, loads, hic_va, hic_span, bytes.fromhex('e8664039280c00342041601e'))
    input_hook = in_sig + 8
    input_cont = input_hook + 4
    input_orig_pat = _find_in_span(data, loads, hic_va, hic_span, bytes.fromhex('48008052680200b9ff030791'))
    input_orig = input_orig_pat + 8

    rac_va, rac_span = _sym_span(syms, ordered_vas, '_ZN12CECMobileHud13RunActionCoreERK12HUDACTIONREQ')
    action_hook = _find_in_span(data, loads, rac_va, rac_span, bytes.fromhex('3f1d017108110054'))
    action_orig_disp = action_hook + 8
    mode_res = _decode_adrp_pair(data, loads, action_orig_disp, max_gap=4)
    if not mode_res or mode_res[1] != 'ldr64':
        raise ValueError('Failed to decode mode_ptr_va in RunActionCore')
    mode_ptr_va = mode_res[0]
    action_jump = None
    for off in range(0, rac_span - 12, 4):
        pc = rac_va + off
        if (_word(data, loads, pc) == 0xAA1303E0
                and (_word(data, loads, pc + 4) >> 26) == 5
                and _word(data, loads, pc + 8) == 0x71000B3F):
            action_jump = pc
            break
    if action_jump is None:
        raise ValueError(f'Pattern e00313aa <b target> 3f0b0071 not found in {hex(rac_va)}..{hex(rac_va + rac_span)}')
    action_jump += 8
    done_sig = _find_in_span(data, loads, rac_va, rac_span, bytes.fromhex('083540f900013fd620008052'))
    action_done = done_sig + 8
    action_obnd = done_sig + 12

    hfw_va, hfw_span = _sym_span(syms, ordered_vas, '_Z12HudFillWorldP12CECGameUIManP13CECHostPlayerP7HUDDATAP10HUDUISTATEdP13HUDWORLDSTATE')
    fill_hook = _find_in_span(data, loads, hfw_va, hfw_span, bytes.fromhex('4803023908e38e52'))
    fill_cont = fill_hook + 4

    # Sit flag offset in CECHostPlayer (from OnMsgPlayerSitDown: ldr w8, [x19, #off]; ...; orr w8, w8, #0x20)
    sit_va, sit_span = _sym_span(syms, ordered_vas, '_ZN13CECHostPlayer18OnMsgPlayerSitDownERK5ECMSG')
    sit_flag_off = None
    for off in range(0, sit_span - 8, 4):
        w0 = _word(data, loads, sit_va + off)
        # ldr w8, [x?, #off] followed closely by orr w?, w?, #0x20 (321b01... / 321b00...) or and/str
        if (w0 & 0xFFC0001F) == 0xB9400008:
            # Verify nearby orr #0x20 (`321b00...`? Wait: #0x20 = bit 5 -> immr=27, imms=0 -> 0x321b0108)
            w_next = [ _word(data, loads, sit_va + off + j*4) for j in range(1, 5) ]
            if any((wn & 0xFFFFFC00) == 0x321B0000 for wn in w_next):
                sit_flag_off = ((w0 >> 10) & 0xFFF) * 4
                break
    if sit_flag_off is None:
        raise ValueError('Failed to decode sit_flag_off in OnMsgPlayerSitDown')

    # NativePage offsets for ensure_atlas (`native_base_off`, `atlas_off`, `native_cnt_off`)
    np_va, np_span = _sym_span(syms, ordered_vas, '_ZN12CECMobileHud10NativePageEP9A2DSpriteiii')
    np_adds = []
    for off in range(0, np_span - 16, 4):
        w0 = _word(data, loads, np_va + off)
        if (w0 & 0xFFC00000) == 0x91400000:
            hi = ((w0 >> 10) & 0xFFF) << 12
            rd0 = w0 & 0x1F
            for j in range(1, 4):
                wn = _word(data, loads, np_va + off + j * 4)
                if (wn & 0xFF800000) == 0x91000000 and ((wn >> 5) & 0x1F) == rd0:
                    val = hi + ((wn >> 10) & 0xFFF)
                    if val > 0x30000:
                        np_adds.append(val)
                    break
    if len(np_adds) < 2:
        raise ValueError(f'Missing CECMobileHud::NativePage offset adds: {[hex(x) for x in np_adds]}')
    native_base_off = np_adds[0]
    atlas_cands = [v for v in np_adds if 0x30000 < v < native_base_off]
    if not atlas_cands:
        raise ValueError(f'Missing atlas_off in CECMobileHud::NativePage: {[hex(x) for x in np_adds]}')
    atlas_off = atlas_cands[0]
    # m_nNativePageCount immediately follows {int m_nPageBase; NativePageSlot[0x60]} at +0x908; verify in NativePage
    cnt_delta = 0x908
    for off in range(0, np_span, 4):
        w = _word(data, loads, np_va + off)
        if (w & 0xFFC00000) == 0xB9400000:
            imm = ((w >> 10) & 0xFFF) * 4
            if 0x800 <= imm <= 0xA00 and imm % 8 == 0:
                cnt_delta = imm
                break
    native_cnt_off = native_base_off + cnt_delta

    # 2. Profiles & SafeProfile bypass
    gmp_va, gmp_span = _sym_span(syms, ordered_vas, '_ZN12CECPerfStats16GetMobileProfileEv')
    profile_t0 = None
    for off in range(0, gmp_span, 4):
        res = _decode_adrp_pair(data, loads, gmp_va + off, max_gap=4)
        if res and res[1] == 'add':
            cand_va = res[0]
            fo_c = _va_to_offset(loads, cand_va, 0x6C * 5)
            if fo_c is not None and data[fo_c:fo_c + 12] == bytes.fromhex('00000000320000001e000000'):
                profile_t0 = cand_va
                break
    if profile_t0 is None:
        raise ValueError('Could not locate MobileProfile T0..T4 table')
    profile_all_bases = tuple(profile_t0 + i * 0x6C for i in range(5))
    profile_cap_vas = tuple(b + 0x2C for b in profile_all_bases if _word(data, loads, b + 0x2C) == 0)
    profile_worker_bases = profile_all_bases[:4]
    profile_gfx_task_auto_vas = tuple(b + 0x60 for b in profile_all_bases)

    safe_profile_bypass = None
    gmp_fo = _va_to_offset(loads, gmp_va, gmp_span)
    safe_vas = _find_all_cstr_vas(data, loads, b'TRUTIEN_TIER_SAFE\0')
    has_tier_safe = any(
        (res := _decode_adrp_pair(data, loads, gmp_va + off, max_gap=4))
        and res[1] == 'add' and res[0] in safe_vas
        for off in range(0, gmp_span, 4)
    )
    if has_tier_safe:
        sp_pos = data[gmp_fo:gmp_fo + gmp_span].find(bytes.fromhex('ca5258390a010034'))
        if sp_pos >= 0:
            va_cbz = gmp_va + sp_pos + 4
            safe_profile_bypass = (va_cbz, _decode_branch(va_cbz, _word(data, loads, va_cbz)), '0a010034')

    # 3. RunInRenderThread sites
    rirt_va, rirt_span = _sym_span(syms, ordered_vas, '_ZN7CECGame17RunInRenderThreadEv')
    set_frame_cap_va = syms['_Z18AndroidSetFrameCapi'][0]
    perf_hint_target_va = syms['_Z24AndroidPerfHintSetTargeti'][0]

    cap_ini_force = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('7f0200719a99b9728d00005428008052')) + 8
    cap_ini_clamp = cap_ini_force + 4
    cap45_stp = _word(data, loads, cap_ini_clamp + 4)
    if (cap45_stp & 0xFFC07FFF) != 0x29004FE8:
        raise ValueError(f'Unexpected cap45 stp instruction: {hex(cap45_stp)}')
    cap45_sp_off = ((cap45_stp >> 15) & 0x7F) * 4
    w_cap_b = _word(data, loads, cap_ini_clamp + 8)
    if (w_cap_b >> 26) != 5:
        raise ValueError(f'Unexpected branch word at cap_ini_clamp+8: {hex(w_cap_b)}')
    cap45_exit = _decode_branch(cap_ini_clamp + 8, w_cap_b)

    startup_120 = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('3f010a6b290080528a078052')) + 8
    # Decode frame_cap_global right before cap_ini_force or around startup_120 (`str w9, [x8, #imm]` -> earlier `adrp x8, ...`?)
    # Better: look at `b3058052` / after startup_120: `frame_cap_global` is set right around `cap_ini_clamp - 0x24` or `bl AndroidSetFrameCap` #3 (`first_tick_overwrite`)!
    fc_res = None
    for off in range(0, rirt_span - 16, 4):
        pc = rirt_va + off
        w0 = _word(data, loads, pc)
        if (w0 & 0x9F000000) == 0x90000000:
            # first_tick_overwrite pattern: `1f01096b 08b1891a <str w8, [x, #imm]>` followed by adrp+ldr `[x?, #imm]` -> frame_cap_global!
            pass
    first_tick_anchor = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('1f01096b08b1891a'))
    first_tick_va = first_tick_anchor + 8
    first_tick_word = _word(data, loads, first_tick_va)
    first_tick_orig = struct.pack('<I', first_tick_word).hex()
    fc_pair = _decode_adrp_pair(data, loads, first_tick_va + 4, max_gap=2)
    if not fc_pair or fc_pair[1] != 'ldr32':
        raise ValueError('Could not decode frame_cap_global near first_tick_overwrite')
    frame_cap_global = fc_pair[0]

    # Startup ladder sites starting from startup_120:
    startup_90_inc_from = None
    for off in range(4, 0x30, 4):
        pc = startup_120 + off
        w = _word(data, loads, pc)
        if (w & 0xFF00001F) == 0x34000008:  # cbz w8, target
            startup_90_inc_from = pc
            startup_90_inc = (pc, _decode_branch(pc, w))
            break
    if startup_90_inc_from is None:
        raise ValueError('Could not locate startup_90_inc cbz w8')
    startup_89 = _find_in_span(data, loads, startup_90_inc_from, 0x24, bytes.fromhex('8a078052'))
    startup_89_inc_from = startup_89 + 12
    w_s89 = _word(data, loads, startup_89_inc_from)
    if (w_s89 >> 26) != 5:
        raise ValueError(f'Unexpected word at startup_89_inc: {hex(w_s89)}')
    startup_89_inc = (startup_89_inc_from, startup_90_inc[1])
    startup_rates = _find_in_span(data, loads, startup_89_inc_from, 0x20, bytes.fromhex('89078052'))
    startup_rates_inc = _find_in_span(data, loads, startup_rates, 0x20, bytes.fromhex('37feff34'))
    startup_mid = _find_in_span(data, loads, startup_rates_inc, 0x30, bytes.fromhex('09058052'))
    startup_mid_inc_va = startup_mid - 4
    startup_mid_inc_hex = struct.pack('<I', _word(data, loads, startup_mid_inc_va)).hex()
    startup_mid_inc = startup_mid_inc_va if startup_mid_inc_hex == 'b4000036' else (startup_mid_inc_va, startup_mid_inc_hex)
    startup_floor = _find_in_span(data, loads, startup_mid, 0x30, bytes.fromhex('ca038052'))

    # Live ladder sites:
    live_120 = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('290d89133f010a6b89078052')) + 8
    live_90_from = None
    for off in range(4, 0x24, 4):
        pc = live_120 + off
        w = _word(data, loads, pc)
        if (w & 0xFF00001F) == 0x34000008:  # cbz w8, target
            live_90_from = pc
            live_90_inc = (pc, _decode_branch(pc, w))
            break
    if live_90_from is None:
        raise ValueError('Could not locate live_90_inc cbz w8')
    live_89 = _find_in_span(data, loads, live_90_from, 0x20, bytes.fromhex('88078052'))
    live_89_from = live_89 + 12
    w_l89 = _word(data, loads, live_89_from)
    if (w_l89 >> 26) != 5:
        raise ValueError(f'Unexpected word at live_89_inc: {hex(w_l89)}')
    live_89_inc = (live_89_from, live_90_inc[1])
    live_rates = _find_in_span(data, loads, live_89_from, 0x18, bytes.fromhex('88078052'))
    live_rates_inc = live_rates + 8
    live_mid_inc = _find_in_span(data, loads, live_rates_inc, 0x24, bytes.fromhex('b6000036'))
    live_mid = _find_in_span(data, loads, live_mid_inc, 0x18, bytes.fromhex('0a058052'))
    live_floor = _find_in_span(data, loads, live_mid, 0x30, bytes.fromhex('ca038052'))

    # Surface.setFrameRate bypass sites inside RunInRenderThread (`bl AndroidSetFrameCap` indices 1, 2, 4 of 5)
    cap_bls = []
    for off in range(0, rirt_span, 4):
        pc = rirt_va + off
        w = _word(data, loads, pc)
        if (w >> 26) == 0x25 and _decode_branch(pc, w) == set_frame_cap_va:
            cap_bls.append(pc)
    if len(cap_bls) != 5:
        raise ValueError(f'Expected 5 calls to AndroidSetFrameCap in RunInRenderThread, found {len(cap_bls)}')
    surface_rate_bypass_sites = (
        (cap_bls[1], 'live display refresh'),
        (cap_bls[2], 'runtime FrameCapNext step'),
        (cap_bls[4], 'thermal transition'),
    )

    fallback_rung = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('f6ffff1796000035c8038052')) + 8
    shared_climb_mark = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('fba39052197d8052ba0a8052')) + 8

    tc_va = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('8d020054'))
    # Verify preceded by `e8d79f1a` 8 bytes before:
    if _word(data, loads, tc_va - 8) != 0x1A9FD7E8:
        tc_anchor = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('e8d79f1a'))
        tc_va = tc_anchor + 8
    thermal_cap_bypass = (tc_va, _decode_branch(tc_va, _word(data, loads, tc_va)))

    rhi_t0_va = _find_in_span(data, loads, rirt_va, rirt_span, bytes.fromhex('b3c0961ae0031f2a36010034')) + 8
    rhi_thermal_tier0 = (rhi_t0_va, _decode_branch(rhi_t0_va, 0x34000136))

    # 4. FrameCapNext
    fcn_name = _find_sym_prefix(syms, '_Z12FrameCapNext')
    fcn_va, fcn_span = _sym_span(syms, ordered_vas, fcn_name)
    login_drop_va = _find_in_span(data, loads, fcn_va, fcn_span, bytes.fromhex('a301f837cc024039ac050034')) + 8
    login_drop_bypass = (login_drop_va, _decode_branch(login_drop_va, 0x340005AC))

    never_step_va = _find_in_span(data, loads, fcn_va, fcn_span, bytes.fromhex('9f01096b6a0600540e7d8052')) + 8
    # Branch target `stay` = `short_hold_ladder` (1f290071) inside FrameCapNext:
    short_hold_ladder = _find_in_span(data, loads, never_step_va, 0x150, bytes.fromhex('1f290071'))
    never_step_down = (never_step_va, short_hold_ladder)
    drop_drop_target = never_step_va + 0x14
    step_one_rung = _find_in_span(data, loads, never_step_va, 0x80, bytes.fromhex('4bffff54'))
    climb_mark_ladder = _find_in_span(data, loads, short_hold_ladder, 0x200, bytes.fromhex('ad0a8052'))
    short_hold_backoff = _find_in_span(data, loads, climb_mark_ladder, 0x150, bytes.fromhex('1f290071'))
    climb_mark_backoff = _find_in_span(data, loads, short_hold_backoff, 0x180, bytes.fromhex('a90a8052'))

    # 5. TuneTick
    tt_va, tt_span = _sym_span(syms, ordered_vas, '_Z8TuneTickj')
    tt_bypass_anchor = _find_in_span(data, loads, tt_va, tt_span, bytes.fromhex('080140b91f050071')) + 8
    tt_orig_w = _word(data, loads, tt_bypass_anchor)
    tt_orig_hex = struct.pack('<I', tt_orig_w).hex()
    tt_tgt = _decode_branch(tt_bypass_anchor, tt_orig_w)
    tune_thermal_bypass = (tt_bypass_anchor, tt_tgt) if tt_orig_hex == 'ab010054' else (tt_bypass_anchor, tt_tgt, tt_orig_hex)

    # 6. Feature CVars (5 stall + 16 enable) via unique string xref inside owner function!
    autotune_va = _find_cvar_default_insn(data, loads, tt_va, tt_span, b'AutoTune\0')
    login_fc_va = _find_cvar_default_insn(data, loads, rirt_va, rirt_span, b'LoginFrameCap\0')
    climb_bo_va = _find_cvar_default_insn(data, loads, rirt_va, rirt_span, b'FrameCapClimbBackoff\0')
    cpu_spd_va = _find_cvar_default_insn(data, loads, rirt_va, rirt_span, b'CpuSpeedLog\0')

    sdl_va, sdl_span = _sym_span(syms, ordered_vas, 'SDL_main')
    stack_smp_va = _find_cvar_default_insn(data, loads, sdl_va, sdl_span, b'StackSampler\0')

    stall_feature_defaults = (
        (autotune_va, '22008052', 'Video.AutoTune default in TuneOnSecond'),
        (login_fc_va, '22008052', 'Video.LoginFrameCap default in RunInRenderThread'),
        (climb_bo_va, '22008052', 'Video.FrameCapClimbBackoff default in RunInRenderThread'),
        (cpu_spd_va, '42008052', 'Video.CpuSpeedLog per-frame sampler default in RunInRenderThread'),
        (stack_smp_va, '22008052', 'Video.StackSampler 5ms SIGPROF sampler default'),
    )

    pst_va, pst_span = _sym_span(syms, ordered_vas, '_ZN12CECPerfStats4TickEv')
    rhi_init_va, rhi_init_span = _sym_span(syms, ordered_vas, '_ZN13A3DRHIDevice94InitEPvjP23_D3DPRESENT_PARAMETERS_')
    rhi_pres_va, rhi_pres_span = _sym_span(syms, ordered_vas, '_ZN13A3DRHIDevice97PresentEPK7tagRECTS2_PvPK8_RGNDATA')
    up_va, up_span = _sym_span(syms, ordered_vas, '_ZN13A3DRHIUpscale7GetModeEv')
    gfx_va, gfx_span = _sym_span(syms, ordered_vas, '_ZN11A3DGFXExMan4InitEP9A3DDevice')

    perfhint_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'PerfHint\0')
    pate_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'PateFrameLazy\0')
    skin_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'SkinTickQueueHash\0')
    uitick_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'UITickLite\0')
    gfxf_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'GfxFillSplit\0')
    port_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'GfxPendSetters\0')
    nice_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'ThreadNice\0')
    mat_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'MatrixSIMD\0')
    blend_va = _find_cvar_default_insn(data, loads, pst_va, pst_span, b'BlendMatLite\0')
    rhi_df1 = _find_cvar_default_insn(data, loads, rhi_init_va, rhi_init_span, b'RHIDrawFast\0')
    rhi_df2 = _find_cvar_default_insn(data, loads, rhi_pres_va, rhi_pres_span, b'RHIDrawFast\0')
    rhi_fsd = _find_cvar_default_insn(data, loads, rhi_init_va, rhi_init_span, b'RHIFrameStreamDraw\0')
    up_w0 = _find_in_span(data, loads, up_va, up_span, bytes.fromhex('00791f53'))
    up_w21 = _find_in_span(data, loads, up_va, up_span, bytes.fromhex('15791f53'))
    gfx_cnt = _find_cvar_default_insn(data, loads, gfx_va, gfx_span, b'GfxCapNoTick\0')
    gfx_wo = _find_cvar_default_insn(data, loads, gfx_va, gfx_span, b'GfxWaterOnce\0')

    enable_feature_defaults = (
        (perfhint_va, 'e2031f2a', '42008052', 'Video.PerfHint=2 ADPF CPU scheduler session'),
        (pate_va, 'e2031f2a', '22008052', 'Video.PateFrameLazy=1 skip unchanged overhead nameplates'),
        (skin_va, 'e2031f2a', '22008052', 'Video.SkinTickQueueHash=1 deduplicate skeleton tick queue'),
        (uitick_va, 'e2031f2a', '22008052', 'Video.UITickLite=1 skip unchanged UI tree rebuilds'),
        (gfxf_va, 'e2031f2a', '22008052', 'Video.GfxFillSplit=1 split GFX fills across task workers'),
        (port_va, 'e2031f2a', '22008052', 'Video.PortraitLazy=1 defer unchanged HUD portrait redraws'),
        (nice_va, 'e2031f2a', '22008052', 'Video.ThreadNice=1 render and worker thread priority'),
        (mat_va, 'e2031f2a', '22008052', 'Video.MatrixSIMD=1 ARM64 NEON matrix transforms'),
        (blend_va, 'e2031f2a', '22008052', 'Video.BlendMatLite=1 skip unmasked bone skinning math safely'),
        (rhi_df1, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on init'),
        (rhi_df2, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on live reload'),
        (rhi_fsd, '02008012', '22008052', 'Video.RHIFrameStreamDraw=1 skip CPU hash on GFX dynamic vertex stream'),
        (up_w0, '00791f53', '20008052', 'Video.Upscale3D=1 (SGSR) default in fast Upscale3D getter'),
        (up_w21, '15791f53', '35008052', 'Video.Upscale3D=1 (SGSR) default in A3DRHIUpscale setup'),
        (gfx_cnt, 'e2d79f1a', '22008052', 'Video.GfxCapNoTick=1 cap excess VFX emitter ticks on Tier 1'),
        (gfx_wo, 'e2031f2a', '22008052', 'Video.GfxWaterOnce=1 merge clipped water flush into single pass'),
    )

    # 7. LowMem / RHI / UIOptimizeAll / SwapChain
    lm_va, lm_span = _sym_span(syms, ordered_vas, '_ZN7CECGame7LoadMidEv')
    lm_pct_vas = _find_all_cstr_vas(data, loads, b'72,77\0')
    lowmem_pct_str = None
    for off in range(0, lm_span, 4):
        res = _decode_adrp_pair(data, loads, lm_va + off, max_gap=4)
        if res and res[1] == 'add' and res[0] in lm_pct_vas:
            lowmem_pct_str = res[0]
            break
    if lowmem_pct_str is None:
        raise ValueError('Could not locate 72,77 LowMemFootprintPct string in LoadMid')
    lowmem_critical = _find_in_span(data, loads, lm_va, lm_span, bytes.fromhex('08b0881a9f130071ca2b8052')) + 8
    lowmem_holdoff = lowmem_critical + 8
    rhi_mem_cache = _find_in_span(data, loads, lm_va, lm_span, bytes.fromhex('150082521f04407109048052')) + 8
    rhi_dyn_cache = rhi_mem_cache + 24

    id_va, id_span = _sym_span(syms, ordered_vas, '_ZN12CECGameUIMan11InitDialogsEv')
    ui_optimize_all = _find_in_span(data, loads, id_va, id_span, bytes.fromhex('fa03002ae8279f1a7f030071ead79f1a')) + 4

    glc_va, glc_span = _sym_span(syms, ordered_vas, '_ZN4bgfx2gl9GlContext6createERKNS_9SwapChainEj')
    glr_va, glr_span = _sym_span(syms, ordered_vas, '_ZN4bgfx2gl9GlContext6resizeERKNS_9SwapChainEj')
    gles_create_swap0 = _find_in_span(data, loads, glc_va, glc_span, bytes.fromhex('601640f97f0a00f9011d0753614600b9')) + 8
    gles_resize_swap0 = _find_in_span(data, loads, glr_va, glr_span, bytes.fromhex('601640f9811e0753614600b9')) + 4

    vk_va, vk_span = _sym_span(syms, ordered_vas, '_ZN4bgfx2vk11SwapChainVK15createSwapChainEv')
    vk_vsync_orig = bytes.fromhex(
        'e88b40b988030034e9031faa290500913f0108ebc3ffff54'
        'e8020034e9031faa290500913f0108ebc3ffff54'
        '48020034e9031faa290500913f0108ebc3ffff54'
        'a8010034ea031faa09008012eb8300916c008052'
        '6d796ab8bf01007129118c1acd0000344a050091'
        '5f0108eb43ffff5402000014090080123f050031'
        'e803891a93ffff17'
    )
    vk_vsync_off = _find_in_span(data, loads, vk_va, vk_span, vk_vsync_orig)

    # 8. Preserved spans (existing Khinh cong / Sit / Layout checks)
    hlc_va, hlc_span = _sym_span(syms, ordered_vas, '_Z13HudLayoutCalcRK14HUDLAYOUTINPUTP9HUDLAYOUT')
    # tail range after HudLayoutCalc+0x2e20 .. +0x5f30 matches existing preserved check:
    hlc_p1 = hlc_va + 0x2E20
    hlc_p2 = hlc_va + 0x5F30
    # Locate ORIGINAL_DIAGNOSTIC ('Khinh cong...')
    diag_fo = data.find(ORIGINAL_DIAGNOSTIC)
    diag_va = _offset_to_va(loads, diag_fo) if diag_fo >= 0 else None

    preserved_spans = [
        (hlc_p1, hlc_p2 - hlc_p1, True),
        (hvc_va + 0xD30, 132, True),
    ]
    if diag_va is not None:
        preserved_spans.append((diag_va, len(ORIGINAL_DIAGNOSTIC), False))
    preserved_spans.extend([
        (rac_va + 0x1F8, 0x30, True),
        (rac_va + 0x684, 160, True),
        (_sym_span(syms, ordered_vas, '_ZN13CECHostPlayer15CanTakeOffWaterEv')[0], 112, True),
        (_sym_span(syms, ordered_vas, '_ZN13CECHostPlayer12OnMsgHstJumpERK5ECMSG')[0], 1048, True),
        (_sym_span(syms, ordered_vas, '_ZN13CECHostPlayer10CmdSitDownEb')[0], 68, True),
        (_sym_span(syms, ordered_vas, '_Z18c2s_SendCmdSitDownv')[0], 76, True),
        (_sym_span(syms, ordered_vas, '_Z18c2s_SendCmdStandUpv')[0], 76, True),
    ])

    cfg = {
        'version_code': 0,
        'original_sha256': hashlib.sha256(data).hexdigest(),
        'code_va': code_va,
        'draw_hook': draw_hook,
        'input_hook': input_hook,
        'action_hook': action_hook,
        'fill_hook': fill_hook,
        'profile_cap_vas': profile_cap_vas,
        'profile_worker_bases': profile_worker_bases,
        'profile_gfx_task_auto_vas': profile_gfx_task_auto_vas,
        'profile_all_bases': profile_all_bases,
        'set_frame_cap_va': set_frame_cap_va,
        'perf_hint_target_va': perf_hint_target_va,
        'surface_rate_bypass_sites': surface_rate_bypass_sites,
        'stall_feature_defaults': stall_feature_defaults,
        'enable_feature_defaults': enable_feature_defaults,
        'frame_cap_global': frame_cap_global,
        'cap_ini_force': cap_ini_force,
        'cap_ini_clamp': cap_ini_clamp,
        'startup_120': startup_120,
        'startup_90_inc': startup_90_inc,
        'startup_89': startup_89,
        'startup_89_inc': startup_89_inc,
        'startup_rates': startup_rates,
        'startup_rates_inc': startup_rates_inc,
        'startup_mid_inc': startup_mid_inc,
        'startup_mid': startup_mid,
        'startup_floor': startup_floor,
        'live_120': live_120,
        'live_90_inc': live_90_inc,
        'live_89': live_89,
        'live_89_inc': live_89_inc,
        'live_rates': live_rates,
        'live_rates_inc': live_rates_inc,
        'live_mid_inc': live_mid_inc,
        'live_mid': live_mid,
        'live_floor': live_floor,
        'fallback_rung': fallback_rung,
        'login_drop_bypass': login_drop_bypass,
        'shared_climb_mark': shared_climb_mark,
        'never_step_down': never_step_down,
        'step_one_rung': step_one_rung,
        'short_hold_ladder': short_hold_ladder,
        'climb_mark_ladder': climb_mark_ladder,
        'short_hold_backoff': short_hold_backoff,
        'climb_mark_backoff': climb_mark_backoff,
        'first_tick_overwrite': (first_tick_va, first_tick_orig),
        'thermal_cap_bypass': thermal_cap_bypass,
        'rhi_thermal_tier0': rhi_thermal_tier0,
        'tune_thermal_bypass': tune_thermal_bypass,
        'safe_profile_bypass': safe_profile_bypass,
        'lowmem_pct_str': lowmem_pct_str,
        'lowmem_critical': lowmem_critical,
        'lowmem_holdoff': lowmem_holdoff,
        'rhi_mem_cache': rhi_mem_cache,
        'rhi_dyn_cache': rhi_dyn_cache,
        'ui_optimize_all': ui_optimize_all,
        'gles_create_swap0': gles_create_swap0,
        'gles_resize_swap0': gles_resize_swap0,
        'vk_vsync_off': vk_vsync_off,
        'preserved_spans': tuple(preserved_spans),
        # Payload build parameters
        'payload_params': {
            'code_va': code_va,
            'atlas_off': atlas_off,
            'native_base_off': native_base_off,
            'native_cnt_off': native_cnt_off,
            'native_page_fn': syms['_ZN12CECMobileHud10NativePageEP9A2DSpriteiii'][0],
            'op_new_fn': syms['_Znwm'][0],
            'tex_ctor_fn': syms['_ZN10A3DTextureC2Ev'][0],
            'tex_load_fn': syms['_ZN10A3DTexture14LoadFromMemoryEP9A3DDevicePhiii9A3DFORMATiP15tagPALETTEENTRY'][0],
            'add_cell_rt_fn': syms['_ZN9CHudAtlas14AddRuntimeCellEiiiiiiiiii'][0],
            'add_round_rect_fn': syms['_ZN12CHudDrawList12AddRoundRectERK7HUDRECTdjjdj'][0],
            'add_cell_fn': syms['_ZN12CHudDrawList7AddCellEiRK7HUDRECTjj'][0],
            'draw_exit': draw_exit,
            'input_orig': input_orig,
            'input_cont': input_cont,
            'mode_ptr_va': mode_ptr_va,
            'action_jump': action_jump,
            'sit_flag_off': sit_flag_off,
            'cmd_sit_down_fn': syms['_ZN13CECHostPlayer10CmdSitDownEb'][0],
            'action_done': action_done,
            'action_obnd': action_obnd,
            'action_odisp': action_orig_disp,
            'fill_cont': fill_cont,
            'cap45_sp_off': cap45_sp_off,
            'cap45_exit': cap45_exit,
            'drop_stay': short_hold_ladder,
            'drop_drop': drop_drop_target,
        },
    }
    return cfg


def _patch_word(buf, at, new_bytes):
    buf[at:at + 4] = new_bytes


def build_auto_profile_and_payload(cfg):
    """Assemble the 4 KiB ARM64 payload & entry VAs directly in pure Python."""
    p = cfg['payload_params']
    code_va = p['code_va']
    # Start with 2411 raw fragment bytes as template (all relocatable words overwritten explicitly below).
    frags_map = {name: bytearray(bytes.fromhex(enc)) for name, _, _, enc in FRAGMENTS}

    # 1. ensure_atlas (base VA = code_va + 0x18c)
    ea = frags_map['ensure_atlas']
    ea_va = code_va + 0x18C
    _patch_word(ea, 0x24, _encode_movz(8, p['atlas_off'] & 0xFFFF))
    _patch_word(ea, 0x28, _encode_movk(8, p['atlas_off'] >> 16))
    _patch_word(ea, 0x30, _encode_movz(8, p['native_cnt_off'] & 0xFFFF))
    _patch_word(ea, 0x34, _encode_movk(8, p['native_cnt_off'] >> 16))
    _patch_word(ea, 0x4C, _encode_movz(10, (p['native_base_off'] + 8) & 0xFFFF))
    _patch_word(ea, 0x50, _encode_movk(10, (p['native_base_off'] + 8) >> 16))
    _patch_word(ea, 0x90, _encode_movz(8, p['native_base_off'] & 0xFFFF))
    _patch_word(ea, 0x94, _encode_movk(8, p['native_base_off'] >> 16))
    _patch_word(ea, 0x138, _encode_bl(ea_va + 0x138, p['native_page_fn']))
    _patch_word(ea, 0x144, _encode_movz(8, p['native_base_off'] & 0xFFFF))
    _patch_word(ea, 0x148, _encode_movk(8, p['native_base_off'] >> 16))
    _patch_word(ea, 0x184, _encode_bl(ea_va + 0x184, p['op_new_fn']))
    _patch_word(ea, 0x190, _encode_bl(ea_va + 0x190, p['tex_ctor_fn']))
    _patch_word(ea, 0x1C0, _encode_bl(ea_va + 0x1C0, p['tex_load_fn']))
    _patch_word(ea, 0x228, _encode_bl(ea_va + 0x228, p['add_cell_rt_fn']))

    # 2. drawing (base VA = code_va + 0x40c)
    dr = frags_map['drawing']
    dr_va = code_va + 0x40C
    _patch_word(dr, 0xB0, _encode_bl(dr_va + 0xB0, p['add_round_rect_fn']))
    _patch_word(dr, 0xDC, _encode_bl(dr_va + 0xDC, p['add_cell_fn']))
    _patch_word(dr, 0xF4, branch_instruction(dr_va + 0xF4, p['draw_exit']))

    # 3. input (base VA = code_va + 0x504)
    inp = frags_map['input']
    inp_va = code_va + 0x504
    _patch_word(inp, 0xD8, branch_instruction(inp_va + 0xD8, p['input_orig']))
    _patch_word(inp, 0xF4, branch_instruction(inp_va + 0xF4, p['input_cont']))

    # 4. action (base VA = code_va + 0x5fc)
    act = frags_map['action']
    act_va = code_va + 0x5FC
    _patch_word(act, 0x08, _encode_adrp(10, act_va + 0x08, p['mode_ptr_va']))
    _patch_word(act, 0x0C, _encode_ldr64_uoff(10, 10, p['mode_ptr_va'] & 0xFFF))
    _patch_word(act, 0x18, branch_instruction(act_va + 0x18, p['action_jump']))
    _patch_word(act, 0x24, _encode_adrp(10, act_va + 0x24, p['mode_ptr_va']))
    _patch_word(act, 0x28, _encode_ldr64_uoff(10, 10, p['mode_ptr_va'] & 0xFFF))
    _patch_word(act, 0x3C, _encode_ldr32_uoff(8, 19, p['sit_flag_off']))
    _patch_word(act, 0x4C, _encode_bl(act_va + 0x4C, p['cmd_sit_down_fn']))
    _patch_word(act, 0x50, branch_instruction(act_va + 0x50, p['action_done']))
    _patch_word(act, 0x5C, branch_instruction(act_va + 0x5C, p['action_obnd']))
    _patch_word(act, 0x60, branch_instruction(act_va + 0x60, p['action_odisp']))

    # 5. fill (base VA = code_va + 0x660)
    fl = frags_map['fill']
    fl_va = code_va + 0x660
    _patch_word(fl, 0x00, _encode_ldrb_uoff(8, 1, p['sit_flag_off']))
    _patch_word(fl, 0x14, branch_instruction(fl_va + 0x14, p['fill_cont']))

    # 6. cap45_ceiling (base VA = code_va + 0x678)
    cap = frags_map['cap45_ceiling']
    cap_va = code_va + 0x678
    _patch_word(cap, 0x10, _encode_stp_w_off(8, 19, 31, p['cap45_sp_off']))
    _patch_word(cap, 0x14, branch_instruction(cap_va + 0x14, p['cap45_exit']))

    # 7. drop_debounce (base VA = code_va + 0x690)
    dd = frags_map['drop_debounce']
    dd_va = code_va + 0x690
    _patch_word(dd, 0x44, branch_instruction(dd_va + 0x44, p['drop_stay']))
    _patch_word(dd, 0x48, branch_instruction(dd_va + 0x48, p['drop_drop']))

    # Assemble 4 KiB code page with NOP fill
    nop = bytes.fromhex('1F2003D5')
    code = bytearray(nop * (CODE_SIZE // 4))
    fragments = []
    for name, offset, src, _ in FRAGMENTS:
        raw = bytes(frags_map[name])
        code[offset:offset + len(raw)] = raw
        fragments.append((name, offset, src, raw.hex()))
    code_bytes = bytes(code)
    arm_profile = {
        'code_va': code_va,
        'label_va': code_va + CODE_SIZE,
        'atlas_png_va': code_va + CODE_SIZE + 0x80,
        'code_sha256': hashlib.sha256(code_bytes).hexdigest(),
        'fragments': tuple(fragments),
        'draw_va': dr_va,
        'input_va': inp_va,
        'action_va': act_va,
        'fill_va': fl_va,
        'cap45_ceiling_va': cap_va,
        'drop_debounce_va': dd_va,
    }
    atlas_bytes = Path(__file__).with_name(ATLAS_PNG_NAME).read_bytes()
    if len(atlas_bytes) != ATLAS_PNG_SIZE or hashlib.sha256(atlas_bytes).hexdigest() != ATLAS_PNG_SHA256:
        raise ValueError('Packed icon atlas PNG failed integrity check')
    when_standing = 'Ng\u1ed3i\0'.encode('utf-32le').ljust(0x20, b'\0')
    when_sitting = '\u0110\u1ee9ng\0'.encode('utf-32le').ljust(0x20, b'\0')
    jump_label = 'Nh\u1ea3y\0'.encode('utf-32le').ljust(0x40, b'\0')
    return arm_profile, code_bytes, when_standing + when_sitting + jump_label + atlas_bytes


def parse_axml_version_code(axml):
    """Extract android:versionCode from compiled AndroidManifest.xml (AXML), or None."""
    if len(axml) < 8 or axml[:4] != b'\x03\x00\x08\x00':
        return None
    pos, res_ids = 8, []
    while pos + 8 <= len(axml):
        ctype, hdr_sz, chunk_sz = struct.unpack_from('<HHI', axml, pos)
        if chunk_sz < 8 or pos + chunk_sz > len(axml):
            break
        if ctype == 0x0180:
            count = max(0, (chunk_sz - hdr_sz) // 4)
            res_ids = list(struct.unpack_from(f'<{count}I', axml, pos + hdr_sz))
        elif ctype == 0x0102 and pos + hdr_sz + 20 <= len(axml):
            _, _, _, _, attr_cnt = struct.unpack_from('<IIHHH', axml, pos + hdr_sz)
            attr_off = pos + hdr_sz + 20
            for idx in range(attr_cnt):
                off = attr_off + idx * 20
                if off + 20 > pos + chunk_sz:
                    break
                _, a_name, _, _, _, a_type, a_data = struct.unpack_from('<IIIHBBI', axml, off)
                if a_name < len(res_ids) and res_ids[a_name] == 0x0101021B and a_type in (0x10, 0x11):
                    return a_data
        pos += chunk_sz
    return None
