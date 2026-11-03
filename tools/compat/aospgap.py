#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
What AOSP's framework.jar calls on the classes Husk's own framework defines, that Husk's classes lack (3.3's AOSP boot path).

    aospgap.py <husk-framework-core.dex> <framework.jar> [more AOSP jars] [--used-by=Landroid/view/autofill/,...]

On that path husk-framework-core.dex comes first, so a class it defines (Context, View, Activity) shadows AOSP's, and AOSP code
that runs (every class Husk does not define) calls into Husk's copy. A member Husk's copy lacks is a NoSuchMethodError or
NoSuchFieldError at run time. Prints those members, the ones most AOSP classes use first.
"""
import collections, os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from javagap import dexes, OBJECT_METHODS

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    scope = None
    for a in sys.argv[1:]:
        if a.startswith('--used-by='): scope = tuple(x for x in a[10:].split(',') if x)
    husk = {}
    for d in dexes(args[0]): husk.update(d.classes())
    aosp_dex = [d for j in args[1:] for d in dexes(j)]
    aosp = {}
    for d in aosp_dex:
        for c, info in d.classes().items(): aosp.setdefault(c, info)
    eff = dict(aosp); eff.update(husk)

    def has(cls, member, field):
        seen, todo = set(), [cls]
        while todo:
            c = todo.pop()
            if c is None or c in seen: continue
            seen.add(c)
            info = eff.get(c)
            if not info:
                if c == 'Ljava/lang/Object;':
                    if not field and member in OBJECT_METHODS: return True
                    continue
                if c.startswith(('Ljava/', 'Ljavax/', 'Llibcore/', 'Ldalvik/', 'Lsun/', 'Lorg/')): return True   # libcore: assume present
                continue
            sup, ifs, meths, fields = info
            if (member in fields) if field else (member in meths): return True
            todo.append(sup); todo.extend(ifs)
        return False

    missing = collections.defaultdict(set)    # (class, member) -> AOSP classes that use it
    for d in aosp_dex:
        for c, (ms, fs, ts) in d.refs_by_class().items():
            if c in husk: continue                 # shadowed: Husk's copy runs, not this code
            if scope and not c.startswith(scope): continue
            for i in ms:
                mc, n, sig = d.method(i)
                if mc in husk and not has(mc, n + sig, False): missing[(mc, n + sig)].add(c)
            for i in fs:
                fc, n, ty = d.field(i)
                if fc in husk and not has(fc, n, True): missing[(fc, n + ':' + ty)].add(c)
    for (c, m), users in sorted(missing.items(), key=lambda kv: (-len(kv[1]), kv[0])):
        print('%4d  %s %s   e.g. %s' % (len(users), c, m, sorted(users)[0]))
    print('%d members missing from %d Husk classes' % (len(missing), len({c for c, _ in missing})), file=sys.stderr)

if __name__ == '__main__':
    main()
