# SPDX-License-Identifier: GPL-2.0-or-later
"""Shared CPU-only native HUD harness, requiring optional Unicorn/pyelftools.

Only libc math/memory primitives are substituted here. Client routines execute
from their actual ELF load segments; callers supply local binaries explicitly.
No game binary, proprietary game data or device action is embedded.
"""
import io
import math
import struct


BASE = 0x50000000
CONFIG = BASE
CONTEXT = BASE + 0x10000
INTERMEDIATE = BASE + 0x20000
LAYOUT = BASE + 0x40000
STOP = BASE + 0x7FFF0
STACK = 0x600F0000


def double_bits(value):
    return struct.unpack('<Q', struct.pack('<d', value))[0]


def rect(data, offset):
    return struct.unpack_from('<4d', data, offset)


def overlap(a, b):
    return (min(a[0] + a[2], b[0] + b[2]) > max(a[0], b[0]) and
            min(a[1] + a[3], b[1] + b[3]) > max(a[1], b[1]))


HARNESS_PROFILES = {
    '2411': {
        'sincos': 0x2F7B7A0,
        'memset': 0x2F7AC40,
        'label_start': 0x242EB58,
        'label_stop': 0x242EBAC,
        'layout_compute': 0x24194EC,
        'layout_apply': 0x241C5FC,
        'layout_hittest': 0x2421098,
    },
    '2503': {
        'sincos': 0x2F9B4D0,
        'memset': 0x2F9A970,
        'label_start': 0x2447C28,
        'label_stop': 0x2447C7C,
        'layout_compute': 0x24325BC,
        'layout_apply': 0x24356CC,
        'layout_hittest': 0x243A168,
    },
    '2685': {
        'sincos': 0x2FC1B40,
        'memset': 0x2FC0FE0,
        'label_start': 0x246AED4,
        'label_stop': 0x246AF28,
        'layout_compute': 0x2455868,
        'layout_apply': 0x2458978,
        'layout_hittest': 0x245D414,
    },
}


def detect_library_version(elf):
    for segment in elf.iter_segments():
        if segment['p_type'] == 'PT_LOAD' and (segment['p_flags'] & 1):
            if segment['p_vaddr'] == 0x1B78E40:
                return '2411'
            if segment['p_vaddr'] == 0x1B7E740:
                return '2503'
            if segment['p_vaddr'] == 0x1B889A0:
                return '2685'
    raise ValueError('Unsupported libmain.so executable segment layout')


class NativeHUD:
    """Execute the actual native routines, replacing only two libc/libm calls."""

    def __init__(self, data):
        from elftools.elf.elffile import ELFFile
        from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM, UC_HOOK_CODE
        from unicorn import arm64_const

        self.reg = arm64_const
        self.uc = Uc(UC_ARCH_ARM64, UC_MODE_ARM)
        elf = ELFFile(io.BytesIO(data))
        self.version = detect_library_version(elf)
        self.hp = HARNESS_PROFILES[self.version]
        for segment in elf.iter_segments():
            if segment['p_type'] != 'PT_LOAD':
                continue
            start = segment['p_vaddr'] & ~4095
            end = (segment['p_vaddr'] + segment['p_memsz'] + 4095) & ~4095
            self.uc.mem_map(start, end - start)
            self.uc.mem_write(segment['p_vaddr'], segment.data())
        self.uc.mem_map(BASE, 0x80000)
        self.uc.mem_map(0x60000000, 0x100000)
        for address in (self.hp['sincos'], self.hp['memset']):
            self.uc.hook_add(UC_HOOK_CODE, self.external,
                             begin=address, end=address)

    def external(self, uc, address, size, _):
        reg = self.reg
        if address == self.hp['sincos']:  # sincos(double, double*, double*)
            angle = struct.unpack('<d', struct.pack('<Q', uc.reg_read(reg.UC_ARM64_REG_D0)))[0]
            uc.mem_write(uc.reg_read(reg.UC_ARM64_REG_X0), struct.pack('<d', math.sin(angle)))
            uc.mem_write(uc.reg_read(reg.UC_ARM64_REG_X1), struct.pack('<d', math.cos(angle)))
        elif address == self.hp['memset']:  # memset(void*, int, size_t)
            destination = uc.reg_read(reg.UC_ARM64_REG_X0)
            value = uc.reg_read(reg.UC_ARM64_REG_X1) & 255
            count = uc.reg_read(reg.UC_ARM64_REG_X2)
            assert count <= 0x10000, 'Unexpected native memset size'
            uc.mem_write(destination, bytes([value]) * count)
        else:
            raise AssertionError('Unexpected external shim address')
        uc.reg_write(reg.UC_ARM64_REG_PC, uc.reg_read(reg.UC_ARM64_REG_LR))

    def clear_inputs(self):
        self.uc.mem_write(BASE, bytes(0x80000))
        self.uc.mem_write(0x60000000, bytes(0x100000))

    def run(self, start, stop, count):
        self.uc.emu_start(start, stop, count=count)
        assert self.uc.reg_read(self.reg.UC_ARM64_REG_PC) == stop, 'Native instruction limit exceeded'

    def call(self, address, args):
        reg = self.reg
        for index, value in enumerate(args):
            self.uc.reg_write(reg.UC_ARM64_REG_X0 + index, value)
        self.uc.reg_write(reg.UC_ARM64_REG_SP, STACK)
        self.uc.reg_write(reg.UC_ARM64_REG_LR, STOP)
        self.run(address, STOP, 1_500_000)
        return self.uc.reg_read(reg.UC_ARM64_REG_X0)

    def label(self, cooldown):
        """Execute the real default/cooldown label-selection instruction range."""
        self.clear_inputs()
        reg = self.reg
        frame, data = BASE + 0x10000, BASE + 0x20000
        context, strings, label = BASE + 0x50000, BASE + 0x60000, BASE + 0x70000
        for register, value in ((reg.UC_ARM64_REG_X29, frame),
                                (reg.UC_ARM64_REG_SP, frame - 0x1000),
                                (reg.UC_ARM64_REG_X20, data),
                                (reg.UC_ARM64_REG_X19, LAYOUT),
                                (reg.UC_ARM64_REG_X23, context),
                                (reg.UC_ARM64_REG_X24, 1)):
            self.uc.reg_write(register, value)
        self.uc.mem_write(context + 0x20, struct.pack('<Q', strings))
        self.uc.mem_write(strings + 0x38, struct.pack('<Q', label))
        self.uc.mem_write(label, 'Original action\0'.encode('utf-32le'))
        self.uc.mem_write(data + 0x6A38, (cooldown + '\0').encode('utf-32le'))
        for register, value in ((reg.UC_ARM64_REG_D0, 35.0),
                                (reg.UC_ARM64_REG_D1, 100.0),
                                (reg.UC_ARM64_REG_D2, 0.8),
                                (reg.UC_ARM64_REG_D8, 1.0)):
            self.uc.reg_write(register, double_bits(value))
        self.run(self.hp['label_start'], self.hp['label_stop'], 100)
        pointer = self.uc.reg_read(reg.UC_ARM64_REG_X1)
        raw = bytes(self.uc.mem_read(pointer, 80))
        characters = []
        for offset in range(0, len(raw), 4):
            character = struct.unpack_from('<I', raw, offset)[0]
            if not character:
                return ''.join(characters), pointer
            characters.append(chr(character))
        raise AssertionError('Selected label is not terminated')

    def layout(self, width, height, unit, scale, v11):
        self.clear_inputs()
        values = [
            (0, width), (8, height), (0x18, width), (0x20, height),
            (0x48, scale), (0x58, 1), (0x68, 1), (0x78, unit),
            (0x80, unit), (0x90, width - 64), (0x98, height - 64),
            (0xB8, 64), (0xD0, 64), (0x388, width), (0x3B0, 0),
            (0x3C0, width), (0x3C8, height),
        ]
        for offset, value in values:
            self.uc.mem_write(CONTEXT + offset, struct.pack('<d', value))
        self.uc.mem_write(CONFIG + 0x42, bytes([v11]))
        self.call(self.hp['layout_compute'], [CONFIG, CONTEXT, INTERMEDIATE])
        intermediate = bytes(self.uc.mem_read(INTERMEDIATE, 0x10000))
        self.call(self.hp['layout_apply'], [CONFIG, CONTEXT, INTERMEDIATE, LAYOUT + 0x14C0])
        return intermediate, bytes(self.uc.mem_read(LAYOUT, 0x10000))

    def hit(self, x, y, first=0):
        self.uc.reg_write(self.reg.UC_ARM64_REG_D0, double_bits(x))
        self.uc.reg_write(self.reg.UC_ARM64_REG_D1, double_bits(y))
        return self.call(self.hp['layout_hittest'], [LAYOUT, first]) & 0xFFFFFFFF

