#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""CPU checks of the actual production-patched plain-Jump ARM64 ELF.

Original library is supplied locally; no game data is embedded or modified.
Draw APIs and bounded environment primitives are substituted for synthetic
fixtures. These checks do not verify raster output, physics or device behavior.
"""
import argparse
import hashlib
import importlib.util
import io
import json
import math
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from trutien_native_harness import NativeHUD, BASE, LAYOUT, double_bits, rect, overlap


def virtual_bytes(binary, address, size):
    from elftools.elf.elffile import ELFFile
    elf = ELFFile(io.BytesIO(binary))
    segments = [s for s in elf.iter_segments() if s['p_type'] == 'PT_LOAD' and
                s['p_vaddr'] <= address and address + size <= s['p_vaddr'] + s['p_filesz']]
    assert len(segments) == 1
    at = segments[0]['p_offset'] + address - segments[0]['p_vaddr']
    return binary[at:at+size]


def verify_restoration(original, patched):
    spans = [(0x241BA08, 4), (0x241BA48, 4), (0x242EB80, 4), (0x242EB8C, 4), (0x125B0EE, 96)]
    for address, size in spans:
        assert virtual_bytes(original, address, size) == virtual_bytes(patched, address, size), ('Khinh cong bytes changed', hex(address))
    for address, size in ((0x23ABE90, 160), (0x1C5A2A0, 112), (0x1C2C6CC, 1048)):
        assert virtual_bytes(original, address, size) == virtual_bytes(patched, address, size)
    return {'oldResizeRenameLogSpansRestored': 5, 'nativeJumpWaterAndPhysicsCodeUnchanged': True}


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
        value = struct.unpack('<I', bytes(uc.mem_read(pointer + at*4, 4)))[0]
        if value == 0:
            return ''.join(out)
        out.append(chr(value))
    raise AssertionError('Unterminated captured text')


class DrawProbe(NativeHUD):
    def __init__(self, data):
        from elftools.elf.elffile import ELFFile
        from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
        super().__init__(data)
        self.calls = []
        self.uc.mem_map(SCRATCH, 0x20000)
        elf = ELFFile(io.BytesIO(data))
        self.api = {s['st_value']: s.name for s in elf.get_section_by_name('.dynsym').iter_symbols()
                    if s['st_value'] and s.name.startswith('_ZN') and 'CHudDrawList' in s.name}
        for address in self.api:
            self.uc.hook_add(UC_HOOK_CODE, self.capture, begin=address, end=address)
        self.uc.hook_add(UC_HOOK_CODE, self.snprintf, begin=0x2F7AC90, end=0x2F7AC90)
        self.uc.hook_add(UC_HOOK_CODE, self.atan2, begin=0x2F7B810, end=0x2F7B810)
        self.uc.hook_add(UC_HOOK_MEM_INVALID, self.invalid)

    def invalid(self, uc, access, address, size, value, _):
        raise AssertionError('Unmapped access PC=%x address=%x size=%d' %
                             (uc.reg_read(self.reg.UC_ARM64_REG_PC), address, size))

    def snprintf(self, uc, address, size, _):
        reg = self.reg
        dst, count, fmt, value = [uc.reg_read(reg.UC_ARM64_REG_X0+i) for i in range(4)]
        raw = bytes(uc.mem_read(fmt, 64)).split(b'\0')[0]
        assert raw == b'%d', ('Unexpected libc formatting fixture', raw)
        value = value & 0xffffffff
        if value & 0x80000000:
            value -= 0x100000000
        output = str(value).encode('ascii')
        if count:
            assert count <= 0x1000
            uc.mem_write(dst, output[:count-1]+b'\0')
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
        args = [uc.reg_read(reg.UC_ARM64_REG_X0+i) for i in range(8)]
        dargs = [struct.unpack('<d', struct.pack('<Q', uc.reg_read(reg.UC_ARM64_REG_D0+i)))[0]
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
            entry['rect'] = struct.unpack('<4d', bytes(uc.mem_read(args[2], 32)))
        if 'MeasureText' in name:
            uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(16))
            uc.reg_write(reg.UC_ARM64_REG_D1, double_bits(12))
        elif 'GetBorderW' in name:
            uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(1))
        else:
            uc.reg_write(reg.UC_ARM64_REG_X0, 0)
        self.calls.append(entry)
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def draw(self, auto_hide, hide=None, v11=True):
        self.layout(932, 430, 32, 1, int(v11))
        self.uc.mem_write(SCRATCH, bytes(0x20000))
        self.uc.mem_write(VIEW, struct.pack('<6Q', DRAWLIST, LAYOUT, DATA, STATE, STRINGS, ICONS))
        self.uc.mem_write(STATE, struct.pack('<I', 2))
        self.uc.mem_write(STATE+0xa8, struct.pack('<d', auto_hide))
        # Layout() executes the geometry helpers rather than their enclosing
        # header initializer. Provide a valid V11 header and draw dimensions.
        self.uc.mem_write(LAYOUT+0x42, bytes([int(v11)]))
        for at, value in ((0x3e20, 1.), (0x3e28, 1.), (0x4030, 12.),
                          (0x4220, 1.), (0x4228, 12.), (0x4230, 12.)):
            self.uc.mem_write(LAYOUT+at, struct.pack('<d', value))
        self.uc.mem_write(TEXT, 'Fixture\0'.encode('utf-32le'))
        self.uc.mem_write(STRINGS, struct.pack('<Q', TEXT)*256)
        self.uc.mem_write(ICONS, b'\xff'*0x2000)
        if hide == 'state19':
            self.uc.mem_write(STATE+0x19, b'\1')
        elif hide == 'config3a':
            self.uc.mem_write(LAYOUT+0x41, b'\1')
            self.uc.mem_write(LAYOUT+0x3a, b'\1')
        elif hide == 'state0b':
            self.uc.mem_write(STATE+0xb, b'\1')
        elif hide in ('mode2', 'mode4'):
            self.uc.mem_write(STATE+0xb0, struct.pack('<I', int(hide[-1])))
        self.calls = []
        try:
            self.call(0x242DDFC, [VIEW])
        except Exception as error:
            raise AssertionError('Draw failed: PC=%x LR=%x calls=%r scenario=%r' %
                                 (self.uc.reg_read(self.reg.UC_ARM64_REG_PC),
                                  self.uc.reg_read(self.reg.UC_ARM64_REG_LR),
                                  self.calls[-5:], (auto_hide, hide, v11))) from error
        return list(self.calls)



def verify_draw(original, modified):
    baseline, patched = DrawProbe(original), DrawProbe(modified)
    cases = []
    for auto_hide in (0., .5, 1.):
        for hide in (None, 'state19', 'config3a', 'state0b', 'mode2', 'mode4'):
            for v11 in (False, True):
                before = baseline.draw(auto_hide, hide, v11)
                after = patched.draw(auto_hide, hide, v11)
                assert after[:len(before)] == before, ('Original HUD commands changed', auto_hide, hide, v11)
                plain_before = [x for x in before if x.get('text') == 'Nh\u1ea3y']
                plain_after = [x for x in after if x.get('text') == 'Nh\u1ea3y']
                assert not plain_before
                extra = after[len(before):]
                if hide is None and v11:
                    assert len(extra) == 5, ('Unexpected extra medallion commands', auto_hide, hide, v11, extra)
                    assert len(plain_after) == 1, (auto_hide, hide, v11, after)
                    outer, inner, top_chev, low_chev, label = extra
                    assert 'AddRoundRect' in outer['method'] and outer['fillFlag'] == 1
                    button = outer['rect']
                    bx, by, bs, bh = button
                    assert bs == bh and bs > 0 and outer['radius'] >= bs * 0.5
                    assert 'AddRoundRect' in inner['method'] and inner['fillFlag'] == 0
                    ir = inner['rect']
                    assert ir[0] > bx and ir[1] > by and ir[2] == ir[3] < bs and inner['radius'] >= ir[2] * 0.5
                    for chev in (top_chev, low_chev):
                        assert 'AddPolyLineAA' in chev['method'] and chev['count'] == 3 and chev['width'] > 0
                        p0, p1, p2 = chev['points']
                        cx = bx + bs * 0.5
                        assert abs(p1[0] - cx) < 1e-6 and abs((p0[0] + p2[0]) * 0.5 - cx) < 1e-6
                        assert p0[0] < p1[0] < p2[0] and abs(p0[1] - p2[1]) < 1e-6 and p1[1] < p0[1]
                        assert bx < p0[0] and p2[0] < bx + bs and by < p1[1] < by + bs
                    assert low_chev['points'][1][1] > top_chev['points'][1][1]
                    assert 'AddText' in label['method'] and label['text'] == 'Nh\u1ea3y'
                    assert abs(label['fontSize'] - bs * 0.1875) < 1e-6
                    lr = label['rect']
                    assert lr[0] == bx and lr[2] == bs and lr[1] > by + bs * 0.5 and lr[1] + lr[3] < by + bs
                else:
                    assert not plain_after and not extra, (auto_hide, hide, v11, extra)
                    assert after == before, ('Hidden/legacy HUD output changed', auto_hide, hide, v11)
                cases.append({'autoHide': auto_hide, 'hide': hide, 'v11': v11,
                              'beforeCount': len(before), 'afterCount': len(after),
                              'jump': extra})
    return {'result': 'PASS', 'scope': 'Actual native HudViewCombat control flow; draw-list APIs captured, no raster/device validation',
              'originalHUDDrawingCommandsUnchanged': True,
              'validV11DrawsCircularMedallion': True,
              'primitiveShims': ['sincos', 'memset', 'snprintf with %d', 'atan2'],
              'cases': cases}


def verify_geometry(original, modified, payload):
    old, new = NativeHUD(original), NativeHUD(modified)
    cases = []
    for width, height in ((932, 430), (1400, 650), (2796, 1290)):
        for unit in (24, 32, 40):
            for scale in (0.8, 1, 1.5):
                before_i, before = old.layout(width, height, unit, scale, 1)
                after_i, after = new.layout(width, height, unit, scale, 1)
                assert (before_i, before) == (after_i, after), 'Original HUD layout changed'
                new.uc.mem_write(LAYOUT + 0x42, b'\1')
                preserved = bytes(new.uc.mem_read(LAYOUT, 0x10000))
                output = BASE + 0x70000
                assert new.call(payload.CODE_VA, [LAYOUT, output]) == 1
                button = rect(bytes(new.uc.mem_read(output, 32)), 0)
                assert 0 <= button[0] and 0 <= button[1]
                assert button[0] + button[2] <= width and button[1] + button[3] <= height
                assert button[2] == button[3] and button[2] > 0
                assert bytes(new.uc.mem_read(LAYOUT, 0x10000)) == preserved
                for control in range(58):
                    at = 0x14F8 + control * 0x68
                    if before[at + 0x60]:
                        assert not overlap(button, rect(before, at)), ('Overlap', width, height, unit, scale, control)
                cases.append({'screen': [width, height], 'unit': unit, 'scale': scale, 'button': button})
    return {'cases': cases, 'existingLayoutUnchanged': True,
            'noOverlapWithOtherActiveNativeControlRects': True}


TOUCH, TOUCH_DATA, TOUCH_UI, HIT = (BASE + at for at in (0x50000, 0x60000, 0x70000, 0x78000))


def classify(binary, payload=None, point=None, gates=(), button=None, auto_hide=0., density=1.):
    runner = NativeHUD(binary)
    reg = runner.reg
    runner.layout(932, 430, 32, 1, 1)
    runner.uc.mem_write(LAYOUT + 0x42, b'\1')
    runner.uc.mem_write(TOUCH, struct.pack('<QQQd', LAYOUT, TOUCH_DATA, TOUCH_UI, density))
    runner.uc.mem_write(TOUCH_UI + 0xA8, struct.pack('<d', auto_hide))
    if button is None:
        assert payload is not None
        assert runner.call(payload.CODE_VA, [LAYOUT, HIT]) == 1
        button = struct.unpack('<4d', bytes(runner.uc.mem_read(HIT, 32)))
    if point is None:
        point = button[0] + button[2]/2, button[1] + button[3]/2
    for address, value in gates:
        runner.uc.mem_write(address, value)
    runner.uc.reg_write(reg.UC_ARM64_REG_D0, double_bits(point[0] * density))
    runner.uc.reg_write(reg.UC_ARM64_REG_D1, double_bits(point[1] * density))
    runner.call(0x2411308, [TOUCH, HIT])
    return bytes(runner.uc.mem_read(HIT, 0x118)), button


def verify_input(original, modified, payload):
    classified, button = classify(modified, payload)
    center = button[0] + button[2]/2, button[1] + button[3]/2
    radius = button[2] / 2
    before, _ = classify(original, point=center, button=button)
    assert struct.unpack_from('<4i', classified) == (4, 1, 74, 74)
    assert struct.unpack_from('<5i', classified, 0xC) == (74, 0, 0, 0, 0)
    assert struct.unpack_from('<4d', classified, 0x20) == button
    assert struct.unpack_from('<d', classified, 0x58) == (1.,)
    assert struct.unpack_from('<i', before, 0xC)[0] != 74
    for inside in ((center[0] + radius * 0.45, center[1]),
                   (center[0], center[1] - radius * 0.45)):
        hit, _ = classify(modified, payload, point=inside)
        assert struct.unpack_from('<4i', hit) == (4, 1, 74, 74), ('Interior circle point missed', inside)
    for progress in (0., .5, 1.):
        result, _ = classify(modified, payload, auto_hide=progress)
        assert struct.unpack_from('<i', result, 0xC)[0] == 74
    gates = {
        'global UI hide': [(TOUCH_UI + 0x19, b'\1')],
        'legacy HUD': [(LAYOUT + 0x42, b'\0')],
        'UI hide flag': [(TOUCH_UI + 0xB, b'\1')],
        'layout hide flag': [(LAYOUT + 0x41, b'\1'), (LAYOUT + 0x3A, b'\1')],
        'dialog mode 2': [(TOUCH_UI + 0xB0, struct.pack('<I', 2))],
        'dialog mode 4': [(TOUCH_UI + 0xB0, struct.pack('<I', 4))],
    }
    for name, gate in gates.items():
        before, _ = classify(original, point=center, gates=gate, button=button)
        after, _ = classify(modified, payload, gates=gate)
        assert before == after, ('Input gate changed', name)
    points = [(0., 0.), (500., 200.), (931., 429.),
              (button[0] + button[2], center[1]), (center[0], button[1] + button[3]),
              (button[0] + 0.5, button[1] + 0.5),
              (button[0] + button[2] - 0.5, button[1] + button[3] - 0.5)]
    for point in points:
        before, _ = classify(original, point=point, button=button)
        after, _ = classify(modified, payload, point=point)
        assert before == after, ('Unrelated/corner/boundary point changed', point)
    chat = [(TOUCH_UI + 0x18, b'\1'),
            (LAYOUT + 0x3A98, struct.pack('<4d', button[0]-1, button[1]-1, button[2]+2, button[3]+2))]
    before, _ = classify(original, point=center, gates=chat, button=button)
    after, _ = classify(modified, payload, gates=chat)
    assert before == after and struct.unpack_from('<i', after, 0xC)[0] != 74
    results = [verify_result_queue(modified, classified, button, center),
               verify_result_queue(modified, classified, button, (button[0]+button[2]+5, center[1])),
               verify_result_queue(modified, classified, button, center, cancel=True)]
    assert results == [True, False, False]
    density_cases = []
    for density in (.5, 2.):
        result, rectangle = classify(modified, payload, density=density)
        assert struct.unpack_from('<4i', result) == (4, 1, 74, 74)
        assert struct.unpack_from('<4d', result, 0x20) == button == rectangle
        assert struct.unpack_from('<d', result, 0x58) == (density,)
        queued = [verify_result_queue(modified, result, button, center, density=density),
                  verify_result_queue(modified, result, button, (button[0]+button[2]+5, center[1]), density=density),
                  verify_result_queue(modified, result, button, center, cancel=True, density=density)]
        assert queued == [True, False, False], (density, queued)
        density_cases.append({'density': density, 'normalizedRectangle': rectangle, 'insideOutsideCancelQueues': queued})
    return {'newRectangle': button, 'newResult': [4, 1, 74, 74], 'gatesPreserved': list(gates),
            'circularHitTestEnforced': True,
            'chatPriorityPreserved': True, 'unrelatedAndCornerBoundaryPointsPreserved': len(points),
            'autoHideStatesAcceptingPlainJump': [0., .5, 1.],
            'insideReleaseQueuesExactlyOnce': True, 'outsideReleaseAndCancellationDoNotQueue': True,
            'densityCases': density_cases}


def verify_result_queue(binary, classified, button, release, cancel=False, density=1.):
    from unicorn import UC_HOOK_CODE
    runner = NativeHUD(binary)
    reg = runner.reg
    object_at, event, output, callback = (BASE + at for at in (0x10000, 0x20000, 0x30000, 0x7FFE0))
    # Fixture callback returns the result already produced by the real patched
    # classifier. No game instruction or patch fragment is replaced here.
    runner.uc.mem_write(callback, bytes.fromhex('c0035fd6'))
    def external(uc, address, size, _):
        if address == callback:
            uc.mem_write(uc.reg_read(reg.UC_ARM64_REG_X1), classified)
        elif address != 0x2F76A80:
            return
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))
    for address in (callback, 0x2F76A80):
        runner.uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)
    runner.call(0x2413880, [object_at])
    runner.uc.mem_write(event + 8, struct.pack('<Q', 42))
    runner.uc.mem_write(event + 0x10, struct.pack('<2d', (button[0]+button[2]/2)*density, (button[1]+button[3]/2)*density))
    runner.uc.mem_write(event + 0x20, struct.pack('<I', 1000))
    runner.call(0x2413D4C, [object_at, event, callback, 0])
    assert runner.uc.mem_read(event + 0x41, 1) == b'\1'
    assert runner.call(0x24159E0, [object_at, output]) == 0
    runner.uc.mem_write(event + 0x10, struct.pack('<2d', release[0]*density, release[1]*density))
    runner.uc.mem_write(event + 0x20, struct.pack('<I', 1100))
    runner.call(0x2414794, [object_at, event, int(cancel)])
    got = runner.call(0x24159E0, [object_at, output])
    if got:
        assert struct.unpack('<5i', bytes(runner.uc.mem_read(output, 20))) == (74, 0, 0, 0, 0)
    assert runner.call(0x24159E0, [object_at, output]) == 0
    return bool(got)


def dispatch(binary, action, mode=0, blocked=False, flying=False, player=True, ready=True,
             water=False, water_height=20., timestamp=0, now=1000):
    from unicorn import UC_HOOK_CODE
    runner = NativeHUD(binary)
    reg = runner.reg
    hud, request = BASE + 0x10000, BASE + 0x60000
    rootptr, game, run, host, mode_at, skill = (BASE + at for at in (0x65000, 0x66000, 0x67000, 0x68000, 0x6B000, 0x6C000))
    for address, value in ((0x31ABCC8, rootptr), (rootptr, game), (game + 0x38, run),
                            (run + 0x60, host if player else 0), (0x31ABCE0, mode_at)):
        runner.uc.mem_write(address, struct.pack('<Q', value))
    runner.uc.mem_write(mode_at + 0x40, struct.pack('<I', mode))
    runner.uc.mem_write(host + 0x115C, bytes([ready]))
    runner.uc.mem_write(host + 0x1C24, bytes([blocked]))
    runner.uc.mem_write(host + 0x9EC, bytes([8 if flying else 0]))
    runner.uc.mem_write(host + 0x9A0, struct.pack('<I', int(water)))
    runner.uc.mem_write(host + 0x48, struct.pack('<3f', 0., 10., 0.))
    runner.uc.mem_write(host + 0x9B4, struct.pack('<f', 10.))
    runner.uc.mem_write(0x32A395C, struct.pack('<I', timestamp))
    runner.uc.mem_write(hud + 0x378A4, struct.pack('<I', 999))
    runner.uc.mem_write(request, struct.pack('<5I', action, 0, 0, 0, 0))
    calls, environment = [], []
    def external(uc, address, size, _):
        if address == 0x1C430F0:
            calls.append(['GetSkill', uc.reg_read(reg.UC_ARM64_REG_X1)])
            uc.reg_write(reg.UC_ARM64_REG_X0, skill)
        elif address == 0x1C4A8FC:
            calls.append(['ApplySkillShortcut', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == 0x1BAAEE8:
            calls.append(['PostMessageW', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == 0x1BFE0F8:
            environment.append(['GetWaterHeight', water_height])
            uc.reg_write(reg.UC_ARM64_REG_S0, struct.unpack('<I', struct.pack('<f', water_height))[0])
        elif address == 0x2579540:
            environment.append(['GetTickCount', now])
            uc.reg_write(reg.UC_ARM64_REG_X0, now)
        else:
            raise AssertionError('Unexpected environment capture')
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))
    for address in (0x1C430F0, 0x1C4A8FC, 0x1BAAEE8, 0x1BFE0F8, 0x2579540):
        runner.uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)
    runner.call(0x23AB80C, [hud, request])
    return {'action': action, 'mode': mode, 'blocked': blocked, 'flying': flying,
            'player': player, 'ready': ready, 'water': water, 'waterHeight': water_height,
            'now': now, 'timestampBefore': timestamp,
            'timestampAfter': struct.unpack('<I', bytes(runner.uc.mem_read(0x32A395C, 4)))[0],
            'calls': calls, 'environment': environment}


def run_mobile_hud_action(binary, action, dlg_keys_flag=1):
    from unicorn import UC_HOOK_CODE
    runner = NativeHUD(binary)
    reg = runner.reg
    hud, request = BASE + 0x10000, BASE + 0x58000
    rootptr, game, run, host, mode_at, skill = (BASE + at for at in (0x65000, 0x66000, 0x67000, 0x68000, 0x6B000, 0x6C000))
    for address, value in ((0x31ABCC8, rootptr), (rootptr, game), (game + 0x38, run),
                            (run + 0x60, host), (0x31ABCE0, mode_at)):
        runner.uc.mem_write(address, struct.pack('<Q', value))
    runner.uc.mem_write(host + 0x115C, b'\1')
    runner.uc.mem_write(hud + 0x68, bytes([dlg_keys_flag]))
    runner.uc.mem_write(hud + 0x378A4, struct.pack('<I', 999))
    runner.uc.mem_write(request, struct.pack('<5I', action, 0, 0, 0, 0))
    calls, logs = [], []
    def external(uc, address, size, _):
        if address == 0x1C430F0:
            calls.append(['GetSkill', uc.reg_read(reg.UC_ARM64_REG_X1)])
            uc.reg_write(reg.UC_ARM64_REG_X0, skill)
        elif address == 0x1C4A8FC:
            calls.append(['ApplySkillShortcut', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == 0x1BAAEE8:
            calls.append(['PostMessageW', uc.reg_read(reg.UC_ARM64_REG_X1)])
        elif address == 0x1BFE0F8:
            uc.reg_write(reg.UC_ARM64_REG_S0, 0)
        elif address == 0x2579540:
            uc.reg_write(reg.UC_ARM64_REG_X0, 1000)
        elif address == 0x257A22C:
            fmt = bytes(uc.mem_read(uc.reg_read(reg.UC_ARM64_REG_X1), 96)).split(b'\0')[0].decode('ascii', 'replace')
            logs.append(fmt)
            uc.reg_write(reg.UC_ARM64_REG_X0, 0)
        else:
            raise AssertionError('Unexpected external capture')
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))
    for address in (0x1C430F0, 0x1C4A8FC, 0x1BAAEE8, 0x1BFE0F8, 0x2579540, 0x257A22C):
        runner.uc.hook_add(UC_HOOK_CODE, external, begin=address, end=address)
    runner.call(0x243DB18, [hud, request])
    return {'action': action, 'dlgKeysFlag': dlg_keys_flag, 'calls': calls, 'logs': logs}


def verify_dispatch(original, modified):
    cases = []
    old_skill, new_skill = dispatch(original, 35), dispatch(modified, 35)
    assert old_skill == new_skill
    assert new_skill['calls'] == [['GetSkill', 999], ['ApplySkillShortcut', 999]]
    cases.append(new_skill)
    plain = dispatch(modified, 74)
    assert plain['calls'] == [['PostMessageW', 0x121]]
    cases.append(plain)
    for flags in ({'mode': 2}, {'blocked': True}, {'flying': True}, {'player': False}, {'ready': False}):
        result = dispatch(modified, 74, **flags)
        assert result['calls'] == [], flags
        cases.append(result)
    for action in (0, 73, 75, 99):
        assert dispatch(original, action) == dispatch(modified, action)
    dropped_73 = run_mobile_hud_action(modified, 73, dlg_keys_flag=1)
    assert dropped_73['calls'] == [] and any('HUDDLG' in x for x in dropped_73['logs']), dropped_73
    delivered_74 = run_mobile_hud_action(modified, 74, dlg_keys_flag=1)
    assert delivered_74['calls'] == [['PostMessageW', 0x121]], delivered_74
    preserved_35 = run_mobile_hud_action(modified, 35, dlg_keys_flag=1)
    assert preserved_35['calls'] == [['GetSkill', 999], ['ApplySkillShortcut', 999]], preserved_35
    water_cases = []
    for flags, expected_calls, expected_timestamp in (
        ({'timestamp': 0, 'now': 1000}, [], 1000),
        ({'timestamp': 1000, 'now': 1999}, [], 1000),
        ({'timestamp': 1000, 'now': 2000}, [['PostMessageW', 0x121]], 0),
        ({'timestamp': 1000, 'now': 2000, 'water_height': 20.001}, [], 1000),
        ({'timestamp': 1000, 'now': 2000, 'water_height': 19.999}, [['PostMessageW', 0x121]], 0),
        ({'timestamp': 0xFFFFFF00, 'now': 744}, [['PostMessageW', 0x121]], 0),
    ):
        result = dispatch(modified, 74, water=True, **flags)
        assert result['calls'] == expected_calls, result
        assert result['timestampAfter'] == expected_timestamp, result
        water_cases.append(result)
    return {'cases': cases, 'waterCases': water_cases, 'khinhCongAction35Unchanged': True,
            'plainJump74NeverCallsGetSkillOrApplySkill': True, 'unknownBoundsPreserved': [0, 73, 75, 99],
            'mobileHudRunActionWithDlgFlag1Drops73AndDispatches74': True,
            'actualWaterHelperAndUnsigned1000msBoundaryExecuted': True,
            'limits': 'Skill APIs, water height, clock and message delivery are captured; physics/server response is not executed.'}


def verify_frozen_assembly(payload):
    if importlib.util.find_spec('keystone') is None:
        return {'result': 'SKIP', 'reason': 'Optional Keystone not installed; frozen bytes still executed in ELF checks'}
    from keystone import Ks, KS_ARCH_ARM64, KS_MODE_LITTLE_ENDIAN
    assembler = Ks(KS_ARCH_ARM64, KS_MODE_LITTLE_ENDIAN)
    for name, offset, assembly, encoded in payload.FRAGMENTS:
        actual, _ = assembler.asm(assembly, payload.CODE_VA + offset)
        assert bytes(actual) == bytes.fromhex(encoded), ('Frozen bytes mismatch', name)
    return {'result': 'PASS', 'fragments': len(payload.FRAGMENTS)}


def verify(original):
    from scripts.patch_trutien_jump import patch_library
    from scripts import trutien_plain_jump_arm64 as payload
    modified, _ = patch_library(original)
    code, label = payload.build_payload()
    assert virtual_bytes(modified, payload.CODE_VA, len(code)) == code
    assert virtual_bytes(modified, payload.LABEL_VA, len(label)) == label
    return {'result': 'PASS', 'originalLibrarySHA256': hashlib.sha256(original).hexdigest(),
            'patchedLibrarySHA256': hashlib.sha256(modified).hexdigest(),
            'restoration': verify_restoration(original, modified),
            'geometry': verify_geometry(original, modified, payload),
            'drawing': verify_draw(original, modified),
            'input': verify_input(original, modified, payload),
            'dispatch': verify_dispatch(original, modified),
            'frozenAssembly': verify_frozen_assembly(payload),
            'scope': 'Actual production-patched ELF, synthetic inputs, captured draw/environment APIs; no independent patch injection',
            'rasterRendering': 'not tested', 'deviceValidation': 'pending', 'physicsAndServerResponse': 'not tested'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--library', type=Path, required=True, help='Original ARM64 libmain.so, never modified')
    parser.add_argument('--deps', type=Path, help='Optional directory containing Unicorn and pyelftools')
    parser.add_argument('--report', type=Path, help='Optional complete JSON report (new file only)')
    args = parser.parse_args()
    if args.report is not None and (args.report.exists() or args.report.resolve() == args.library.resolve()):
        parser.error('--report must name a new file separate from --library')
    if args.deps is not None:
        sys.path.insert(0, str(args.deps.resolve()))
    try:
        report = verify(args.library.read_bytes())
        if args.report is not None:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            with args.report.open('x', encoding='utf8') as output:
                json.dump(report, output, ensure_ascii=False, indent=2)
                output.write('\n')
    except (ImportError, OSError, ValueError, AssertionError) as error:
        parser.exit(1, f'Native plain-jump verification failed: {error}\n')
    print(json.dumps({'result': report['result'], 'geometryCases': len(report['geometry']['cases']),
                      'drawCases': len(report['drawing']['cases']), 'inputGates': len(report['input']['gatesPreserved']),
                      'dispatchCases': len(report['dispatch']['cases']), 'waterCases': len(report['dispatch']['waterCases']),
                      'khinhCongRestored': True, 'productionPatchedELFExecuted': True,
                      'deviceValidation': 'pending', 'frozenAssembly': report['frozenAssembly']}))


if __name__ == '__main__':
    main()
