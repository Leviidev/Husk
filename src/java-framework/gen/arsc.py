#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
A reader for Android's compiled resource table (resources.arsc), for Husk's build tools: the names and ids of a package's
resources, which of them are public, and their values in the default configuration.
"""
import struct

RES_STRING_POOL, RES_TABLE, RES_TABLE_PACKAGE, RES_TABLE_TYPE, RES_TABLE_TYPE_SPEC = 0x0001, 0x0002, 0x0200, 0x0201, 0x0202
SPEC_PUBLIC = 0x40000000


def _pool(b, off):
    typ, hsz, size = struct.unpack_from('<HHI', b, off)
    count, styles, flags, sstart, _ = struct.unpack_from('<IIIII', b, off + 8)
    utf8 = bool(flags & 0x100)
    offs = struct.unpack_from('<%dI' % count, b, off + hsz)
    out = []
    base = off + sstart
    for o in offs:
        p = base + o
        if utf8:
            n = b[p]; p += 2 if n & 0x80 else 1
            n = b[p]
            if n & 0x80: n = ((n & 0x7f) << 8) | b[p + 1]; p += 2
            else: p += 1
            out.append(b[p:p + n].decode('utf-8', 'replace'))
        else:
            n = struct.unpack_from('<H', b, p)[0]; p += 2
            if n & 0x8000: n = ((n & 0x7fff) << 16) | struct.unpack_from('<H', b, p)[0]; p += 2
            out.append(b[p:p + 2 * n].decode('utf-16-le', 'replace'))
    return out


class Package:
    def __init__(self):
        self.id = 0; self.name = ''
        self.types = []          # type index (1-based) -> name
        self.keys = []
        self.spec = {}           # type id -> [flags per entry]
        self.entries = {}        # (type id, entry) -> key name, from any configuration
        self.default = {}        # (type id, entry) -> (data type, data) in the default configuration


def read(path_or_bytes):
    b = path_or_bytes if isinstance(path_or_bytes, (bytes, bytearray)) else open(path_or_bytes, 'rb').read()
    typ, hsz, size = struct.unpack_from('<HHI', b, 0)
    assert typ == RES_TABLE
    off = hsz
    strings, pkgs = [], []
    while off < size:
        t, h, s = struct.unpack_from('<HHI', b, off)
        if t == RES_STRING_POOL: strings = _pool(b, off)
        elif t == RES_TABLE_PACKAGE: pkgs.append(_package(b, off))
        off += s
    return strings, pkgs


def _package(b, off):
    t, h, s = struct.unpack_from('<HHI', b, off)
    p = Package()
    p.id = struct.unpack_from('<I', b, off + 8)[0]
    p.name = b[off + 12:off + 12 + 256].decode('utf-16-le').split('\0')[0]
    type_strings, _, key_strings = struct.unpack_from('<III', b, off + 268)
    p.types = [''] + _pool(b, off + type_strings)
    p.keys = _pool(b, off + key_strings)
    c = off + h
    end = off + s
    while c < end:
        ct, ch, cs = struct.unpack_from('<HHI', b, c)
        if ct == RES_TABLE_TYPE_SPEC:
            tid = b[c + 8]
            n = struct.unpack_from('<I', b, c + 12)[0]
            p.spec[tid] = list(struct.unpack_from('<%dI' % n, b, c + ch))
        elif ct == RES_TABLE_TYPE:
            tid = b[c + 8]; flags = b[c + 9]
            n, estart = struct.unpack_from('<II', b, c + 12)
            cfgsize = struct.unpack_from('<I', b, c + 20)[0]
            cfg = b[c + 20:c + 20 + cfgsize]
            is_default = all(x == 0 for x in cfg[4:])
            sparse = bool(flags & 1)
            idx = []
            if sparse:
                for i in range(n):
                    e, o = struct.unpack_from('<HH', b, c + ch + 4 * i)
                    idx.append((e, o * 4))
            elif flags & 2:   # offset16
                for i in range(n):
                    o = struct.unpack_from('<H', b, c + ch + 2 * i)[0]
                    if o != 0xffff: idx.append((i, o * 4))
            else:
                for i in range(n):
                    o = struct.unpack_from('<I', b, c + ch + 4 * i)[0]
                    if o != 0xffffffff: idx.append((i, o))
            for e, o in idx:
                ep = c + estart + o
                esz, eflags, key = struct.unpack_from('<HHI', b, ep)
                if eflags & 0x8:   # compact
                    key = esz
                p.entries[(tid, e)] = p.keys[key]
                if is_default and not (eflags & 1) and not (eflags & 0x8):
                    vsz, _, dt, data = struct.unpack_from('<HBBI', b, ep + esz)
                    p.default[(tid, e)] = (dt, data)
        c += cs
    return p
