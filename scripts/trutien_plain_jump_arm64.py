# SPDX-License-Identifier: GPL-2.0-or-later
"""Own ARM64 HUD extension: separate plain-Jump button, without replacing Khinh cong.

Assembly and frozen machine bytes are kept together for reproducible review.
Runtime patching uses only Python stdlib; optional Keystone verifies assembly.
Addresses refer exclusively to the SHA-guarded Tru Tien ARM64 client.
"""
import hashlib

CODE_VA = 0x36B8000
CODE_SIZE = 0x800
LABEL_VA = CODE_VA + CODE_SIZE
DRAW_VA = CODE_VA + 0x100
INPUT_VA = CODE_VA + 0x300
ACTION_VA = CODE_VA + 0x600
CODE_SHA256 = '6952e4b71e3170959930db2c61e867530caac675c151bbd7f1249095316134dc'

FRAGMENTS = (
    ('rectangle', 0x0, r"""
    ldrb w8, [x0, #0x42]
    cbz w8, invalid
    ldr d0, [x0, #0x1b20]
    fcmp d0, #0.0
    b.ls invalid
    ldr d1, [x0, #0x1b28]
    fcmp d1, #0.0
    b.ls invalid
    fmax d2, d0, d1
    fmov d3, #0.75
    fmul d2, d2, d3
    fmov d3, #0.5
    ldr d4, [x0, #0x1b10]
    fmadd d4, d0, d3, d4
    fmul d3, d2, d3
    fsub d4, d4, d3
    ldr d5, [x0, #0x1b18]
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
    fmov d0, #0.1875
    fmul d0, d2, d0
    fadd d0, d0, d2
    fsub d5, d5, d0
    fmov d0, #2.0
    fmax d4, d4, d0
    fmax d5, d5, d0
    stp d4, d5, [x1]
    stp d2, d2, [x1, #16]
    mov w0, #1
    ret
invalid:
    mov w0, #0
    ret
""",
     '080841396805003400904dfd0820601e0905005401944dfd2820601ea90400540248611e03106d1e4208631e03106c1e04884dfd0410431f4308631e8438631e058c4dfd089f82520800088b690280520a8141392a010034000940fd0820601ec9000054000d40fd0820601e69000054000540fda558601e08a101912905007181feff540010691e4008601e0028621ea538601e0010601e8448601ea548601e2414006d2208016d20008052c0035fd600008052c0035fd6'),
    ('drawing', 0x100, r"""
    sub sp, sp, #0x90
    ldr x0, [x23, #8]
    mov x1, sp
    bl 0x36b8000
    cbz w0, finished
    ldr x19, [x23]
    ldp d8, d9, [sp]
    ldr d10, [sp, #0x10]
    fmov d0, #0.5
    fmadd d11, d10, d0, d8
    fmadd d12, d10, d0, d9

    mov x0, x19
    mov x1, sp
    fmov d0, d10
    mov w2, #0x101a
    movk w2, #0x7809, lsl #16
    mov w3, #0xe6f5
    movk w3, #0x99dc, lsl #16
    fmov d1, #1.0
    mov w4, #1
    bl 0x24088e8

    fmov d0, #0.1875
    fmul d1, d10, d0
    fsub d2, d10, d1
    fmov d0, #0.5
    fmul d1, d1, d0
    fadd d3, d8, d1
    fadd d4, d9, d1
    add x1, sp, #0x20
    stp d3, d4, [x1]
    stp d2, d2, [x1, #0x10]
    mov x0, x19
    fmov d0, d2
    mov w2, wzr
    mov w3, #0xca8a
    movk w3, #0x59e8, lsl #16
    fmov d1, #0.75
    mov w4, wzr
    bl 0x24088e8

    fmov d0, #0.1875
    fmul d1, d10, d0
    fsub d2, d11, d1
    fadd d3, d11, d1
    fmov d0, #0.25
    fmul d4, d10, d0
    fmul d5, d4, d0
    fsub d6, d12, d4
    fsub d7, d12, d5
    add x1, sp, #0x40
    stp d2, d7, [x1]
    stp d11, d6, [x1, #0x10]
    stp d3, d7, [x1, #0x20]
    mov x0, x19
    mov w2, #3
    fmov d0, #0.25
    fmul d0, d1, d0
    mov w3, #0xf7fa
    movk w3, #0xfff5, lsl #16
    bl 0x240eaec

    fmov d0, #0.125
    fmul d1, d10, d0
    fsub d2, d11, d1
    fadd d3, d11, d1
    fmov d0, #0.1875
    fmov d4, #0.5
    fmul d5, d10, d0
    fmul d5, d5, d4
    fsub d6, d12, d5
    fmov d4, #0.25
    fmul d7, d1, d4
    fadd d7, d12, d7
    add x1, sp, #0x40
    stp d2, d7, [x1]
    stp d11, d6, [x1, #0x10]
    stp d3, d7, [x1, #0x20]
    mov x0, x19
    mov w2, #3
    fmul d4, d10, d0
    fmul d0, d4, d0
    mov w3, #0xca8a
    movk w3, #0xffe8, lsl #16
    bl 0x240eaec

    fmov d0, #0.5
    fmov d1, #0.25
    fmul d2, d1, d1
    fadd d0, d0, d2
    fadd d2, d1, d2
    fmadd d3, d10, d0, d9
    fmul d4, d10, d2
    add x2, sp, #0x20
    stp d8, d3, [x2]
    stp d10, d4, [x2, #0x10]
    mov x0, x19
    adrp x1, 0x36b8000
    add x1, x1, #0x800
    fmov d5, #0.1875
    fmul d0, d10, d5
    mov w3, #0xf4f6
    movk w3, #0xfff2, lsl #16
    mov w4, #2
    mov w5, #1
    mov w6, #6
    mov w7, #-1
    bl 0x240c09c
finished:
    add sp, sp, #0x90
    b 0x242de5c
""",
     'ff4302d1e00640f9e1030091bdffff97800c0034f30240f9e827406dea0b40fd00106c1e4b21401f4c25401fe00313aae10300914041601e420382522201af72a3de9c52833bb37201106e1e24008052e641b5970010691e4109601e4239611e00106c1e2108601e0329611e2429611ee18300912310006d2208016de00313aa4040601ee2031f2a43519952033dab7201106d1ee4031f2ad441b5970010691e4109601e6239611e6329611e00106a1e4409601e8508601e8639641e8739651ee1030191221c006d2b18016d231c026de00313aa6200805200106a1e2008601e43ff9e52a3febf72415ab5970010681e4109601e6239611e6329611e0010691e04106c1e4509601ea508641e8639651e04106a1e2708641e8729671ee1030191221c006d2b18016d231c026de00313aa620080524409601e8008601e4351995203fdbf722a5ab59700106c1e01106a1e2208611e0028621e2228621e4325401f4409621ee2830091480c006d4a10016de00313aa01000090210020910510691e4009651ec39e9e5243febf724400805225008052c600805207008012804fb597ff430291eed6b517'),
    ('input', 0x300, r"""// Assemble at VA 0x36b8300. Replaces fmov d0,d9 at VA 0x24114a8.
// Original frame: x19=HUDFINGERHIT, x20=HUDLAYOUT, x21=HUDTOUCHCTX,
// x22=HUDDATA, x23=HUDUISTATE, d9=tapX, d8=tapY (already normalized).
// Native dialog/chat/pick/box_a priority has run before this hit-test point.
// Uses circular interior hit-test matching the visual button medallion,
// emitting HIT(kind=4, subkind=1, tag=74, action=74) on the combat cluster path.

    sub sp, sp, #0x20
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
    ldp d0, d1, [sp]
    fcmp d9, d0
    b.lt original
    fcmp d8, d1
    b.lt original
    ldr d2, [sp, #0x10]
    fadd d3, d0, d2
    fcmp d9, d3
    b.hs original
    fadd d3, d1, d2
    fcmp d8, d3
    b.hs original
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
    b.ge original

    mov w8, #4
    str w8, [x19]
    mov w8, #1
    str w8, [x19, #4]
    mov w8, #74
    str w8, [x19, #8]
    str w8, [x19, #0xc]
    stp xzr, xzr, [x19, #0x10]
    ldp q0, q1, [sp]
    stp q0, q1, [x19, #0x20]
    add sp, sp, #0x20
    b 0x24113c8

original:
    add sp, sp, #0x20
    fmov d0, d9
    b 0x24114ac
""",
     'ff8300d1e866403988060035880a413948060034880641396800003488ea403902000014e82e403988050035e8b240b9080900511f791e7200050054e00314aae10300912fffff9780040034e007406d2021601e2b0400540021611eeb030054e20b40fd0328621e2021631e620300542328621e0021631e0203005403106c1e4208631e0028621e2128621e2039601e0139611e0008601e2000411f4208621e0020621eaa01005488008052680200b928008052680600b948098052680a00b9680e00b97f7e01a9e00740ad600601adff830091fd63b517ff8300912041601e3364b517'),
    ('action', 0x600, r"""
    cmp w9, #73
    b.ne original_bounds
    adrp x10, 0x31ab000
    ldr x10, [x10, #0xce0]
    cbz x10, reject_null
    ldr w25, [x10, #0x40]
    b 0x23abd8c
reject_null:
    b 0x23aba90
original_bounds:
    cmp w9, #71
    b.ls original_dispatch
    b 0x23aba94
original_dispatch:
    b 0x23ab878
""",
     '3f250171e10000548ad7fff04a7146f96a0000b4594140b9ddcdb3171dcdb3173f1d0171490000541bcdb31793ccb317'),
)

def build_payload():
    code = bytearray(bytes.fromhex("1f2003d5") * (CODE_SIZE // 4))
    occupied = set()
    for name, offset, _, encoded in FRAGMENTS:
        data = bytes.fromhex(encoded)
        if offset % 4 or len(data) % 4 or not data or offset + len(data) > len(code):
            raise ValueError("Invalid ARM64 payload fragment: " + name)
        positions = set(range(offset, offset + len(data)))
        if occupied & positions:
            raise ValueError("Overlapping ARM64 payload fragments")
        occupied.update(positions)
        code[offset:offset + len(data)] = data
    if hashlib.sha256(code).hexdigest() != CODE_SHA256:
        raise ValueError("ARM64 payload bytes failed their checksum")
    return bytes(code), "Nh\u1ea3y\0".encode("utf-32le")


def unwind_ranges():
    """CFI rows verified against the original FDEs at all three hook sites.

    Stubs borrow their containing native function's frame pointer. Their local
    stack allocations do not change x29, so these CFA rules remain valid.
    """
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
    rows = ((0, 184, 31, 0, {}), (0x100, 424, 29, 96, draw),
            (0x300, 228, 29, 80, touch), (0x600, 48, 29, 80, action))
    return [{'offset': offset, 'size': size, 'cfaRegister': register,
             'cfaOffset': cfa_offset, 'savedRegisters': saved}
            for offset, size, register, cfa_offset, saved in rows]
