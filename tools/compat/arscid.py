#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Every value one resource id has in an APK, per configuration: what Android would choose between.

    arscid.py <apk> <id, e.g. 0x7f0d0000>

Prints each configuration that defines the entry (density, sdk, night, locale...) and its value (a file path for drawables).
"""
import struct, sys, zipfile

def strpool(b, o):
    _, hsz, size, n, nstyle, flags, sstart, _ = struct.unpack_from('<HHIIIIII', b, o)
    utf8 = flags & 0x100
    offs = struct.unpack_from('<%dI' % n, b, o + hsz)
    out = []
    for so in offs:
        p = o + sstart + so
        if utf8:
            l = b[p]; p += 2 if l & 0x80 else 1
            l = b[p]
            if l & 0x80: l = ((l & 0x7f) << 8) | b[p + 1]; p += 2
            else: p += 1
            out.append(b[p:p + l].decode('utf-8', 'replace'))
        else:
            l = struct.unpack_from('<H', b, p)[0]; p += 2
            if l & 0x8000: l = ((l & 0x7fff) << 16) | struct.unpack_from('<H', b, p)[0]; p += 2
            out.append(b[p:p + 2 * l].decode('utf-16-le', 'replace'))
    return out

def config(b, o):
    size = struct.unpack_from('<I', b, o)[0]
    c = b[o:o + size]
    mcc, mnc = struct.unpack_from('<HH', c, 4)
    lang, country = c[8:10], c[10:12]
    orient, touch, dens = c[12], c[13], struct.unpack_from('<H', c, 14)[0]
    sdk = struct.unpack_from('<H', c, 24)[0] if size >= 26 else 0
    uimode = c[29] if size > 29 else 0
    parts = []
    if lang != b'\0\0': parts.append(lang.decode('latin1') + ('-r' + country.decode('latin1') if country != b'\0\0' else ''))
    if orient: parts.append(['', 'port', 'land', 'square'][orient] if orient < 4 else 'orient%d' % orient)
    if uimode & 0x30: parts.append('night' if uimode & 0x30 == 0x20 else 'notnight')
    if dens: parts.append({120: 'ldpi', 160: 'mdpi', 213: 'tvdpi', 240: 'hdpi', 320: 'xhdpi', 480: 'xxhdpi', 640: 'xxxhdpi', 0xfffe: 'anydpi', 0xffff: 'nodpi'}.get(dens, '%ddpi' % dens))
    if sdk: parts.append('v%d' % sdk)
    return '-'.join(parts) or '(default)'

def main():
    apk, rid = sys.argv[1], int(sys.argv[2], 0)
    b = zipfile.ZipFile(apk).read('resources.arsc')
    want_pkg, want_type, want_entry = rid >> 24, (rid >> 16) & 0xff, rid & 0xffff
    _, hsz, size, npkg = struct.unpack_from('<HHII', b, 0)
    o = hsz
    gstr = None
    while o < size:
        t, h, s = struct.unpack_from('<HHI', b, o)
        if t == 1: gstr = strpool(b, o)
        elif t == 0x200:
            pid = struct.unpack_from('<I', b, o + 8)[0]
            tstr_off, _, kstr_off = struct.unpack_from('<III', b, o + 268)
            types = strpool(b, o + tstr_off); keys = strpool(b, o + kstr_off)
            p = o + h
            while p < o + s:
                ct, ch, cs = struct.unpack_from('<HHI', b, p)
                if ct == 0x201 and pid == want_pkg:
                    tid = b[p + 8]; flags = b[p + 9]
                    if tid == want_type:
                        n, start = struct.unpack_from('<II', b, p + 12)
                        cfg = config(b, p + 20)
                        sparse = flags & 1
                        eo = None
                        if sparse:
                            for k in range(n):
                                idx, off = struct.unpack_from('<HH', b, p + ch + 4 * k)
                                if idx == want_entry: eo = off * 4; break
                        elif want_entry < n:
                            v = struct.unpack_from('<I', b, p + ch + 4 * want_entry)[0]
                            if v != 0xffffffff: eo = v
                        if eo is not None:
                            e = p + start + eo
                            esz, eflags, key = struct.unpack_from('<HHI', b, e)
                            if eflags & 1: val = '(bag)'
                            else:
                                vsz, _, vt, vd = struct.unpack_from('<HBBI', b, e + esz)
                                val = gstr[vd] if vt == 3 and gstr else 'type %#x data %#x' % (vt, vd)
                            print('%-24s %s/%s = %s' % (cfg, types[tid - 1], keys[key], val))
                p += cs
        o += s

if __name__ == '__main__':
    main()
