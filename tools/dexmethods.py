#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
List the methods (with signatures and flags) of classes in an APK's dex files.

    python3 tools/dexmethods.py <apk> <class-prefix> [...]

e.g. io/flutter/embedding/engine/FlutterJNI. A class prefix matches every class whose name starts with it.
"""
import struct
import sys
import zipfile

ACC = [(0x1, 'public'), (0x2, 'private'), (0x4, 'protected'), (0x8, 'static'), (0x10, 'final'), (0x20, 'synchronized'),
       (0x100, 'native'), (0x400, 'abstract')]


def uleb(b, o):
    r = s = 0
    while True:
        x = b[o]; o += 1
        r |= (x & 0x7f) << s; s += 7
        if x < 0x80:
            return r, o


def mutf8(b, o):
    _, o = uleb(b, o)
    e = b.index(0, o)
    return b[o:e].decode('utf-8', 'replace')


def parse(dex, prefixes):
    h = lambda off: struct.unpack_from('<II', dex, off)
    (nstr, ostr), (ntype, otype), (nproto, oproto) = h(0x38), h(0x40), h(0x48)
    (nmeth, ometh), (ncls, ocls) = h(0x58), h(0x60)
    strs = [mutf8(dex, struct.unpack_from('<I', dex, ostr + 4 * i)[0]) for i in range(nstr)]
    types = [strs[struct.unpack_from('<I', dex, otype + 4 * i)[0]] for i in range(ntype)]

    def proto(i):
        _, ret, params = struct.unpack_from('<III', dex, oproto + 12 * i)
        ps = ''
        if params:
            n = struct.unpack_from('<I', dex, params)[0]
            ps = ''.join(types[struct.unpack_from('<H', dex, params + 4 + 2 * k)[0]] for k in range(n))
        return '(%s)%s' % (ps, types[ret])

    meths = [struct.unpack_from('<HHI', dex, ometh + 8 * i) for i in range(nmeth)]
    for c in range(ncls):
        cls_idx, flags, sup, _, _, _, cdata, _ = struct.unpack_from('<8I', dex, ocls + 32 * c)
        name = types[cls_idx][1:-1]
        if not any(name.startswith(p) for p in prefixes) or not cdata:
            continue
        print('class %s extends %s' % (name, types[sup][1:-1] if sup != 0xffffffff else '-'))
        static_values = struct.unpack_from('<I', dex, ocls + 32 * c + 28)[0]
        vals = []
        if static_values:
            so = static_values
            nv, so = uleb(dex, so)
            for _ in range(nv):
                hdr = dex[so]; so += 1
                vt, va = hdr & 0x1f, hdr >> 5
                if vt in (0x00, 0x02, 0x03, 0x04, 0x06):       # byte, short, char, int, long
                    raw = dex[so:so + va + 1]; so += va + 1
                    vals.append(int.from_bytes(raw, 'little', signed=(vt != 0x03)))
                elif vt == 0x1e: vals.append(None)
                elif vt == 0x1f: vals.append(bool(va))
                else:
                    vals.append('?'); so += va + 1
        o = cdata
        sf, o = uleb(dex, o); inf, o = uleb(dex, o); dm, o = uleb(dex, o); vm, o = uleb(dex, o)
        fidx = 0
        for k in range(sf + inf):
            d, o = uleb(dex, o); _, o = uleb(dex, o)
            fidx += d if k != sf else 0
            if k == sf: fidx = d
            if k < sf and k < len(vals) and vals[k] is not None:
                _, ftype, fname = struct.unpack_from('<HHI', dex, struct.unpack_from('<II', dex, 0x50)[1] + 8 * fidx)
                print('    static %s = %s' % (strs[fname], vals[k]))
        for count in (dm, vm):
            idx = 0
            for _ in range(count):
                d, o = uleb(dex, o); fl, o = uleb(dex, o); _, o = uleb(dex, o)
                idx += d
                _, pi, ni = meths[idx]
                f = ' '.join(n for bit, n in ACC if fl & bit)
                print('    %-24s %s%s' % (f, strs[ni], proto(pi)))


z = zipfile.ZipFile(sys.argv[1])
for n in sorted(x for x in z.namelist() if x.startswith('classes') and x.endswith('.dex')):
    parse(z.read(n), sys.argv[2:])
