#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
genstubs.py <out-dir> <class> [<class> ...]  [--fw=husk-framework.dex] [--platform=framework.jar]

Java source for platform classes Husk's framework lacks, from the platform's own framework.jar: the class's kind, superclass and
interfaces, its public and protected fields (static constants with the platform's values) and methods (bodies return 0, false or
null; void ones do nothing), so app bytecode that names them links. Classes are given as descriptors or dotted names
(android.view.InputDevice$MotionRange). Types their signatures need that neither framework has are generated too, as empty shells.
Members that name com.android.internal or other non-API types are left out.

A nested class whose outer class Husk writes by hand is printed to <out-dir>/NESTED-<Outer>.txt to paste into that class.
"""
import os, re, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'javagap.py')).read().split('def main')[0])

API = ('Landroid/', 'Ljava/', 'Ljavax/', 'Lorg/xml/', 'Lorg/w3c/', 'Lorg/json/', 'Lorg/xmlpull/', 'Ldalvik/')
PRIM = {'V': 'void', 'Z': 'boolean', 'B': 'byte', 'S': 'short', 'C': 'char', 'I': 'int', 'J': 'long', 'F': 'float', 'D': 'double'}
DEFAULT = {'Z': 'false', 'B': '(byte) 0', 'S': '(short) 0', 'C': "'\\0'", 'I': '0', 'J': '0L', 'F': '0f', 'D': '0d'}

def split_params(sig):
    p = sig[1:sig.index(')')]; out = []; i = 0
    while i < len(p):
        j = i
        while p[j] == '[': j += 1
        if p[j] == 'L': j = p.index(';', j)
        out.append(p[i:j + 1]); i = j + 1
    return out, sig[sig.index(')') + 1:]

def jtype(d):
    dims = 0
    while d.startswith('['): dims += 1; d = d[1:]
    t = PRIM.get(d) or d[1:-1].replace('/', '.').replace('$', '.')
    return t + '[]' * dims

def base(d): return d.lstrip('[')

class Plat:
    def __init__(self, paths):
        self.cls = {}
        for p in paths:
            for d in dexes(p):
                for k in range(d.ncls):
                    ci, fl, sup, ifo, src, ann, data, sv = struct.unpack_from('<8I', d.b, d.cls_off + 32 * k)
                    self.cls[d.t(ci)] = (d, k)
    def info(self, c):
        """kind flags, super, interfaces, fields [(name, type, flags, value)], methods [(name, sig, flags)]"""
        d, k = self.cls[c]
        b = d.b
        ci, fl, sup, ifo, src, ann, data, sv = struct.unpack_from('<8I', b, d.cls_off + 32 * k)
        ifs = []
        if ifo:
            n = struct.unpack_from('<I', b, ifo)[0]
            ifs = [d.t(struct.unpack_from('<H', b, ifo + 4 + 2 * j)[0]) for j in range(n)]
        fields, meths = [], []
        statics = []
        if data:
            o = data
            sf, o = uleb(b, o); inf, o = uleb(b, o); dm, o = uleb(b, o); vm, o = uleb(b, o)
            idx = 0
            for j in range(sf + inf):
                if j == sf: idx = 0
                dd, o = uleb(b, o); af, o = uleb(b, o); idx += dd
                _, n, ty = d.field(idx)
                fields.append([n, ty, af, None])
                if j < sf: statics.append(len(fields) - 1)
            for cnt in (dm, vm):
                idx = 0
                for j in range(cnt):
                    dd, o = uleb(b, o); af, o = uleb(b, o); _, o = uleb(b, o); idx += dd
                    _, n, sig = d.method(idx)
                    meths.append((n, sig, af))
        if sv:
            vals = self.encoded_array(d, sv)
            for i, v in enumerate(vals):
                if i < len(statics): fields[statics[i]][3] = v
        return fl, (d.t(sup) if sup != 0xffffffff else None), ifs, fields, meths
    def encoded_array(self, d, o):
        b = d.b
        n, o = uleb(b, o)
        out = []
        for _ in range(n):
            v, o = self.encoded_value(d, o)
            out.append(v)
        return out
    def encoded_value(self, d, o):
        b = d.b
        h = b[o]; o += 1
        t, arg = h & 0x1f, h >> 5
        size = arg + 1
        def raw(signed):
            v = int.from_bytes(b[o:o + size], 'little', signed=signed)
            return v
        if t in (0x00, 0x02, 0x04, 0x06):
            v = int.from_bytes(b[o:o + size], 'little', signed=True); return ('int', v, t), o + size
        if t == 0x03: return ('char', int.from_bytes(b[o:o + size], 'little'), t), o + size
        if t == 0x10:
            v = int.from_bytes(b[o:o + size], 'little') << (8 * (4 - size)); return ('float', struct.unpack('<f', v.to_bytes(4, 'little'))[0], t), o + size
        if t == 0x11:
            v = int.from_bytes(b[o:o + size], 'little') << (8 * (8 - size)); return ('double', struct.unpack('<d', v.to_bytes(8, 'little'))[0], t), o + size
        if t == 0x17: return ('string', d.s(int.from_bytes(b[o:o + size], 'little')), t), o + size
        if t == 0x1f: return ('bool', arg != 0, t), o
        if t == 0x1e: return None, o
        if t in (0x18, 0x19, 0x1a, 0x1b, 0x15, 0x16): return None, o + size
        if t == 0x1c:
            n, o2 = uleb(b, o)
            for _ in range(n): _, o2 = self.encoded_value(d, o2)
            return None, o2
        if t == 0x1d:
            _, o2 = uleb(b, o); n, o2 = uleb(b, o2)
            for _ in range(n): _, o2 = uleb(b, o2); _, o2 = self.encoded_value(d, o2)
            return None, o2
        raise ValueError('encoded value type %#x' % t)

def jstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n').replace('\r', '\\r').replace('\t', '\\t') + '"'

def literal(ty, v):
    if v is None: return None
    kind, val, t = v
    if ty == 'Z': return 'true' if val else 'false'
    if ty == 'J': return '%dL' % val
    if ty in 'IBS': return '(%s) %d' % (PRIM[ty], val) if ty in 'BS' else str(val)
    if ty == 'C': return '(char) %d' % val
    if ty == 'F':
        if val != val: return 'Float.NaN'
        if val in (float('inf'), float('-inf')): return 'Float.%s_INFINITY' % ('POSITIVE' if val > 0 else 'NEGATIVE')
        return repr(float(val)) + 'f'
    if ty == 'D':
        if val != val: return 'Double.NaN'
        if val in (float('inf'), float('-inf')): return 'Double.%s_INFINITY' % ('POSITIVE' if val > 0 else 'NEGATIVE')
        return repr(float(val))
    if ty == 'Ljava/lang/String;' and kind == 'string': return jstr(val)
    return None

EMPTY = {'Ljava/util/List;': 'new java.util.ArrayList()', 'Ljava/util/Collection;': 'new java.util.ArrayList()', 'Ljava/util/ArrayList;': 'new java.util.ArrayList()',
         'Ljava/util/Set;': 'new java.util.HashSet()', 'Ljava/util/Map;': 'new java.util.HashMap()', 'Ljava/lang/Iterable;': 'new java.util.ArrayList()'}
def dflt(t): return DEFAULT.get(t) or EMPTY.get(t) or 'null'

def box(t, v):
    return {'Z': 'Boolean.valueOf(%s)', 'B': 'Byte.valueOf(%s)', 'S': 'Short.valueOf(%s)', 'C': 'Character.valueOf(%s)', 'I': 'Integer.valueOf(%s)', 'J': 'Long.valueOf(%s)', 'F': 'Float.valueOf(%s)', 'D': 'Double.valueOf(%s)'}.get(t, '%s') % v

def unbox(t, e):
    w = {'Z': 'Boolean', 'B': 'Byte', 'S': 'Short', 'C': 'Character', 'I': 'Integer', 'J': 'Long', 'F': 'Float', 'D': 'Double'}.get(t)
    if w: return '(%s instanceof %s ? (%s) %s : %s)' % (e, w, w, e, DEFAULT[t])
    if t in EMPTY: return '(%s != null ? (%s) %s : %s)' % (e, jtype(t), e, EMPTY[t])
    return '(%s) %s' % (jtype(t), e)

# the platform: framework.jar and the mainline modules' framework jars (MediaStore, connectivity, Wi-Fi, Bluetooth...)
import glob as _glob
PLATFORM = ','.join(['/Volumes/GTAV/husk2/aosp/fw/framework.jar'] + sorted(_glob.glob('/Volumes/GTAV/husk2/modules/jars/framework-*.jar')) + ['/Volumes/GTAV/husk2/modules/jars/android.net.ipsec.ike.jar', '/Volumes/GTAV/husk2/modules/jars/updatable-media.jar'])

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    opts = dict(a[2:].split('=', 1) for a in sys.argv[1:] if a.startswith('--') and '=' in a)
    out_dir = args[0]
    names = []
    for a in args[1:]:
        if a.startswith('@'): names += [l.split('#')[0].strip() for l in open(a[1:]) if l.split('#')[0].strip()]
        else: names.append(a)
    want = ['L' + a.replace('.', '/') + ';' if not a.startswith('L') else a for a in names]
    fw = {}
    for d in dexes(opts.get('fw', '/Volumes/GTAV/husk2/java/husk-framework.dex')): fw.update(d.classes())
    plat = Plat(opts.get('platform', PLATFORM).split(','))
    # Husk's own classes: the sources in src/ (not the dex, which may be older); a nested one when its outer file declares it
    src_dir = opts.get('src', os.path.join(os.path.dirname(os.path.abspath(out_dir)), 'src'))
    src_cache = {}
    NMARK = '// ---- generated by tools/compat/genstubs.py: the platform\'s nested classes this class does not write'
    NEND = '// ---- end of generated nested classes'
    def strip_nested(text):
        b = text.find(NMARK)
        if b < 0: return text
        e = text.find(NEND, b)
        if e < 0: return text
        ls = text.rfind('\n', 0, b) + 1
        le = text.find('\n', e)
        return text[:ls] + text[le + 1 if le >= 0 else len(text):]
    def in_src(c):
        parts = c[1:-1].split('$')
        f = os.path.join(src_dir, parts[0] + '.java')
        if f not in src_cache: src_cache[f] = strip_nested(open(f).read()) if os.path.exists(f) else None
        text = src_cache[f]
        if text is None: return False
        return all(re.search(r'\b(class|interface|enum) ' + re.escape(n) + r'\b', text) for n in parts[1:])
    have = lambda c: in_src(c) or not c.startswith(('Landroid/', 'Ljavax/microedition/', 'Ldalvik/system/'))
    # the public classes nested in Husk's own classes that Husk does not write: part of those classes' API (a method taking
    # StrictMode.OnThreadViolationListener cannot be added without it); written into the class's source below
    for root, dirs, fs in os.walk(src_dir):
        for fn in fs:
            if not fn.endswith('.java'): continue
            top = 'L' + os.path.relpath(os.path.join(root, fn), src_dir)[:-5] + ';'
            if top not in plat.cls: continue
            for k in plat.cls:
                if k.startswith(top[:-1] + '$') and '$' not in k[len(top):-1] and not k[len(top):-1].isdigit() and plat.info(k)[0] & 1 \
                        and not plat.info(k)[0] & 0x2000 and not in_src(k) and k not in want:
                    want.append(k)
    full, shells, todo = [], set(), list(want)
    gen = set()
    while todo:
        c = todo.pop()
        if c in gen or have(c) or c not in plat.cls: continue
        if plat.info(c)[0] & 0x2000: continue                 # annotation types: for source only
        gen.add(c)
        if c in want or ('$' in c and c[:c.rindex('$')] + ';' in full):
            full.append(c)
            # its public nested classes are part of its API: full too
            for k in plat.cls:
                if k.startswith(c[:-1] + '$') and '$' not in k[len(c):-1] and not k[len(c):-1].isdigit() and plat.info(k)[0] & 1 and k not in gen and not have(k):
                    want.append(k); todo.append(k)
        else: shells.add(c)
        fl, sup, ifs, fields, meths = plat.info(c)
        refs = [sup] + ifs
        if c in full:
            for n, ty, af, v in fields:
                if af & 5: refs.append(base(ty))
            for n, sig, af in meths:
                if af & 5 and not af & 0x1040:
                    ps, r = split_params(sig); refs += [base(x) for x in ps + [r]]
        if '$' in c: refs.append(c[:c.rindex('$')] + ';')
        for r in refs:
            if r and r.startswith('L') and r.startswith(API) and not have(r) and r not in gen: todo.append(r)
    JDK9 = ('Ljava/util/concurrent/Flow', 'Ljava/lang/StackWalker', 'Ljava/lang/Module', 'Ljava/lang/ModuleLayer', 'Ljava/lang/ProcessHandle',
            'Ljava/lang/Runtime$Version', 'Ljava/lang/invoke/VarHandle', 'Ljava/net/http/', 'Ljava/lang/Record')
    def ok_type(t):
        t = base(t)
        if t.startswith(JDK9): return False                    # libcore has them; the Java 8 the framework compiles against does not
        return t in PRIM or (t.startswith(API) and (have(t) or t in gen))
    # source per top-level class; nested ones inside their outer
    def body(c, indent, shell):
        fl, sup, ifs, fields, meths = plat.info(c)
        simple = c[1:-1].rsplit('/', 1)[-1].rsplit('$', 1)[-1]
        is_if, is_abs, is_enum, is_ann = fl & 0x200, fl & 0x400, fl & 0x4000, fl & 0x2000
        nested = '$' in c[1:-1].rsplit('/', 1)[-1]
        mods = 'public '
        if nested and (fl & 0x8 or is_if or is_enum): pass
        stat = 'static ' if nested and not is_if and not is_enum else ''
        if is_ann: kw = '@interface'
        elif is_if: kw = 'interface'
        elif is_enum: kw = 'enum'
        else: kw = ('abstract ' if is_abs else '') + 'class'
        if fl & 0x10 and not is_if and not is_enum and not shell: kw = 'final ' + kw
        if shell and not is_if and not is_enum and not is_ann and not is_abs: kw = 'abstract ' + kw     # it has none of its interfaces' methods
        elif not shell and not is_if and not is_enum and not is_ann and not is_abs and sup in shells:
            kw = kw.replace('final ', '')
            kw = 'abstract ' + kw                                                                      # nor does what it extends
        head = '%s%s%s %s' % (mods, stat, kw, simple)
        sup_ok = sup and sup != 'Ljava/lang/Object;' and ok_type(sup) and not is_enum and not is_if
        if sup_ok: head += ' extends ' + jtype(sup)
        ifs_ok = [i for i in ifs if ok_type(i) and i != 'Ljava/lang/annotation/Annotation;']
        if ifs_ok: head += (' extends ' if is_if else ' implements ') + ', '.join(jtype(i) for i in ifs_ok)
        lines = [indent + head + ' {']
        ind = indent + '    '
        props = not shell and not is_if and not is_ann and not is_enum
        if props: lines.append(ind + 'private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();')
        # a no-argument constructor this class will have (its own public one, or the generated protected one)
        forced_abs = not shell and not is_if and not is_enum and not is_ann and not is_abs and sup in shells
        # (constructors that are written: visible, with types that exist -- one taking a hidden type is left out, and then the
        # class has the protected no-argument one written for it)
        emitted_ctors = [s2 for n, s2, af in meths if n == '<init>' and af & 5 and all(ok_type(x) for x in split_params(s2)[0])]
        can_new = not is_if and not is_abs and not is_enum and not forced_abs
        if is_enum:
            consts = [n for n, ty, af, v in fields if af & 0x4000]
            lines.append(ind + (', '.join(consts) if consts else '') + ';')
        if not shell and not is_ann:
            for n, ty, af, v in fields:
                if not af & 5 or af & 0x1000 or af & 0x4000 or not ok_type(ty): continue
                m = 'public ' if af & 1 else 'protected '
                if af & 8: m += 'static '
                lit = literal(ty, v) if af & 8 and af & 0x10 else None
                if af & 0x10 and af & 8 and lit is not None: m += 'final '
                if is_if: m = ''
                if is_if and lit is None: lit = DEFAULT.get(ty, 'null')
                lines.append(ind + '%s%s %s%s;' % (m, jtype(ty), n, (' = ' + lit) if lit is not None else ''))
            seen = set()
            real_params = {(n, sig[:sig.index(')')]) for n, sig, af in meths if not af & 0x1040}
            for n, sig, af in meths:
                bridge = af & 0x40 and not is_if and (n, sig[:sig.index(')')]) not in real_params
                if not af & 5 or (af & 0x1040 and not bridge) or n == '<clinit>' or n.startswith(('lambda$', 'access$')): continue
                if n + sig in ('equals(Ljava/lang/Object;)Z', 'hashCode()I', 'toString()Ljava/lang/String;'): continue   # keep Object's
                ps, r = split_params(sig)
                if not all(ok_type(x) for x in ps + [r]): continue
                if is_enum and (n in ('values', 'valueOf') or n == '<init>'): continue
                key = n + sig
                if key in seen: continue
                seen.add(key)
                params = ', '.join('%s p%d' % (jtype(t), i) for i, t in enumerate(ps))
                m = 'public ' if af & 1 else 'protected '
                # an override of a method Husk's own superclass declares (perhaps public where the platform has protected): public
                if not af & 1 and not af & 8 and n != '<init>':
                    k, guard = sup, 0
                    while k and guard < 64:
                        guard += 1
                        if k in fw:
                            if key in fw[k][2]: m = 'public '; break
                            k = fw[k][0]
                        elif k in gen and k in plat.cls: k = plat.info(k)[1]           # through generated classes to Husk's
                        else: break
                if af & 8: m += 'static '
                if af & 0x20 and not is_if: m += 'synchronized '
                if n == '<init>':
                    if is_if: continue
                    call = ''
                    if sup_ok:
                        call = ' ' + super_call(sup) + ' '
                        # the superclass has a constructor taking the same: the arguments go to it
                        sup_ctors = set(fw[sup][2]) if sup in fw and sup not in gen else {'<init>' + sg for nn, sg, aa in plat.info(sup)[4] if nn == '<init>' and aa & 5} if sup in gen and sup not in shells else set()
                        if ps and '<init>' + sig in sup_ctors:
                            call = ' super(%s); ' % ', '.join('p%d' % i for i in range(len(ps)))
                    lines.append(ind + '%s%s(%s) {%s}' % (m, simple, params, call))
                    continue
                ret = '' if r == 'V' else ' return %s; ' % dflt(r)
                # data holders behave: setX(v) keeps v, getX()/isX() give it back, builders chain, factories make one
                if props and not af & 8:
                    if n.startswith('set') and len(n) > 3 and len(ps) == 1 and r in ('V', c):
                        ret = ' huskProps.put("%s", %s);%s ' % (n[3:], box(ps[0], 'p0'), ' return this;' if r == c else '')
                    elif (n.startswith('get') and len(n) > 3 or n.startswith('is') and len(n) > 2) and not ps and r != 'V':
                        ret = ' return %s; ' % unbox(r, 'huskProps.get("%s")' % (n[3:] if n.startswith('get') else n[2:]))
                    elif r == c and n not in ('<init>',):
                        ret = ' return this; '
                if af & 8 and r == c and can_new:
                    ret = ' return new %s(); ' % simple
                elif n == 'build' and not af & 8 and props and r in newable and '$' in c and r == c[:c.rindex('$')] + ';' and r not in shells:
                    # a Builder's product: what was set on the builder, it has (same property names, kept the same way)
                    ret = ' %s x = new %s(); x.huskProps.putAll(huskProps); return x; ' % (jtype(r), jtype(r))
                elif n in ('build', 'create', 'obtain', 'newInstance', 'getInstance', 'from', 'of', 'copy', 'clone') and r in newable:
                    ret = ' return new %s(); ' % jtype(r)
                if is_if:
                    if af & 0x400: lines.append(ind + '%s %s(%s);' % (jtype(r), n, params))
                    elif af & 8: lines.append(ind + 'static %s %s(%s) {%s}' % (jtype(r), n, params, ret))
                    else: lines.append(ind + 'default %s %s(%s) {%s}' % (jtype(r), n, params, ret))
                elif af & 0x400: lines.append(ind + '%sabstract %s %s(%s);' % (m, jtype(r), n, params))
                else: lines.append(ind + '%s%s %s(%s) {%s}' % (m, jtype(r), n, params, ret))
            # a concrete class implements what its generated superclasses and interfaces leave abstract (the platform's does, in
            # methods that are hidden or that take types left out)
            if not is_if and not is_abs and not forced_abs and not is_enum:
                pk = lambda n, sg: n + sg[:sg.index(')')]       # by parameters: a covariant return is the same method
                done = {pk(x[:x.index('(')], x[x.index('('):]) for x in seen}
                chain, k = [], sup
                while k and k in gen and k not in shells:
                    chain.append(k)
                    k = plat.info(k)[1]
                for k in chain:
                    for n, sg, af in plat.info(k)[4]:
                        if not af & 0x400 and af & 5: done.add(pk(n, sg))
                todo_if, seen_if = list(ifs_ok), set()
                for k in chain: todo_if += [i for i in plat.info(k)[2] if ok_type(i)]
                abstract = []
                for k in chain: abstract += [(n, sg, af) for n, sg, af in plat.info(k)[4] if af & 0x400 and af & 5]
                while todo_if:
                    i = todo_if.pop()
                    if i in seen_if or i not in gen or i in shells: continue
                    seen_if.add(i)
                    ifl, isup, iifs, ifields, imeths = plat.info(i)
                    abstract += [(n, sg, af) for n, sg, af in imeths if af & 0x400 and not af & 8]
                    todo_if += iifs
                for n, sg, af in abstract:
                    if pk(n, sg) in done: continue
                    ps, r = split_params(sg)
                    if not all(ok_type(x) for x in ps + [r]): continue
                    done.add(pk(n, sg))
                    params = ', '.join('%s p%d' % (jtype(t), i) for i, t in enumerate(ps))
                    ret = '' if r == 'V' else ' return %s; ' % dflt(r)
                    lines.append(ind + '%s%s %s(%s) {%s}' % ('public ' if af & 1 else 'protected ', jtype(r), n, params, ret))
            # a class with no public constructor still needs one its generated subclasses can call; and one with only
            # constructors taking arguments gets a hidden no-argument one, so its factories and builders can make it
            if not is_if and not is_enum and not any(n == '<init>' and af & 5 for n, s, af in meths):
                lines.append(ind + 'protected %s() {%s}' % (simple, (' ' + super_call(sup) + ' ') if sup_ok else ''))
            elif not is_if and not is_enum and emitted_ctors and '()V' not in emitted_ctors:
                # through one of its own constructors: that one already reaches a superclass constructor it can call
                best = min(emitted_ctors, key=lambda sg: len(split_params(sg)[0]))
                args = ', '.join('(%s) %s' % (jtype(t), DEFAULT.get(t, 'null')) for t in split_params(best)[0])
                lines.append(ind + '%s() { this(%s); }' % (simple, args))
        elif shell and not is_if and not is_enum and not is_ann:
            lines.append(ind + 'protected %s() {%s}' % (simple, (' ' + super_call(sup) + ' ') if sup_ok else ''))
        for k in sorted(gen):
            if k.startswith(c[:-1] + '$') and '$' not in k[len(c):-1] and not plat.info(k)[0] & 0x2000:
                lines += body(k, ind, k in shells)
        lines.append(indent + '}')
        return lines
    # generated classes that can be made with no arguments (their factories and builders hand out one rather than null)
    newable = set()
    for k in gen:
        if k in shells: continue
        kfl, ksup, kifs, kfields, kmeths = plat.info(k)
        if kfl & 0x600 or kfl & 0x4000 or ksup in shells: continue
        if '$' in k[1:-1].rsplit('/', 1)[-1] and not kfl & 0x8 and any(n == '<init>' and len(split_params(sg)[0]) > 0 and split_params(sg)[0][0] == k[:k.rindex('$')] + ';' for n, sg, a in kmeths): continue
        newable.add(k)                                         # every concrete generated class has a no-argument constructor
    ctor_cache = {}
    def super_call(sup):
        """super(...) to a constructor the superclass really has: the one with the fewest parameters"""
        cands = []
        if sup in fw and sup not in gen: cands = [m for m in fw[sup][2] if m.startswith('<init>(')]
        elif sup not in gen and sup.startswith(('Ljava/', 'Ljavax/')):
            if 'core' not in ctor_cache:
                art = '/Volumes/GTAV/husk2/root/apex/com.android.art/javalib/'
                ctor_cache['core'] = Plat([art + 'core-oj.jar', art + 'core-libart.jar'])
            core = ctor_cache['core']
            if sup in core.cls:
                cands = ['<init>' + sg for n, sg, a in core.info(sup)[4] if n == '<init>' and a & 5 and all(ok_type(t) or base(t).startswith('Ljava/') for t in split_params(sg)[0])]
        elif sup in gen and sup not in shells:
            # a generated class has the platform's visible constructors (or a made-up no-argument one when it has none)
            cands = ['<init>' + sg for n, sg, a in plat.info(sup)[4] if n == '<init>' and a & 5 and all(ok_type(t) for t in split_params(sg)[0])]
            if '$' in sup:                                      # an inner class's outer instance: the generated nested class is static
                outer = sup[:sup.rindex('$')] + ';'
                inner = [m for m in cands if not m.startswith('<init>(' + outer)]
                if inner or not cands: cands = inner
            if not cands: cands = ['<init>()V']
        elif sup in gen: cands = ['<init>()V']
        if not cands or '<init>()V' in cands: return 'super();' if cands else ''
        # the fewest parameters; among those, not a file or a name (java.io's constructors that open files throw)
        best = min(cands, key=lambda m: (len(split_params(m[6:])[0]), any(t in ('Ljava/io/File;', 'Ljava/lang/String;') for t in split_params(m[6:])[0])))
        ps, _ = split_params(best[6:])
        return 'super(%s);' % ', '.join('(%s) %s' % (jtype(t), DEFAULT.get(t, 'null')) for t in ps)
    # start clean: the previous run's files go (NESTED snippets too)
    for root, dirs, files in os.walk(out_dir):
        for f in files:
            if f.endswith('.java') or f.startswith('NESTED-'): os.remove(os.path.join(root, f))
    written = []
    nested_for = {}
    for c in sorted(gen):
        name = c[1:-1]
        if '$' in name.rsplit('/', 1)[-1]:
            outer = c[:c.rindex('$')] + ';'
            if outer in gen: continue                          # written inside its outer
            # outer is Husk's own top-level class: into its source, in a marked section rewritten every run
            if '$' not in outer and os.path.exists(os.path.join(src_dir, outer[1:-1] + '.java')):
                nested_for.setdefault(outer, []).extend(body(c, '    ', c in shells))
                continue
            # outer is Husk's: print to paste into it (NESTED-android.app.Notification$Action.txt goes inside Action)
            path = os.path.join(out_dir, 'NESTED-' + outer[1:-1].replace('/', '.') + '.txt')
            with open(path, 'a') as f: f.write('\n'.join(body(c, '    ', c in shells)) + '\n')
            written.append(path)
            continue
        pkg, simple = name.rsplit('/', 1)
        path = os.path.join(out_dir, pkg, simple + '.java')
        os.makedirs(os.path.dirname(path), exist_ok=True)
        src = ['// Generated by tools/compat/genstubs.py from the platform\'s framework.jar: signatures and constants only.', 'package %s;' % pkg.replace('/', '.'), '', '@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})']
        src += body(c, '', c in shells)
        open(path, 'w').write('\n'.join(src) + '\n')
        written.append(path)
    # Husk's classes: their missing nested classes go in before the class's closing brace (old sections first come out)
    for root, dirs, fs in os.walk(src_dir):
        for fn in fs:
            if not fn.endswith('.java'): continue
            path = os.path.join(root, fn)
            top = 'L' + os.path.relpath(path, src_dir)[:-5] + ';'
            text = open(path).read()
            new_text = strip_nested(text)
            lines = nested_for.get(top)
            if lines:
                end = new_text.rstrip().rfind('}')
                new_text = new_text[:end].rstrip('\n') + '\n    ' + NMARK + '\n' + '\n'.join(lines) + '\n    ' + NEND + '\n' + new_text[end:]
            if new_text != text:
                open(path, 'w').write(new_text)
                written.append(path)
    for w in sorted(set(written)): print(w)

main()
