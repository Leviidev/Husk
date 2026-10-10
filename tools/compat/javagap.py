#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
What an app's Java code uses of the Android framework that Husk's Java framework (husk-framework.dex) does not have.

    javagap.py <apk> [husk-framework.dex] [--all] [--from=Lcom/foo/,Lcom/badlogic/gdx/backends/android/]

--from counts only references made by code in classes under those prefixes (the app's own code and its engine's backend),
which is what runs: a bundled library's references to every system service it might wrap are left out.

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
    # dex instruction sizes in 16-bit units, by opcode (payloads handled separately)
    SIZES = None
    @staticmethod
    def _sizes():
        """code units per opcode, from the dex instruction formats"""
        sz = [1] * 256
        def put(n, *ops):
            for o in ops: sz[o] = n
        put(2, 0x02, 0x05, 0x08, 0x13, 0x15, 0x16, 0x19, 0x1a, 0x1c, 0x1f, 0x20, 0x22, 0x23, 0x29, 0xfe, 0xff)
        put(3, 0x03, 0x06, 0x09, 0x14, 0x17, 0x1b, 0x24, 0x25, 0x26, 0x2a, 0x2b, 0x2c, 0xfc, 0xfd)
        put(5, 0x18)
        put(4, 0xfa, 0xfb)
        put(2, *range(0x2d, 0x3e))        # cmp, if-test, if-testz
        put(2, *range(0x44, 0x6e))        # aget/aput, iget/iput, sget/sput
        put(3, *range(0x6e, 0x73), *range(0x74, 0x79))   # invoke, invoke/range
        put(2, *range(0x90, 0xb0))        # binop
        put(2, *range(0xd0, 0xe3))        # binop/lit16, binop/lit8
        return sz
    def refs_by_class(self):
        """{class descriptor: (set of method ids, set of field ids, set of type ids)} referenced from its code"""
        if Dex.SIZES is None: Dex.SIZES = Dex._sizes()
        sz, b = Dex.SIZES, self.b
        out = {}
        for k in range(self.ncls):
            ci, fl, sup, ifo, src, ann, data, sv = struct.unpack_from('<8I', b, self.cls_off + 32 * k)
            if not data: continue
            name = self.t(ci)
            ms, fs, ts = out.setdefault(name, (set(), set(), set()))
            o = data
            sf, o = uleb(b, o); inf, o = uleb(b, o); dm, o = uleb(b, o); vm, o = uleb(b, o)
            for j in range(sf + inf): _, o = uleb(b, o); _, o = uleb(b, o)
            for j in range(dm + vm):
                _, o = uleb(b, o); _, o = uleb(b, o); code, o = uleb(b, o)
                if not code: continue
                n = struct.unpack_from('<I', b, code + 12)[0]
                p, end = code + 16, code + 16 + 2 * n
                while p < end:
                    w = struct.unpack_from('<H', b, p)[0]
                    op = w & 0xff
                    if op == 0 and w != 0:
                        ident = w
                        if ident == 0x100: size = struct.unpack_from('<H', b, p + 2)[0]; p += 2 * (size * 2 + 4); continue
                        if ident == 0x200: size = struct.unpack_from('<H', b, p + 2)[0]; p += 2 * (size * 4 + 2); continue
                        if ident == 0x300:
                            width, cnt = struct.unpack_from('<HI', b, p + 2); p += 2 * ((cnt * width + 1) // 2 + 4); continue
                    if 0x6e <= op <= 0x72 or 0x74 <= op <= 0x78 or op in (0xfa, 0xfb):
                        ms.add(struct.unpack_from('<H', b, p + 2)[0])
                    elif 0x52 <= op <= 0x6d:
                        fs.add(struct.unpack_from('<H', b, p + 2)[0])
                    elif op in (0x1c, 0x1f, 0x20, 0x22, 0x23, 0x24, 0x25):
                        ts.add(struct.unpack_from('<H', b, p + 2)[0])
                    p += 2 * sz[op]
        return out
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

    scope = None
    for a in sys.argv[1:]:
        if a.startswith('--from='): scope = tuple(x for x in a[7:].split(',') if x)
    missing_cls, missing = set(), {}
    if scope:
        for d in app_dex:
            for c, (ms, fs, ts) in d.refs_by_class().items():
                if not c.startswith(scope): continue
                for i in ms:
                    mc, n, sig = d.method(i)
                    if not mc.startswith(FRAMEWORK) or mc in own: continue
                    if mc not in fw: missing_cls.add(mc); continue
                    if not has(mc, n + sig, False): missing.setdefault(mc, set()).add(n + sig)
                for i in fs:
                    fc, n, ty = d.field(i)
                    if not fc.startswith(FRAMEWORK) or fc in own: continue
                    if fc not in fw: missing_cls.add(fc); continue
                    if not has(fc, n, True): missing.setdefault(fc, set()).add(n + ':' + ty)
                for i in ts:
                    tc = d.t(i).lstrip('[')
                    if tc.startswith(FRAMEWORK) and tc not in own and tc not in fw: missing_cls.add(tc)
            # the scoped classes' own framework superclasses and interfaces
            for c, info in d.classes().items():
                if not c.startswith(scope): continue
                for t in [info[0]] + info[1]:
                    if t and t.startswith(FRAMEWORK) and t not in own and t not in fw: missing_cls.add(t)
        print('%s (from %s): %d framework classes missing, %d present classes lacking members' % (apk.split('/')[-1], ','.join(scope), len(missing_cls), len(missing)))
        for c in sorted(missing_cls): print('  class  ', c)
        for c in sorted(missing):
            for m in sorted(missing[c]): print('  member ', c, m)
        return
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
