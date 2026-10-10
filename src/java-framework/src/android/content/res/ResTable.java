package android.content.res;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.HashMap;

/**
 * A compiled resource table (resources.arsc): its packages, each type's configurations, and the entries looked up by id for the
 * device's configuration, as AssetManager's native ResTable does. Read straight from the bytes; nothing is decoded until asked for.
 */
public final class ResTable {
    static final int RES_STRING_POOL = 0x0001, RES_TABLE = 0x0002, RES_TABLE_PACKAGE = 0x0200, RES_TABLE_TYPE = 0x0201, RES_TABLE_TYPE_SPEC = 0x0202,
        RES_TABLE_LIBRARY = 0x0203;
    public static final int TYPE_NULL = 0, TYPE_REFERENCE = 1, TYPE_ATTRIBUTE = 2, TYPE_STRING = 3, TYPE_FLOAT = 4, TYPE_DIMENSION = 5, TYPE_FRACTION = 6,
        TYPE_DYNAMIC_REFERENCE = 7, TYPE_DYNAMIC_ATTRIBUTE = 8, TYPE_INT_DEC = 0x10, TYPE_INT_HEX = 0x11, TYPE_INT_BOOLEAN = 0x12;

    /** A string pool: offsets, decoded on demand. */
    public static final class StringPool {
        final ByteBuffer b;
        final int count, stringsStart, base;
        final boolean utf8;
        final String[] cache;
        StringPool(ByteBuffer b, int off) {
            this.b = b; base = off;
            int hsz = b.getShort(off + 2) & 0xffff;
            count = b.getInt(off + 8);
            int flags = b.getInt(off + 16);
            stringsStart = off + b.getInt(off + 20);
            utf8 = (flags & 0x100) != 0;
            cache = new String[count];
            this.hsz = hsz;
        }
        private final int hsz;
        public int size() { return count; }
        public String get(int i) {
            if (i < 0 || i >= count) return null;
            String s = cache[i];
            if (s != null) return s;
            int p = stringsStart + b.getInt(base + hsz + 4 * i);
            if (utf8) {
                int n = b.get(p) & 255; p += (n & 0x80) != 0 ? 2 : 1;
                n = b.get(p) & 255;
                if ((n & 0x80) != 0) { n = ((n & 0x7f) << 8) | (b.get(p + 1) & 255); p += 2; } else p++;
                byte[] d = new byte[n];
                for (int k = 0; k < n; k++) d[k] = b.get(p + k);
                s = new String(d, java.nio.charset.StandardCharsets.UTF_8);
            } else {
                int n = b.getShort(p) & 0xffff; p += 2;
                if ((n & 0x8000) != 0) { n = ((n & 0x7fff) << 16) | (b.getShort(p) & 0xffff); p += 2; }
                char[] c = new char[n];
                for (int k = 0; k < n; k++) c[k] = b.getChar(p + 2 * k);
                s = new String(c);
            }
            cache[i] = s;
            return s;
        }
    }

    /** One configuration of one type: where its entries are. */
    static final class TypeChunk {
        int off, entryCount, entriesStart, indexStart;
        StringPool keys;                    /* the key names of the package chunk this came in (a table may split one package over several) */
        boolean sparse, offset16;
        final int[] cfg = new int[Config.N];
        int density;
    }
    static final class Type {
        int id;
        String name;
        int specFlagsOff = -1, specCount;
        final ArrayList<TypeChunk> configs = new ArrayList<>();
    }
    public static final class Package {
        public int id;
        public String name;
        StringPool typeStrings, keyStrings;
        final Type[] types = new Type[256];
        HashMap<String, Integer> names;     /* "type/entry" -> id, made on first use */
        final HashMap<Integer, Integer> dynamic = new HashMap<>();
    }

    final ByteBuffer b;
    public final StringPool strings;
    public final ArrayList<Package> packages = new ArrayList<>();
    final Package[] byId = new Package[256];

    public ResTable(byte[] data) {
        b = ByteBuffer.wrap(data).order(ByteOrder.LITTLE_ENDIAN);
        int size = b.getInt(4), off = b.getShort(2) & 0xffff;
        StringPool sp = null;
        while (off + 8 <= size) {
            int t = b.getShort(off) & 0xffff, s = b.getInt(off + 4);
            if (t == RES_STRING_POOL) sp = new StringPool(b, off);
            else if (t == RES_TABLE_PACKAGE) readPackage(off, s);
            if (s <= 0) break;
            off += s;
        }
        strings = sp;
    }

    private void readPackage(int off, int size) {
        int pid = b.getInt(off + 8);
        // a table can carry one package in several chunks (the platform's does): they are one package, each chunk with its own name pools
        Package p = byId[pid & 255];
        boolean fresh = p == null;
        if (fresh) {
            p = new Package();
            p.id = pid;
            StringBuilder n = new StringBuilder();
            for (int i = 0; i < 128; i++) { char c = b.getChar(off + 12 + 2 * i); if (c == 0) break; n.append(c); }
            p.name = n.toString();
        }
        int hsz = b.getShort(off + 2) & 0xffff;
        StringPool typeStrings = new StringPool(b, off + b.getInt(off + 268)), keyStrings = new StringPool(b, off + b.getInt(off + 276));
        if (fresh) { p.typeStrings = typeStrings; p.keyStrings = keyStrings; }
        int c = off + hsz, end = off + size;
        while (c + 8 <= end) {
            int t = b.getShort(c) & 0xffff, ch = b.getShort(c + 2) & 0xffff, cs = b.getInt(c + 4);
            if (t == RES_TABLE_TYPE_SPEC) {
                int tid = b.get(c + 8) & 255;
                Type ty = type(p, tid, typeStrings);
                if (ty.specFlagsOff < 0 || b.getInt(c + 12) > ty.specCount) { ty.specCount = b.getInt(c + 12); ty.specFlagsOff = c + ch; }
            } else if (t == RES_TABLE_TYPE) {
                int tid = b.get(c + 8) & 255, flags = b.get(c + 9) & 255;
                Type ty = type(p, tid, typeStrings);
                TypeChunk tc = new TypeChunk();
                tc.off = c;
                tc.keys = keyStrings;
                tc.entryCount = b.getInt(c + 12);
                tc.entriesStart = c + b.getInt(c + 16);
                tc.indexStart = c + ch;
                tc.sparse = (flags & 1) != 0;
                tc.offset16 = (flags & 2) != 0;
                Config.read(b, c + 20, tc.cfg);
                tc.density = tc.cfg[Config.DENSITY];
                ty.configs.add(tc);
            } else if (t == RES_TABLE_LIBRARY) {
                int count = b.getInt(c + 8);
                for (int i = 0; i < count; i++) {
                    int e = c + ch + i * 260;
                    p.dynamic.put(b.getInt(e), b.getInt(e));
                }
            }
            if (cs <= 0) break;
            c += cs;
        }
        if (fresh) { packages.add(p); byId[p.id & 255] = p; }
    }
    private Type type(Package p, int tid, StringPool typeStrings) {
        Type t = p.types[tid];
        if (t == null) { t = p.types[tid] = new Type(); t.id = tid; t.name = typeStrings.get(tid - 1); }
        return t;
    }

    /** The offset of entry e in a type chunk, or -1 when this configuration does not have it. */
    int entryOffset(TypeChunk tc, int e) {
        if (tc.sparse) {
            int lo = 0, hi = tc.entryCount - 1;
            while (lo <= hi) {
                int mid = (lo + hi) >>> 1, k = b.getShort(tc.indexStart + 4 * mid) & 0xffff;
                if (k == e) return tc.entriesStart + (b.getShort(tc.indexStart + 4 * mid + 2) & 0xffff) * 4;
                if (k < e) lo = mid + 1; else hi = mid - 1;
            }
            return -1;
        }
        if (e >= tc.entryCount) return -1;
        if (tc.offset16) { int o = b.getShort(tc.indexStart + 2 * e) & 0xffff; return o == 0xffff ? -1 : tc.entriesStart + o * 4; }
        int o = b.getInt(tc.indexStart + 4 * e);
        return o == -1 ? -1 : tc.entriesStart + o;
    }

    /** A looked-up entry: where it is and in which configuration. */
    public static final class Entry {
        public int off, flags, key, density, typeChunk, specFlags;
        public boolean complex() { return (flags & 1) != 0; }
    }

    /** Husk debugging: what the table has for an id's package and type. */
    public String huskDescribe(int id) {
        Package p = byId[(id >>> 24) & 255];
        if (p == null) return "no package " + (id >>> 24) + " (" + packages.size() + " packages)";
        Type t = p.types[(id >>> 16) & 255];
        if (t == null) return "package " + p.name + " has no type " + ((id >>> 16) & 255);
        StringBuilder b = new StringBuilder("type " + t.name + ", " + t.configs.size() + " configs:");
        for (TypeChunk tc : t.configs) b.append(' ').append(tc.sparse ? "S" : "").append(tc.offset16 ? "16" : "").append('[').append(tc.entryCount).append(']').append(entryOffset(tc, id & 0xffff) >= 0 ? "has" : "-");
        return b.toString();
    }
    /** The entry for id best matching the device configuration, or null. */
    public Entry find(int id, Config cfg) {
        Package p = byId[(id >>> 24) & 255];
        if (p == null) return null;
        Type t = p.types[(id >>> 16) & 255];
        if (t == null) return null;
        int e = id & 0xffff;
        TypeChunk best = null; int bestOff = -1;
        for (int i = 0, n = t.configs.size(); i < n; i++) {
            TypeChunk tc = t.configs.get(i);
            if (!cfg.matches(tc.cfg)) continue;
            int o = entryOffset(tc, e);
            if (o < 0) continue;
            if (best == null || cfg.isBetter(tc.cfg, best.cfg)) { best = tc; bestOff = o; }
        }
        if (best == null) return null;
        Entry r = new Entry();
        r.off = bestOff;
        int sz = b.getShort(bestOff) & 0xffff;
        r.flags = b.getShort(bestOff + 2) & 0xffff;
        r.key = (r.flags & 8) != 0 ? sz : b.getInt(bestOff + 4);
        r.density = best.density;
        r.specFlags = t.specFlagsOff >= 0 && e < t.specCount ? b.getInt(t.specFlagsOff + 4 * e) : 0;
        return r;
    }

    /** A simple entry's value into v (type, data, string); false for a bag. */
    public boolean value(Entry en, android.util.TypedValue v) {
        if (en.complex()) return false;
        if ((en.flags & 8) != 0) {   /* compact: type in the high byte of flags, data where the key would be */
            v.type = (en.flags >>> 8) & 0xff;
            v.data = b.getInt(en.off + 4);
        } else {
            int sz = b.getShort(en.off) & 0xffff;
            int vo = en.off + sz;
            v.type = b.get(vo + 3) & 255;
            v.data = b.getInt(vo + 4);
        }
        v.string = v.type == TYPE_STRING ? strings.get(v.data) : null;
        v.density = en.density;
        if (v.type == TYPE_DYNAMIC_REFERENCE) v.type = TYPE_REFERENCE;
        if (v.type == TYPE_DYNAMIC_ATTRIBUTE) v.type = TYPE_ATTRIBUTE;
        return true;
    }

    /** A bag's parent and its items (attr id -> value), parents' items first, then its own. */
    public int bagParent(Entry en) { return en.complex() ? b.getInt(en.off + 8) : 0; }
    public int bagCount(Entry en) { return en.complex() ? b.getInt(en.off + 12) : 0; }
    /** The i-th item: its name (attr id) and value into v. */
    public int bagItem(Entry en, int i, android.util.TypedValue v) {
        int sz = b.getShort(en.off) & 0xffff;
        int m = en.off + sz + 12 * i;
        int name = b.getInt(m);
        v.type = b.get(m + 4 + 3) & 255;
        v.data = b.getInt(m + 8);
        if (v.type == TYPE_DYNAMIC_REFERENCE) v.type = TYPE_REFERENCE;
        if (v.type == TYPE_DYNAMIC_ATTRIBUTE) v.type = TYPE_ATTRIBUTE;
        v.string = v.type == TYPE_STRING ? strings.get(v.data) : null;
        v.density = en.density;
        return name;
    }

    public Package pkg(int id) { return byId[(id >>> 24) & 255]; }
    public String typeName(int id) { Package p = pkg(id); if (p == null) return null; Type t = p.types[(id >>> 16) & 255]; return t == null ? null : t.name; }
    public String keyName(int id, Config cfg) {
        Package p = pkg(id);
        if (p == null) return null;
        Type t = p.types[(id >>> 16) & 255];
        if (t == null) return null;
        for (TypeChunk tc : t.configs) {
            int o = entryOffset(tc, id & 0xffff);
            if (o < 0) continue;
            int fl = b.getShort(o + 2) & 0xffff;
            int key = (fl & 8) != 0 ? (b.getShort(o) & 0xffff) : b.getInt(o + 4);
            return tc.keys.get(key);
        }
        return null;
    }
    /** "type/name" -> id, over every configuration. */
    public int identifier(String pkgName, String type, String name) {
        for (Package p : packages) {
            if (pkgName != null && !pkgName.equals(p.name) && !(p.id == 0x7f && pkgName.length() > 0 && !"android".equals(pkgName))) continue;
            if (p.names == null) indexNames(p);
            Integer id = p.names.get(type + "/" + name.replace('.', '_'));
            if (id == null) id = p.names.get(type + "/" + name);
            if (id != null) return id;
        }
        return 0;
    }
    private void indexNames(Package p) {
        HashMap<String, Integer> m = new HashMap<>();
        for (Type t : p.types) {
            if (t == null) continue;
            for (TypeChunk tc : t.configs) {
                int n = tc.sparse ? tc.entryCount : tc.entryCount;
                for (int i = 0; i < n; i++) {
                    int e, o;
                    if (tc.sparse) { e = b.getShort(tc.indexStart + 4 * i) & 0xffff; o = tc.entriesStart + (b.getShort(tc.indexStart + 4 * i + 2) & 0xffff) * 4; }
                    else { e = i; o = entryOffset(tc, i); if (o < 0) continue; }
                    int fl = b.getShort(o + 2) & 0xffff;
                    int key = (fl & 8) != 0 ? (b.getShort(o) & 0xffff) : b.getInt(o + 4);
                    String k = tc.keys.get(key);
                    int id = (p.id << 24) | (t.id << 16) | e;
                    m.put(t.name + "/" + k, id);
                    m.put(t.name + "/" + k.replace('.', '_'), id);
                }
            }
        }
        p.names = m;
    }

    /** The device's configuration, and Android's rules for which resource configuration suits it best. */
    public static final class Config {
        static final int MCC = 0, MNC = 1, LANG = 2, COUNTRY = 3, ORIENTATION = 4, TOUCH = 5, DENSITY = 6, KEYBOARD = 7, NAV = 8, INPUT = 9,
            SCREEN_W = 10, SCREEN_H = 11, SDK = 12, LAYOUT = 13, UIMODE = 14, SMALLEST_W = 15, WIDTH_DP = 16, HEIGHT_DP = 17, LAYOUT2 = 18, COLOR = 19, N = 20;
        final int[] d = new int[N];
        static void read(ByteBuffer b, int o, int[] c) {
            int size = b.getInt(o);
            java.util.Arrays.fill(c, 0);
            if (size >= 8) { c[MCC] = b.getShort(o + 4) & 0xffff; c[MNC] = b.getShort(o + 6) & 0xffff; }
            if (size >= 12) { c[LANG] = b.getShort(o + 8) & 0xffff; c[COUNTRY] = b.getShort(o + 10) & 0xffff; }
            if (size >= 16) { c[ORIENTATION] = b.get(o + 12) & 255; c[TOUCH] = b.get(o + 13) & 255; c[DENSITY] = b.getShort(o + 14) & 0xffff; }
            if (size >= 20) { c[KEYBOARD] = b.get(o + 16) & 255; c[NAV] = b.get(o + 17) & 255; c[INPUT] = b.get(o + 18) & 255; }
            if (size >= 24) { c[SCREEN_W] = b.getShort(o + 20) & 0xffff; c[SCREEN_H] = b.getShort(o + 22) & 0xffff; }
            if (size >= 28) c[SDK] = b.getShort(o + 24) & 0xffff;
            if (size >= 32) { c[LAYOUT] = b.get(o + 28) & 255; c[UIMODE] = b.get(o + 29) & 255; c[SMALLEST_W] = b.getShort(o + 30) & 0xffff; }
            if (size >= 36) { c[WIDTH_DP] = b.getShort(o + 32) & 0xffff; c[HEIGHT_DP] = b.getShort(o + 34) & 0xffff; }
            if (size >= 52) { c[LAYOUT2] = b.get(o + 48) & 255; c[COLOR] = b.get(o + 49) & 255; }
        }
        /** The device: its language (two letters, packed as in the table), screen in dp, density, orientation, night mode. */
        public Config(String lang, String country, int widthDp, int heightDp, int densityDpi, boolean night, int sdk) {
            d[LANG] = pack(lang); d[COUNTRY] = pack(country);
            d[ORIENTATION] = widthDp > heightDp ? 2 : 1;
            d[TOUCH] = 3; d[DENSITY] = densityDpi; d[KEYBOARD] = 1; d[NAV] = 1;
            d[SDK] = sdk;
            int sw = Math.min(widthDp, heightDp);
            int size = sw >= 720 ? 4 : sw >= 600 ? 3 : sw >= 320 ? 2 : 1;
            d[LAYOUT] = size | 0x20 /* long */ | 0x40 /* ltr */;
            d[UIMODE] = 1 | (night ? 0x20 : 0x10);
            d[SMALLEST_W] = sw; d[WIDTH_DP] = widthDp; d[HEIGHT_DP] = heightDp;
            d[LAYOUT2] = 0x1;   /* not round */
        }
        private static int pack(String s) { return s == null || s.length() < 2 ? 0 : (s.charAt(0) & 0xff) | (s.charAt(1) & 0xff) << 8; }
        public int density() { return d[DENSITY]; }
        /** Whether a resource configuration can be used on this device at all. */
        boolean matches(int[] c) {
            if (c[LANG] != 0 && c[LANG] != d[LANG]) return false;
            if (c[COUNTRY] != 0 && c[COUNTRY] != d[COUNTRY]) return false;
            if (c[MCC] != 0 || c[MNC] != 0) return false;
            if (c[ORIENTATION] != 0 && c[ORIENTATION] != d[ORIENTATION]) return false;
            if (c[TOUCH] != 0 && c[TOUCH] != d[TOUCH]) return false;
            if (c[KEYBOARD] != 0 && c[KEYBOARD] != d[KEYBOARD]) return false;
            if (c[NAV] != 0 && c[NAV] != d[NAV]) return false;
            if ((c[INPUT] & 3) != 0 && (c[INPUT] & 3) != 1) return false;
            if ((c[INPUT] & 0xc) != 0 && (c[INPUT] & 0xc) != 0xc /* navhidden */ && (c[INPUT] & 0xc) != 4) return false;
            if (c[SCREEN_W] != 0) return false;
            if (c[SDK] != 0 && c[SDK] > d[SDK]) return false;
            int lsz = c[LAYOUT] & 0xf;
            if (lsz != 0 && lsz > (d[LAYOUT] & 0xf)) return false;
            int llong = c[LAYOUT] & 0x30;
            if (llong != 0 && llong != (d[LAYOUT] & 0x30)) return false;
            int ldir = c[LAYOUT] & 0xc0;
            if (ldir != 0 && ldir != 0x40) return false;
            int round = c[LAYOUT2] & 3;
            if (round != 0 && round != 1) return false;
            int ut = c[UIMODE] & 0xf;
            if (ut != 0 && ut != 1) return false;
            int un = c[UIMODE] & 0x30;
            if (un != 0 && un != (d[UIMODE] & 0x30)) return false;
            if (c[SMALLEST_W] != 0 && c[SMALLEST_W] > d[SMALLEST_W]) return false;
            if (c[WIDTH_DP] != 0 && c[WIDTH_DP] > d[WIDTH_DP]) return false;
            if (c[HEIGHT_DP] != 0 && c[HEIGHT_DP] > d[HEIGHT_DP]) return false;
            int wcg = c[COLOR] & 3, hdr = c[COLOR] & 0xc;
            if (wcg == 2 || hdr == 8) return false;
            return true;
        }
        /** Whether configuration a suits the device better than b (both match). */
        boolean isBetter(int[] a, int[] b) {
            if (a[LANG] != b[LANG]) return a[LANG] != 0;
            if (a[COUNTRY] != b[COUNTRY]) return a[COUNTRY] != 0;
            if ((a[LAYOUT] & 0xc0) != (b[LAYOUT] & 0xc0)) return (a[LAYOUT] & 0xc0) != 0;
            if (a[SMALLEST_W] != b[SMALLEST_W]) return a[SMALLEST_W] > b[SMALLEST_W];
            if (a[WIDTH_DP] != b[WIDTH_DP]) return a[WIDTH_DP] > b[WIDTH_DP];
            if (a[HEIGHT_DP] != b[HEIGHT_DP]) return a[HEIGHT_DP] > b[HEIGHT_DP];
            if ((a[LAYOUT] & 0xf) != (b[LAYOUT] & 0xf)) return (a[LAYOUT] & 0xf) > (b[LAYOUT] & 0xf);
            if ((a[LAYOUT] & 0x30) != (b[LAYOUT] & 0x30)) return (a[LAYOUT] & 0x30) != 0;
            if ((a[LAYOUT2] & 3) != (b[LAYOUT2] & 3)) return (a[LAYOUT2] & 3) != 0;
            if (a[ORIENTATION] != b[ORIENTATION]) return a[ORIENTATION] != 0;
            if ((a[UIMODE] & 0xf) != (b[UIMODE] & 0xf)) return (a[UIMODE] & 0xf) != 0;
            if ((a[UIMODE] & 0x30) != (b[UIMODE] & 0x30)) return (a[UIMODE] & 0x30) != 0;
            if (a[DENSITY] != b[DENSITY]) return betterDensity(a[DENSITY], b[DENSITY]);
            if (a[TOUCH] != b[TOUCH]) return a[TOUCH] != 0;
            if (a[KEYBOARD] != b[KEYBOARD]) return a[KEYBOARD] != 0;
            if (a[NAV] != b[NAV]) return a[NAV] != 0;
            if (a[SDK] != b[SDK]) return a[SDK] > b[SDK];
            return false;
        }
        /** As ResTable_config::isBetterThan for density: anydpi first, then the closest, scaling down counted twice as good as up. */
        private boolean betterDensity(int a, int b) {
            int want = d[DENSITY];
            if (a == 0xfffe) return true;
            if (b == 0xfffe) return false;
            int h = a == 0 ? 160 : a == 0xffff ? want : a, l = b == 0 ? 160 : b == 0xffff ? want : b;
            boolean aBigger = true;
            if (l > h) { int t = h; h = l; l = t; aBigger = false; }
            if (want >= h) return aBigger;
            if (l >= want) return !aBigger;
            return ((2 * l) - want) * h > want * want ? !aBigger : aBigger;
        }
    }
}
