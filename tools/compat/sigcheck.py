#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""sigcheck.py [husk-framework.dex] [framework.jar ...]: methods of Husk's Java framework whose signature is not the platform's.

For each class both define, a Husk method is reported when no platform method of that class has its exact name and descriptor
but one has the same name and parameter count (a stand-in type such as Object where the platform has MotionRange): app bytecode
names the platform descriptor and would not link to it. Also lists classes whose kind differs (interface / class)."""
import os, sys, zipfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
src = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'javagap.py')).read().split('def main')[0]
exec(src)

def params(sig):
    p = sig[1:sig.index(')')]; n = 0; i = 0
    while i < len(p):
        while p[i] == '[': i += 1
        if p[i] == 'L': i = p.index(';', i)
        i += 1; n += 1
    return n

def flags_of(d):
    out = {}
    for k in range(d.ncls):
        ci, fl = struct.unpack_from('<2I', d.b, d.cls_off + 32 * k)
        out[d.t(ci)] = fl
    return out

args = sys.argv[1:]
fw_path = args[0] if args else '/Volumes/GTAV/husk2/java/husk-framework.dex'
plat_paths = args[1:] or ['/Volumes/GTAV/husk2/aosp/fw/framework.jar']
husk, hflags = {}, {}
for d in dexes(fw_path): husk.update(d.classes()); hflags.update(flags_of(d))
plat, pflags = {}, {}
for p in plat_paths:
    for d in dexes(p): plat.update(d.classes()); pflags.update(flags_of(d))
bad = 0
for c in sorted(husk):
    if c not in plat or c.startswith('Lhusk/'): continue
    if (hflags.get(c, 0) & 0x200) != (pflags.get(c, 0) & 0x200):
        print('kind   ', c, 'husk', 'interface' if hflags[c] & 0x200 else 'class', 'platform', 'interface' if pflags[c] & 0x200 else 'class'); bad += 1
    pm = plat[c][2]
    names = {}
    for m in pm: names.setdefault(m[:m.index('(')], []).append(m)
    for m in sorted(husk[c][2]):
        if m in pm: continue
        n = m[:m.index('(')]
        if n.startswith('husk') or n in ('<clinit>',): continue
        cands = [x for x in names.get(n, []) if params(x[len(n):]) == params(m[len(n):]) and x not in husk[c][2]]
        if cands and not n.startswith(('access$', 'lambda$')) and '$$' not in c and '-IA' not in ''.join(cands):
            print('sig    ', c, m, ' platform:', ' | '.join(cands)); bad += 1
print('%d mismatches' % bad, file=sys.stderr)
