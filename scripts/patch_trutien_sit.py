#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Prepare an unsigned Tru Tien APK with paired separate Jump + Sit/Stand buttons.

Supports verified static profiles (versionCode 2411, 2503, 2685) plus automatic
ARM64 ELF symbol/instruction discovery for unknown future Tru Tien updates.
No build, signing, installation, device access or original-file overwrite is performed.
Khinh cong and jump physics are preserved.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.patch_trutien_jump import (
    DIAGNOSTIC_VA, Edit, LIBRARY_ENTRY, ORIGINAL_DIAGNOSTIC, ORIGINAL_SHA256,
    apply_edits, branch_instruction, file_offset, load_segments, prepare_apk as _base_prepare_apk,
)
from scripts.trutien_jump_elf import append_payload
from scripts.trutien_sit_arm64 import (
    PROFILES as ARM64_PROFILES, build_payload, unwind_ranges,
)
from scripts.trutien_sit_auto import (
    build_auto_profile_and_payload, discover_trutien_profile, parse_axml_version_code,
)

ORIGINAL_SHA256_2411 = ORIGINAL_SHA256
ORIGINAL_SHA256_2503 = '598b6b246e2a8a9a8217b3947c10595f6da2d68e6136b0b4974dab682af18bbf'
ORIGINAL_SHA256_2685 = 'ec0a7156ea22b4dc5ce0d964634b23c8558b4c8142a5748cbb2a0cc7b5aa487d'
SHA256_TO_VERSION = {
    ORIGINAL_SHA256_2411: '2411',
    ORIGINAL_SHA256_2503: '2503',
    ORIGINAL_SHA256_2685: '2685',
}

_PROFILE_WORKER_SLOTS = (
    (0x4C, 'JobPool', '01000000'),
    (0x50, 'MoveJobs', '01000000'),
    (0x54, 'ChildSkinJobs', '01000000'),
    (0x58, 'RegisterJobs', '03000000'),
    (0x5C, 'GfxLateRegister', '01000000'),
)

# SwapChainVK::createSwapChain vsync=false path (132 bytes): prefer MAILBOX > IMMEDIATE > FIFO_RELAXED > FIFO.
_VK_VSYNC_OFF_ORIG = bytes.fromhex(
    'e88b40b988030034e9031faa290500913f0108ebc3ffff54'
    'e8020034e9031faa290500913f0108ebc3ffff54'
    '48020034e9031faa290500913f0108ebc3ffff54'
    'a8010034ea031faa09008012eb8300916c008052'
    '6d796ab8bf01007129118c1acd0000344a050091'
    '5f0108eb43ffff5402000014090080123f050031'
    'e803891a93ffff17'
)
_VK_VSYNC_OFF_MAILBOX_FIRST = bytes.fromhex(
    'e88b40b9e9031f2a08020034ea031faaeb830091'
    '6e4688520e22a4726d796ab8bf0d0071c8000054'
    'ad711d53cf25cd1aef1d0012ff01096be981891a'
    '4a0500915f0108ebc3feff54280d0012a0ffff17'
    + ('1f2003d5' * 13)
)

VERSION_CONFIGS = {
    '2411': {
        'version_code': 2411,
        'original_sha256': ORIGINAL_SHA256_2411,
        'draw_hook': 0x242ECB4,
        'input_hook': 0x24114A8,
        'action_hook': 0x23AB870,
        'fill_hook': 0x2402090,
        'profile_cap_vas': (0x132662C, 0x1326704, 0x1326770, 0x13267DC),
        'profile_worker_bases': (0x1326600, 0x132666C, 0x13266D8, 0x1326744),
        'profile_gfx_task_auto_vas': (0x1326660, 0x13266CC, 0x1326738, 0x13267A4, 0x1326810),
        'profile_all_bases': (0x1326600, 0x132666C, 0x13266D8, 0x1326744, 0x13267B0),
        'set_frame_cap_va': 0x2442F10,
        'perf_hint_target_va': 0x2442F98,
        'surface_rate_bypass_sites': (
            (0x1B87748, 'live display refresh'),
            (0x1B885A4, 'runtime FrameCapNext step'),
            (0x1B88DD8, 'thermal transition'),
        ),
        'stall_feature_defaults': (
            (0x1E05350, '22008052', 'Video.AutoTune default in TuneOnSecond'),
            (0x1B86C98, '22008052', 'Video.LoginFrameCap default in RunInRenderThread'),
            (0x1B86DD8, '22008052', 'Video.FrameCapClimbBackoff default in RunInRenderThread'),
            (0x1B871B4, '42008052', 'Video.CpuSpeedLog per-frame sampler default in RunInRenderThread'),
            (0x2447318, '22008052', 'Video.StackSampler 5ms SIGPROF sampler default'),
        ),
        'enable_feature_defaults': (
            (0x1E0721C, 'e2031f2a', '42008052', 'Video.PerfHint=2 ADPF CPU scheduler session'),
            (0x1E07240, 'e2031f2a', '22008052', 'Video.PateFrameLazy=1 skip unchanged overhead nameplates'),
            (0x1E072F8, 'e2031f2a', '22008052', 'Video.SkinTickQueueHash=1 deduplicate skeleton tick queue'),
            (0x1E07340, 'e2031f2a', '22008052', 'Video.UITickLite=1 skip unchanged UI tree rebuilds'),
            (0x1E0740C, 'e2031f2a', '22008052', 'Video.GfxFillSplit=1 split GFX fills across task workers'),
            (0x1E07A38, 'e2031f2a', '22008052', 'Video.PortraitLazy=1 defer unchanged HUD portrait redraws'),
            (0x1E07AE8, 'e2031f2a', '22008052', 'Video.ThreadNice=1 render and worker thread priority'),
            (0x1E07C20, 'e2031f2a', '22008052', 'Video.MatrixSIMD=1 ARM64 NEON matrix transforms'),
            (0x1E07CD0, 'e2031f2a', '22008052', 'Video.BlendMatLite=1 skip unmasked bone skinning math safely'),
            (0x27EF300, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on init'),
            (0x27F6260, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on live reload'),
            (0x27F01E0, '02008012', '22008052', 'Video.RHIFrameStreamDraw=1 skip CPU hash on GFX dynamic vertex stream'),
            (0x281DAA0, '00791f53', '20008052', 'Video.Upscale3D=1 (SGSR) default in fast Upscale3D getter'),
            (0x281DB4C, '15791f53', '35008052', 'Video.Upscale3D=1 (SGSR) default in A3DRHIUpscale setup'),
            (0x29651D4, 'e2d79f1a', '22008052', 'Video.GfxCapNoTick=1 cap excess VFX emitter ticks on Tier 1'),
            (0x2965230, 'e2031f2a', '22008052', 'Video.GfxWaterOnce=1 merge clipped water flush into single pass'),
        ),
        'frame_cap_global': 0x31EC188,
        'cap_ini_force': 0x1B86964,
        'cap_ini_clamp': 0x1B86968,
        'startup_120': 0x1B86B68,
        'startup_90_inc': (0x1B86B7C, 0x1B86BC0),
        'startup_89': 0x1B86B94,
        'startup_89_inc': (0x1B86BA0, 0x1B86BC0),
        'startup_rates': 0x1B86BAC,
        'startup_rates_inc': 0x1B86BBC,
        'startup_mid_inc': 0x1B86BD4,
        'startup_mid': 0x1B86BD8,
        'startup_floor': 0x1B86BF4,
        'live_120': 0x1B8756C,
        'live_90_inc': (0x1B87578, 0x1B875AC),
        'live_89': 0x1B8758C,
        'live_89_inc': (0x1B87598, 0x1B875AC),
        'live_rates': 0x1B875A0,
        'live_rates_inc': 0x1B875A8,
        'live_mid_inc': 0x1B875BC,
        'live_mid': 0x1B875C8,
        'live_floor': 0x1B875E0,
        'fallback_rung': 0x1B87D4C,
        'login_drop_bypass': (0x1B89B00, 0x1B89BB4),
        'shared_climb_mark': 0x1B88164,
        'never_step_down': (0x1B89BE4, 0x1B89CAC),
        'step_one_rung': 0x1B89C34,
        'short_hold_ladder': 0x1B89CAC,
        'climb_mark_ladder': 0x1B89E30,
        'short_hold_backoff': 0x1B89F1C,
        'climb_mark_backoff': 0x1B8A02C,
        'first_tick_overwrite': (0x1B889BC, '488901b9'),
        'thermal_cap_bypass': (0x1B889FC, 0x1B88A4C),
        'rhi_thermal_tier0': (0x1B88D88, 0x1B88DAC),
        'tune_thermal_bypass': (0x1E052C0, 0x1E052F4),
        'lowmem_pct_str': 0x1252F95,
        'lowmem_critical': 0x1B8FFCC,
        'lowmem_holdoff': 0x1B8FFD4,
        'rhi_mem_cache': 0x1B90198,
        'rhi_dyn_cache': 0x1B901B0,
        'ui_optimize_all': 0x1D08300,
        'gles_create_swap0': 0x249CC94,
        'gles_resize_swap0': 0x249DE50,
        'vk_vsync_off': 0x24C01B4,
        'preserved_spans': (
            (0x24194EC, 0x241C5FC - 0x24194EC, True),
            (0x242EB2C, 0x242EBB0 - 0x242EB2C, True),
            (DIAGNOSTIC_VA, len(ORIGINAL_DIAGNOSTIC), False),
            (0x23ABA04, 0x23ABA34 - 0x23ABA04, True),
            (0x23ABE90, 160, True), (0x1C5A2A0, 112, True), (0x1C2C6CC, 1048, True),
            (0x1C22E00, 68, True), (0x2190CC8, 76, True), (0x2190D14, 76, True),
        ),
    },
    '2503': {
        'version_code': 2503,
        'original_sha256': ORIGINAL_SHA256_2503,
        'draw_hook': 0x2447D84,
        'input_hook': 0x242A55C,
        'action_hook': 0x23B1B00,
        'fill_hook': 0x241AFD0,
        'profile_cap_vas': (0x1329B5C, 0x1329C34, 0x1329CA0, 0x1329D0C),
        'profile_worker_bases': (0x1329B30, 0x1329B9C, 0x1329C08, 0x1329C74),
        'profile_gfx_task_auto_vas': (0x1329B90, 0x1329BFC, 0x1329C68, 0x1329CD4, 0x1329D40),
        'profile_all_bases': (0x1329B30, 0x1329B9C, 0x1329C08, 0x1329C74, 0x1329CE0),
        'set_frame_cap_va': 0x245C174,
        'perf_hint_target_va': 0x245C1FC,
        'surface_rate_bypass_sites': (
            (0x1B8D048, 'live display refresh'),
            (0x1B8DEA4, 'runtime FrameCapNext step'),
            (0x1B8E6D8, 'thermal transition'),
        ),
        'stall_feature_defaults': (
            (0x1E0AC84, '22008052', 'Video.AutoTune default in TuneOnSecond'),
            (0x1B8C598, '22008052', 'Video.LoginFrameCap default in RunInRenderThread'),
            (0x1B8C6D8, '22008052', 'Video.FrameCapClimbBackoff default in RunInRenderThread'),
            (0x1B8CAB4, '42008052', 'Video.CpuSpeedLog per-frame sampler default in RunInRenderThread'),
            (0x246057C, '22008052', 'Video.StackSampler 5ms SIGPROF sampler default'),
        ),
        'enable_feature_defaults': (
            (0x1E0CB50, 'e2031f2a', '42008052', 'Video.PerfHint=2 ADPF CPU scheduler session'),
            (0x1E0CB74, 'e2031f2a', '22008052', 'Video.PateFrameLazy=1 skip unchanged overhead nameplates'),
            (0x1E0CC2C, 'e2031f2a', '22008052', 'Video.SkinTickQueueHash=1 deduplicate skeleton tick queue'),
            (0x1E0CC74, 'e2031f2a', '22008052', 'Video.UITickLite=1 skip unchanged UI tree rebuilds'),
            (0x1E0CD40, 'e2031f2a', '22008052', 'Video.GfxFillSplit=1 split GFX fills across task workers'),
            (0x1E0D36C, 'e2031f2a', '22008052', 'Video.PortraitLazy=1 defer unchanged HUD portrait redraws'),
            (0x1E0D41C, 'e2031f2a', '22008052', 'Video.ThreadNice=1 render and worker thread priority'),
            (0x1E0D554, 'e2031f2a', '22008052', 'Video.MatrixSIMD=1 ARM64 NEON matrix transforms'),
            (0x1E0D604, 'e2031f2a', '22008052', 'Video.BlendMatLite=1 skip unmasked bone skinning math safely'),
            (0x2809C84, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on init'),
            (0x2810BB4, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on live reload'),
            (0x280AB84, '02008012', '22008052', 'Video.RHIFrameStreamDraw=1 skip CPU hash on GFX dynamic vertex stream'),
            (0x283AF74, '00791f53', '20008052', 'Video.Upscale3D=1 (SGSR) default in fast Upscale3D getter'),
            (0x283B020, '15791f53', '35008052', 'Video.Upscale3D=1 (SGSR) default in A3DRHIUpscale setup'),
            (0x298272C, 'e2d79f1a', '22008052', 'Video.GfxCapNoTick=1 cap excess VFX emitter ticks on Tier 1'),
            (0x2982788, 'e2031f2a', '22008052', 'Video.GfxWaterOnce=1 merge clipped water flush into single pass'),
        ),
        'frame_cap_global': 0x320BFF0,
        'cap_ini_force': 0x1B8C264,
        'cap_ini_clamp': 0x1B8C268,
        'startup_120': 0x1B8C468,
        'startup_90_inc': (0x1B8C47C, 0x1B8C4C0),
        'startup_89': 0x1B8C494,
        'startup_89_inc': (0x1B8C4A0, 0x1B8C4C0),
        'startup_rates': 0x1B8C4AC,
        'startup_rates_inc': 0x1B8C4BC,
        'startup_mid_inc': 0x1B8C4D4,
        'startup_mid': 0x1B8C4D8,
        'startup_floor': 0x1B8C4F4,
        'live_120': 0x1B8CE6C,
        'live_90_inc': (0x1B8CE78, 0x1B8CEAC),
        'live_89': 0x1B8CE8C,
        'live_89_inc': (0x1B8CE98, 0x1B8CEAC),
        'live_rates': 0x1B8CEA0,
        'live_rates_inc': 0x1B8CEA8,
        'live_mid_inc': 0x1B8CEBC,
        'live_mid': 0x1B8CEC8,
        'live_floor': 0x1B8CEE0,
        'fallback_rung': 0x1B8D64C,
        'login_drop_bypass': (0x1B8F400, 0x1B8F4B4),
        'shared_climb_mark': 0x1B8DA64,
        'never_step_down': (0x1B8F4E4, 0x1B8F5AC),
        'step_one_rung': 0x1B8F534,
        'short_hold_ladder': 0x1B8F5AC,
        'climb_mark_ladder': 0x1B8F730,
        'short_hold_backoff': 0x1B8F81C,
        'climb_mark_backoff': 0x1B8F92C,
        'first_tick_overwrite': (0x1B8E2BC, '48f10fb9'),
        'thermal_cap_bypass': (0x1B8E2FC, 0x1B8E34C),
        'rhi_thermal_tier0': (0x1B8E688, 0x1B8E6AC),
        'tune_thermal_bypass': (0x1E0ABF4, 0x1E0AC28),
        'lowmem_pct_str': 0x1255A28,
        'lowmem_critical': 0x1B958CC,
        'lowmem_holdoff': 0x1B958D4,
        'rhi_mem_cache': 0x1B95A98,
        'rhi_dyn_cache': 0x1B95AB0,
        'ui_optimize_all': 0x1D0DC34,
        'gles_create_swap0': 0x24B5EF8,
        'gles_resize_swap0': 0x24B70B4,
        'vk_vsync_off': 0x24D9418,
        'preserved_spans': (
            (0x24325BC, 0x24356CC - 0x24325BC, True),
            (0x2447BFC, 0x2447C80 - 0x2447BFC, True),
            (0x125DBA0, len(ORIGINAL_DIAGNOSTIC), False),
            (0x23B1C94, 0x30, True),
            (0x23B2120, 160, True), (0x1C5FBD4, 112, True), (0x1C32000, 1048, True),
            (0x1C28734, 68, True), (0x2196F58, 76, True), (0x2196FA4, 76, True),
        ),
    },
    '2685': {
        'version_code': 2685,
        'original_sha256': ORIGINAL_SHA256_2685,
        'draw_hook': 0x246B030,
        'input_hook': 0x244D768,
        'action_hook': 0x23C7FB0,
        'fill_hook': 0x243E1DC,
        'profile_cap_vas': (0x1330884, 0x133095C, 0x13309C8, 0x1330A34),
        'profile_worker_bases': (0x1330858, 0x13308C4, 0x1330930, 0x133099C),
        'profile_gfx_task_auto_vas': (0x13308B8, 0x1330924, 0x1330990, 0x13309FC, 0x1330A68),
        'profile_all_bases': (0x1330858, 0x13308C4, 0x1330930, 0x133099C, 0x1330A08),
        'set_frame_cap_va': 0x247F5BC,
        'perf_hint_target_va': 0x247F644,
        'surface_rate_bypass_sites': (
            (0x1B9747C, 'live display refresh'),
            (0x1B982DC, 'runtime FrameCapNext step'),
            (0x1B98BA4, 'thermal transition'),
        ),
        'stall_feature_defaults': (
            (0x1E174A8, '22008052', 'Video.AutoTune default in TuneOnSecond'),
            (0x1B9699C, '22008052', 'Video.LoginFrameCap default in RunInRenderThread'),
            (0x1B96AF4, '22008052', 'Video.FrameCapClimbBackoff default in RunInRenderThread'),
            (0x1B96EE8, '42008052', 'Video.CpuSpeedLog per-frame sampler default in RunInRenderThread'),
            (0x2483AD0, '22008052', 'Video.StackSampler 5ms SIGPROF sampler default'),
        ),
        'enable_feature_defaults': (
            (0x1E1947C, 'e2031f2a', '42008052', 'Video.PerfHint=2 ADPF CPU scheduler session'),
            (0x1E194A0, 'e2031f2a', '22008052', 'Video.PateFrameLazy=1 skip unchanged overhead nameplates'),
            (0x1E19554, 'e2031f2a', '22008052', 'Video.SkinTickQueueHash=1 deduplicate skeleton tick queue'),
            (0x1E1959C, 'e2031f2a', '22008052', 'Video.UITickLite=1 skip unchanged UI tree rebuilds'),
            (0x1E19680, 'e2031f2a', '22008052', 'Video.GfxFillSplit=1 split GFX fills across task workers'),
            (0x1E19C9C, 'e2031f2a', '22008052', 'Video.PortraitLazy=1 defer unchanged HUD portrait redraws'),
            (0x1E19D50, 'e2031f2a', '22008052', 'Video.ThreadNice=1 render and worker thread priority'),
            (0x1E19E88, 'e2031f2a', '22008052', 'Video.MatrixSIMD=1 ARM64 NEON matrix transforms'),
            (0x1E19F38, 'e2031f2a', '22008052', 'Video.BlendMatLite=1 skip unmasked bone skinning math safely'),
            (0x282DD60, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on init'),
            (0x283538C, 'e2031f2a', '22008052', 'Video.RHIDrawFast=1 cheaper DoDraw path on live reload'),
            (0x282EC60, '02008012', '22008052', 'Video.RHIFrameStreamDraw=1 skip CPU hash on GFX dynamic vertex stream'),
            (0x285F888, '00791f53', '20008052', 'Video.Upscale3D=1 (SGSR) default in fast Upscale3D getter'),
            (0x285F934, '15791f53', '35008052', 'Video.Upscale3D=1 (SGSR) default in A3DRHIUpscale setup'),
            (0x29A8228, 'e2d79f1a', '22008052', 'Video.GfxCapNoTick=1 cap excess VFX emitter ticks on Tier 1'),
            (0x29A8284, 'e2031f2a', '22008052', 'Video.GfxWaterOnce=1 merge clipped water flush into single pass'),
        ),
        'frame_cap_global': 0x3234100,
        'cap_ini_force': 0x1B96574,
        'cap_ini_clamp': 0x1B96578,
        'startup_120': 0x1B96778,
        'startup_90_inc': (0x1B9678C, 0x1B967D0),
        'startup_89': 0x1B967A4,
        'startup_89_inc': (0x1B967B0, 0x1B967D0),
        'startup_rates': 0x1B967BC,
        'startup_rates_inc': 0x1B967CC,
        'startup_mid_inc': (0x1B967E4, 'b5000036'),
        'startup_mid': 0x1B967E8,
        'startup_floor': 0x1B96804,
        'live_120': 0x1B972A0,
        'live_90_inc': (0x1B972AC, 0x1B972E0),
        'live_89': 0x1B972C0,
        'live_89_inc': (0x1B972CC, 0x1B972E0),
        'live_rates': 0x1B972D4,
        'live_rates_inc': 0x1B972DC,
        'live_mid_inc': 0x1B972F0,
        'live_mid': 0x1B972FC,
        'live_floor': 0x1B97314,
        'fallback_rung': 0x1B97A78,
        'login_drop_bypass': (0x1B99914, 0x1B999C8),
        'shared_climb_mark': 0x1B97E9C,
        'never_step_down': (0x1B999F8, 0x1B99AC0),
        'step_one_rung': 0x1B99A48,
        'short_hold_ladder': 0x1B99AC0,
        'climb_mark_ladder': 0x1B99C44,
        'short_hold_backoff': 0x1B99D30,
        'climb_mark_backoff': 0x1B99E40,
        'first_tick_overwrite': (0x1B98788, '480101b9'),
        'thermal_cap_bypass': (0x1B987C8, 0x1B98818),
        'rhi_thermal_tier0': (0x1B98B54, 0x1B98B78),
        'tune_thermal_bypass': (0x1E16D8C, 0x1E16DA0, 'ab000054'),
        'safe_profile_bypass': (0x1E1A450, 0x1E1A470, '0a010034'),
        'lowmem_pct_str': 0x125AA07,
        'lowmem_critical': 0x1B9FDF8,
        'lowmem_holdoff': 0x1B9FE00,
        'rhi_mem_cache': 0x1B9FFC4,
        'rhi_dyn_cache': 0x1B9FFDC,
        'ui_optimize_all': 0x1D19830,
        'gles_create_swap0': 0x24D97DC,
        'gles_resize_swap0': 0x24DA998,
        'vk_vsync_off': 0x24FCCFC,
        'preserved_spans': (
            (0x2455868, 0x2458978 - 0x2455868, True),
            (0x246AEA8, 132, True),
            (0x1262AEC, len(ORIGINAL_DIAGNOSTIC), False),
            (0x23C8144, 0x30, True),
            (0x23C85D0, 160, True), (0x1C6B0D8, 112, True), (0x1C3D2E0, 1048, True),
            (0x1C339E4, 68, True), (0x21AD448, 76, True), (0x21AD494, 76, True),
        ),
    },
}


def _bl_instruction(at, target):
    """Encode ARM64 BL relative call while validating alignment and range."""
    displacement = target - at
    if at % 4 or target % 4 or not -(1 << 27) <= displacement < (1 << 27):
        raise ValueError('ARM64 BL target is unaligned or out of range')
    return struct.pack('<I', 0x94000000 | ((displacement // 4) & 0x3FFFFFF))


def _fps45_edits(version='2411', *, cfg=None, arm=None):
    """Return hard-locked 45 FPS and anti-hitch edits for the requested TruTien profile."""
    if cfg is None:
        cfg = VERSION_CONFIGS[version]
    if arm is None:
        arm = ARM64_PROFILES[version]
    profile_edits = tuple(
        Edit(f'set mobile profile FrameCap ceiling {idx} to 45 fps', va,
             bytes.fromhex('00000000'), bytes.fromhex('2d000000'), executable=False)
        for idx, va in enumerate(cfg['profile_cap_vas'])
    )
    worker_edits = tuple(
        Edit(f'enable mobile profile T{idx} {key} worker offload', base + offset,
             bytes.fromhex('00000000'), bytes.fromhex(after_hex), executable=False)
        for idx, base in enumerate(cfg['profile_worker_bases'])
        for offset, key, after_hex in _PROFILE_WORKER_SLOTS
    )
    gfx_task_edits = tuple(
        Edit(f'keep mobile profile T{idx} gfx task worker threads always on (GfxTaskAuto=0)', va,
             bytes.fromhex('01000000'), bytes.fromhex('00000000'), executable=False)
        for idx, va in enumerate(cfg['profile_gfx_task_auto_vas'])
    ) + tuple(
        Edit(f'enable mobile profile T{idx} {key} optimization', base + offset,
             bytes.fromhex('00000000'), bytes.fromhex('01000000'), executable=False)
        for idx, base in enumerate(cfg['profile_all_bases'])
        for offset, key in ((0x64, 'GfxModelSlotSkip'), (0x68, 'GfxSlotStateCache'))
    )
    surface_bypass_edits = tuple(
        Edit(f'skip mid-game Surface.setFrameRate stall on {label} (keep ADPF target)', va,
             _bl_instruction(va, cfg['set_frame_cap_va']), _bl_instruction(va, cfg['perf_hint_target_va']))
        for va, label in cfg['surface_rate_bypass_sites']
    )
    disable_feature_edits = tuple(
        Edit(f'disable {label} (=0) unless user overrides INI', va,
             bytes.fromhex(orig_hex), bytes.fromhex('e2031f2a'))
        for va, orig_hex, label in cfg['stall_feature_defaults']
    )
    enable_feature_edits = tuple(
        Edit(f'enable {label} by default', va,
             bytes.fromhex(orig_hex), bytes.fromhex(after_hex))
        for va, orig_hex, after_hex, label in cfg['enable_feature_defaults']
    )
    ft_va, ft_orig = cfg['first_tick_overwrite']
    sm = cfg['startup_mid_inc']
    sm_va, sm_orig = sm if isinstance(sm, tuple) else (sm, 'b4000036')
    tt = cfg['tune_thermal_bypass']
    tt_from, tt_to = tt[:2]
    tt_orig = tt[2] if len(tt) > 2 else 'ab010054'
    extra_direct = ()
    if cfg.get('safe_profile_bypass'):
        sp_from, sp_to, sp_orig = cfg['safe_profile_bypass']
        extra_direct = (
            Edit('bypass TRUTIEN_TIER_SAFE 30 fps SafeProfile selection in GetMobileProfile',
                 sp_from, bytes.fromhex(sp_orig), branch_instruction(sp_from, sp_to)),
        )
    direct = (
        Edit('start initial FrameCap at 45 fps instead of 30 fps', cfg['frame_cap_global'],
             bytes.fromhex('1e000000'), bytes.fromhex('2d000000'), executable=False),
        Edit('force Video.FrameCap INI value to 45 fps even when <=0 or 30', cfg['cap_ini_force'],
             bytes.fromhex('8d000054'), bytes.fromhex('b3058052')),
        Edit('clamp positive Video.FrameCap INI ceiling to at most 45 fps', cfg['cap_ini_clamp'],
             bytes.fromhex('28008052'), branch_instruction(cfg['cap_ini_clamp'], arm['cap45_ceiling_va'])),
        Edit('set >=120Hz startup primary rung to 45 fps', cfg['startup_120'],
             bytes.fromhex('8a078052'), bytes.fromhex('aa058052')),
        Edit('always include 45 fps rung on >=90Hz startup ladder', cfg['startup_90_inc'][0],
             bytes.fromhex('28020034'), branch_instruction(*cfg['startup_90_inc'])),
        Edit('set <=89Hz startup primary rung to 45 fps', cfg['startup_89'],
             bytes.fromhex('8a078052'), bytes.fromhex('aa058052')),
        Edit('always include 45 fps rung on <=89Hz startup ladder', cfg['startup_89_inc'][0],
             bytes.fromhex('12000014'), branch_instruction(*cfg['startup_89_inc'])),
        Edit('set TRUTIEN_RATES startup primary rung to 45 fps', cfg['startup_rates'],
             bytes.fromhex('89078052'), bytes.fromhex('a9058052')),
        Edit('always include 45 fps rung when TRUTIEN_RATES lacks 90/180Hz', cfg['startup_rates_inc'],
             bytes.fromhex('37feff34'), bytes.fromhex('1f2003d5')),
        Edit('always include middle slot on startup ladder', sm_va,
             bytes.fromhex(sm_orig), bytes.fromhex('1f2003d5')),
        Edit('lock startup middle rung to 45 fps instead of 40 fps', cfg['startup_mid'],
             bytes.fromhex('09058052'), bytes.fromhex('a9058052')),
        Edit('lock startup floor rung to 45 fps instead of 30 fps', cfg['startup_floor'],
             bytes.fromhex('ca038052'), bytes.fromhex('aa058052')),
        Edit('set >=120Hz live refresh primary rung to 45 fps', cfg['live_120'],
             bytes.fromhex('89078052'), bytes.fromhex('a9058052')),
        Edit('always include 45 fps rung on >=90Hz live display refresh', cfg['live_90_inc'][0],
             bytes.fromhex('a8010034'), branch_instruction(*cfg['live_90_inc'])),
        Edit('set <=89Hz live refresh primary rung to 45 fps', cfg['live_89'],
             bytes.fromhex('88078052'), bytes.fromhex('a8058052')),
        Edit('always include 45 fps rung on <=89Hz live display refresh', cfg['live_89_inc'][0],
             bytes.fromhex('0e000014'), branch_instruction(*cfg['live_89_inc'])),
        Edit('set live TRUTIEN_RATES primary rung to 45 fps', cfg['live_rates'],
             bytes.fromhex('88078052'), bytes.fromhex('a8058052')),
        Edit('always include 45 fps rung on live TRUTIEN_RATES refresh', cfg['live_rates_inc'],
             bytes.fromhex('b8feff34'), bytes.fromhex('1f2003d5')),
        Edit('always include middle slot on live refresh ladder', cfg['live_mid_inc'],
             bytes.fromhex('b6000036'), bytes.fromhex('1f2003d5')),
        Edit('lock live refresh middle rung to 45 fps instead of 40 fps', cfg['live_mid'],
             bytes.fromhex('0a058052'), bytes.fromhex('aa058052')),
        Edit('lock live refresh floor rung to 45 fps instead of 30 fps', cfg['live_floor'],
             bytes.fromhex('ca038052'), bytes.fromhex('aa058052')),
        Edit('lock FrameCapNext fallback rung to 45 fps instead of 30 fps', cfg['fallback_rung'],
             bytes.fromhex('c8038052'), bytes.fromhex('a8058052')),
        Edit('bypass FrameCapLogin drop inside FrameCapNext', cfg['login_drop_bypass'][0],
             bytes.fromhex('ac050034'), branch_instruction(*cfg['login_drop_bypass'])),
        Edit('raise shared climb mark fraction from 85% to 97% for PerfHint/AutoTune', cfg['shared_climb_mark'],
             bytes.fromhex('ba0a8052'), bytes.fromhex('3a0c8052')),
        Edit('never step down from 45 fps in FrameCapNext', cfg['never_step_down'][0],
             bytes.fromhex('0e7d8052'), branch_instruction(*cfg['never_step_down'])),
        Edit('step down one FPS ladder rung at a time if ever reached', cfg['step_one_rung'],
             bytes.fromhex('4bffff54'), bytes.fromhex('1f2003d5')),
        Edit('short-hold climb check from 10s to 3s at ladder recovery path', cfg['short_hold_ladder'],
             bytes.fromhex('1f290071'), bytes.fromhex('1f0d0071')),
        Edit('raise ladder climb mark fraction from 85% to 97% (21ms mark -> <=20ms climbs to 45fps)',
             cfg['climb_mark_ladder'], bytes.fromhex('ad0a8052'), bytes.fromhex('2d0c8052')),
        Edit('short-hold climb check from 10s to 3s at backoff recovery path', cfg['short_hold_backoff'],
             bytes.fromhex('1f290071'), bytes.fromhex('1f0d0071')),
        Edit('raise backoff climb mark fraction from 85% to 97% (21ms mark -> <=20ms climbs to 45fps)',
             cfg['climb_mark_backoff'], bytes.fromhex('a90a8052'), bytes.fromhex('290c8052')),
        Edit('bypass missed thermal/first-tick FrameCap overwrite below 45 fps in RunInRenderThread',
             ft_va, bytes.fromhex(ft_orig), bytes.fromhex('1f2003d5')),
        Edit('bypass ThermalTier FPS cap drop in RunInRenderThread', cfg['thermal_cap_bypass'][0],
             bytes.fromhex('8d020054'), branch_instruction(*cfg['thermal_cap_bypass'])),
        Edit('keep A3DRHISetThermalTier at tier 0 in RunInRenderThread', cfg['rhi_thermal_tier0'][0],
             bytes.fromhex('36010034'), branch_instruction(*cfg['rhi_thermal_tier0'])),
        Edit('bypass ThermalTier quality drop in TuneOnSecond', tt_from,
             bytes.fromhex(tt_orig), branch_instruction(tt_from, tt_to)),
        Edit('raise <=4GB LowMemFootprintPct threshold from 72,77 to 86,91 to stop 2s cache thrashing',
             cfg['lowmem_pct_str'], bytes.fromhex('37322c3737'), bytes.fromhex('38362c3931'), executable=False),
        Edit('lower <=4GB LowMemCriticalMB default from 350 to 250 MB while preserving OOM guard',
             cfg['lowmem_critical'], bytes.fromhex('ca2b8052'), bytes.fromhex('4a1f8052')),
        Edit('lower <=4GB LowMemHoldOffMB default from 450 to 320 MB',
             cfg['lowmem_holdoff'], bytes.fromhex('4b388052'), bytes.fromhex('0b288052')),
        Edit('raise <=4GB RHIMemCacheMB default from 32 to 64 MB',
             cfg['rhi_mem_cache'], bytes.fromhex('09048052'), bytes.fromhex('09088052')),
        Edit('raise <=4GB RHIDynCacheMB default from 16 to 64 MB to stop UP draw cache overflow',
             cfg['rhi_dyn_cache'], bytes.fromhex('0b028052'), bytes.fromhex('0b088052')),
        Edit('disable automatic UIOptimizeAll=1 hidden-panel texture eviction by default',
             cfg['ui_optimize_all'], bytes.fromhex('e8279f1a'), bytes.fromhex('e8031f2a')),
        Edit('force eglSwapInterval(0) on GLES context create to avoid 60->30Hz VSync quantization',
             cfg['gles_create_swap0'], bytes.fromhex('011d0753'), bytes.fromhex('e1031f2a')),
        Edit('force eglSwapInterval(0) on GLES context reset/swap for ANGLE/OpenGL ES 45fps pacing',
             cfg['gles_resize_swap0'], bytes.fromhex('811e0753'), bytes.fromhex('e1031f2a')),
        Edit('prefer non-blocking MAILBOX/IMMEDIATE/FIFO_RELAXED in Vulkan SwapChainVK vsync=false',
             cfg['vk_vsync_off'], _VK_VSYNC_OFF_ORIG, _VK_VSYNC_OFF_MAILBOX_FIRST),
    )
    return (profile_edits + worker_edits + gfx_task_edits + surface_bypass_edits
            + disable_feature_edits + enable_feature_edits + direct + extra_direct)


def edits(version='2411', *, cfg=None, arm=None):
    """Return the HUD hooks and hard-locked 45 FPS edits for the specified TruTien profile."""
    if cfg is None:
        if version not in VERSION_CONFIGS:
            raise ValueError('Unsupported TruTien version: ' + str(version))
        cfg = VERSION_CONFIGS[version]
    if arm is None:
        arm = ARM64_PROFILES[version]
    return (
        Edit('append paired Sit/Stand and Jump medallions after native combat HUD', cfg['draw_hook'],
             bytes.fromhex('6afcff17'), branch_instruction(cfg['draw_hook'], arm['draw_va'])),
        Edit('classify paired Sit/Stand (action 75) and Jump (action 74) after panel priority', cfg['input_hook'],
             bytes.fromhex('2041601e'), branch_instruction(cfg['input_hook'], arm['input_va'])),
        Edit('dispatch Jump (action 74) and Sit/Stand (action 75) after native player checks', cfg['action_hook'],
             bytes.fromhex('3f1d0171'), branch_instruction(cfg['action_hook'], arm['action_va'])),
        Edit('mirror host sitting bit (+0x9ec bit 5) into HUDDATA +0x8ab2', cfg['fill_hook'],
             bytes.fromhex('48030239'), branch_instruction(cfg['fill_hook'], arm['fill_va'])),
    ) + _fps45_edits(version, cfg=cfg, arm=arm)


def patch_library(data, *, allow_auto=True, force_auto=False):
    """Apply the Sit/Stand native hooks and appended ELF segment to a TruTien libmain.so."""
    sha = hashlib.sha256(data).hexdigest()
    version = SHA256_TO_VERSION.get(sha)
    auto_discovered = False
    if version is not None and not force_auto:
        cfg = VERSION_CONFIGS[version]
        arm = ARM64_PROFILES[version]
        code, label = build_payload(version)
        planned = edits(version)
        u_ranges = unwind_ranges(version)
    elif allow_auto:
        cfg = discover_trutien_profile(data)
        arm, code, label = build_auto_profile_and_payload(cfg)
        planned = edits(cfg=cfg, arm=arm)
        u_ranges = unwind_ranges('2411')
        auto_discovered = True
    else:
        raise ValueError('Unsupported/already modified libmain.so; original library SHA-256 must match')

    hooked, evidence = apply_edits(data, planned)
    result, extension = append_payload(hooked, code, label, code_va=arm['code_va'],
                                       unwind_ranges=u_ranges)
    segments = load_segments(data)
    for start, size, executable in cfg['preserved_spans']:
        at = file_offset(segments, start, size, executable)
        if result[at:at + size] != data[at:at + size]:
            raise ValueError('Original Khinh cong, jump physics or CmdSitDown code changed')
    extra_ev = {'name': 'separate Sit/Stand code, labels and unwind metadata',
                'autoDiscovered': auto_discovered, **extension}
    if cfg.get('version_code'):
        extra_ev['gameVersionCode'] = cfg['version_code']
    evidence.append(extra_ev)
    return result, evidence


def prepare_apk(source, destination, *, force_auto=False):
    """Create an unsigned TruTien APK with the native Sit/Stand toggle button and 45 FPS lock."""
    orig_sha = None
    version_code = None
    auto_mode = force_auto
    with zipfile.ZipFile(Path(source).resolve(strict=True)) as z:
        names = set(z.namelist())
        if LIBRARY_ENTRY in names:
            orig_sha = hashlib.sha256(z.read(LIBRARY_ENTRY)).hexdigest()
            ver = SHA256_TO_VERSION.get(orig_sha)
            if ver is not None:
                version_code = VERSION_CONFIGS[ver]['version_code']
            else:
                auto_mode = True
                if 'AndroidManifest.xml' in names:
                    version_code = parse_axml_version_code(z.read('AndroidManifest.xml'))
    extra = {
        'patchVersion': 'separate-jump-and-sit-v9-45fps-turbo',
        'existingSitAndStandCodeUnchanged': True,
        'maxFpsTarget': 45,
        'autoDiscovered': auto_mode,
        'scope': (
            'Paired custom Wuxia PNG HUD buttons Ngồi/Đứng (action75 -> CmdSitDown) and Nhảy (action74) '
            'at 0.625x Attack scale with bezel-safe margins, hard-locked 45 FPS across every scene '
            '([45] locked ladders, INI/login/thermal/first-tick/FrameCapNext step-down lockouts, '
            'clock_nanosleep 22.22ms pacing), non-blocking presentation on BOTH Vulkan '
            '(MAILBOX-first SwapChainVK) and OpenGL ES (eglSwapInterval(0) on ANGLE/GLES), full T0..T4 worker '
            'pool + CPU/draw/GPU savings (JobPool=1, MoveJobs=1, ChildSkinJobs=1, RegisterJobs=3, '
            'GfxLateRegister=1, GfxTaskAuto=0, GfxFillSplit=1, GfxModelSlotSkip=1, GfxSlotStateCache=1, '
            'SkinTickQueueHash=1, UITickLite=1, PortraitLazy=1, BlendMatLite=1, RHIFrameStreamDraw=1, '
            'GfxCapNoTick=1, GfxWaterOnce=1, DynRes=1+Upscale3D=1 SGSR), anti-thrash memory cache defaults '
            '(LowMemFootprintPct=86,91, LowMemCriticalMB=250, RHIMemCacheMB=64, RHIDynCacheMB=64, '
            'UIOptimizeAll=0), StackSampler=0, CpuSpeedLog=0, AutoTune=0, PateFrameLazy=1, '
            'RHIDrawFast=1, PerfHint=2, ThreadNice=1, MatrixSIMD=1, and mid-game Surface.setFrameRate '
            'stall bypasses; original Khinh cong/control16/action35 and jump physics preserved.'
        ),
    }
    if orig_sha is not None and (version_code is not None or auto_mode):
        extra['originalLibrarySHA256'] = orig_sha
    if version_code is not None:
        extra['gameVersionCode'] = version_code
    patch_fn = (lambda raw: patch_library(raw, force_auto=True)) if force_auto else patch_library
    return _base_prepare_apk(source, destination, patch_fn=patch_fn, report_extra=extra)


def main():
    """CLI entry point for building the patched Sit/Stand APK and report."""
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--apk', type=Path, required=True)
    ap.add_argument('--output', type=Path, required=True)
    ap.add_argument('--report', type=Path, required=True)
    ap.add_argument('--auto', action='store_true', help='Force dynamic ELF discovery instead of static SHA profile lookup')
    args = ap.parse_args()
    try:
        if args.report.resolve() in (args.apk.resolve(), args.output.resolve()) or args.report.exists():
            raise ValueError('Choose a new separate report path')
        report = prepare_apk(args.apk, args.output, force_auto=args.auto)
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf8')
        print(json.dumps({'output': str(args.output), 'report': str(args.report),
                          'gameVersionCode': report.get('gameVersionCode'), 'signed': False, 'installed': False}))
    except (ValueError, KeyError, zipfile.BadZipFile, OSError) as error:
        ap.exit(1, 'Sit patch refused: ' + str(error) + '\n')


if __name__ == '__main__':
    main()
