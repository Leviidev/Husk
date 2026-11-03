#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Which of Husk's generated stand-in classes can be taken from AOSP's framework.jar instead (Husk 3.3, B5).

    aosppure.py <husk-framework.dex> <husk-framework-core.dex> <framework.jar> [more AOSP jars] [--out list.txt] [--why CLASS]

The generated stand-ins (gen-src) have every public signature but no behaviour: a static helper returns null, a builder keeps
nothing. Many of those classes are plain Java in AOSP (DocumentsContract's URI builders, a Builder, a Parcelable), and AOSP's
bytecode would just work -- but AOSP code also calls hidden members of the classes Husk writes by hand (Context, View), and some
of it is native (libandroid_runtime, which Husk does not have).

So a stand-in is replaced by AOSP's class only when that class has no native methods and every method and field its code
names resolves: in Husk's hand-written classes (with the member really there), in a stand-in that stays (only its public
members), in another AOSP class that is taken, or in libcore. Classes only AOSP has (com.android.internal.*) are held to the
same test. A class failing it drops out and the test runs again, until nothing changes. The classes left are written to
--out, one descriptor per line: build.sh leaves their stand-ins out of the core dex.
"""
import os, struct, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from javagap import dexes, uleb, OBJECT_METHODS

LIBCORE = ('Ljava/', 'Ljavax/', 'Llibcore/', 'Ldalvik/', 'Lsun/', 'Lorg/json/', 'Lorg/xml/', 'Lorg/w3c/', 'Lorg/xmlpull/',
           'Lorg/apache/harmony/', 'Lcom/android/org/conscrypt/', 'Lcom/android/okhttp/', 'Lcom/android/org/bouncycastle/',
           'Lcom/android/icu/', 'Landroid/icu/', 'Landroid/system/', 'Ljdk/', 'Lkotlin/')

def natives(d):
    """{class: True} for classes that declare a native method"""
    out = set()
    for k in range(d.ncls):
        ci, fl, sup, ifo, src, ann, data, sv = struct.unpack_from('<8I', d.b, d.cls_off + 32 * k)
        if not data: continue
        o = data
        sf, o = uleb(d.b, o); inf, o = uleb(d.b, o); dm, o = uleb(d.b, o); vm, o = uleb(d.b, o)
        for j in range(sf + inf): _, o = uleb(d.b, o); _, o = uleb(d.b, o)
        for j in range(dm + vm):
            _, o = uleb(d.b, o); acc, o = uleb(d.b, o); _, o = uleb(d.b, o)
            if acc & 0x100: out.add(d.t(ci)); break
    return out

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opt = {a.split('=')[0]: (a.split('=', 1)[1] if '=' in a else True) for a in sys.argv[1:] if a.startswith('--')}
    fat_p, core_p, jars = args[0], args[1], args[2:]
    fat, core = {}, {}
    for d in dexes(fat_p): fat.update(d.classes())
    for d in dexes(core_p): core.update(d.classes())
    stubs = {c: v for c, v in fat.items() if c not in core}
    aosp, refs, nat = {}, {}, set()
    for j in jars:
        for d in dexes(j):
            for c, v in d.classes().items(): aosp.setdefault(c, v)
            nat |= natives(d)
            for c, (ms, fs, ts) in d.refs_by_class().items():
                if c in refs: continue
                refs[c] = ([d.method(i) for i in ms], [d.field(i) for i in fs], [d.t(i).lstrip('[') for i in ts])
    # candidates: every AOSP class Husk does not write by hand (stand-ins and AOSP-only classes alike)
    taken = {c for c in aosp if c not in core and c not in nat}
    why = {}

    def info(c):
        if c in core: return core[c]
        if c in taken: return aosp[c]
        return stubs.get(c)

    def has(c, member, field):
        seen = set()
        while c and c not in seen:
            seen.add(c)
            i = info(c)
            if i is None:
                if c == 'Ljava/lang/Object;': return field is False and member in OBJECT_METHODS
                return c.startswith(LIBCORE)
            sup, ifs, ms, fs = i
            if (member in fs) if field else (member in ms): return True
            for x in ifs:
                if has(x, member, field): return True
            c = sup
        return False

    def ok(c):
        if c not in refs: return True
        ms, fs, ts = refs[c]
        sup, ifs = aosp[c][0], aosp[c][1]
        for t in [sup] + ifs + ts:
            if t and t.startswith('L') and info(t) is None and not t.startswith(LIBCORE): return 'needs %s' % t
        for mc, n, sig in ms:
            if mc.startswith('[') or (mc.startswith(LIBCORE) and info(mc) is None): continue
            if info(mc) is None: return 'needs %s' % mc
            if not has(mc, n + sig, False): return 'needs %s.%s%s' % (mc, n, sig)
        for fc, n, ty in fs:
            if fc.startswith(LIBCORE) and info(fc) is None: continue
            if info(fc) is None: return 'needs %s' % fc
            if not has(fc, n, True): return 'needs %s.%s' % (fc, n)
        return True

    # what Husk's hand-written classes call on other classes: a stand-in AOSP replaces must still have it
    core_refs = []
    for d in dexes(core_p):
        for c, (ms, fs, ts) in d.refs_by_class().items():
            core_refs += [(d.method(i), False) for i in ms] + [(d.field(i), True) for i in fs]

    changed = True
    while changed:
        changed = False
        for c in sorted(taken):
            r = ok(c)
            if r is not True:
                taken.discard(c); why[c] = r; changed = True
        for (tc, n, sig), field in core_refs:
            if tc in taken and tc in stubs and not has(tc, n if field else n + sig, field):
                taken.discard(tc); why[tc] = 'is called by Husk for %s%s' % (n, '' if field else sig); changed = True
    replaced = sorted(c for c in taken if c in stubs)
    print('%d AOSP classes usable; %d of Husk\'s %d stand-ins replaced; %d AOSP classes with natives' %
          (len(taken), len(replaced), len(stubs), len(nat)), file=sys.stderr)
    top = collections.Counter(r.split('.')[0].split('(')[0] for r in why.values())
    for t, n in top.most_common(15): print('  %5d  dropped because it %s' % (n, t), file=sys.stderr)
    if opt.get('--why'):
        c = opt['--why']; print(c, '->', why.get(c, 'taken' if c in taken else 'not a candidate'), file=sys.stderr)
    if opt.get('--out'):
        with open(opt['--out'], 'w') as f:
            for c in replaced: f.write(c + '\n')
    else:
        for c in replaced: print(c)

if __name__ == '__main__':
    main()
