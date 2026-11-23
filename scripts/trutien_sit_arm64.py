# SPDX-License-Identifier: GPL-2.0-or-later
"""Own ARM64 HUD extension: paired Wuxia PNG Sit/Stand (action 75) + Jump (action 74).

Places two mirror-balanced circular HUD medallions cleanly above the combat wheel
at 0.625x Attack scale with guaranteed bezel safe margins and without modifying
Khinh cong (control 16 / action 35) or jump physics. Registers one 512x512 GPU
atlas page once via CECMobileHud::NativePage + A3DTexture::LoadFromMemory and
swaps Ngồi (standing) <-> Đứng (sitting) + Nhảy cells via CHudDrawList::AddCell.
Runtime patching uses only Python stdlib; optional Keystone/Capstone/Pillow verify assets.
"""
import hashlib
from pathlib import Path

CODE_VA = 0x36B8000
CODE_SIZE = 0x1000
LABEL_VA = CODE_VA + CODE_SIZE
LABEL_WHEN_STANDING_VA = LABEL_VA          # UTF-32LE "Ngồi\0"
LABEL_WHEN_SITTING_VA = LABEL_VA + 0x20    # UTF-32LE "Đứng\0"
LABEL_JUMP_VA = LABEL_VA + 0x40            # UTF-32LE "Nhảy\0"
ATLAS_PNG_VA = LABEL_VA + 0x80             # 512x512 packed RGBA PNG atlas

GEOMETRY_VA = CODE_VA + 0x000
ENSURE_ATLAS_VA = CODE_VA + 0x18C
DRAW_VA = CODE_VA + 0x40C
INPUT_VA = CODE_VA + 0x504
ACTION_VA = CODE_VA + 0x5FC
FILL_VA = CODE_VA + 0x660
CAP45_CEILING_VA = CODE_VA + 0x678
DROP_DEBOUNCE_VA = CODE_VA + 0x690

ATLAS_PNG_NAME = 'trutien_icons_atlas.png'
ATLAS_PNG_SIZE = 153982
ATLAS_PNG_SHA256 = 'f707be3321c1c50e030fc67863610ee405968e9d995431ec9d18bd0ce4d23a4a'
CODE_SHA256 = 'bf7c3ed3b491aa8a67ebea15af5938013990458139beee5d59583a52000500d1'

FRAGMENTS = (
    ('geometry', 0x0, r"""
    ldrb w8, [x0, #0x42]
    cbz w8, invalid
    ldr d0, [x0, #0x1b20]
    fcmp d0, #0.0
    b.ls invalid
    ldr d1, [x0, #0x1b28]
    fcmp d1, #0.0
    b.ls invalid

    fmax d2, d0, d1          // s = max(aw, ah) * 0.625
    fmov d3, #0.625
    fmul d2, d2, d3

    fmov d3, #0.5
    ldr d4, [x0, #0x1b10]
    fmadd d4, d0, d3, d4     // atk_cx = ax + 0.5 * aw

    ldr d5, [x0, #0x1b18]    // min_y = ay
    mov w8, #0x14f8
    add x8, x0, x8
    mov w9, #19
scan:
    ldrb w10, [x8, #0x60]
    cbz w10, next
    ldr d0, [x8, #0x10]
    fcmp d0, #0.0
    b.ls next
    ldr d0, [x8, #0x18]
    fcmp d0, #0.0
    b.ls next
    ldr d0, [x8, #8]
    fmin d5, d5, d0
next:
    add x8, x8, #0x68
    subs w9, w9, #1
    b.ne scan

    fmov d0, #0.4375         // y = min_y - s * 1.4375
    fmul d0, d2, d0
    fadd d0, d2, d0
    fsub d5, d5, d0

    fmov d0, #0.375          // gap = s * 0.375
    fmul d16, d2, d0
    fadd d17, d2, d2
    fadd d17, d17, d16       // pair_w = 2*s + gap
    fmov d0, #0.5
    fmul d0, d17, d0
    fsub d6, d4, d0          // sit_x = atk_cx - 0.5 * pair_w

    movi d18, #0
    ldr d0, [x0, #0x14c0]    // screen W
    fcmp d0, #0.0
    b.le fallback_x
    ldr d1, [x0, #0x14d8]    // safe_x
    ldr d3, [x0, #0x14e8]    // safe_w
    fadd d4, d1, d3
    fmaxnm d1, d1, d18       // left_base
    fcmp d3, #0.0
    fcsel d4, d4, d0, gt
    fminnm d4, d4, d0        // right_base
    mov x8, #0x3fa0000000000000 // 0.03125
    fmov d3, x8
    fmul d3, d0, d3
    fmaxnm d3, d2, d3        // pad_x = max(s, W * 0.03125)
    fadd d1, d1, d3          // left_lim
    fsub d4, d4, d3
    fsub d4, d4, d17         // right_lim - pair_w
    fminnm d6, d6, d4
    fmaxnm d6, d6, d1
    b check_y
fallback_x:
    fmov d3, #12.0
    fmaxnm d3, d2, d3
    fmaxnm d6, d6, d3

check_y:
    ldr d0, [x0, #0x14c8]    // screen H
    fcmp d0, #0.0
    b.le fallback_y
    ldr d1, [x0, #0x14e0]    // safe_y
    ldr d3, [x0, #0x14f0]    // safe_h
    fadd d4, d1, d3
    fmaxnm d1, d1, d18       // top_base
    fcmp d3, #0.0
    fcsel d4, d4, d0, gt
    fminnm d4, d4, d0        // bot_base
    mov x8, #0x3fb0000000000000 // 0.0625
    fmov d3, x8
    fmul d3, d0, d3
    fmaxnm d3, d2, d3        // pad_y = max(s, H * 0.0625)
    fadd d1, d1, d3          // top_lim
    fsub d4, d4, d3
    fsub d4, d4, d2          // bot_lim - s
    fminnm d5, d5, d4
    fmaxnm d5, d5, d1
    b store_out
fallback_y:
    fmov d3, #12.0
    fmaxnm d3, d2, d3
    fmaxnm d5, d5, d3

store_out:
    fadd d7, d6, d2
    fadd d7, d7, d16         // jump_x = sit_x + s + gap
    stp d6, d5, [x1]
    stp d2, d2, [x1, #0x10]
    stp d7, d5, [x1, #0x20]
    stp d2, d2, [x1, #0x30]
    mov w0, #1
    ret
invalid:
    mov w0, wzr
    ret
""",
     '08084139080c003400904dfd0820601ea90b005401944dfd2820601e490b00540248611e03906c1e4208631e03106c1e04884dfd0410431f058c4dfd089f82520800088b690280520a8141392a010034000940fd0820601ec9000054000d40fd0820601e69000054000540fda558601e08a101912905007181feff5400906b1e4008601e4028601ea538601e00106b1e5008601e5128621e312a701e00106c1e200a601e8638601e12e4002f00604afd0820601e4d020054016c4afd03744afd2428631e2168721e6820601e84cc601e8478601e08f4e7d20301679e0308631e4368631e2128631e8438631e8438711ec678641ec668611e040000140310651e4368631ec668631e00644afd0820601e4d02005401704afd03784afd2428631e2168721e6820601e84cc601e8478601e08f6e7d20301679e0308631e4368631e2128631e8438631e8438621ea578641ea568611e040000140310651e4368631ea568631ec728621ee728701e2614006d2208016d2714026d2208036d20008052c0035fd6e0031f2ac0035fd6'),
    ('ensure_atlas', 0x18C, r"""
    sub sp, sp, #0x60
    stp x29, x30, [sp, #0x20]
    stp x24, x23, [sp, #0x30]
    stp x22, x21, [sp, #0x40]
    stp x20, x19, [sp, #0x50]
    add x29, sp, #0x20

    cbz x0, fail
    ldr x19, [x0, #0x30]     // atlas = CHudAtlas*
    cbz x19, fail
    mov w8, #0x8610
    movk w8, #3, lsl #16
    sub x20, x19, x8         // hud = CECMobileHud*

    mov w8, #0xb288
    movk w8, #3, lsl #16
    ldr w9, [x20, x8]        // page_cnt
    cmp w9, #0x60
    b.hi fail

    mov w14, #0x4a53
    movk w14, #0x5454, lsl #16 // 'TTJS'
    mov w10, #0xa988
    movk w10, #3, lsl #16
    add x11, x20, x10
    mov x21, xzr
    mov w22, wzr
scan_pages:
    cmp w22, w9
    b.ge not_cached
    ldr x12, [x11]
    cbnz x12, scan_next
    ldr w12, [x11, #8]
    cmp w12, w14
    b.ne scan_next
    mov x21, x11
    b found_slot
scan_next:
    add x11, x11, #0x18
    add w22, w22, #1
    b scan_pages

found_slot:
    mov w8, #0xa980
    movk w8, #3, lsl #16
    ldr w8, [x20, x8]
    add w22, w22, w8         // page_id = base + k
    ldr x8, [x21, #0x10]     // tex
    cbz x8, check_dev
    ldr w10, [x21, #0x0c]    // cell0 + 1
    cbz w10, check_cells
    sub w10, w10, #1
    ldr w11, [x19, #0x18d0]
    add w12, w10, #2
    cmp w12, w11
    b.ge check_cells
    ldr x11, [x19, #0x18c8]
    cbz x11, check_cells
    mov w12, #0x14
    umaddl x11, w10, w12, x11
    ldrsh w12, [x11]
    cmp w12, w22
    b.ne check_cells
    mov w0, w10
    b ret_val

not_cached:
    cmp w9, #0x5f
    b.gt fail
    mov x21, xzr
    mov w22, #-1

check_dev:
    ldr x23, [x20, #0x30]    // A3DDevice*
    cbz x23, fail
    ldr x8, [x23, #0x38]     // IDirect3DDevice9*
    cbz x8, fail

check_cells:
    ldr w8, [x19, #0x18d0]
    ldr w9, [x19, #0x18d4]
    add w8, w8, #3
    cmp w8, w9
    b.gt fail

    cbnz x21, have_page
    mov x0, x20
    mov x1, xzr
    mov w2, #0x4a53
    movk w2, #0x5454, lsl #16
    mov w3, #512
    mov w4, #512
    bl 0x243e66c             // CECMobileHud::NativePage
    tbnz w0, #0x1f, fail
    mov w22, w0
    mov w8, #0xa980
    movk w8, #3, lsl #16
    ldr w9, [x20, x8]
    sub w9, w22, w9
    cmp w9, #0x60
    b.hs fail
    add x8, x20, x8
    add x8, x8, #8
    mov w10, #0x18
    umaddl x21, w9, w10, x8
    str wzr, [x21, #0x0c]

have_page:
    ldr x24, [x21, #0x10]
    cbnz x24, add_cells
    ldr x23, [x20, #0x30]
    cbz x23, fail
    mov w0, #0x90
    bl 0x256de64             // operator new(0x90)
    cbz x0, fail
    mov x24, x0
    bl 0x27cacb0             // A3DTexture::A3DTexture
    mov x0, x24
    mov x1, x23
    adr x2, 0x36b9080        // ATLAS_PNG_VA
    movz w3, #0x597e
    movk w3, #2, lsl #16     // ATLAS_PNG_SIZE = 153982
    mov w4, wzr
    mov w5, wzr
    mov w6, wzr
    mov w7, #1
    str xzr, [sp]
    bl 0x27cd770             // A3DTexture::LoadFromMemory
    cbnz w0, store_tex
    ldr x8, [x24]
    mov x0, x24
    ldr x8, [x8, #8]
    blr x8
    b fail
store_tex:
    str x24, [x21, #0x10]

add_cells:
    mov w24, wzr
    mov w23, #-1
cell_loop:
    cmp w24, #1
    mov w8, #4
    mov w9, #260
    csel w2, w9, w8, eq
    cmp w24, #2
    csel w3, w9, w8, eq
    mov w4, #248
    mov w5, #248
    mov w6, wzr
    mov w7, wzr
    mov x0, x19
    mov w1, w22
    str wzr, [sp]
    str wzr, [sp, #8]
    mov w8, #-1
    str w8, [sp, #0x10]
    bl 0x2404d90             // CHudAtlas::AddCellRuntime
    tbnz w0, #0x1f, fail
    cbnz w24, check_seq
    mov w23, w0
    b next_cell
check_seq:
    add w8, w23, w24
    cmp w0, w8
    b.ne fail
next_cell:
    add w24, w24, #1
    cmp w24, #3
    b.lt cell_loop

    add w8, w23, #1
    str w8, [x21, #0x0c]
    mov w0, w23
    b ret_val

fail:
    mov w0, #-1
ret_val:
    ldp x20, x19, [sp, #0x50]
    ldp x22, x21, [sp, #0x40]
    ldp x24, x23, [sp, #0x30]
    ldp x29, x30, [sp, #0x20]
    add sp, sp, #0x60
    ret
""",
     'ff8301d1fd7b02a9f85f03a9f65704a9f44f05a9fd830091601200b4131840f9331200b408c290526800a072740208cb085196526800a072896a68b83f810171281100546e4a89528e8aaa720a3195526a00a0728b020a8bf5031faaf6031f2adf02096b2a0400546c0140f9cc0000b56c0940b99f010e6b61000054f5030baa040000146b610091d6060011f5ffff17083095526800a072886a68b8d602080ba80a40f9a80200b4aa0e40b9ea0200344a0500516bd258b94c0900119f010b6b4a0200546b664cf90b0200b48c0280524b2dac9b6c01c0799f01166b61010054e0030a2a610000143f7d0171cc0b0054f5031faa16008012971a40f9570b00b4e81e40f9080b00b468d258b969d658b9080d00111f01096b6c0a0054b50200b5e00314aae1031faa624a8952828aaa720340805204408052ea18b6974009f837f603002a083095526800a072896a68b8c902094b3f810171620800548802088b082100910a0380523521aa9bbf0e00b9b80a40f9780300b5971a40f9570700b400128052d5d6ba97e00600b4f80300aa654ac497e00318aae10317aa1f2003d5a26a0010c32f8b524300a072e4031f2ae5031f2ae6031f2a27008052ff0300f90955c497c0000035080340f9e00318aa080540f900013fd623000014b80a00f9f8031f2a170080121f07007188008052892080522201881a1f0b00712301881a041f8052051f8052e6031f2ae7031f2ae00313aae103162aff0300f9ff0700f908008012e81300b97732b597c001f83778000035f703002a04000014e802180b1f00086b01010054180700111f0f0071cbfcff54e8060011a80e00b9e003172a0200001400008012f44f45a9f65744a9f85f43a9fd7b42a9ff830191c0035fd6'),
    ('drawing', 0x40C, r"""
    sub sp, sp, #0x120
    ldr x0, [x23, #8]
    mov x1, sp
    bl 0x36b8000             // geometry
    cbz w0, finished

    ldr x19, [x23]
    ldr x20, [x23, #0x10]
    mov w21, wzr
    cbz x20, have_state
    mov w8, #0x8ab2
    ldrb w21, [x20, x8]
have_state:
    mov x0, x19
    bl 0x36b818c             // ensure_atlas
    mov w22, w0

    mov w25, wzr
    mov x26, sp
btn_loop:
    ldp d8, d9, [x26]
    ldr d10, [x26, #0x10]

    mov x8, #0x3fa0000000000000 // 0.03125
    fmov d0, x8
    fmul d1, d10, d0
    fadd d2, d1, d1
    fsub d2, d10, d2         // s_in = 0.9375 * s
    fadd d3, d8, d1
    fadd d4, d9, d1
    add x1, sp, #0x40
    stp d3, d4, [x1]
    stp d2, d2, [x1, #0x10]

    cbnz w25, glass_palette
    cbz w21, glass_palette
    movz w2, #0x190c
    movk w2, #0x9c24, lsl #16
    movz w3, #0xca8a
    movk w3, #0xffe8, lsl #16
    fmov d1, #1.25
    b draw_disc
glass_palette:
    movz w2, #0x1018
    movk w2, #0x7e0a, lsl #16
    movz w3, #0xe8f8
    movk w3, #0x70dc, lsl #16
    fmov d1, #0.75
draw_disc:
    mov x0, x19
    fmov d0, d2
    mov w4, #1
    bl 0x24088e8             // CHudDrawList::AddRoundRect

    tbnz w22, #0x1f, next_btn
    cbnz w25, cell_jump
    cmp w21, #0
    cinc w1, w22, ne         // cell0 + (1 if sitting else 0)
    b draw_cell
cell_jump:
    add w1, w22, #2          // cell0 + 2
draw_cell:
    mov x0, x19
    mov x2, x26
    mov w3, #-1
    mov w4, wzr
    bl 0x2405988             // CHudDrawList::AddCell

next_btn:
    add x26, x26, #0x20
    add w25, w25, #1
    cmp w25, #2
    b.lt btn_loop

finished:
    add sp, sp, #0x120
    b 0x242de5c
""",
     'ff8304d1e00640f9e1030091fafeff9700070034f30240f9f40a40f9f5031f2a740000b448569152956a6838e00313aa54ffff97f603002af9031f2afa0300914827406d4a0b40fd08f4e7d20001679e4109601e2228611e4239621e0329611e2429611ee10301912310006d2208016d19010035f5000034822183528284b3724351995203fdbf7201906e1e060000140203825242c1af72031f9d52831bae7201106d1ee00313aa4040601e240080520b41b5977601f83799000035bf020071c106961a02000014c10a0011e00313aae2031aaa03008012e4031f2a2835b5975a830091390700113f0b0071abfaff54ff83049157d6b517'),
    ('input', 0x504, r"""
    sub sp, sp, #0x40
    ldrb w8, [x23, #0x19]
    cbnz w8, original
    ldrb w8, [x20, #0x42]
    cbz w8, original
    ldrb w8, [x20, #0x41]
    cbz w8, ui_hide_flag
    ldrb w8, [x20, #0x3a]
    b check_hide_flag
ui_hide_flag:
    ldrb w8, [x23, #0xb]
check_hide_flag:
    cbnz w8, original
    ldr w8, [x23, #0xb0]
    sub w8, w8, #2
    tst w8, #0xfffffffd
    b.eq original

    mov x0, x20
    mov x1, sp
    bl 0x36b8000
    cbz w0, original
    mov w9, #75
    mov x10, sp
hit_loop:
    ldp d0, d1, [x10]
    fcmp d9, d0
    b.lt next_btn
    fcmp d8, d1
    b.lt next_btn
    ldr d2, [x10, #0x10]
    fadd d3, d0, d2
    fcmp d9, d3
    b.hs next_btn
    fadd d3, d1, d2
    fcmp d8, d3
    b.hs next_btn
    fmov d3, #0.5
    fmul d2, d2, d3
    fadd d0, d0, d2
    fadd d1, d1, d2
    fsub d0, d9, d0
    fsub d1, d8, d1
    fmul d0, d0, d0
    fmadd d0, d1, d1, d0
    fmul d2, d2, d2
    fcmp d0, d2
    b.ge next_btn

    mov w8, #4
    str w8, [x19]
    mov w8, #1
    str w8, [x19, #4]
    str w9, [x19, #8]
    str w9, [x19, #0xc]
    stp xzr, xzr, [x19, #0x10]
    ldp q0, q1, [x10]
    stp q0, q1, [x19, #0x20]
    add sp, sp, #0x40
    b 0x24113c8
next_btn:
    add x10, x10, #0x20
    sub w9, w9, #1
    cmp w9, #74
    b.eq hit_loop

original:
    add sp, sp, #0x40
    fmov d0, d9
    b 0x24114ac
""",
     'ff0301d1e866403928070035880a4139e8060034880641396800003488ea403902000014e82e403928060035e8b240b9080900511f791e72a0050054e00314aae1030091aefeff972005003469098052ea0300914005406d2021601e0b0400540021611ecb030054420940fd0328621e2021631e420300542328621e0021631ee202005403106c1e4208631e0028621e2128621e2039601e0139611e0008601e2000411f4208621e0020621e8a01005488008052680200b928008052680600b9690a00b9690e00b97f7e01a9400540ad600601adff0301917b63b5174a810091290500513f29017160fbff54ff0301912041601ead63b517'),
    ('action', 0x5FC, r"""
    cmp w9, #73
    b.ne check_sit
    adrp x10, 0x31ab000
    ldr x10, [x10, #0xce0]
    cbz x10, done
    ldr w25, [x10, #0x40]
    b 0x23abd8c
check_sit:
    cmp w9, #74
    b.ne original_bounds
    adrp x10, 0x31ab000
    ldr x10, [x10, #0xce0]
    cbz x10, done
    ldr w25, [x10, #0x40]
    cmp w25, #2
    b.eq done
    ldr w8, [x19, #0x9ec]
    mov x0, x19
    tst w8, #0x20
    cset w1, eq
    bl 0x1c22e00
done:
    b 0x23aba90
original_bounds:
    cmp w9, #71
    b.ls original_dispatch
    b 0x23aba94
original_dispatch:
    b 0x23ab878
""",
     '3f250171c10000548ad7fff04a7146f90a0200b4594140b9decdb3173f290171a10100548ad7fff04a7146f92a0100b4594140b93f0b0071c000005468ee49b9e00313aa1f011b72e1179f1aeea9959711cdb3173f1d0171490000540fcdb31787ccb317'),
    ('fill', 0x660, r"""
    ldrb w8, [x1, #0x9ec]
    ubfx w9, w8, #3, #1
    strb w9, [x26, #0x80]
    ubfx w9, w8, #5, #1
    strb w9, [x26, #0x82]
    b 0x2402094
""",
     '28b06739090d03534903023909150553490b02398826b517'),
    ('cap45_ceiling', 0x678, r"""
    cmp w19, #45
    mov w8, #45
    csel w19, w8, w19, gt
    mov w8, #1
    stp w8, w19, [sp, #0x50]
    b 0x1b869f4
""",
     '7fb60071a805805213c1931a28008052e84f0a29da389317'),
    ('drop_debounce', 0x690, r"""
    mov w14, #0x3e8
    sdiv w14, w14, w20
    add w14, w14, #1
    cmp w13, w14
    b.le stay
    ldr w15, [x23, #8]
    cmp w15, w14
    b.le stay
    ldr w15, [x19, #0x50]
    cmp w15, #3
    b.lt stay
    ldr w15, [x19, #4]
    cmp w15, w14
    b.le stay
    ldr w15, [x19, #8]
    cmp w15, w14
    b.gt do_drop
stay:
    b 0x1b89cac
do_drop:
    b 0x1b89bf8
""",
     '0e7d8052ce0dd41ace050011bf010e6bad010054ef0a40b9ff010e6b4d0100546f5240b9ff0d0071eb0000546f0640b9ff010e6b8d0000546f0a40b9ff010e6b4c0000547645931748459317'),
)

CODE_VA_2503 = 0x36E4000
LABEL_VA_2503 = CODE_VA_2503 + CODE_SIZE
ATLAS_PNG_VA_2503 = LABEL_VA_2503 + 0x80
CODE_SHA256_2503 = 'f2eda0e74d2f3cb91d3e2818e2ac35d5fac0f724ed4989684a18720d4ac39ac5'

CODE_VA_2685 = 0x3808000
LABEL_VA_2685 = CODE_VA_2685 + CODE_SIZE
ATLAS_PNG_VA_2685 = LABEL_VA_2685 + 0x80
CODE_SHA256_2685 = '313cd20c704b02d9da816b5a6f9d6b47c6dd6b08462388ad05fbb02b47ee7653'

# v0.909a (versionCode 2503) fragments: same verified ARM64 control flow with
# 2503 VAs and CECMobileHud +0x650 member shift (CHudAtlas 0x38C60, NativePage 0x3AFD0/0x3AFD8/0x3B8D8).
_HEX_2503 = {
    'geometry': FRAGMENTS[0][3],
    'ensure_atlas': (
        'ff8301d1fd7b02a9f85f03a9f65704a9f44f05a9fd830091601200b4131840f9'
        '331200b4088c91526800a072740208cb081b97526800a072896a68b83f810171'
        '281100546e4a89528e8aaa720afb95526a00a0728b020a8bf5031faaf6031f2a'
        'df02096b2a0400546c0140f9cc0000b56c0940b99f010e6b61000054f5030baa'
        '040000146b610091d6060011f5ffff1708fa95526800a072886a68b8d602080b'
        'a80a40f9a80200b4aa0e40b9ea0200344a0500516bd258b94c0900119f010b6b'
        '4a0200546b664cf90b0200b48c0280524b2dac9b6c01c0799f01166b61010054'
        'e0030a2a610000143f7d0171cc0b0054f5031faa16008012971a40f9570b00b4'
        'e81e40f9080b00b468d258b969d658b9080d00111f01096b6c0a0054b50200b5'
        'e00314aae1031faa624a8952828aaa72034080520440805204cdb5974009f837'
        'f603002a08fa95526800a072896a68b8c902094b3f810171620800548802088b'
        '082100910a0380523521aa9bbf0e00b9b80a40f9780300b5971a40f9570700b4'
        '001280526e8bba97e00600b4f80300aa9f04c497e00318aae10317aa1f2003d5'
        'a26a0010c32f8b524300a072e4031f2ae5031f2ae6031f2a27008052ff0300f9'
        '430fc497c0000035080340f9e00318aa080540f900013fd623000014b80a00f9'
        'f8031f2a170080121f07007188008052892080522201881a1f0b00712301881a'
        '041f8052051f8052e6031f2ae7031f2ae00313aae103162aff0300f9ff0700f9'
        '08008012e81300b947e6b497c001f83778000035f703002a04000014e802180b'
        '1f00086b01010054180700111f0f0071cbfcff54e8060011a80e00b9e003172a'
        '0200001400008012f44f45a9f65744a9f85f43a9fd7b42a9ff830191c0035fd6'
    ),
    'drawing': (
        'ff8304d1e00640f9e1030091fafeff9700070034f30240f9f40a40f9f5031f2a'
        '740000b448569152956a6838e00313aa54ffff97f603002af9031f2afa030091'
        '4827406d4a0b40fd08f4e7d20001679e4109601e2228611e4239621e0329611e'
        '2429611ee10301912310006d2208016d19010035f5000034822183528284b372'
        '4351995203fdbf7201906e1e060000140203825242c1af72031f9d52831bae72'
        '01106d1ee00313aa4040601e24008052dbf4b4977601f83799000035bf020071'
        'c106961a02000014c10a0011e00313aae2031aaa03008012e4031f2af8e8b497'
        '5a830091390700113f0b0071abfaff54ff8304918b8ab517'
    ),
    'input': (
        'ff0301d1e866403928070035880a4139e8060034880641396800003488ea4039'
        '02000014e82e403928060035e8b240b9080900511f791e72a0050054e00314aa'
        'e1030091aefeff972005003469098052ea0300914005406d2021601e0b040054'
        '0021611ecb030054420940fd0328621e2021631e420300542328621e0021631e'
        'e202005403106c1e4208631e0028621e2128621e2039601e0139611e0008601e'
        '2000411f4208621e0020621e8a01005488008052680200b928008052680600b9'
        '690a00b9690e00b97f7e01a9400540ad600601adff030191a717b5174a810091'
        '290500513f29017160fbff54ff0301912041601eda17b517'
    ),
    'action': (
        '3f250171c10000542ad7fff04a8945f90a0200b4594140b98236b3173f290171'
        'a10100542ad7fff04a8945f92a0100b4594140b93f0b0071c000005468ee49b9'
        'e00313aa1f011b72e1179f1a3b109597b535b3173f1d017149000054b335b317'
        '2b35b317'
    ),
    'fill': '28b06739090d03534903023909150553490b023958dab417',
    'cap45_ceiling': '7fb60071a805805213c1931a28008052e84f0a291a9f9217',
    'drop_debounce': (
        '0e7d8052ce0dd41ace050011bf010e6bad010054ef0a40b9ff010e6b4d010054'
        '6f5240b9ff0d0071eb0000546f0640b9ff010e6b8d0000546f0a40b9ff010e6b'
        '4c000054b6ab921788ab9217'
    ),
}

FRAGMENTS_2503 = tuple(
    (name, offset, src, _HEX_2503[name])
    for name, offset, src, _ in FRAGMENTS
)

_HEX_2685 = {
    'geometry': (
        '08084139080c003400904dfd0820601ea90b005401944dfd2820601e490b0054'
        '0248611e03906c1e4208631e03106c1e04884dfd0410431f058c4dfd089f8252'
        '0800088b690280520a8141392a010034000940fd0820601ec9000054000d40fd'
        '0820601e69000054000540fda558601e08a101912905007181feff5400906b1e'
        '4008601e4028601ea538601e00106b1e5008601e5128621e312a701e00106c1e'
        '200a601e8638601e12e4002f00604afd0820601e4d020054016c4afd03744afd'
        '2428631e2168721e6820601e84cc601e8478601e08f4e7d20301679e0308631e'
        '4368631e2128631e8438631e8438711ec678641ec668611e040000140310651e'
        '4368631ec668631e00644afd0820601e4d02005401704afd03784afd2428631e'
        '2168721e6820601e84cc601e8478601e08f6e7d20301679e0308631e4368631e'
        '2128631e8438631e8438621ea578641ea568611e040000140310651e4368631e'
        'a568631ec728621ee728701e2614006d2208016d2714026d2208036d20008052'
        'c0035fd6e0031f2ac0035fd6'
    ),
    'ensure_atlas': (
        'ff8301d1fd7b02a9f85f03a9f65704a9f44f05a9fd830091601200b4131840f9'
        '331200b4089992526800a072740208cb082898526800a072896a68b83f810171'
        '281100546e4a89528e8aaa720a0897526a00a0728b020a8bf5031faaf6031f2a'
        'df02096b2a0400546c0140f9cc0000b56c0940b99f010e6b61000054f5030baa'
        '040000146b610091d6060011f5ffff17080797526800a072886a68b8d602080b'
        'a80a40f9a80200b4aa0e40b9ea0200344a0500516bd258b94c0900119f010b6b'
        '4a0200546b664cf90b0200b48c0280524b2dac9b6c01c0799f01166b61010054'
        'e0030a2a610000143f7d0171cc0b0054f5031faa16008012971a40f9570b00b4'
        'e81e40f9080b00b468d258b969d658b9080d00111f01096b6c0a0054b50200b5'
        'e00314aae1031faa624a8952828aaa72034080520440805253cab1974009f837'
        'f603002a080797526800a072896a68b8c902094b3f810171620800548802088b'
        '082100910a0380523521aa9bbf0e00b9b80a40f9780300b5971a40f9570700b4'
        '00128052a789b697e00600b4f80300aa8704c097e00318aae10317aa1f2003d5'
        'a26a0010c32f8b524300a072e4031f2ae5031f2ae6031f2a27008052ff0300f9'
        '2b0fc097c0000035080340f9e00318aa080540f900013fd623000014b80a00f9'
        'f8031f2a170080121f07007188008052892080522201881a1f0b00712301881a'
        '041f8052051f8052e6031f2ae7031f2ae00313aae103162aff0300f9ff0700f9'
        '08008012e81300b9cae2b097c001f83778000035f703002a04000014e802180b'
        '1f00086b01010054180700111f0f0071cbfcff54e8060011a80e00b9e003172a'
        '0200001400008012f44f45a9f65744a9f85f43a9fd7b42a9ff830191c0035fd6'
    ),
    'drawing': (
        'ff8304d1e00640f9e1030091fafeff9700070034f30240f9f40a40f9f5031f2a'
        '740000b448569152956a6838e00313aa54ffff97f603002af9031f2afa030091'
        '4827406d4a0b40fd08f4e7d20001679e4109601e2228611e4239621e0329611e'
        '2429611ee10301912310006d2208016d19010035f5000034822183528284b372'
        '4351995203fdbf7201906e1e060000140203825242c1af72031f9d52831bae72'
        '01106d1ee00313aa4040601e240080525ef1b0977601f83799000035bf020071'
        'c106961a02000014c10a0011e00313aae2031aaa03008012e4031f2a7be5b097'
        '5a830091390700113f0b0071abfaff54ff8304913687b117'
    ),
    'input': (
        'ff0301d1e866403928070035880a4139e8060034880641396800003488ea4039'
        '02000014e82e403928060035e8b240b9080900511f791e72a0050054e00314aa'
        'e1030091aefeff972005003469098052ea0300914005406d2021601e0b040054'
        '0021611ecb030054420940fd0328621e2021631e420300542328621e0021631e'
        'e202005403106c1e4208631e0028621e2128621e2039601e0139611e0008601e'
        '2000411f4208621e0020621e8a01005488008052680200b928008052680600b9'
        '690a00b9690e00b97f7e01a9400540ad600601adff0301912a14b1174a810091'
        '290500513f29017160fbff54ff0301912041601e5d14b117'
    ),
    'action': (
        '3f250171c10000544acffff04af545f90a0200b4594140b9aeffae173f290171'
        'a10100544acffff04af545f92a0100b4594140b93f0b0071c000005468fe49b9'
        'e00313aa1f011b72e1179f1ae7ac9097e1feae173f1d017149000054dffeae17'
        '57feae17'
    ),
    'fill': '28f06739090d03534903023909150553490b0239dbd6b017',
    'cap45_ceiling': '7fb60071a805805213c1931a28008052e84f0929de378e17',
    'drop_debounce': (
        '0e7d8052ce0dd41ace050011bf010e6bad010054ef0a40b9ff010e6b4d010054'
        '6f5240b9ff0d0071eb0000546f0640b9ff010e6b8d0000546f0a40b9ff010e6b'
        '4c000054fb448e17cd448e17'
    ),
}

FRAGMENTS_2685 = tuple(
    (name, offset, src, _HEX_2685[name])
    for name, offset, src, _ in FRAGMENTS
)


def _profile(code_va, code_sha256, fragments):
    label_va = code_va + CODE_SIZE
    return {
        'code_va': code_va,
        'code_size': CODE_SIZE,
        'label_va': label_va,
        'label_when_standing_va': label_va,
        'label_when_sitting_va': label_va + 0x20,
        'label_jump_va': label_va + 0x40,
        'atlas_png_va': label_va + 0x80,
        'geometry_va': code_va + 0x000,
        'ensure_atlas_va': code_va + 0x18C,
        'draw_va': code_va + 0x40C,
        'input_va': code_va + 0x504,
        'action_va': code_va + 0x5FC,
        'fill_va': code_va + 0x660,
        'cap45_ceiling_va': code_va + 0x678,
        'drop_debounce_va': code_va + 0x690,
        'code_sha256': code_sha256,
        'fragments': fragments,
    }


PROFILES = {
    '2411': _profile(CODE_VA, CODE_SHA256, FRAGMENTS),
    '2503': _profile(CODE_VA_2503, CODE_SHA256_2503, FRAGMENTS_2503),
    '2685': _profile(CODE_VA_2685, CODE_SHA256_2685, FRAGMENTS_2685),
}


def build_icon_atlas_png(file_dir):
    """Build the 512x512 packed RGBA PNG atlas from Nut-Ngoi, Nut-Dung, and Nut-Nhay."""
    from PIL import Image
    atlas = Image.new('RGBA', (512, 512), (0, 0, 0, 0))
    sources = (('Nut-Ngoi.png', (4, 4)), ('Nut-Dung.png', (260, 4)), ('Nut-Nhay.png', (4, 260)))
    for fname, (dx, dy) in sources:
        im = Image.open(Path(file_dir) / fname).convert('RGBA')
        px = im.load()
        w, h = im.size
        xs = [x for y in range(h) for x in range(w) if px[x, y][3] > 10]
        ys = [y for y in range(h) for x in range(w) if px[x, y][3] > 10]
        cx = (min(xs) + max(xs)) / 2.0
        cy = (min(ys) + max(ys)) / 2.0
        span = max(max(xs) - min(xs) + 1, max(ys) - min(ys) + 1)
        half = span * 0.515
        box = (int(round(cx - half)), int(round(cy - half)), int(round(cx + half)), int(round(cy + half)))
        cell = im.crop(box).resize((248, 248), Image.Resampling.LANCZOS)
        atlas.paste(cell, (dx, dy), cell)
    import io
    buf = io.BytesIO()
    atlas.save(buf, format='PNG', optimize=True)
    return buf.getvalue()


def build_payload(version='2411'):
    """Build immutable code page, UTF-32LE labels, and packed 512x512 icon atlas PNG."""
    if version not in PROFILES:
        raise ValueError('Unsupported TruTien payload version: ' + str(version))
    prof = PROFILES[version]
    code = bytearray(bytes.fromhex('1f2003d5') * (CODE_SIZE // 4))
    occupied = set()
    for name, offset, _, encoded in prof['fragments']:
        data = bytes.fromhex(encoded)
        if offset % 4 or len(data) % 4 or not data or offset + len(data) > len(code):
            raise ValueError('Invalid ARM64 payload fragment: ' + name)
        positions = set(range(offset, offset + len(data)))
        if occupied & positions:
            raise ValueError('Overlapping ARM64 payload fragments')
        occupied.update(positions)
        code[offset:offset + len(data)] = data
    if hashlib.sha256(code).hexdigest() != prof['code_sha256']:
        raise ValueError('ARM64 payload bytes failed their checksum')
    atlas_bytes = Path(__file__).with_name(ATLAS_PNG_NAME).read_bytes()
    if len(atlas_bytes) != ATLAS_PNG_SIZE or hashlib.sha256(atlas_bytes).hexdigest() != ATLAS_PNG_SHA256:
        raise ValueError('Packed icon atlas PNG failed integrity check')
    when_standing = 'Ng\u1ed3i\0'.encode('utf-32le').ljust(0x20, b'\0')
    when_sitting = '\u0110\u1ee9ng\0'.encode('utf-32le').ljust(0x20, b'\0')
    jump_label = 'Nh\u1ea3y\0'.encode('utf-32le').ljust(0x40, b'\0')
    return bytes(code), when_standing + when_sitting + jump_label + atlas_bytes


def unwind_ranges(version='2411'):
    """CFI rows verified against the original FDEs and AAPCS64 helper prologues."""
    if version not in PROFILES:
        raise ValueError('Unsupported TruTien payload version: ' + str(version))
    atlas = {29: -64, 30: -56, 24: -48, 23: -40, 22: -32, 21: -24, 20: -16, 19: -8}
    draw = {29: -96, 30: -88, 28: -80, 27: -72, 26: -64, 25: -56,
            24: -48, 23: -40, 22: -32, 21: -24, 20: -16, 19: -8,
            79: -160, 78: -152, 77: -144, 76: -136, 75: -128,
            74: -120, 73: -112, 72: -104}
    touch = {29: -80, 30: -72, 28: -64, 24: -48, 23: -40,
             22: -32, 21: -24, 20: -16, 19: -8, 79: -144,
             78: -136, 77: -128, 76: -120, 75: -112, 74: -104,
             73: -96, 72: -88}
    action = {29: -80, 30: -72, 25: -64, 24: -48, 23: -40,
              22: -32, 21: -24, 20: -16, 19: -8}
    rows = ((0, 396, 31, 0, {}), (0x18C, 640, 29, 64, atlas),
            (0x40C, 248, 29, 96, draw), (0x504, 248, 29, 80, touch),
            (0x5FC, 100, 29, 80, action), (0x660, 24, 29, 96, draw),
            (0x678, 24, 29, 96, draw), (0x690, 76, 29, 96, draw))
    return [{'offset': offset, 'size': size, 'cfaRegister': register,
             'cfaOffset': cfa_offset, 'savedRegisters': saved}
            for offset, size, register, cfa_offset, saved in rows]
