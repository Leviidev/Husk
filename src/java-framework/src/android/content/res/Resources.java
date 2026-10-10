package android.content.res;

import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.util.TypedValue;
import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.util.HashMap;

/**
 * The app's resources: its resources.arsc and the platform's (framework-res, package 0x01), looked up for the device's configuration.
 * Files (layouts, drawables, raw) are read out of the APK they belong to. Drawable and colour inflation is in husk.ResInflate.
 */
public class Resources {
    public static final int ID_NULL = 0;
    public static class NotFoundException extends RuntimeException {
        public NotFoundException() {} public NotFoundException(String s) { super(s); } public NotFoundException(String s, Exception e) { super(s, e); }
    }

    // ---- the tables, shared by every Resources of the app
    static ResTable sApp, sFramework;
    static final Object sLock = new Object();
    private static Resources sSystem;

    private final AssetManager mAssets;
    private final DisplayMetrics mMetrics = new DisplayMetrics();
    private final Configuration mConfig = Configuration.huskDevice();
    private ResTable.Config mCfg;
    private final HashMap<Integer, Drawable.ConstantState> mDrawableCache = new HashMap<>();
    private final HashMap<Integer, ColorStateList> mColorCache = new HashMap<>();
    private final HashMap<Integer, Typeface> mFontCache = new HashMap<>();

    public Resources(AssetManager a) { this(a, null, null); }
    public Resources(AssetManager a, DisplayMetrics m, Configuration c) {
        mAssets = a;
        mMetrics.setToDefaults();
        if (c != null) mConfig.setTo(c);
        updateCfg();
        loadTables();
    }
    private void updateCfg() {
        String lang = mConfig.locale != null ? mConfig.locale.getLanguage() : "en", country = mConfig.locale != null ? mConfig.locale.getCountry() : "US";
        mCfg = new ResTable.Config(lang, country, mConfig.screenWidthDp, mConfig.screenHeightDp, mMetrics.densityDpi,
                                   (mConfig.uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES, android.os.Build.VERSION.SDK_INT);
    }
    private static void loadTables() {
        synchronized (sLock) {
            if (sApp == null) {
                byte[] d = husk.Native.readApkFile(0, "resources.arsc");
                sApp = d != null ? new ResTable(d) : new ResTable(emptyTable());
            }
            if (sFramework == null) {
                byte[] d = husk.Native.readApkFile(1, "resources.arsc");
                sFramework = d != null ? new ResTable(d) : new ResTable(emptyTable());
            }
        }
    }
    private static byte[] emptyTable() { return new byte[] { 2, 0, 12, 0, 12, 0, 0, 0, 0, 0, 0, 0 }; }
    static ResTable table(int id) { return ((id >>> 24) == 1) ? sFramework : sApp; }

    public static Resources getSystem() { synchronized (sLock) { if (sSystem == null) sSystem = new Resources(new AssetManager()); return sSystem; } }
    public final AssetManager getAssets() { return mAssets; }
    public DisplayMetrics getDisplayMetrics() { return mMetrics; }
    public Configuration getConfiguration() { return mConfig; }
    public void updateConfiguration(Configuration c, DisplayMetrics m) {
        if (c != null) mConfig.setTo(c);
        if (m != null) mMetrics.setTo(m);
        updateCfg();
        mDrawableCache.clear(); mColorCache.clear();
    }
    public final void flushLayoutCache() {}
    public ResTable.Config huskConfig() { return mCfg; }

    // ---- raw lookups
    /** The entry's value, references followed (when resolveRefs), into out; false when there is none. */
    public boolean huskValue(int id, TypedValue out, boolean resolveRefs) {
        for (int depth = 0; depth < 20; depth++) {
            ResTable t = table(id);
            ResTable.Entry e = t.find(id, mCfg);
            if (e == null) return false;
            out.resourceId = id;
            out.changingConfigurations = 0;
            out.assetCookie = (id >>> 24) == 1 ? 1 : 0;
            out.sourceResourceId = id;
            if (e.complex()) { out.type = TypedValue.TYPE_NULL; out.data = 0; out.string = null; out.density = e.density; return true; }
            t.value(e, out);
            out.resourceId = id;
            if (!resolveRefs || out.type != TypedValue.TYPE_REFERENCE || out.data == 0) return true;
            id = out.data;
        }
        return false;
    }
    public void getValue(int id, TypedValue out, boolean resolveRefs) throws NotFoundException {
        if (!huskValue(id, out, resolveRefs)) throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id));
    }
    public void getValueForDensity(int id, int density, TypedValue out, boolean resolveRefs) { getValue(id, out, resolveRefs); }
    public void getValue(String name, TypedValue out, boolean resolveRefs) { int id = getIdentifier(name, "string", null); getValue(id, out, resolveRefs); }

    /** A bag (style, array, plurals, attr): its items with parents' first; null when the id is not a bag. */
    public Bag huskBag(int id) {
        if (id == 0) return null;
        HashMap<Integer, Bag> cache = bagCache();
        Bag cached = cache.get(id);
        if (cached != null) return cached;
        ResTable t = table(id);
        ResTable.Entry e = t.find(id, mCfg);
        if (e == null || !e.complex()) return null;
        Bag b = new Bag();
        int parent = t.bagParent(e);
        if (parent != 0 && parent != id) {
            Bag pb = huskBag(parent);
            if (pb != null) { for (int i = 0; i < pb.n; i++) b.put(pb.keys[i], pb.types[i], pb.data[i], pb.strings[i], pb.dens[i]); }
        }
        int n = t.bagCount(e);
        TypedValue v = new TypedValue();
        for (int i = 0; i < n; i++) {
            int name = t.bagItem(e, i, v);
            b.put(name, v.type, v.data, v.string, v.density);
        }
        cache.put(id, b);
        return b;
    }
    private HashMap<Integer, Bag> mBags;
    private HashMap<Integer, Bag> bagCache() { if (mBags == null) mBags = new HashMap<>(); return mBags; }
    /** A bag's items, keyed by attribute (or array index). */
    public static final class Bag {
        public int n; public int[] keys = new int[8], types = new int[8], data = new int[8], dens = new int[8]; public CharSequence[] strings = new CharSequence[8];
        void put(int k, int t, int d, CharSequence s, int density) {
            for (int i = 0; i < n; i++) if (keys[i] == k) { types[i] = t; data[i] = d; strings[i] = s; dens[i] = density; return; }
            if (n == keys.length) { keys = java.util.Arrays.copyOf(keys, n * 2); types = java.util.Arrays.copyOf(types, n * 2); data = java.util.Arrays.copyOf(data, n * 2); strings = java.util.Arrays.copyOf(strings, n * 2); dens = java.util.Arrays.copyOf(dens, n * 2); }
            keys[n] = k; types[n] = t; data[n] = d; strings[n] = s; dens[n] = density; n++;
        }
        public int indexOf(int k) { for (int i = 0; i < n; i++) if (keys[i] == k) return i; return -1; }
    }

    // ---- names
    public int getIdentifier(String name, String defType, String defPackage) {
        if (name == null) return 0;
        String pkg = defPackage, type = defType, n = name;
        if (n.startsWith("@")) n = n.substring(1);
        int colon = n.indexOf(':');
        if (colon >= 0) { pkg = n.substring(0, colon); n = n.substring(colon + 1); }
        int slash = n.indexOf('/');
        if (slash >= 0) { type = n.substring(0, slash); n = n.substring(slash + 1); }
        if (type == null) return 0;
        try { return Integer.parseInt(n); } catch (NumberFormatException e) {}
        if ("android".equals(pkg)) return sFramework.identifier("android", type, n);
        int id = sApp.identifier(null, type, n);
        if (id == 0 && (pkg == null || "android".equals(pkg))) id = sFramework.identifier("android", type, n);
        return id;
    }
    public String getResourceName(int id) { String t = getResourceTypeName(id), e = getResourceEntryName(id); return getResourcePackageName(id) + ":" + t + "/" + e; }
    public String getResourcePackageName(int id) { ResTable.Package p = table(id).pkg(id); if (p == null) throw new NotFoundException("Unable to find resource ID #0x" + Integer.toHexString(id)); return (id >>> 24) == 0x7f ? husk.Native.packageName() : p.name; }
    public String getResourceTypeName(int id) { String s = table(id).typeName(id); if (s == null) throw new NotFoundException("Unable to find resource ID #0x" + Integer.toHexString(id)); return s; }
    public String getResourceEntryName(int id) { String s = table(id).keyName(id, mCfg); if (s == null) throw new NotFoundException("Unable to find resource ID #0x" + Integer.toHexString(id)); return s; }

    // ---- values
    private TypedValue value(int id) { TypedValue v = new TypedValue(); getValue(id, v, true); return v; }
    public CharSequence getText(int id) {
        TypedValue v = value(id);
        if (v.type == TypedValue.TYPE_STRING) return v.string;
        CharSequence s = v.coerceToString();
        if (s != null) return s;
        throw new NotFoundException("String resource ID #0x" + Integer.toHexString(id));
    }
    public CharSequence getText(int id, CharSequence def) { try { return id != 0 ? getText(id) : def; } catch (NotFoundException e) { return def; } }
    public String getString(int id) { return getText(id).toString(); }
    public String getString(int id, Object... args) { return String.format(mConfig.locale, getString(id), args); }
    public CharSequence getQuantityText(int id, int quantity) {
        Bag b = huskBag(id);
        if (b == null) return getText(id);
        int key = quantity == 1 ? 0x01000006 : quantity == 0 ? 0x01000005 : 0x01000004;
        int i = b.indexOf(key);
        if (i < 0) i = b.indexOf(0x01000004);
        if (i < 0 && b.n > 0) i = 0;
        if (i < 0) throw new NotFoundException("Plural resource ID #0x" + Integer.toHexString(id));
        return itemText(b, i);
    }
    private CharSequence itemText(Bag b, int i) {
        if (b.types[i] == TypedValue.TYPE_STRING) return b.strings[i];
        if (b.types[i] == TypedValue.TYPE_REFERENCE) return getText(b.data[i]);
        return TypedValue.coerceToString(b.types[i], b.data[i]);
    }
    public String getQuantityString(int id, int quantity) { return getQuantityText(id, quantity).toString(); }
    public String getQuantityString(int id, int quantity, Object... args) { return String.format(mConfig.locale, getQuantityString(id, quantity), args); }
    public CharSequence[] getTextArray(int id) {
        Bag b = huskBag(id);
        if (b == null) throw new NotFoundException("Text array resource ID #0x" + Integer.toHexString(id));
        CharSequence[] r = new CharSequence[b.n];
        for (int i = 0; i < b.n; i++) r[i] = itemText(b, i);
        return r;
    }
    public String[] getStringArray(int id) { CharSequence[] t = getTextArray(id); String[] r = new String[t.length]; for (int i = 0; i < t.length; i++) r[i] = t[i] == null ? null : t[i].toString(); return r; }
    public int[] getIntArray(int id) {
        Bag b = huskBag(id);
        if (b == null) throw new NotFoundException("Int array resource ID #0x" + Integer.toHexString(id));
        int[] r = new int[b.n];
        for (int i = 0; i < b.n; i++) { if (b.types[i] == TypedValue.TYPE_REFERENCE) { TypedValue v = value(b.data[i]); r[i] = v.data; } else r[i] = b.data[i]; }
        return r;
    }
    public TypedArray obtainTypedArray(int id) {
        Bag b = huskBag(id);
        if (b == null) throw new NotFoundException("Array resource ID #0x" + Integer.toHexString(id));
        TypedArray a = new TypedArray(this, null, b.n);
        for (int i = 0; i < b.n; i++) {
            TypedValue v = new TypedValue();
            v.type = b.types[i]; v.data = b.data[i]; v.string = b.strings[i]; v.density = b.dens[i];
            if (v.type == TypedValue.TYPE_REFERENCE && v.data != 0) { int ref = v.data; if (huskValue(ref, v, true)) { if (v.type == TypedValue.TYPE_NULL) { v.type = TypedValue.TYPE_REFERENCE; v.data = ref; } } }
            a.set(i, v);
        }
        return a;
    }
    public int getInteger(int id) { TypedValue v = value(id); if (v.type >= TypedValue.TYPE_FIRST_INT && v.type <= TypedValue.TYPE_LAST_INT) return v.data; throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " type #0x" + Integer.toHexString(v.type) + " is not valid"); }
    public boolean getBoolean(int id) { TypedValue v = value(id); if (v.type >= TypedValue.TYPE_FIRST_INT && v.type <= TypedValue.TYPE_LAST_INT) return v.data != 0; throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a boolean"); }
    public float getFloat(int id) { TypedValue v = value(id); if (v.type == TypedValue.TYPE_FLOAT) return v.getFloat(); throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a float"); }
    public float getDimension(int id) { TypedValue v = value(id); if (v.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimension(v.data, mMetrics); throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " type #0x" + Integer.toHexString(v.type) + " is not valid"); }
    public int getDimensionPixelOffset(int id) { TypedValue v = value(id); if (v.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimensionPixelOffset(v.data, mMetrics); throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a dimension"); }
    public int getDimensionPixelSize(int id) { TypedValue v = value(id); if (v.type == TypedValue.TYPE_DIMENSION) return TypedValue.complexToDimensionPixelSize(v.data, mMetrics); throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a dimension"); }
    public float getFraction(int id, int base, int pbase) { TypedValue v = value(id); if (v.type == TypedValue.TYPE_FRACTION) return TypedValue.complexToFraction(v.data, base, pbase); throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a fraction"); }
    @Deprecated public int getColor(int id) { return getColor(id, null); }
    public int getColor(int id, Theme theme) {
        TypedValue v = new TypedValue();
        getValue(id, v, true);
        if (v.type >= TypedValue.TYPE_FIRST_INT && v.type <= TypedValue.TYPE_LAST_INT) return v.data;
        if (v.type == TypedValue.TYPE_ATTRIBUTE && theme != null) { TypedValue r = new TypedValue(); if (theme.resolveAttribute(v.data, r, true)) return r.data; }
        if (v.type == TypedValue.TYPE_STRING) return loadColorStateList(v, id, theme).getDefaultColor();
        throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " type #0x" + Integer.toHexString(v.type) + " is not valid");
    }
    @Deprecated public ColorStateList getColorStateList(int id) { return getColorStateList(id, null); }
    public ColorStateList getColorStateList(int id, Theme theme) {
        TypedValue v = new TypedValue();
        getValue(id, v, true);
        if (v.type >= TypedValue.TYPE_FIRST_COLOR_INT && v.type <= TypedValue.TYPE_LAST_COLOR_INT) return ColorStateList.valueOf(v.data);
        if (v.type == TypedValue.TYPE_ATTRIBUTE && theme != null) { TypedValue r = new TypedValue(); if (theme.resolveAttribute(v.data, r, true)) return r.type == TypedValue.TYPE_STRING ? loadColorStateList(r, r.resourceId, theme) : ColorStateList.valueOf(r.data); }
        return loadColorStateList(v, id, theme);
    }
    /** A colour value: an int, or a colour XML file (a selector). */
    public ColorStateList loadColorStateList(TypedValue v, int id, Theme theme) {
        if (v.type >= TypedValue.TYPE_FIRST_INT && v.type <= TypedValue.TYPE_LAST_INT) return ColorStateList.valueOf(v.data);
        if (v.type != TypedValue.TYPE_STRING || v.string == null) throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a color");
        ColorStateList c = theme == null ? mColorCache.get(id) : null;
        if (c != null) return c;
        String file = v.string.toString();
        try {
            XmlResourceParser p = huskXml(file, (v.resourceId != 0 ? v.resourceId : id));
            c = husk.ResInflate.colorStateList(this, p, theme);
        } catch (Exception e) {
            throw new NotFoundException("File " + file + " from color state list resource ID #0x" + Integer.toHexString(id), e);
        }
        if (theme == null && id != 0) mColorCache.put(id, c);
        return c;
    }

    // ---- drawables
    @Deprecated public Drawable getDrawable(int id) { return getDrawable(id, null); }
    public Drawable getDrawable(int id, Theme theme) { return getDrawableForDensity(id, 0, theme); }
    @Deprecated public Drawable getDrawableForDensity(int id, int density) { return getDrawableForDensity(id, density, null); }
    public Drawable getDrawableForDensity(int id, int density, Theme theme) {
        TypedValue v = new TypedValue();
        getValue(id, v, true);
        return loadDrawable(v, id, theme);
    }
    /** A drawable value: a colour, an image file, or a drawable XML file. */
    public Drawable loadDrawable(TypedValue v, int id, Theme theme) {
        if (v.type >= TypedValue.TYPE_FIRST_COLOR_INT && v.type <= TypedValue.TYPE_LAST_COLOR_INT) return new android.graphics.drawable.ColorDrawable(v.data);
        if (v.type == TypedValue.TYPE_ATTRIBUTE && theme != null) { TypedValue r = new TypedValue(); if (theme.resolveAttribute(v.data, r, true)) return loadDrawable(r, r.resourceId, theme); }
        if (v.type != TypedValue.TYPE_STRING || v.string == null) {
            if (v.type == TypedValue.TYPE_NULL) return null;
            throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " type #0x" + Integer.toHexString(v.type) + " is not valid");
        }
        int rid = v.resourceId != 0 ? v.resourceId : id;
        Drawable.ConstantState cs = mDrawableCache.get(rid);
        if (cs != null) return cs.newDrawable(this, theme);
        String file = v.string.toString();
        Drawable d;
        try {
            if (file.endsWith(".xml")) d = husk.ResInflate.drawable(this, huskXml(file, rid), theme);
            else d = husk.ResInflate.image(this, huskFile(file, rid), file, v.density);
        } catch (NotFoundException e) { throw e; }
        catch (Exception e) {
            NotFoundException n = new NotFoundException("File " + file + " from drawable resource ID #0x" + Integer.toHexString(id));
            n.initCause(e);
            throw n;
        }
        if (d != null && !file.endsWith(".xml")) { Drawable.ConstantState s = d.getConstantState(); if (s != null) mDrawableCache.put(rid, s); }
        return d;
    }
    public Drawable huskInflateDrawable(org.xmlpull.v1.XmlPullParser p, Theme t) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException {
        return husk.ResInflate.drawable(this, p, t);
    }
    public android.graphics.Movie getMovie(int id) { return null; }

    // ---- files
    /** The bytes of a file in the APK that holds resource id (res/...). */
    public byte[] huskFile(String path, int id) {
        byte[] d = husk.Native.readApkFile((id >>> 24) == 1 ? 1 : 0, path);
        if (d == null && (id >>> 24) != 1) {
            // a split APK may hold it
            for (int k = 2; k < 6 && d == null; k++) d = husk.Native.readApkFile(k, path);
        }
        if (d == null) throw new NotFoundException("File " + path + " not found (resource ID #0x" + Integer.toHexString(id) + ")");
        return d;
    }
    public XmlResourceParser huskXml(String path, int id) {
        try {
            XmlBlock.Parser p = new XmlBlock(huskFile(path, id), path).newParser();
            p.mRes = this;
            return p;
        } catch (org.xmlpull.v1.XmlPullParserException e) { throw new NotFoundException(e.getMessage()); }
    }
    public XmlResourceParser getXml(int id) { return getLayoutOrXml(id, "xml"); }
    public XmlResourceParser getLayout(int id) { return getLayoutOrXml(id, "layout"); }
    public XmlResourceParser getAnimation(int id) { return getLayoutOrXml(id, "anim"); }
    private XmlResourceParser getLayoutOrXml(int id, String type) {
        TypedValue v = new TypedValue();
        getValue(id, v, true);
        if (v.type != TypedValue.TYPE_STRING) throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " type #0x" + Integer.toHexString(v.type) + " is not valid");
        return huskXml(v.string.toString(), v.resourceId != 0 ? v.resourceId : id);
    }
    public InputStream openRawResource(int id) { TypedValue v = new TypedValue(); return openRawResource(id, v); }
    public InputStream openRawResource(int id, TypedValue v) {
        getValue(id, v, true);
        if (v.type != TypedValue.TYPE_STRING) throw new NotFoundException("Resource ID #0x" + Integer.toHexString(id) + " is not a file");
        return new ByteArrayInputStream(huskFile(v.string.toString(), v.resourceId != 0 ? v.resourceId : id));
    }
    public AssetFileDescriptor openRawResourceFd(int id) {
        TypedValue v = new TypedValue();
        getValue(id, v, true);
        String path = v.string.toString();
        long fdoff = husk.Native.apkFileFd((id >>> 24) == 1 ? 1 : 0, path);
        if (fdoff == -1) throw new NotFoundException("File " + path + " is compressed and cannot be opened as a file descriptor");
        java.io.FileDescriptor fd = new java.io.FileDescriptor();
        try { java.lang.reflect.Field f = java.io.FileDescriptor.class.getDeclaredField("descriptor"); f.setAccessible(true); f.setInt(fd, (int) (fdoff >>> 40)); } catch (Exception e) {}
        return new AssetFileDescriptor(path, fd, fdoff & 0xFFFFFFFFFFL, husk.Native.apkFileLength((id >>> 24) == 1 ? 1 : 0, path));
    }
    /** The density a drawable resource's image was made for. */
    public int huskDrawableDensity(int id) { TypedValue v = new TypedValue(); return huskValue(id, v, true) ? (v.density == 0 ? 160 : v.density == 0xffff ? mMetrics.densityDpi : v.density) : 160; }
    public Typeface getFont(int id) {
        Typeface t = mFontCache.get(id);
        if (t != null) return t;
        TypedValue v = new TypedValue();
        getValue(id, v, true);
        String file = v.string == null ? "" : v.string.toString();
        if (file.endsWith(".xml")) t = husk.ResInflate.fontFamily(this, huskXml(file, id));
        else t = Typeface.huskFromData(huskFile(file, id));
        mFontCache.put(id, t);
        return t;
    }

    // ---- animations (husk.ResInflate)
    public android.view.animation.Animation huskLoadAnimation(int id) { return husk.ResInflate.animation(this, getAnimation(id)); }
    public android.view.animation.Interpolator huskLoadInterpolator(int id) { return husk.ResInflate.interpolator(this, getAnimation(id)); }
    public android.animation.Animator huskLoadAnimator(int id) { return husk.ResInflate.animator(this, getAnimation(id)); }

    // ---- themes and attributes
    public final Theme newTheme() { return new Theme(); }
    public TypedArray obtainAttributes(android.util.AttributeSet set, int[] attrs) { return new Theme().obtainStyledAttributes(set, attrs, 0, 0, false); }
    public int getConfigurationChangesHusk() { return 0; }

    /** A theme: attribute values from the styles applied to it, each later style overriding (or not, without force). */
    public final class Theme {
        final HashMap<Integer, TypedValue> mAttrs = new HashMap<>();
        private int mKey;
        private final java.util.ArrayList<int[]> mApplied = new java.util.ArrayList<>();
        Theme() {}
        public Resources getResources() { return Resources.this; }
        public void applyStyle(int resId, boolean force) {
            if (resId == 0) return;
            Bag b = huskBag(resId);
            if (b == null) return;
            mApplied.add(new int[] { resId, force ? 1 : 0 });
            for (int i = 0; i < b.n; i++) {
                if (!force && mAttrs.containsKey(b.keys[i])) continue;
                TypedValue v = new TypedValue();
                v.type = b.types[i]; v.data = b.data[i]; v.string = b.strings[i]; v.density = b.dens[i];
                mAttrs.put(b.keys[i], v);
            }
            mKey = mKey * 31 + resId;
        }
        public void setTo(Theme o) { mAttrs.clear(); mAttrs.putAll(o.mAttrs); mApplied.clear(); mApplied.addAll(o.mApplied); mKey = o.mKey; }
        public void rebase() {}
        @Override public String toString() {
            String probe = System.getenv("TL_RES_PROBE");
            if (probe != null) for (String x : probe.split(",")) { int id = (int) Long.parseLong(x.replace("0x", ""), 16); ResTable t = table(id); ResTable.Entry e = t.find(id, mCfg); android.util.Log.d("ResProbe", x + ": " + t.huskDescribe(id) + " table " + (t == sFramework ? "framework" : "app") + " entry " + (e == null ? "null" : "complex=" + e.complex() + " parent=0x" + Integer.toHexString(e.complex() ? t.bagParent(e) : 0) + " count=" + (e.complex() ? t.bagCount(e) : 0)) + " bag " + (huskBag(id) == null ? "null" : huskBag(id).n)); } StringBuilder b = new StringBuilder("Theme{"); for (int[] a : mApplied) { Bag g = huskBag(a[0]); b.append("0x").append(Integer.toHexString(a[0])).append('(').append(g == null ? "missing" : g.n + " attrs").append(") "); } return b.append(mAttrs.size()).append(" attrs}").toString(); }
        public int getChangingConfigurations() { return 0; }
        public int[] getAttributeResolutionStack(int defStyleAttr, int defStyleRes, int explicitStyleRes) { return new int[0]; }
        public int getExplicitStyle(android.util.AttributeSet set) { return set == null ? 0 : set.getStyleAttribute(); }
        public void dump(int priority, String tag, String prefix) {}
        public Drawable getDrawable(int id) { return Resources.this.getDrawable(id, this); }
        public int huskKey() { return mKey; }
        /** The attribute's value in this theme, attributes followed, and references when resolveRefs. */
        public boolean resolveAttribute(int attr, TypedValue out, boolean resolveRefs) {
            TypedValue v = mAttrs.get(attr);
            for (int depth = 0; v != null && v.type == TypedValue.TYPE_ATTRIBUTE && depth < 20; depth++) v = mAttrs.get(v.data);
            if (v == null) return false;
            out.setTo(v);
            out.resourceId = 0;
            if (resolveRefs && out.type == TypedValue.TYPE_REFERENCE && out.data != 0) {
                int ref = out.data;
                if (huskValue(ref, out, true)) {
                    if (out.type == TypedValue.TYPE_NULL) { out.type = TypedValue.TYPE_REFERENCE; out.data = ref; out.resourceId = ref; }
                } else { out.type = TypedValue.TYPE_REFERENCE; out.data = ref; out.resourceId = ref; }
            } else if (out.type == TypedValue.TYPE_REFERENCE) out.resourceId = out.data;
            return true;
        }
        public TypedArray obtainStyledAttributes(int[] attrs) { return obtainStyledAttributes(null, attrs, 0, 0); }
        public TypedArray obtainStyledAttributes(int resId, int[] attrs) throws NotFoundException {
            TypedArray a = obtainStyledAttributes(null, attrs, 0, resId);
            return a;
        }
        public TypedArray obtainStyledAttributes(android.util.AttributeSet set, int[] attrs, int defStyleAttr, int defStyleRes) { return obtainStyledAttributes(set, attrs, defStyleAttr, defStyleRes, true); }
        /**
         * As AttributeResolution::ApplyStyle: for each attribute, the XML's value, else the style named in the XML, else the default
         * style the theme gives for defStyleAttr, else defStyleRes, else the theme's own value; then attribute references are resolved in
         * this theme and plain references to simple values followed.
         */
        TypedArray obtainStyledAttributes(android.util.AttributeSet set, int[] attrs, int defStyleAttr, int defStyleRes, boolean useTheme) {
            TypedArray a = new TypedArray(Resources.this, useTheme ? this : null, attrs.length);
            XmlBlock.Parser xp = set instanceof XmlBlock.Parser ? (XmlBlock.Parser) set : null;
            Bag xmlStyle = null, defStyle = null;
            if (set != null) {
                int st = set.getStyleAttribute();
                if (st != 0 && xp != null && xp.huskStyleIsAttr()) { TypedValue r = new TypedValue(); st = useTheme && resolveAttribute(st, r, false) && r.type == TypedValue.TYPE_REFERENCE ? r.data : 0; }
                if (st != 0) xmlStyle = huskBag(st);
            }
            if (useTheme && defStyleAttr != 0) {
                TypedValue v = new TypedValue();
                if (resolveAttribute(defStyleAttr, v, false) && (v.type == TypedValue.TYPE_REFERENCE)) defStyle = huskBag(v.data);
            }
            if (defStyle == null && defStyleRes != 0) defStyle = huskBag(defStyleRes);
            // the XML's attributes by resource id
            int[] xmlIdx = null;
            if (set != null) {
                int n = set.getAttributeCount();
                xmlIdx = new int[attrs.length];
                java.util.Arrays.fill(xmlIdx, -1);
                for (int i = 0; i < n; i++) {
                    int rid = set.getAttributeNameResource(i);
                    if (rid == 0) {
                        // a text parser (no compiled ids): match android attributes by name
                        if (xp == null) { String nm = set.getAttributeName(i); int id = nm == null ? 0 : sFramework.identifier("android", "attr", nm); if (id != 0) rid = id; }
                        if (rid == 0) continue;
                    }
                    for (int k = 0; k < attrs.length; k++) if (attrs[k] == rid) { xmlIdx[k] = i; break; }
                }
            }
            TypedValue v = new TypedValue();
            for (int k = 0; k < attrs.length; k++) {
                int attr = attrs[k];
                boolean found = false;
                v.type = TypedValue.TYPE_NULL; v.data = 0; v.string = null; v.resourceId = 0; v.density = 0;
                if (xmlIdx != null && xmlIdx[k] >= 0) {
                    int i = xmlIdx[k];
                    if (xp != null) xp.getAttributeTypedValue(i, v);
                    else { v.type = TypedValue.TYPE_STRING; v.string = set.getAttributeValue(i); }
                    found = true;
                }
                if (!found && xmlStyle != null) { int i = xmlStyle.indexOf(attr); if (i >= 0) { fromBag(xmlStyle, i, v); found = true; } }
                if (!found && defStyle != null) { int i = defStyle.indexOf(attr); if (i >= 0) { fromBag(defStyle, i, v); found = true; } }
                if (!found && useTheme) { TypedValue tv = mAttrs.get(attr); if (tv != null) { v.setTo(tv); found = true; } }
                if (!found) continue;
                // ?attr: from this theme
                for (int depth = 0; v.type == TypedValue.TYPE_ATTRIBUTE && depth < 20; depth++) {
                    TypedValue tv = useTheme ? mAttrs.get(v.data) : null;
                    if (tv == null) { v.type = TypedValue.TYPE_NULL; break; }
                    v.setTo(tv);
                }
                // @null: a reference to nothing, set explicitly (it hides whatever a style or the theme says)
                if (v.type == TypedValue.TYPE_REFERENCE && v.data == 0) { v.type = TypedValue.TYPE_NULL; v.data = TypedValue.DATA_NULL_UNDEFINED; }
                if (v.type == TypedValue.TYPE_NULL && v.data != TypedValue.DATA_NULL_EMPTY) continue;
                // @ref: to a simple value; references to files or bags stay references (with their file path when it is a file)
                if (v.type == TypedValue.TYPE_REFERENCE && v.data != 0) {
                    int ref = v.data;
                    TypedValue r = new TypedValue();
                    if (huskValue(ref, r, true)) {
                        if (r.type == TypedValue.TYPE_NULL) { v.resourceId = ref; }
                        else if (r.type == TypedValue.TYPE_ATTRIBUTE && useTheme) { TypedValue t2 = new TypedValue(); if (resolveAttribute(r.data, t2, true)) { v.setTo(t2); v.resourceId = t2.resourceId != 0 ? t2.resourceId : ref; } }
                        else { v.setTo(r); v.resourceId = r.resourceId != 0 ? r.resourceId : ref; }
                    } else v.resourceId = ref;
                }
                a.set(k, v);
            }
            if (xp != null) a.mXml = xp;
            return a;
        }
        private void fromBag(Bag b, int i, TypedValue v) { v.type = b.types[i]; v.data = b.data[i]; v.string = b.strings[i]; v.density = b.dens[i]; v.resourceId = 0; }
    }
}
