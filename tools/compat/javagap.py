#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
What an app's Java code uses of the Android framework that Husk's Java framework (husk-framework.dex) does not have.

    javagap.py <apk> [husk-framework.dex] [--all]

Reads every classesN.dex in the APK and lists the android.* (and javax.microedition.khronos.*) classes, methods and fields it
refers to that the framework neither defines nor inherits. Classes the app bundles itself (androidx, its own copies) are left out.
"""
import struct, sys, zipfile

def uleb(b, o):
    r = s = 0
    while True:
        x = b[o]; o += 1
        r |= (x & 0x7f) << s; s += 7
        if x < 0x80: return r, o

class Dex:
    def __init__(self, b):
        self.b = b
        h = struct.unpack_from('<8I', b, 0x38)
        (self.nstr, self.str_off, self.ntype, self.type_off, self.nproto, self.proto_off, self.nfield, self.field_off) = h
        self.nmeth, self.meth_off, self.ncls, self.cls_off = struct.unpack_from('<4I', b, 0x58)
        self._s = {}
    def s(self, i):
        if i in self._s: return self._s[i]
        o = struct.unpack_from('<I', self.b, self.str_off + 4 * i)[0]
        _, o = uleb(self.b, o)
        e = self.b.index(b'\0', o)
        v = self._s[i] = self.b[o:e].decode('utf-8', 'replace')
        return v
    def t(self, i): return self.s(struct.unpack_from('<I', self.b, self.type_off + 4 * i)[0])
    def proto(self, i):
        shorty, ret, poff = struct.unpack_from('<3I', self.b, self.proto_off + 12 * i)
        ps = ''
        if poff:
            n = struct.unpack_from('<I', self.b, poff)[0]
            ps = ''.join(self.t(struct.unpack_from('<H', self.b, poff + 4 + 2 * k)[0]) for k in range(n))
        return '(%s)%s' % (ps, self.t(ret))
    def method(self, i):
        c, p, n = struct.unpack_from('<HHI', self.b, self.meth_off + 8 * i)
        return self.t(c), self.s(n), self.proto(p)
    def field(self, i):
        c, ty, n = struct.unpack_from('<HHI', self.b, self.field_off + 8 * i)
        return self.t(c), self.s(n), self.t(ty)
    def classes(self):
        """{descriptor: (super, [interfaces], {name+sig}, {field names})}"""
        out = {}
        for k in range(self.ncls):
            ci, fl, sup, ifo, src, ann, data, sv = struct.unpack_from('<8I', self.b, self.cls_off + 32 * k)
            name = self.t(ci)
            ifs = []
            if ifo:
                n = struct.unpack_from('<I', self.b, ifo)[0]
                ifs = [self.t(struct.unpack_from('<H', self.b, ifo + 4 + 2 * j)[0]) for j in range(n)]
            meths, fields = set(), set()
            if data:
                o = data
                sf, o = uleb(self.b, o); inf, o = uleb(self.b, o); dm, o = uleb(self.b, o); vm, o = uleb(self.b, o)
                idx = 0
                for j in range(sf + inf):
                    if j == sf: idx = 0
                    d, o = uleb(self.b, o); _, o = uleb(self.b, o); idx += d
                    fields.add(self.field(idx)[1])
                for cnt in (dm, vm):
                    idx = 0
                    for j in range(cnt):
                        d, o = uleb(self.b, o); _, o = uleb(self.b, o); _, o = uleb(self.b, o); idx += d
                        _, n, sig = self.method(idx)
                        meths.add(n + sig)
            out[name] = (self.t(sup) if sup != 0xffffffff else None, ifs, meths, fields)
        return out

def dexes(path):
    if path.endswith('.dex'):
        return [Dex(open(path, 'rb').read())]
    z = zipfile.ZipFile(path)
    return [Dex(z.read(n)) for n in sorted(z.namelist()) if n.startswith('classes') and n.endswith('.dex')]

FRAMEWORK = ('Landroid/', 'Ljavax/microedition/khronos/', 'Ldalvik/system/')
# reaching java.lang.Object up the chain does not make a missing framework member present: only Object's own are
OBJECT_METHODS = {'equals(Ljava/lang/Object;)Z', 'hashCode()I', 'toString()Ljava/lang/String;', 'getClass()Ljava/lang/Class;', 'notify()V',
                  'notifyAll()V', 'wait()V', 'wait(J)V', 'wait(JI)V', 'clone()Ljava/lang/Object;', 'finalize()V', '<init>()V'}
OBJECT_FIELDS = set()

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    show_all = '--all' in sys.argv
    apk = args[0]
    fw_path = args[1] if len(args) > 1 else '/Volumes/GTAV/husk2/java/husk-framework.dex'
    fw = {}
    for d in dexes(fw_path): fw.update(d.classes())
    app_dex = dexes(apk)
    own = {}
    for d in app_dex: own.update(d.classes())

    def has(cls, member, field):
        seen = set(); todo = [cls]
        while todo:
            c = todo.pop()
            if c in seen or c is None: continue
            seen.add(c)
            info = fw.get(c) or own.get(c)
            if not info:
                if c == 'Ljava/lang/Object;':
                    if (member in OBJECT_FIELDS) if field else (member in OBJECT_METHODS): return True
                    continue
                if c.startswith('Ljava/') or c.startswith('Ljavax/') and not c.startswith('Ljavax/microedition/'):
                    return True                  # another libcore class (a framework class extending one): assume present
                continue
            sup, ifs, meths, fields = info
            if (member in fields) if field else (member in meths): return True
            todo.append(sup); todo.extend(ifs)
        return False

    missing_cls, missing = set(), {}
    for d in app_dex:
        for i in range(d.nmeth):
            c, n, sig = d.method(i)
            if not c.startswith(FRAMEWORK) or c in own: continue
            if c not in fw: missing_cls.add(c); continue
            if not has(c, n + sig, False): missing.setdefault(c, set()).add(n + sig)
        for i in range(d.nfield):
            c, n, ty = d.field(i)
            if not c.startswith(FRAMEWORK) or c in own: continue
            if c not in fw: missing_cls.add(c); continue
            if not has(c, n, True): missing.setdefault(c, set()).add(n + ':' + ty)
        for i in range(d.ntype):
            c = d.t(i)
            if c.startswith(FRAMEWORK) and c not in own and c not in fw: missing_cls.add(c)
    # an app class extending a framework class whose methods the app calls on itself: those refs name the app class, so also
    # check app classes' framework superclasses exist
    print('%s: %d framework classes missing, %d present classes lacking members' % (apk.split('/')[-1], len(missing_cls), len(missing)))
    for c in sorted(missing_cls): print('  class  ', c)
    for c in sorted(missing):
        for m in sorted(missing[c]): print('  member ', c, m)
    # members reached through the app's own subclasses (GameView extends View: GameView.getResources())
    sub = {}
    for d in app_dex:
        for i in range(d.nmeth):
            c, n, sig = d.method(i)
            if c in own and not has(c, n + sig, False):
                sub.setdefault(c, set()).add(n + sig)
    shown = 0
    for c in sorted(sub):
        # only classes whose chain reaches the framework
        chain = []; k = c
        while k in own: k = own[k][0]
        if k and k.startswith(FRAMEWORK):
            for m in sorted(sub[c]):
                print('  inherit', c, '->', k, m); shown += 1
    if show_all: pass

main()
