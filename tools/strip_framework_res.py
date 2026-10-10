#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""strip_framework_res.py <framework-res.apk> <out.apk>: the platform's resources as Husk's Java framework uses them on an iPhone.

Kept: resources.arsc, the manifest, and every res/ file the resource table could pick on a phone at xhdpi (2x) or xxhdpi (3x).
Dropped: watch / car / television variants (their UI modes never match), and ldpi, mdpi, hdpi and xxxhdpi images that also come in
xhdpi or xxhdpi (the table always prefers those at 2x and 3x). Paths stay as the table names them."""
import re, sys, zipfile

src, out = sys.argv[1:3]
z = zipfile.ZipFile(src)
names = z.namelist()
DENS = {'ldpi', 'mdpi', 'hdpi', 'xhdpi', 'xxhdpi', 'xxxhdpi', 'tvdpi', 'nodpi', 'anydpi'}
SKIP_MODE = re.compile(r'-(watch|car|television|appliance|vrheadset|desk)(-|$)')

def split(path):
    parts = path.split('/')
    if len(parts) != 3 or parts[0] != 'res': return None
    quals = parts[1].split('-')
    typ, rest = quals[0], quals[1:]
    dens = [q for q in rest if q in DENS]
    other = '-'.join(q for q in rest if q not in DENS and not re.fullmatch(r'v\d+', q))
    return typ, other, dens[0] if dens else None, parts[2]

high = set()
for n in names:
    s = split(n)
    if s and s[2] in ('xhdpi', 'xxhdpi'): high.add((s[0], s[1], s[3]))
keep, dropped = [], 0
for n in names:
    s = split(n)
    if s:
        typ, other, dens, fname = s
        if SKIP_MODE.search('-' + n.split('/')[1]): dropped += 1; continue
        if dens in ('ldpi', 'mdpi', 'hdpi', 'xxxhdpi', 'tvdpi') and (typ, other, fname) in high: dropped += 1; continue
    keep.append(n)
with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as o:
    for n in keep:
        info = z.getinfo(n)
        data = z.read(n)
        # PNGs are compressed already: stored, so they can be read straight out of the file; everything else deflated
        o.writestr(zipfile.ZipInfo(n, date_time=info.date_time), data, compress_type=zipfile.ZIP_STORED if n.endswith('.png') else zipfile.ZIP_DEFLATED)
print('kept %d files, dropped %d' % (len(keep), dropped))
