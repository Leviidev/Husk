#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""CPU checks of the production-patched paired Jump + Sit/Stand ARM64 ELF.

Supports verified TruTien versionCode 2411 and 2503 libraries.
Original library is supplied locally; no game data is embedded or modified.
Draw APIs and bounded environment primitives are substituted for synthetic
fixtures. These checks do not verify raster output, physics or live server behavior.
"""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from trutien_native_harness import NativeHUD, BASE, LAYOUT, double_bits, rect, overlap

VERIFY_PROFILES = {
    '2411': {
        'restoration_spans': (
            (0x241BA08, 4), (0x241BA48, 4), (0x242EB80, 4), (0x242EB8C, 4), (0x125B0EE, 96),
            (0x23ABE90, 160), (0x1C5A2A0, 112), (0x1C2C6CC, 1048),
            (0x1C22E00, 68), (0x2190CC8, 76), (0x2190D14, 76),
        ),
        'snprintf': 0x2F7AC90,
        'atan2': 0x2F7B810,
        'getenv': 0x2F7AD20,
        'atoi': 0x2F7AD60,
        'log': 0x257A22C,
        'atomic_add': 0x2F76A80,
        'hud_view_combat': 0x242DDFC,
        'hud_input_classify': 0x2411308,
        'hud_input_ctor': 0x2413880,
        'hud_input_down': 0x2413D4C,
        'hud_input_up': 0x2414794,
        'hud_input_pop': 0x24159E0,
        'hud_fill_start': 0x2402038,
        'hud_fill_stop': 0x24020E4,
        'atlas_off': 0x38610,
        'native_base_off': 0x3A980,
        'native_slot0_tex_off': 0x3A998,
        'native_cnt_off': 0x3B288,
        'op_new': 0x256DE64,
        'a3dtex_ctor': 0x27CACB0,
        'a3dtex_load_mem': 0x27CD770,
        'rootptr_va': 0x31ABCC8,
        'mode_ptr_va': 0x31ABCE0,
        'ts_va': 0x32A395C,
        'hud_jump_skill_off': 0x378A4,
        'get_skill': 0x1C430F0,
        'apply_skill': 0x1C4A8FC,
        'cmd_sit_down': 0x1C22E00,
        'post_msg': 0x1BAAEE8,
        'get_water_h': 0x1BFE0F8,
        'get_tick': 0x2579540,
        'run_action_core': 0x23AB80C,
        'run_action': 0x243DB18,
        'profile_all_cap_vas': (0x132662C, 0x1326698, 0x1326704, 0x1326770, 0x13267DC),
        'profile_gfx_auto_vas': (0x1326660, 0x13266CC, 0x1326738, 0x13267A4, 0x1326810),
        'frame_cap_va': 0x31EC188,
        'zero_w2_vas': (0x1B86C98, 0x1B86DD8, 0x1B871B4, 0x1E05350),
        'dynres_vas': (0x1B870D4, 0x27F3D70),
        'perf_hint_va': 0x1E0721C,
        'thread_nice_va': 0x1E07AE8,
        'matrix_simd_va': 0x1E07C20,
        'fallback_rung_va': 0x1B87D4C,
        'nop_vas': (0x1B86BD4, 0x1B875BC, 0x1B89C34),
        'branch_checks': (
            (0x1B889FC, 0x1B88A4C), (0x1B88D88, 0x1B88DAC),
            (0x1B89B00, 0x1B89BB4), (0x1B89BE4, 0x1B89CAC), (0x1E052C0, 0x1E052F4),
        ),
        'adpf_only_vas': (0x1B87748, 0x1B885A4, 0x1B88DD8),
        'perf_hint_target_va': 0x2442F98,
        'startup_set_rate_bl_va': 0x1B889C8,
        'set_frame_cap_va': 0x2442F10,
        'tier_global_va': 0x31ECDC8,
        'get_mobile_profile': 0x1E08170,
        'cap_ini_start': 0x1B86958,
        'cap_ini_stop': 0x1B869F4,
        'startup_ladder_start': 0x1B86A04,
        'startup_ladder_stop': 0x1B86C34,
        'x28_base': 0x31EC000,
        'tiers_va': 0x31EC190,
        'tier_count_va': 0x31EC1A0,
        'live_ladder_start': 0x1B87468,
        'live_ladder_stop': 0x1B875E8,
        'frame_cap_next': 0x1B895E8,
        'vk_start': 0x24C01B4,
        'vk_stop': 0x24C0080,
        'lowmem_crit_start': 0x1B8FFC8,
        'lowmem_crit_stop': 0x1B8FFE0,
        'lowmem_pct_start': 0x1B90060,
        'lowmem_pct_stop': 0x1B90078,
        'rhi_cache_start': 0x1B90198,
        'rhi_cache_stop': 0x1B901C0,
        'ui_opt_start': 0x1D082F4,
        'ui_opt_stop': 0x1D08314,
        'ui_opt_x9_off': 0xCC0,
        'gles_sites': ((0x249CC94, 0x249CC9C, 'w8'), (0x249DE50, 0x249DE58, 'w20')),
        'gles_swap_start': 0x249E1BC,
        'gles_swap_stop': 0x249E1CC,
        'fast_w2_pcs': (
            0x1E07240, 0x1E072F8, 0x1E07340, 0x1E0740C, 0x1E07A38, 0x1E07CD0,
            0x27EF300, 0x27F6260, 0x27F01E0, 0x27F3D70, 0x29651D4, 0x2965230,
        ),
        'upscale_w0_pc': 0x281DAA0,
        'upscale_w21_pc': 0x281DB4C,
        'stack_sampler_pc': 0x2447318,
        'first_tick_start': 0x1B88950,
        'first_tick_stop': 0x1B889C8,
        'first_tick_flag_va': 0x323BE1D,
    },
    '2503': {
        'restoration_spans': (
            (0x2434AD8, 4), (0x2434B18, 4), (0x2447C50, 4), (0x2447C5C, 4), (0x125DBA0, 96),
            (0x23B2120, 160), (0x1C5FBD4, 112), (0x1C32000, 1048),
            (0x1C28734, 68), (0x2196F58, 76), (0x2196FA4, 76),
        ),
        'snprintf': 0x2F9A9C0,
        'atan2': 0x2F9B540,
        'getenv': 0x2F9AA50,
        'atoi': 0x2F9AA90,
        'log': 0x2593490,
        'atomic_add': 0x2F967C0,
        'hud_view_combat': 0x2446ECC,
        'hud_input_classify': 0x242A3B8,
        'hud_input_ctor': 0x242C934,
        'hud_input_down': 0x242CE00,
        'hud_input_up': 0x242D848,
        'hud_input_pop': 0x242EA94,
        'hud_fill_start': 0x241AF78,
        'hud_fill_stop': 0x241B024,
        'atlas_off': 0x38C60,
        'native_base_off': 0x3AFD0,
        'native_slot0_tex_off': 0x3AFE8,
        'native_cnt_off': 0x3B8D8,
        'op_new': 0x25870C8,
        'a3dtex_ctor': 0x27E5598,
        'a3dtex_load_mem': 0x27E8058,
        'rootptr_va': 0x31CBAF8,
        'mode_ptr_va': 0x31CBB10,
        'ts_va': 0x32C38AC,
        'hud_jump_skill_off': 0x37E8C,
        'get_skill': 0x1C48A24,
        'apply_skill': 0x1C50230,
        'cmd_sit_down': 0x1C28734,
        'post_msg': 0x1BB07E8,
        'get_water_h': 0x1C03A18,
        'get_tick': 0x25927A4,
        'run_action_core': 0x23B1A9C,
        'run_action': 0x2456B64,
        'profile_all_cap_vas': (0x1329B5C, 0x1329BC8, 0x1329C34, 0x1329CA0, 0x1329D0C),
        'profile_gfx_auto_vas': (0x1329B90, 0x1329BFC, 0x1329C68, 0x1329CD4, 0x1329D40),
        'frame_cap_va': 0x320BFF0,
        'zero_w2_vas': (0x1B8C598, 0x1B8C6D8, 0x1B8CAB4, 0x1E0AC84),
        'dynres_vas': (0x1B8C9D4, 0x280E6D4),
        'perf_hint_va': 0x1E0CB50,
        'thread_nice_va': 0x1E0D41C,
        'matrix_simd_va': 0x1E0D554,
        'fallback_rung_va': 0x1B8D64C,
        'nop_vas': (0x1B8C4D4, 0x1B8CEBC, 0x1B8F534),
        'branch_checks': (
            (0x1B8E2FC, 0x1B8E34C), (0x1B8E688, 0x1B8E6AC),
            (0x1B8F400, 0x1B8F4B4), (0x1B8F4E4, 0x1B8F5AC), (0x1E0ABF4, 0x1E0AC28),
        ),
        'adpf_only_vas': (0x1B8D048, 0x1B8DEA4, 0x1B8E6D8),
        'perf_hint_target_va': 0x245C1FC,
        'startup_set_rate_bl_va': 0x1B8E2C8,
        'set_frame_cap_va': 0x245C174,
        'tier_global_va': 0x320CC30,
        'get_mobile_profile': 0x1E0DAA4,
        'cap_ini_start': 0x1B8C258,
        'cap_ini_stop': 0x1B8C2F4,
        'startup_ladder_start': 0x1B8C304,
        'startup_ladder_stop': 0x1B8C534,
        'x28_base': 0x320C000,
        'tiers_va': 0x320BFF8,
        'tier_count_va': 0x320C008,
        'live_ladder_start': 0x1B8CD68,
        'live_ladder_stop': 0x1B8CEE8,
        'frame_cap_next': 0x1B8EEE8,
        'vk_start': 0x24D9418,
        'vk_stop': 0x24D92E4,
        'lowmem_crit_start': 0x1B958C8,
        'lowmem_crit_stop': 0x1B958E0,
        'lowmem_pct_start': 0x1B95960,
        'lowmem_pct_stop': 0x1B95978,
        'rhi_cache_start': 0x1B95A98,
        'rhi_cache_stop': 0x1B95AC0,
        'ui_opt_start': 0x1D0DC28,
        'ui_opt_stop': 0x1D0DC48,
        'ui_opt_x9_off': 0xAF0,
        'gles_sites': ((0x24B5EF8, 0x24B5F00, 'w8'), (0x24B70B4, 0x24B70BC, 'w20')),
        'gles_swap_start': 0x24B7420,
        'gles_swap_stop': 0x24B7430,
        'fast_w2_pcs': (
            0x1E0CB74, 0x1E0CC2C, 0x1E0CC74, 0x1E0CD40, 0x1E0D36C, 0x1E0D604,
            0x2809C84, 0x2810BB4, 0x280AB84, 0x280E6D4, 0x298272C, 0x2982788,
        ),
        'upscale_w0_pc': 0x283AF74,
        'upscale_w21_pc': 0x283B020,
        'stack_sampler_pc': 0x246057C,
        'first_tick_start': 0x1B8E250,
        'first_tick_stop': 0x1B8E2C8,
        'first_tick_flag_va': 0x325BC9D,
    },
    '2685': {
        'restoration_spans': (
            (0x2457D84, 4), (0x2457DC4, 4), (0x246AEFC, 4), (0x246AF08, 4), (0x1262AEC, 96),
            (0x23C85D0, 160), (0x1C6B0D8, 112), (0x1C3D2E0, 1048),
            (0x1C339E4, 68), (0x21AD448, 76), (0x21AD494, 76),
        ),
        'snprintf': 0x2FC1030,
        'atan2': 0x2FC1C20,
        'getenv': 0x2FC10C0,
        'atoi': 0x2FC1100,
        'log': 0x25B6D74,
        'atomic_add': 0x2FBCE30,
        'hud_view_combat': 0x246A178,
        'hud_input_classify': 0x244D5C4,
        'hud_input_ctor': 0x244FB40,
        'hud_input_down': 0x2450058,
        'hud_input_up': 0x2450AB0,
        'hud_input_pop': 0x2451CFC,
        'hud_fill_start': 0x243E184,
        'hud_fill_stop': 0x243E230,
        'atlas_off': 0x394C8,
        'native_base_off': 0x3B838,
        'native_slot0_tex_off': 0x3B850,
        'native_cnt_off': 0x3C140,
        'op_new': 0x25AA9AC,
        'a3dtex_ctor': 0x2809538,
        'a3dtex_load_mem': 0x280BFF8,
        'rootptr_va': 0x31F3BD0,
        'mode_ptr_va': 0x31F3BE8,
        'ts_va': 0x32EC054,
        'hud_jump_skill_off': 0x3810C,
        'hud_dlg_keys_off': 0x7C,
        'host_ready_off': 0x1174,
        'host_blocked_off': 0x1C44,
        'host_flags_off': 0x9FC,
        'host_water_off': 0x9B0,
        'host_water_z_off': 0x9C4,
        'get_skill': 0x1C53DC0,
        'apply_skill': 0x1C5B5E4,
        'cmd_sit_down': 0x1C339E4,
        'post_msg': 0x1BBAD14,
        'get_water_h': 0x1C0E930,
        'get_tick': 0x25B6088,
        'run_action_core': 0x23C7F4C,
        'run_action': 0x247A074,
        'profile_all_cap_vas': (0x1330884, 0x13308F0, 0x133095C, 0x13309C8, 0x1330A34),
        'profile_gfx_auto_vas': (0x13308B8, 0x1330924, 0x1330990, 0x13309FC, 0x1330A68),
        'frame_cap_va': 0x3234100,
        'zero_w2_vas': (0x1B9699C, 0x1B96AF4, 0x1B96EE8, 0x1E174A8),
        'dynres_vas': (0x1B96E00, 0x2832DD4),
        'perf_hint_va': 0x1E1947C,
        'thread_nice_va': 0x1E19D50,
        'matrix_simd_va': 0x1E19E88,
        'fallback_rung_va': 0x1B97A78,
        'nop_vas': (0x1B967E4, 0x1B972F0, 0x1B99A48),
        'branch_checks': (
            (0x1B987C8, 0x1B98818), (0x1B98B54, 0x1B98B78),
            (0x1B99914, 0x1B999C8), (0x1B999F8, 0x1B99AC0),
            (0x1E16D8C, 0x1E16DA0), (0x1E1A450, 0x1E1A470),
        ),
        'adpf_only_vas': (0x1B9747C, 0x1B982DC, 0x1B98BA4),
        'perf_hint_target_va': 0x247F644,
        'startup_set_rate_bl_va': 0x1B985D8,
        'set_frame_cap_va': 0x247F5BC,
        'tier_global_va': 0x3234D40,
        'get_mobile_profile': 0x1E1A414,
        'cap_ini_start': 0x1B96568,
        'cap_ini_stop': 0x1B96604,
        'startup_ladder_start': 0x1B96614,
        'startup_ladder_stop': 0x1B96844,
        'x28_base': 0x3234000,
        'tiers_va': 0x3234108,
        'tier_count_va': 0x3234118,
        'live_ladder_start': 0x1B9719C,
        'live_ladder_stop': 0x1B9731C,
        'frame_cap_next': 0x1B993FC,
        'vk_start': 0x24FCCFC,
        'vk_stop': 0x24FCBC8,
        'lowmem_crit_start': 0x1B9FDF4,
        'lowmem_crit_stop': 0x1B9FE0C,
        'lowmem_pct_start': 0x1B9FE8C,
        'lowmem_pct_stop': 0x1B9FEA4,
        'rhi_cache_start': 0x1B9FFC4,
        'rhi_cache_stop': 0x1B9FFEC,
        'ui_opt_start': 0x1D19824,
        'ui_opt_stop': 0x1D19844,
        'ui_opt_x9_off': 0xBC8,
        'gles_sites': ((0x24D97DC, 0x24D97E4, 'w8'), (0x24DA998, 0x24DA9A0, 'w20')),
        'gles_swap_start': 0x24DAD04,
        'gles_swap_stop': 0x24DAD14,
        'fast_w2_pcs': (
            0x1E194A0, 0x1E19554, 0x1E1959C, 0x1E19680, 0x1E19C9C, 0x1E19F38,
            0x282DD60, 0x283538C, 0x282EC60, 0x2832DD4, 0x29A8228, 0x29A8284,
        ),
        'upscale_w0_pc': 0x285F888,
        'upscale_w21_pc': 0x285F934,
        'stack_sampler_pc': 0x2483AD0,
        'first_tick_start': 0x1B9871C,
        'first_tick_stop': 0x1B98794,
        'first_tick_flag_va': 0x3283DE5,
    },
}


def virtual_bytes(binary, address, size):
    from elftools.elf.elffile import ELFFile
    elf = ELFFile(io.BytesIO(binary))
    segments = [s for s in elf.iter_segments() if s['p_type'] == 'PT_LOAD' and
                s['p_vaddr'] <= address and address + size <= s['p_vaddr'] + s['p_filesz']]
    assert len(segments) == 1
    at = segments[0]['p_offset'] + address - segments[0]['p_vaddr']
    return binary[at:at + size]


def verify_restoration(original, patched, vp):
    for address, size in vp['restoration_spans']:
        assert virtual_bytes(original, address, size) == virtual_bytes(patched, address, size)
    return {'nativeJumpWaterPhysicsAndCmdSitDownUnchanged': True}


SCRATCH = 0x51000000
VIEW = SCRATCH
DRAWLIST = SCRATCH + 0x1000
DATA = SCRATCH + 0x2000
STATE = SCRATCH + 0x12000
STRINGS = SCRATCH + 0x13000
ICONS = SCRATCH + 0x15000
TEXT = SCRATCH + 0x17000


def utf32(uc, pointer):
    out = []
    if not pointer:
        return ''
    for at in range(100):
        value = struct.unpack('<I', bytes(uc.mem_read(pointer + at * 4, 4)))[0]
        if value == 0:
            return ''.join(out)
        out.append(chr(value))
    raise AssertionError('Unterminated captured text')


class DrawProbe(NativeHUD):
    def __init__(self, data, vp):
        from elftools.elf.elffile import ELFFile
        from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
        super().__init__(data)
        self.vp = vp
        self.calls = []
        self.uc.mem_map(SCRATCH, 0x20000)
        elf = ELFFile(io.BytesIO(data))
        self.api = {s['st_value']: s.name for s in elf.get_section_by_name('.dynsym').iter_symbols()
                    if s['st_value'] and s.name.startswith('_ZN') and 'CHudDrawList' in s.name}
        for address in self.api:
            self.uc.hook_add(UC_HOOK_CODE, self.capture, begin=address, end=address)
        self.uc.hook_add(UC_HOOK_CODE, self.snprintf, begin=vp['snprintf'], end=vp['snprintf'])
        self.uc.hook_add(UC_HOOK_CODE, self.atan2, begin=vp['atan2'], end=vp['atan2'])
        self.uc.hook_add(UC_HOOK_MEM_INVALID, self.invalid)

    def invalid(self, uc, access, address, size, value, _):
        raise AssertionError('Unmapped access PC=%x address=%x size=%d' %
                             (uc.reg_read(self.reg.UC_ARM64_REG_PC), address, size))

    def snprintf(self, uc, address, size, _):
        reg = self.reg
        dst, count, fmt, value = [uc.reg_read(reg.UC_ARM64_REG_X0 + i) for i in range(4)]
        raw = bytes(uc.mem_read(fmt, 64)).split(b'\0')[0]
        assert raw == b'%d', ('Unexpected libc formatting fixture', raw)
        value = value & 0xFFFFFFFF
        if value & 0x80000000:
            value -= 0x100000000
        output = str(value).encode('ascii')
        if count:
            assert count <= 0x1000
            uc.mem_write(dst, output[:count - 1] + b'\0')
        uc.reg_write(reg.UC_ARM64_REG_X0, len(output))
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def atan2(self, uc, address, size, _):
        reg = self.reg
        values = [struct.unpack('<d', struct.pack('<Q', uc.reg_read(register)))[0]
                  for register in (reg.UC_ARM64_REG_D0, reg.UC_ARM64_REG_D1)]
        uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(math.atan2(*values)))
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def capture(self, uc, address, size, _):
        reg = self.reg
        name = self.api[address]
        args = [uc.reg_read(reg.UC_ARM64_REG_X0 + i) for i in range(8)]
        dargs = [struct.unpack('<d', struct.pack('<Q', uc.reg_read(reg.UC_ARM64_REG_D0 + i)))[0]
                 for i in range(2)]
        entry = {'method': name, 'callerReturn': hex(uc.reg_read(reg.UC_ARM64_REG_LR))}
        if 'AddText' in name:
            entry['text'] = utf32(uc, args[1])
            entry['rect'] = struct.unpack('<4d', bytes(uc.mem_read(args[2], 32)))
            entry['fontSize'] = dargs[0]
            entry['color'] = hex(args[3] & 0xFFFFFFFF)
            entry['align'] = [args[4] & 0xFFFFFFFF, args[5] & 0xFFFFFFFF, args[6] & 0xFFFFFFFF]
        elif 'AddRoundRect' in name or 'AddRing' in name:
            entry['rect'] = struct.unpack('<4d', bytes(uc.mem_read(args[1], 32)))
            entry['radius'] = dargs[0]
            entry['strokeWidth'] = dargs[1]
            entry['fill'] = hex(args[2] & 0xFFFFFFFF)
            entry['border'] = hex(args[3] & 0xFFFFFFFF)
            entry['fillFlag'] = args[4] & 0xFFFFFFFF
        elif 'AddPolyLineAA' in name:
            count = args[2] & 0xFFFFFFFF
            entry['count'] = count
            entry['width'] = dargs[0]
            entry['color'] = hex(args[3] & 0xFFFFFFFF)
            entry['points'] = [struct.unpack('<2d', bytes(uc.mem_read(args[1] + i * 16, 16)))
                               for i in range(min(count, 16))]
        elif 'AddCell' in name:
            entry['cellId'] = args[1] & 0xFFFFFFFF
            entry['rect'] = struct.unpack('<4d', bytes(uc.mem_read(args[2], 32)))
            entry['tint'] = hex(args[3] & 0xFFFFFFFF)
            entry['flags'] = args[4] & 0xFFFFFFFF
        if 'MeasureText' in name:
            uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(16))
            uc.reg_write(reg.UC_ARM64_REG_D1, double_bits(12))
        elif 'GetBorderW' in name:
            uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(1))
        else:
            uc.reg_write(reg.UC_ARM64_REG_X0, 0)
        self.calls.append(entry)
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def draw(self, auto_hide, hide=None, v11=True, sitting=False):
        self.layout(932, 430, 32, 1, int(v11))
        self.uc.mem_write(SCRATCH, bytes(0x20000))
        self.uc.mem_write(VIEW, struct.pack('<6Q', DRAWLIST, LAYOUT, DATA, STATE, STRINGS, ICONS))
        self.uc.mem_write(STATE, struct.pack('<I', 2))
        self.uc.mem_write(STATE + 0xA8, struct.pack('<d', auto_hide))
        self.uc.mem_write(DATA + 0x8AB2, bytes([int(bool(sitting))]))
        self.uc.mem_write(LAYOUT + 0x42, bytes([int(v11)]))
        for at, value in ((0x3E20, 1.), (0x3E28, 1.), (0x4030, 12.),
                          (0x4220, 1.), (0x4228, 12.), (0x4230, 12.)):
            self.uc.mem_write(LAYOUT + at, struct.pack('<d', value))
        self.uc.mem_write(TEXT, 'Fixture\0'.encode('utf-32le'))
        self.uc.mem_write(STRINGS, struct.pack('<Q', TEXT) * 256)
        self.uc.mem_write(ICONS, b'\xff' * 0x2000)
        if hide == 'state19':
            self.uc.mem_write(STATE + 0x19, b'\1')
        elif hide == 'config3a':
            self.uc.mem_write(LAYOUT + 0x41, b'\1')
            self.uc.mem_write(LAYOUT + 0x3A, b'\1')
        elif hide == 'state0b':
            self.uc.mem_write(STATE + 0xB, b'\1')
        elif hide in ('mode2', 'mode4'):
            self.uc.mem_write(STATE + 0xB0, struct.pack('<I', int(hide[-1])))
        self.calls = []
        self.call(self.vp['hud_view_combat'], [VIEW])
        return list(self.calls)


def verify_draw(original, modified, vp):
    baseline, patched = DrawProbe(original, vp), DrawProbe(modified, vp)
    cases = []
    for auto_hide in (0., .5, 1.):
        for hide in (None, 'state19', 'config3a', 'state0b', 'mode2', 'mode4'):
            for v11 in (False, True):
                before = baseline.draw(auto_hide, hide, v11, sitting=False)
                for sitting in (False, True):
                    after = patched.draw(auto_hide, hide, v11, sitting=sitting)
                    assert after[:len(before)] == before
                    extra = after[len(before):]
                    if hide is None and v11:
                        assert len(extra) == 2 and all('AddRoundRect' in e['method'] for e in extra), extra
                        disc_s, disc_j = extra
                        sbx, sby, sbs, sbh = disc_s['rect']
                        jbx, jby, jbs, jbh = disc_j['rect']
                        assert sbs == sbh and sbs > 0 and abs(jby - sby) < 1e-6 and abs(jbs - sbs) < 1e-6
                        assert jbx > sbx + sbs
                        if sitting:
                            assert disc_s['fill'] == '0x9c24190c' and disc_s['border'] == '0xffe8ca8a'
                        else:
                            assert disc_s['fill'] == '0x7e0a1018' and disc_s['border'] == '0x70dce8f8'
                        assert disc_j['fill'] == '0x7e0a1018' and disc_j['border'] == '0x70dce8f8'
                        cases.append({'autoHide': auto_hide, 'sitting': sitting,
                                      'sitFill': disc_s['fill'], 'sitBorder': disc_s['border']})
                    else:
                        assert not extra and after == before
    return {'result': 'PASS', 'fallbackDiscsVerified': True, 'cases': cases}


def verify_atlas_draw(original, modified, payload, version, vp):
    from unicorn import UC_HOOK_CODE
    arm = payload.PROFILES[version]
    probe = DrawProbe(modified, vp)
    reg = probe.reg
    hud_base = 0x52000000
    probe.uc.mem_map(hud_base, 0x60000)
    atlas_ptr = hud_base + vp['atlas_off']
    cells_buf = hud_base + 0x40000
    hash_buf = hud_base + 0x48000
    dev_ptr = hud_base + 0x4A000
    d3ddev_ptr = hud_base + 0x4B000
    heap_ptr = hud_base + 0x4C000
    vtable_ptr = hud_base + 0x4E000
    dtor_stub = hud_base + 0x4F000

    probe.uc.mem_write(hud_base + 0x30, struct.pack('<Q', dev_ptr))
    probe.uc.mem_write(dev_ptr + 0x38, struct.pack('<Q', d3ddev_ptr))
    probe.uc.mem_write(hud_base + vp['native_base_off'], struct.pack('<I', 5))
    probe.uc.mem_write(atlas_ptr + 0x18C0, struct.pack('<I', 5))
    probe.uc.mem_write(atlas_ptr + 0x18D0, struct.pack('<II', 40, 809))
    probe.uc.mem_write(atlas_ptr + 0x18C8, struct.pack('<Q', cells_buf))
    probe.uc.mem_write(atlas_ptr + 0x18E0, struct.pack('<Q', hash_buf))
    probe.uc.mem_write(dtor_stub, bytes.fromhex('c0035fd6'))
    probe.uc.mem_write(vtable_ptr + 8, struct.pack('<Q', dtor_stub))

    load_should_succeed = [False]
    events = []
    _, rodata = payload.build_payload(version)
    expected_png = rodata[0x80:]

    def hook_alloc(uc, address, size, _):
        sz = uc.reg_read(reg.UC_ARM64_REG_X0)
        assert sz == 0x90
        events.append(('alloc', sz))
        uc.reg_write(reg.UC_ARM64_REG_X0, heap_ptr)
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def hook_ctor(uc, address, size, _):
        tex = uc.reg_read(reg.UC_ARM64_REG_X0)
        uc.mem_write(tex, struct.pack('<Q', vtable_ptr))
        events.append(('ctor', tex == heap_ptr))
        uc.reg_write(reg.UC_ARM64_REG_X0, tex)
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def hook_load_mem(uc, address, size, _):
        x = [uc.reg_read(reg.UC_ARM64_REG_X0 + i) for i in range(8)]
        pal = struct.unpack('<Q', bytes(uc.mem_read(uc.reg_read(reg.UC_ARM64_REG_SP), 8)))[0]
        assert x[0] == heap_ptr and x[1] == dev_ptr
        assert x[2] == arm['atlas_png_va'] and (x[3] & 0xFFFFFFFF) == payload.ATLAS_PNG_SIZE
        assert bytes(uc.mem_read(x[2], x[3] & 0xFFFFFFFF)) == expected_png
        assert (x[4] & 0xFFFFFFFF, x[5] & 0xFFFFFFFF, x[6] & 0xFFFFFFFF, x[7] & 0xFFFFFFFF, pal) == (0, 0, 0, 1, 0)
        events.append(('loadFromMemory', load_should_succeed[0]))
        uc.reg_write(reg.UC_ARM64_REG_X0, 1 if load_should_succeed[0] else 0)
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def hook_dtor(uc, address, size, _):
        events.append(('dtor', uc.reg_read(reg.UC_ARM64_REG_X0) == heap_ptr))

    for addr, fn in ((vp['op_new'], hook_alloc), (vp['a3dtex_ctor'], hook_ctor),
                     (vp['a3dtex_load_mem'], hook_load_mem), (dtor_stub, hook_dtor)):
        probe.uc.hook_add(UC_HOOK_CODE, fn, begin=addr, end=addr)

    base_probe = DrawProbe(original, vp)

    def run_frame(sitting):
        before = base_probe.draw(0.0, None, True, sitting=sitting)
        probe.layout(932, 430, 32, 1, 1)
        probe.uc.mem_write(SCRATCH, bytes(0x20000))
        probe.uc.mem_write(VIEW, struct.pack('<6Q', DRAWLIST, LAYOUT, DATA, STATE, STRINGS, ICONS))
        probe.uc.mem_write(DRAWLIST + 0x30, struct.pack('<Q', atlas_ptr))
        probe.uc.mem_write(STATE, struct.pack('<I', 2))
        probe.uc.mem_write(STATE + 0xA8, struct.pack('<d', 0.0))
        probe.uc.mem_write(DATA + 0x8AB2, bytes([int(sitting)]))
        probe.uc.mem_write(LAYOUT + 0x42, b'\1')
        for at, val in ((0x3E20, 1.), (0x3E28, 1.), (0x4030, 12.), (0x4220, 1.), (0x4228, 12.), (0x4230, 12.)):
            probe.uc.mem_write(LAYOUT + at, struct.pack('<d', val))
        probe.uc.mem_write(TEXT, 'Fixture\0'.encode('utf-32le'))
        probe.uc.mem_write(STRINGS, struct.pack('<Q', TEXT) * 256)
        probe.uc.mem_write(ICONS, b'\xff' * 0x2000)
        probe.calls = []
        probe.call(vp['hud_view_combat'], [VIEW])
        assert probe.calls[:len(before)] == before
        return probe.calls[len(before):]

    tail_fail = run_frame(sitting=False)
    assert events == [('alloc', 0x90), ('ctor', True), ('loadFromMemory', False), ('dtor', True)]
    assert struct.unpack('<Q', bytes(probe.uc.mem_read(hud_base + vp['native_slot0_tex_off'], 8)))[0] == 0
    assert len(tail_fail) == 2 and all('AddRoundRect' in c['method'] for c in tail_fail)

    events.clear()
    load_should_succeed[0] = True
    tail_stand = run_frame(sitting=False)
    assert events == [('alloc', 0x90), ('ctor', True), ('loadFromMemory', True)]
    assert struct.unpack('<I', bytes(probe.uc.mem_read(hud_base + vp['native_cnt_off'], 4)))[0] == 1
    assert struct.unpack('<Q', bytes(probe.uc.mem_read(hud_base + vp['native_slot0_tex_off'], 8)))[0] == heap_ptr
    assert struct.unpack('<I', bytes(probe.uc.mem_read(atlas_ptr + 0x18D0, 4)))[0] == 43
    expected_cells = ((5, 4, 4, 248, 248, 0), (5, 260, 4, 248, 248, 0), (5, 4, 260, 248, 248, 0))
    for i, exp in enumerate(expected_cells):
        raw = bytes(probe.uc.mem_read(cells_buf + (40 + i) * 0x14, 0x14))
        got = struct.unpack('<5h', raw[:10]) + (struct.unpack('<h', raw[0x12:0x14])[0],)
        assert got == exp, (i, got, exp)
    assert len(tail_stand) == 4
    d_sit, c_sit, d_jmp, c_jmp = tail_stand
    assert 'AddRoundRect' in d_sit['method'] and d_sit['fill'] == '0x7e0a1018'
    assert 'AddCell' in c_sit['method'] and c_sit['cellId'] == 40 and c_sit['tint'] == '0xffffffff'
    assert 'AddRoundRect' in d_jmp['method'] and d_jmp['fill'] == '0x7e0a1018'
    assert 'AddCell' in c_jmp['method'] and c_jmp['cellId'] == 42 and c_jmp['tint'] == '0xffffffff'

    events.clear()
    tail_sit = run_frame(sitting=True)
    assert events == []
    d_sit2, c_sit2, d_jmp2, c_jmp2 = tail_sit
    assert d_sit2['fill'] == '0x9c24190c' and d_sit2['border'] == '0xffe8ca8a'
    assert c_sit2['cellId'] == 41 and c_sit2['tint'] == '0xffffffff'
    assert c_jmp2['cellId'] == 42 and c_jmp2['tint'] == '0xffffffff'
    return {'result': 'PASS', 'atlasPageAllocated': 5, 'cellsRegistered': [40, 41, 42],
            'standingLeftCell': c_sit['cellId'], 'sittingLeftCell': c_sit2['cellId'],
            'jumpCell': c_jmp['cellId'], 'cachedZeroRealloc': True}


def verify_geometry(original, modified, code_va):
    old, new = NativeHUD(original), NativeHUD(modified)
    cases = []
    for width, height in ((932, 430), (1400, 650), (2796, 1290)):
        for unit in (24, 32, 40):
            for scale in (0.8, 1, 1.5):
                before_i, before = old.layout(width, height, unit, scale, 1)
                after_i, after = new.layout(width, height, unit, scale, 1)
                assert (before_i, before) == (after_i, after)
                new.uc.mem_write(LAYOUT + 0x42, b'\1')
                preserved = bytes(new.uc.mem_read(LAYOUT, 0x10000))
                output = BASE + 0x70000
                assert new.call(code_va, [LAYOUT, output]) == 1
                sit_btn = rect(bytes(new.uc.mem_read(output, 32)), 0)
                jump_btn = rect(bytes(new.uc.mem_read(output + 32, 32)), 0)
                ax, _, aw, ah = rect(before, 0x1B10)
                atk_cx = ax + 0.5 * aw
                assert abs(sit_btn[2] - max(aw, ah) * 0.625) < 1e-6
                assert not overlap(sit_btn, jump_btn)
                min_edge_x = max(sit_btn[2], width * 0.03125)
                min_edge_y = max(sit_btn[3], height * 0.0625)
                for btn in (sit_btn, jump_btn):
                    assert min_edge_x - 1e-6 <= btn[0] and btn[0] + btn[2] <= width - min_edge_x + 1e-6
                    assert min_edge_y - 1e-6 <= btn[1] and btn[1] + btn[3] <= height - min_edge_y + 1e-6
                    assert btn[2] == btn[3] and btn[2] > 0
                    for control in range(58):
                        at = 0x14F8 + control * 0x68
                        if before[at + 0x60]:
                            assert not overlap(btn, rect(before, at))
                assert abs(sit_btn[1] - jump_btn[1]) < 1e-6 and abs(sit_btn[2] - jump_btn[2]) < 1e-6
                gap = jump_btn[0] - (sit_btn[0] + sit_btn[2])
                assert abs(gap - sit_btn[2] * 0.375) < 1e-6
                pair_mid = (sit_btn[0] + jump_btn[0] + jump_btn[2]) * 0.5
                assert abs(pair_mid - atk_cx) < 1e-6
                assert bytes(new.uc.mem_read(LAYOUT, 0x10000)) == preserved
                cases.append({'screen': [width, height], 'unit': unit, 'scale': scale,
                              'sitButton': sit_btn, 'jumpButton': jump_btn})

    new.layout(932, 430, 32, 1, 1)
    new.uc.mem_write(LAYOUT + 0x42, b'\1')
    new.uc.mem_write(LAYOUT + 0x1B10, struct.pack('<4d', 900.0, 390.0, 27.2, 27.2))
    out_edge = BASE + 0x70000
    assert new.call(code_va, [LAYOUT, out_edge]) == 1
    esit = rect(bytes(new.uc.mem_read(out_edge, 32)), 0)
    ejump = rect(bytes(new.uc.mem_read(out_edge + 32, 32)), 0)
    assert ejump[0] + ejump[2] <= 932.0 - max(esit[2], 932.0 * 0.03125) + 1e-6

    return {'cases': cases, 'existingLayoutUnchanged': True, 'symmetricalCenteredAboveWheel': True,
            'scaleRatioToAttack': 0.625, 'bezelSafeMarginVerified': True,
            'noOverlapWithOtherActiveNativeControlRects': True}


TOUCH, TOUCH_DATA, TOUCH_UI, HIT = (BASE + at for at in (0x50000, 0x60000, 0x70000, 0x78000))


def classify(binary, vp, code_va=None, point=None, gates=(), buttons=None, which=0, auto_hide=0., density=1.):
    runner = NativeHUD(binary)
    reg = runner.reg
    runner.layout(932, 430, 32, 1, 1)
    runner.uc.mem_write(LAYOUT + 0x42, b'\1')
    runner.uc.mem_write(TOUCH, struct.pack('<QQQd', LAYOUT, TOUCH_DATA, TOUCH_UI, density))
    runner.uc.mem_write(TOUCH_UI + 0xA8, struct.pack('<d', auto_hide))
    if buttons is None:
        assert runner.call(code_va, [LAYOUT, HIT]) == 1
        buttons = (struct.unpack('<4d', bytes(runner.uc.mem_read(HIT, 32))),
                   struct.unpack('<4d', bytes(runner.uc.mem_read(HIT + 32, 32))))
    btn = buttons[which]
    if point is None:
        point = btn[0] + btn[2] / 2, btn[1] + btn[3] / 2
    for address, value in gates:
        runner.uc.mem_write(address, value)
    runner.uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(point[0] * density))
    runner.uc.reg_write(reg.UC_ARM64_REG_D1, double_bits(point[1] * density))
    runner.call(vp['hud_input_classify'], [TOUCH, HIT])
    return bytes(runner.uc.mem_read(HIT, 0x118)), buttons


def verify_result_queue(binary, vp, classified, button, release, expected_action, cancel=False, density=1.):
    from unicorn import UC_HOOK_CODE
    runner = NativeHUD(binary)
    reg = runner.reg
    object_at, event, output, callback = (BASE + at for at in (0x10000, 0x20000, 0x30000, 0x7FFE0))
    runner.uc.mem_write(callback, bytes.fromhex('c0035fd6'))

    def external(uc, address, size, _):
        if address == callback:
            uc.mem_write(uc.reg_read(reg.UC_ARM64_REG_X1), classified)
        elif address != vp['atomic_add']:
            return
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    for address in (callback, vp['atomic_add']):
        runner.uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)
    runner.call(vp['hud_input_ctor'], [object_at])
    runner.uc.mem_write(event + 8, struct.pack('<Q', 42))
    runner.uc.mem_write(event + 0x10, struct.pack('<2d', (button[0] + button[2] / 2) * density, (button[1] + button[3] / 2) * density))
    runner.uc.mem_write(event + 0x20, struct.pack('<I', 1000))
    runner.call(vp['hud_input_down'], [object_at, event, callback, 0])
    assert runner.call(vp['hud_input_pop'], [object_at, output]) == 0
    runner.uc.mem_write(event + 0x10, struct.pack('<2d', release[0] * density, release[1] * density))
    runner.uc.mem_write(event + 0x20, struct.pack('<I', 1100))
    runner.call(vp['hud_input_up'], [object_at, event, int(cancel)])
    got = runner.call(vp['hud_input_pop'], [object_at, output])
    if got:
        assert struct.unpack('<5i', bytes(runner.uc.mem_read(output, 20))) == (expected_action, 0, 0, 0, 0)
    assert runner.call(vp['hud_input_pop'], [object_at, output]) == 0
    return bool(got)


def verify_input(original, modified, code_va, vp):
    cls_sit, buttons = classify(modified, vp, code_va, which=0)
    cls_jump, _ = classify(modified, vp, code_va, which=1)
    sit_btn, jump_btn = buttons
    for btn, cls, act in ((sit_btn, cls_sit, 75), (jump_btn, cls_jump, 74)):
        center = btn[0] + btn[2] / 2, btn[1] + btn[3] / 2
        radius = btn[2] / 2
        before, _ = classify(original, vp, point=center, buttons=buttons)
        assert struct.unpack_from('<4i', cls) == (4, 1, act, act)
        assert struct.unpack_from('<5i', cls, 0xC) == (act, 0, 0, 0, 0)
        assert struct.unpack_from('<i', before, 0xC)[0] not in (74, 75)
        for inside in ((center[0] + radius * 0.45, center[1]), (center[0], center[1] - radius * 0.45)):
            hit, _ = classify(modified, vp, code_va, point=inside)
            assert struct.unpack_from('<4i', hit) == (4, 1, act, act)
        assert [verify_result_queue(modified, vp, cls, btn, center, act),
                verify_result_queue(modified, vp, cls, btn, (btn[0] + btn[2] + 5, center[1]), act),
                verify_result_queue(modified, vp, cls, btn, center, act, cancel=True)] == [True, False, False]

    mid_gap = ((sit_btn[0] + sit_btn[2] + jump_btn[0]) * 0.5, sit_btn[1] + sit_btn[2] * 0.5)
    b_gap, _ = classify(original, vp, point=mid_gap, buttons=buttons)
    a_gap, _ = classify(modified, vp, code_va, point=mid_gap)
    assert b_gap == a_gap and struct.unpack_from('<i', a_gap, 0xC)[0] not in (74, 75)

    gates = {
        'global UI hide': [(TOUCH_UI + 0x19, b'\1')],
        'legacy HUD': [(LAYOUT + 0x42, b'\0')],
        'UI hide flag': [(TOUCH_UI + 0xB, b'\1')],
        'layout hide flag': [(LAYOUT + 0x41, b'\1'), (LAYOUT + 0x3A, b'\1')],
        'dialog mode 2': [(TOUCH_UI + 0xB0, struct.pack('<I', 2))],
        'dialog mode 4': [(TOUCH_UI + 0xB0, struct.pack('<I', 4))],
    }
    for which in (0, 1):
        btn = buttons[which]
        center = btn[0] + btn[2] / 2, btn[1] + btn[3] / 2
        for name, gate in gates.items():
            before_g, _ = classify(original, vp, point=center, gates=gate, buttons=buttons)
            after_g, _ = classify(modified, vp, code_va, gates=gate, which=which)
            assert before_g == after_g, (which, name)
    return {'sitAction': 75, 'jumpAction': 74, 'circularHitTestEnforced': True, 'gapIsolated': True, 'gatesPreserved': list(gates)}


def dispatch(binary, vp, action, mode=0, flags_9ec=0, blocked=False, player=True, ready=True, water=False, timestamp=0, now=1000):
    from unicorn import UC_HOOK_CODE
    runner = NativeHUD(binary)
    reg = runner.reg
    hud, request = BASE + 0x10000, BASE + 0x60000
    rootptr, game, run, host, mode_at, skill = (BASE + at for at in (0x65000, 0x66000, 0x67000, 0x68000, 0x6B000, 0x6C000))
    for address, value in ((vp['rootptr_va'], rootptr), (rootptr, game), (game + 0x38, run),
                            (run + 0x60, host if player else 0), (vp['mode_ptr_va'], mode_at)):
        runner.uc.mem_write(address, struct.pack('<Q', value))
    runner.uc.mem_write(mode_at + 0x40, struct.pack('<I', mode))
    runner.uc.mem_write(host + vp.get('host_ready_off', 0x115C), bytes([ready]))
    runner.uc.mem_write(host + vp.get('host_blocked_off', 0x1C24), bytes([blocked]))
    runner.uc.mem_write(host + vp.get('host_flags_off', 0x9EC), struct.pack('<I', flags_9ec))
    runner.uc.mem_write(host + vp.get('host_water_off', 0x9A0), struct.pack('<I', int(water)))
    runner.uc.mem_write(host + vp.get('host_water_z_off', 0x9B4), struct.pack('<f', 10.))
    runner.uc.mem_write(vp['ts_va'], struct.pack('<I', timestamp))
    runner.uc.mem_write(hud + vp['hud_jump_skill_off'], struct.pack('<I', 999))
    runner.uc.mem_write(request, struct.pack('<5I', action, 0, 0, 0, 0))
    calls = []

    def external(uc, address, size, _):
        if address == vp['get_skill']:
            calls.append(['GetSkill', uc.reg_read(reg.UC_ARM64_REG_X1)])
            uc.reg_write(reg.UC_ARM64_REG_X0, skill)
        elif address == vp['apply_skill']:
            calls.append(['ApplySkillShortcut', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == vp['cmd_sit_down']:
            calls.append(['CmdSitDown', uc.reg_read(reg.UC_ARM64_REG_X0) == host, uc.reg_read(reg.UC_ARM64_REG_X1) & 1])
            uc.reg_write(reg.UC_ARM64_REG_X0, 1)
        elif address == vp['post_msg']:
            calls.append(['PostMessageW', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == vp['get_water_h']:
            uc.reg_write(reg.UC_ARM64_REG_S0, 0)
        elif address == vp['get_tick']:
            uc.reg_write(reg.UC_ARM64_REG_X0, now)
        else:
            raise AssertionError('Unexpected hook ' + hex(address))
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    for address in (vp['get_skill'], vp['apply_skill'], vp['cmd_sit_down'],
                    vp['post_msg'], vp['get_water_h'], vp['get_tick']):
        runner.uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)
    ret = runner.call(vp['run_action_core'], [hud, request])
    return {'action': action, 'mode': mode, 'flags_9ec': flags_9ec, 'ret': ret & 0xFFFFFFFF, 'calls': calls}


def run_mobile_hud_action(binary, vp, action, flags_9ec=0, dlg_keys_flag=1):
    from unicorn import UC_HOOK_CODE
    runner = NativeHUD(binary)
    reg = runner.reg
    hud, request = BASE + 0x10000, BASE + 0x58000
    rootptr, game, run, host, mode_at, skill = (BASE + at for at in (0x65000, 0x66000, 0x67000, 0x68000, 0x6B000, 0x6C000))
    for address, value in ((vp['rootptr_va'], rootptr), (rootptr, game), (game + 0x38, run),
                            (run + 0x60, host), (vp['mode_ptr_va'], mode_at)):
        runner.uc.mem_write(address, struct.pack('<Q', value))
    runner.uc.mem_write(host + vp.get('host_ready_off', 0x115C), b'\1')
    runner.uc.mem_write(host + vp.get('host_flags_off', 0x9EC), struct.pack('<I', flags_9ec))
    runner.uc.mem_write(hud + vp.get('hud_dlg_keys_off', 0x68), bytes([dlg_keys_flag]))
    runner.uc.mem_write(hud + vp['hud_jump_skill_off'], struct.pack('<I', 999))
    runner.uc.mem_write(request, struct.pack('<5I', action, 0, 0, 0, 0))
    calls, logs = [], []

    def external(uc, address, size, _):
        if address == vp['get_skill']:
            calls.append(['GetSkill', uc.reg_read(reg.UC_ARM64_REG_X1)])
            uc.reg_write(reg.UC_ARM64_REG_X0, skill)
        elif address == vp['apply_skill']:
            calls.append(['ApplySkillShortcut', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == vp['cmd_sit_down']:
            calls.append(['CmdSitDown', uc.reg_read(reg.UC_ARM64_REG_X0) == host, uc.reg_read(reg.UC_ARM64_REG_X1) & 1])
            uc.reg_write(reg.UC_ARM64_REG_X0, 1)
        elif address == vp['post_msg']:
            calls.append(['PostMessageW', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == vp['get_water_h']:
            uc.reg_write(reg.UC_ARM64_REG_S0, 0)
        elif address == vp['get_tick']:
            uc.reg_write(reg.UC_ARM64_REG_X0, 1000)
        elif address == vp['log']:
            logs.append(bytes(uc.mem_read(uc.reg_read(reg.UC_ARM64_REG_X1), 96)).split(b'\0')[0].decode('ascii', 'replace'))
            uc.reg_write(reg.UC_ARM64_REG_X0, 0)
        else:
            uc.reg_write(reg.UC_ARM64_REG_X0, 0)
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    for address in (vp['get_skill'], vp['apply_skill'], vp['cmd_sit_down'],
                    vp['post_msg'], vp['get_water_h'], vp['get_tick'], vp['log']):
        runner.uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)
    runner.call(vp['run_action'], [hud, request])
    return {'action': action, 'calls': calls, 'logs': logs}


def verify_dispatch(original, modified, vp):
    assert dispatch(original, vp, 35) == dispatch(modified, vp, 35)
    assert dispatch(modified, vp, 35)['calls'] == [['GetSkill', 999], ['ApplySkillShortcut', 999]]

    plain_jump = dispatch(modified, vp, 74)
    assert plain_jump['ret'] == 1 and plain_jump['calls'] == [['PostMessageW', 0x121]]
    for blk in ({'mode': 2}, {'blocked': True}, {'flags_9ec': 0x08}, {'player': False}, {'ready': False}):
        assert dispatch(modified, vp, 74, **blk)['calls'] == [], blk

    stand_tap = dispatch(modified, vp, 75, flags_9ec=0x00)
    sit_tap = dispatch(modified, vp, 75, flags_9ec=0x20)
    fly_sit_tap = dispatch(modified, vp, 75, flags_9ec=0x28)
    assert stand_tap['ret'] == 1 and stand_tap['calls'] == [['CmdSitDown', True, 1]]
    assert sit_tap['ret'] == 1 and sit_tap['calls'] == [['CmdSitDown', True, 0]]
    assert fly_sit_tap['ret'] == 1 and fly_sit_tap['calls'] == [['CmdSitDown', True, 0]]
    for blocked in ({'mode': 2}, {'player': False}, {'ready': False}):
        assert dispatch(modified, vp, 75, **blocked)['calls'] == [], blocked

    for action in (0, 73, 76, 99):
        assert dispatch(original, vp, action) == dispatch(modified, vp, action)

    dropped_73 = run_mobile_hud_action(modified, vp, 73, dlg_keys_flag=1)
    assert dropped_73['calls'] == [] and any('HUDDLG' in x for x in dropped_73['logs'])
    del_jump = run_mobile_hud_action(modified, vp, 74, flags_9ec=0, dlg_keys_flag=1)
    del_stand = run_mobile_hud_action(modified, vp, 75, flags_9ec=0, dlg_keys_flag=1)
    del_sit = run_mobile_hud_action(modified, vp, 75, flags_9ec=0x20, dlg_keys_flag=1)
    assert del_jump['calls'] == [['PostMessageW', 0x121]]
    assert del_stand['calls'] == [['CmdSitDown', True, 1]]
    assert del_sit['calls'] == [['CmdSitDown', True, 0]]
    return {'khinhCongAction35Preserved': True, 'plainJump74Dispatched': True,
            'standTapCallsCmdSitDown1': True, 'sitTapCallsCmdSitDown0': True}


def verify_fill_hook(modified, vp):
    runner = NativeHUD(modified)
    reg = runner.reg
    uiman, player, data, uistate, world = (BASE + at for at in (0x10000, 0x20000, 0x30000, 0x40000, 0x50000))
    results = []
    for flags_9ec, exp_fly, exp_sit in ((0x00, 0, 0), (0x08, 1, 0), (0x20, 0, 1), (0x28, 1, 1), (0xFF, 1, 1)):
        runner.clear_inputs()
        runner.uc.mem_write(player + vp.get('host_flags_off', 0x9EC), bytes([flags_9ec]))
        runner.uc.mem_write(data + 0x8AB1, b'\x77')
        for index, val in enumerate((uiman, player, data, uistate, world)):
            runner.uc.reg_write(reg.UC_ARM64_REG_X0 + index, val)
        runner.uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(1.0))
        runner.uc.reg_write(reg.UC_ARM64_REG_SP, 0x600F0000)
        runner.uc.reg_write(reg.UC_ARM64_REG_LR, BASE + 0x7FFF0)
        runner.run(vp['hud_fill_start'], vp['hud_fill_stop'], 5000)
        got_fly = runner.uc.mem_read(data + 0x8AB0, 1)[0]
        got_item = runner.uc.mem_read(data + 0x8AB1, 1)[0]
        got_sit = runner.uc.mem_read(data + 0x8AB2, 1)[0]
        assert (got_fly, got_item, got_sit) == (exp_fly, 0, exp_sit)
        results.append({'flags_9ec': hex(flags_9ec), 'fly': got_fly, 'sit': got_sit})
    return {'result': 'PASS', 'cases': results}


def verify_capstone_fragments(payload, version):
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM
    arm = payload.PROFILES[version]
    md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)
    for name, offset, _, encoded in arm['fragments']:
        raw = bytes.fromhex(encoded)
        insns = list(md.disasm(raw, arm['code_va'] + offset))
        assert sum(ins.size for ins in insns) == len(raw), ('Undecodable instruction in', name)
    return {'result': 'PASS', 'fragments': len(arm['fragments'])}


class FpsProbe(NativeHUD):
    """Unicorn runner for native 45 FPS ceiling, ladders, and FrameCapNext hysteresis."""

    def __init__(self, data, vp):
        from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
        super().__init__(data)
        self.uc.mem_map(SCRATCH, 0x20000)
        self.env = {}
        self.uc.hook_add(UC_HOOK_CODE, self._getenv, begin=vp['getenv'], end=vp['getenv'])
        self.uc.hook_add(UC_HOOK_CODE, self._atoi, begin=vp['atoi'], end=vp['atoi'])
        self.uc.hook_add(UC_HOOK_CODE, self._log, begin=vp['log'], end=vp['log'])
        self.uc.hook_add(UC_HOOK_MEM_INVALID, self._invalid)

    def _invalid(self, uc, access, address, size, value, _):
        raise AssertionError('FPS probe unmapped access PC=%x addr=%x size=%d' %
                             (uc.reg_read(self.reg.UC_ARM64_REG_PC), address, size))

    def _getenv(self, uc, address, size, _):
        ptr = uc.reg_read(self.reg.UC_ARM64_REG_X0)
        key = bytes(uc.mem_read(ptr, 64)).split(b'\0')[0].decode('ascii', 'replace')
        val = self.env.get(key)
        if val is None:
            uc.reg_write(self.reg.UC_ARM64_REG_X0, 0)
        else:
            slot = STRINGS + (abs(hash(key)) & 0x7FF)
            uc.mem_write(slot, val.encode('ascii') + b'\0')
            uc.reg_write(self.reg.UC_ARM64_REG_X0, slot)
        uc.reg_write(self.reg.UC_ARM64_REG_PC, uc.reg_read(self.reg.UC_ARM64_REG_LR))

    def _atoi(self, uc, address, size, _):
        ptr = uc.reg_read(self.reg.UC_ARM64_REG_X0)
        raw = bytes(uc.mem_read(ptr, 32)).split(b'\0')[0].decode('ascii', 'replace')
        digits = ''
        for ch in raw.lstrip():
            if ch.isdigit() or (not digits and ch in '+-'):
                digits += ch
            else:
                break
        uc.reg_write(self.reg.UC_ARM64_REG_X0, int(digits) if digits not in ('', '+', '-') else 0)
        uc.reg_write(self.reg.UC_ARM64_REG_PC, uc.reg_read(self.reg.UC_ARM64_REG_LR))

    def _log(self, uc, address, size, _):
        uc.reg_write(self.reg.UC_ARM64_REG_X0, 0)
        uc.reg_write(self.reg.UC_ARM64_REG_PC, uc.reg_read(self.reg.UC_ARM64_REG_LR))


def _record_second(probe, sec_va, work_ms, gpu_ms=10, work_p75_ms=None):
    probe.uc.mem_write(sec_va, bytes(0x3F0))
    b95 = max(0, min(40, int(work_ms)))
    if work_p75_ms is None:
        counts = {b95: 1}
        frames = 1
    else:
        b75 = max(0, min(40, int(work_p75_ms)))
        counts = {b75: 8}
        counts[b95] = counts.get(b95, 0) + 2
        frames = 10
    for bkt, cnt in counts.items():
        probe.uc.mem_write(sec_va + bkt * 4, struct.pack('<I', cnt))
        probe.uc.mem_write(sec_va + 0xA4 + bkt * 4, struct.pack('<I', cnt))
    probe.uc.mem_write(sec_va + 0x3D8, struct.pack('<I', frames))
    probe.uc.mem_write(sec_va + 0x3DC, struct.pack('<I', frames))
    probe.uc.mem_write(sec_va + 0x3E8, struct.pack('<Q', gpu_ms * 1000 * frames))


def _b_target(so_bytes, site_va):
    word = struct.unpack('<I', virtual_bytes(so_bytes, site_va, 4))[0]
    assert word >> 26 == 5, hex(word)
    disp = word & 0x3FFFFFF
    if disp & 0x2000000:
        disp -= 0x4000000
    return site_va + disp * 4


def _bl_target(so_bytes, site_va):
    word = struct.unpack('<I', virtual_bytes(so_bytes, site_va, 4))[0]
    assert word >> 26 == 0x25, hex(word)
    disp = word & 0x3FFFFFF
    if disp & 0x2000000:
        disp -= 0x4000000
    return site_va + disp * 4


def verify_fps45(modified, vp):
    reg = FpsProbe(modified, vp).reg
    for tier_va in vp['profile_all_cap_vas']:
        assert struct.unpack('<i', virtual_bytes(modified, tier_va, 4))[0] == 45
    for gfx_auto_va in vp['profile_gfx_auto_vas']:
        assert struct.unpack('<i', virtual_bytes(modified, gfx_auto_va, 4))[0] == 0
    assert struct.unpack('<i', virtual_bytes(modified, vp['frame_cap_va'], 4))[0] == 45
    for zero_w2_va in vp['zero_w2_vas']:
        assert struct.unpack('<I', virtual_bytes(modified, zero_w2_va, 4))[0] == 0x2A1F03E2
    for dynres_va in vp['dynres_vas']:
        assert struct.unpack('<I', virtual_bytes(modified, dynres_va, 4))[0] == 0x52800022
    assert struct.unpack('<I', virtual_bytes(modified, vp['perf_hint_va'], 4))[0] == 0x52800042
    assert struct.unpack('<I', virtual_bytes(modified, vp['thread_nice_va'], 4))[0] == 0x52800022
    assert struct.unpack('<I', virtual_bytes(modified, vp['matrix_simd_va'], 4))[0] == 0x52800022
    assert struct.unpack('<I', virtual_bytes(modified, vp['fallback_rung_va'], 4))[0] == 0x528005A8
    for nop_va in vp['nop_vas']:
        assert struct.unpack('<I', virtual_bytes(modified, nop_va, 4))[0] == 0xD503201F
    for site_va, exp_tgt in vp['branch_checks']:
        assert _b_target(modified, site_va) == exp_tgt
    for adpf_only_va in vp['adpf_only_vas']:
        assert _bl_target(modified, adpf_only_va) == vp['perf_hint_target_va']
    assert _bl_target(modified, vp['startup_set_rate_bl_va']) == vp['set_frame_cap_va']

    ceiling_cases = []
    for tier in range(5):
        probe = FpsProbe(modified, vp)
        probe.uc.mem_write(vp['tier_global_va'], struct.pack('<i', tier))
        sp = 0x600F0000
        probe.uc.reg_write(reg.UC_ARM64_REG_SP, sp)
        probe.uc.reg_write(reg.UC_ARM64_REG_W19, 0xFFFFFFFF)
        probe.run(vp['cap_ini_start'], vp['cap_ini_stop'], 2000)
        is_ini, cap = struct.unpack('<ii', bytes(probe.uc.mem_read(sp + 0x50, 8)))
        assert (is_ini, cap) == (1, 45), (tier, is_ini, cap)
        ceiling_cases.append({'tier': tier, 'ini': -1, 'cap': cap})
    for ini_val in (-1, 0, 15, 30, 40, 45, 60, 90, 120):
        probe = FpsProbe(modified, vp)
        sp = 0x600F0000
        probe.uc.reg_write(reg.UC_ARM64_REG_SP, sp)
        probe.uc.reg_write(reg.UC_ARM64_REG_W19, ini_val & 0xFFFFFFFF)
        probe.run(vp['cap_ini_start'], vp['cap_ini_stop'], 2000)
        is_ini, cap = struct.unpack('<ii', bytes(probe.uc.mem_read(sp + 0x50, 8)))
        assert (is_ini, cap) == (1, 45), (ini_val, is_ini, cap)
        ceiling_cases.append({'ini': ini_val, 'cap': cap})

    startup_ladders = {}
    for label, env in (
        ('60Hz', {'TRUTIEN_REFRESH': '60'}),
        ('90Hz', {'TRUTIEN_REFRESH': '90'}),
        ('120Hz', {'TRUTIEN_REFRESH': '120'}),
        ('rates60', {'TRUTIEN_REFRESH': '60', 'TRUTIEN_RATES': '60'}),
        ('rates60_120', {'TRUTIEN_REFRESH': '60', 'TRUTIEN_RATES': '60,120'}),
    ):
        probe = FpsProbe(modified, vp)
        probe.env = dict(env)
        probe.uc.reg_write(reg.UC_ARM64_REG_SP, 0x600F0000)
        probe.uc.reg_write(reg.UC_ARM64_REG_X28, vp['x28_base'])
        probe.uc.reg_write(reg.UC_ARM64_REG_X27, vp['tiers_va'])
        probe.uc.reg_write(reg.UC_ARM64_REG_W26, 0xCCCCCCCD)
        probe.run(vp['startup_ladder_start'], vp['startup_ladder_stop'], 5000)
        cnt = struct.unpack('<i', bytes(probe.uc.mem_read(vp['tier_count_va'], 4)))[0]
        tiers = list(struct.unpack(f'<{cnt}i', bytes(probe.uc.mem_read(vp['tiers_va'], cnt * 4))))
        assert tiers == [45, 45, 45, 45], (label, tiers)
        startup_ladders[label] = tiers

    live_ladders = {}
    for refresh in (60, 90, 120):
        probe = FpsProbe(modified, vp)
        sp, x29 = 0x600F0000, 0x600F0800
        probe.uc.mem_write(x29 - 0xF8, b'\0')
        probe.uc.reg_write(reg.UC_ARM64_REG_SP, sp)
        probe.uc.reg_write(reg.UC_ARM64_REG_X29, x29)
        probe.uc.reg_write(reg.UC_ARM64_REG_X28, vp['x28_base'])
        probe.uc.reg_write(reg.UC_ARM64_REG_W20, refresh)
        probe.uc.reg_write(reg.UC_ARM64_REG_W22, 0 if refresh < 120 else 1)
        probe.uc.reg_write(reg.UC_ARM64_REG_W26, 0xCCCCCCCD)
        probe.run(vp['live_ladder_start'], vp['live_ladder_stop'], 4000)
        cnt = probe.uc.reg_read(reg.UC_ARM64_REG_X8)
        tiers = list(struct.unpack(f'<{cnt}i', bytes(probe.uc.mem_read(sp + 0x4A0, cnt * 4))))
        assert tiers == [45, 45, 45, 45], (refresh, tiers)
        live_ladders[f'{refresh}Hz'] = tiers

    probe = FpsProbe(modified, vp)
    state_va, tiers_va, sec_va, inp_va, fig_va, why_va = (
        SCRATCH + at for at in (0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000)
    )
    probe.uc.mem_write(state_va, bytes(0x800))
    probe.uc.mem_write(tiers_va, struct.pack('<3i', 45, 40, 30))
    probe.uc.mem_write(inp_va, bytes(0x20))
    probe.uc.mem_write(inp_va + 0x08, bytes([1, 1, 0, 0]))

    def step_second(cur, work_ms, work_p75_ms=None, login_flag=0):
        _record_second(probe, sec_va, work_ms, work_p75_ms=work_p75_ms)
        probe.uc.mem_write(fig_va, bytes(0x40))
        probe.uc.mem_write(why_va, bytes([login_flag]) + bytes(0x7F))
        probe.uc.reg_write(reg.UC_ARM64_REG_SP, 0x600F0000)
        probe.call(vp['frame_cap_next'], [state_va, tiers_va, 3, cur, sec_va, inp_va, fig_va, why_va])
        return probe.uc.reg_read(reg.UC_ARM64_REG_W0) & 0xFFFFFFFF

    cur = 45
    for _ in range(3):
        assert (cur := step_second(cur, 16)) == 45
    for work in (40, 42, 45, 36, 28, 35, 45, 44, 38, 40):
        assert (cur := step_second(cur, work, work_p75_ms=work)) == 45
    assert (cur := step_second(cur, 40, work_p75_ms=40, login_flag=1)) == 45
    probe.uc.mem_write(state_va, bytes(0x800))
    cur = 30
    for _ in range(3):
        assert (cur := step_second(cur, 20)) in (30, 40)
    assert cur == 40
    for _ in range(3):
        cur = step_second(cur, 20)
    assert cur == 45

    probe.uc.mem_write(state_va, bytes(0x800))
    probe.uc.mem_write(tiers_va, struct.pack('<4i', 45, 45, 45, 45))
    probe.uc.reg_write(reg.UC_ARM64_REG_SP, 0x600F0000)
    probe.call(vp['frame_cap_next'], [state_va, tiers_va, 4, 30, sec_va, inp_va, fig_va, why_va])
    assert (probe.uc.reg_read(reg.UC_ARM64_REG_W0) & 0xFFFFFFFF) == 45

    vk_cases = []
    sentinels = {r: 0x12340000 + i for i, r in enumerate(range(reg.UC_ARM64_REG_X19, reg.UC_ARM64_REG_X29))}
    for modes, expected_lut in (
        ([2, 1], 2),
        ([2, 3, 1, 0], 2),
        ([2, 0], 3),
        ([2, 3], 1),
        ([2], 0),
        ([], 0),
        ([1000111000, 1], 2),
    ):
        vk_probe = FpsProbe(modified, vp)
        sp = 0x600F0000
        vk_probe.uc.mem_write(sp + 0x88, struct.pack('<I', len(modes)))
        if modes:
            vk_probe.uc.mem_write(sp + 0x20, struct.pack(f'<{len(modes)}I', *modes))
        vk_probe.uc.reg_write(reg.UC_ARM64_REG_SP, sp)
        for r, val in sentinels.items():
            vk_probe.uc.reg_write(r, val)
        vk_probe.run(vp['vk_start'], vp['vk_stop'], 500)
        got = vk_probe.uc.reg_read(reg.UC_ARM64_REG_W8)
        assert got == expected_lut, (modes, got, expected_lut)
        for r, val in sentinels.items():
            assert vk_probe.uc.reg_read(r) == val
        vk_cases.append({'modes': modes, 'lutIndex': got})

    return {
        'result': 'PASS',
        'ceilingCases': ceiling_cases,
        'startupLadders': startup_ladders,
        'liveLadders': live_ladders,
        'vulkanPresentModes': vk_cases,
        'frameCapNextHysteresis': 'PASS (hard-locked @45 across 40-45ms load & login screen; recovers 30->40->45)',
    }


def verify_antihitch_v7(modified, vp):
    """Execute GLES vsync=0, worker profile T0..T4, and <=4GB cache stability paths in Unicorn."""
    vm = NativeHUD(modified)
    reg = vm.reg

    worker_tiers = []
    for tier in range(5):
        vm.uc.mem_write(vp['tier_global_va'], struct.pack('<I', tier))
        ptr = vm.call(vp['get_mobile_profile'], ())
        fields = struct.unpack('<27i', vm.uc.mem_read(ptr, 27 * 4))
        assert fields[11] == 45, f'Tier T{tier} ceiling expected 45, got {fields[11]}'
        assert (fields[19], fields[20], fields[21], fields[22], fields[23], fields[24],
                fields[25], fields[26]) == (1, 1, 1, 3, 1, 0, 1, 1)
        worker_tiers.append({'tier': tier, 'cap': fields[11], 'workers': list(fields[19:27])})

    vm.uc.reg_write(reg.UC_ARM64_REG_X0, 0xFFFFFFFF)
    vm.uc.reg_write(reg.UC_ARM64_REG_X28, 3)
    vm.run(vp['lowmem_crit_start'], vp['lowmem_crit_stop'], 50)
    critical_mb = vm.uc.reg_read(reg.UC_ARM64_REG_W9)
    holdoff_mb = vm.uc.reg_read(reg.UC_ARM64_REG_W22)
    assert (critical_mb, holdoff_mb) == (250, 320)

    vm.uc.reg_write(reg.UC_ARM64_REG_X28, 3)
    vm.run(vp['lowmem_pct_start'], vp['lowmem_pct_stop'], 50)
    pct_ptr = vm.uc.reg_read(reg.UC_ARM64_REG_X20)
    pct_str = bytes(vm.uc.mem_read(pct_ptr, 6)).split(b'\0', 1)[0].decode('ascii')
    assert pct_str == '86,91', pct_str

    vm.uc.reg_write(reg.UC_ARM64_REG_X0, 0xFFFFFFFF)
    vm.uc.reg_write(reg.UC_ARM64_REG_X28, 3)
    vm.run(vp['rhi_cache_start'], vp['rhi_cache_stop'], 50)
    rhi_mem_mb = vm.uc.reg_read(reg.UC_ARM64_REG_W9)
    rhi_dyn_mb = vm.uc.reg_read(reg.UC_ARM64_REG_W25)
    assert (rhi_mem_mb, rhi_dyn_mb) == (64, 64)

    vm.uc.mem_write(BASE + 0x3000, struct.pack('<Q', 0))
    vm.uc.reg_write(reg.UC_ARM64_REG_X8, 3)
    vm.uc.reg_write(reg.UC_ARM64_REG_X27, 0xFFFFFFFF)
    vm.uc.reg_write(reg.UC_ARM64_REG_X9, BASE + 0x2400)
    vm.uc.mem_write(BASE + 0x2400 + vp['ui_opt_x9_off'], struct.pack('<Q', BASE + 0x3000))
    vm.run(vp['ui_opt_start'], vp['ui_opt_stop'], 50)
    ui_opt = vm.uc.reg_read(reg.UC_ARM64_REG_W8)
    assert ui_opt == 0, ui_opt

    egl_ctx, egl_surf = BASE + 0x4000, BASE + 0x4100
    reg_map = {'w8': reg.UC_ARM64_REG_W8, 'w20': reg.UC_ARM64_REG_W20}
    for start, stop, arg_name in vp['gles_sites']:
        vm.uc.reg_write(reg.UC_ARM64_REG_X19, egl_ctx)
        vm.uc.reg_write(reg_map[arg_name], 0x80)
        vm.run(start, stop, 20)
        vm.uc.reg_write(reg.UC_ARM64_REG_X19, egl_ctx)
        vm.uc.reg_write(reg.UC_ARM64_REG_X20, egl_surf)
        vm.uc.reg_write(reg.UC_ARM64_REG_W21, 1)
        vm.run(vp['gles_swap_start'], vp['gles_swap_stop'], 20)
        assert vm.uc.reg_read(reg.UC_ARM64_REG_W1) == 0

    for pc in vp['fast_w2_pcs']:
        vm.uc.reg_write(reg.UC_ARM64_REG_X2, 0)
        vm.run(pc, pc + 4, 10)
        assert vm.uc.reg_read(reg.UC_ARM64_REG_W2) == 1, hex(pc)
    vm.uc.reg_write(reg.UC_ARM64_REG_X0, 0)
    vm.run(vp['upscale_w0_pc'], vp['upscale_w0_pc'] + 4, 10)
    assert vm.uc.reg_read(reg.UC_ARM64_REG_W0) == 1
    vm.uc.reg_write(reg.UC_ARM64_REG_X21, 0)
    vm.run(vp['upscale_w21_pc'], vp['upscale_w21_pc'] + 4, 10)
    assert vm.uc.reg_read(reg.UC_ARM64_REG_W21) == 1

    vm.uc.reg_write(reg.UC_ARM64_REG_X2, 1)
    vm.run(vp['stack_sampler_pc'], vp['stack_sampler_pc'] + 4, 10)
    assert vm.uc.reg_read(reg.UC_ARM64_REG_W2) == 0

    vm.uc.mem_write(vp['frame_cap_va'], struct.pack('<I', 45))
    vm.uc.mem_write(vp['tiers_va'], struct.pack('<3i', 45, 40, 30))
    vm.uc.mem_write(vp['tier_count_va'], struct.pack('<I', 3))
    vm.uc.mem_write(vp['first_tick_flag_va'], b'\0')
    vm.uc.reg_write(reg.UC_ARM64_REG_X28, vp['x28_base'])
    vm.uc.reg_write(reg.UC_ARM64_REG_X27, vp['tiers_va'])
    vm.uc.reg_write(reg.UC_ARM64_REG_W8, 45)
    vm.uc.reg_write(reg.UC_ARM64_REG_W12, 30)
    vm.run(vp['first_tick_start'], vp['first_tick_stop'], 200)
    assert struct.unpack('<I', vm.uc.mem_read(vp['frame_cap_va'], 4))[0] == 45
    assert vm.uc.reg_read(reg.UC_ARM64_REG_W0) == 45

    return {
        'result': 'PASS',
        'workerTiers': worker_tiers,
        'lowMemPct': pct_str,
        'lowMemCriticalMB': critical_mb,
        'lowMemHoldOffMB': holdoff_mb,
        'rhiMemCacheMB': rhi_mem_mb,
        'rhiDynCacheMB': rhi_dyn_mb,
        'uiOptimizeAllDefault': ui_opt,
        'glesSwapInterval': 0,
        'stackSamplerDefault': 0,
        'firstTickThermalFrameCapPreserved': 45,
        'fastDefaultsEnabled': [
            'PateFrameLazy=1', 'SkinTickQueueHash=1', 'UITickLite=1',
            'GfxFillSplit=1', 'GfxModelSlotSkip=1', 'GfxSlotStateCache=1',
            'PortraitLazy=1', 'BlendMatLite=1', 'RHIDrawFast=1',
            'RHIFrameStreamDraw=1', 'DynRes=1', 'Upscale3D=1(SGSR)',
            'GfxCapNoTick=1', 'GfxWaterOnce=1',
        ],
    }


def verify(original):
    from scripts.patch_trutien_sit import SHA256_TO_VERSION, patch_library
    from scripts import trutien_sit_arm64 as payload
    orig_sha = hashlib.sha256(original).hexdigest()
    version = SHA256_TO_VERSION[orig_sha]
    vp = VERIFY_PROFILES[version]
    arm = payload.PROFILES[version]
    modified, _ = patch_library(original)
    code, rodata = payload.build_payload(version)
    assert virtual_bytes(modified, arm['code_va'], len(code)) == code
    assert virtual_bytes(modified, arm['label_va'], len(rodata)) == rodata
    return {'result': 'PASS',
            'gameVersionCode': int(version),
            'originalLibrarySHA256': orig_sha,
            'patchedLibrarySHA256': hashlib.sha256(modified).hexdigest(),
            'restoration': verify_restoration(original, modified, vp),
            'geometry': verify_geometry(original, modified, arm['code_va']),
            'drawing': verify_draw(original, modified, vp),
            'atlasDrawing': verify_atlas_draw(original, modified, payload, version, vp),
            'input': verify_input(original, modified, arm['code_va'], vp),
            'dispatch': verify_dispatch(original, modified, vp),
            'fillHook': verify_fill_hook(modified, vp),
            'fps45': verify_fps45(modified, vp),
            'antiHitchV7': verify_antihitch_v7(modified, vp),
            'capstoneFragments': verify_capstone_fragments(payload, version),
            'deviceValidation': 'pending', 'serverResponse': 'not tested'}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--library', type=Path, required=True)
    ap.add_argument('--deps', type=Path)
    ap.add_argument('--report', type=Path)
    args = ap.parse_args()
    if args.report is not None and (args.report.exists() or args.report.resolve() == args.library.resolve()):
        ap.error('--report must name a new file separate from --library')
    if args.deps is not None:
        sys.path.insert(0, str(args.deps.resolve()))
    report = verify(args.library.read_bytes())
    if args.report is not None:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with args.report.open('x', encoding='utf8') as f:
            json.dump(report, f, ensure_ascii=False, indent=2)
            f.write('\n')
    print(json.dumps({'result': report['result'], 'gameVersionCode': report['gameVersionCode'],
                      'geometryCases': len(report['geometry']['cases']),
                      'drawCases': len(report['drawing']['cases']), 'atlasDrawing': report['atlasDrawing'],
                      'fillCases': len(report['fillHook']['cases']),
                      'fps45': report['fps45'],
                      'antiHitchV7': report['antiHitchV7'],
                      'capstoneFragments': report['capstoneFragments']}))


if __name__ == '__main__':
    main()
