package android.graphics;

import java.util.HashMap;

public class Typeface {
    public static final int NORMAL = 0, BOLD = 1, ITALIC = 2, BOLD_ITALIC = 3;
    public static final Typeface DEFAULT, DEFAULT_BOLD, SANS_SERIF, SERIF, MONOSPACE;
    private static final HashMap<String, Typeface> sCache = new HashMap<>();
    static {
        DEFAULT = new Typeface(husk.Gfx.tfDefault(), NORMAL);
        DEFAULT_BOLD = new Typeface(husk.Gfx.tfCreate("sans-serif", BOLD), BOLD);
        SANS_SERIF = DEFAULT;
        SERIF = new Typeface(husk.Gfx.tfCreate("serif", NORMAL), NORMAL);
        MONOSPACE = new Typeface(husk.Gfx.tfCreate("monospace", NORMAL), NORMAL);
    }
    /** The native face (husk.Gfx). */
    public final long native_instance;
    private final int mStyle;
    private String mFamily;

    Typeface(long ni, int style) { native_instance = ni; mStyle = style; }
    public int getStyle() { return mStyle; }
    public final boolean isBold() { return (mStyle & BOLD) != 0 || getWeight() >= 600; }
    public final boolean isItalic() { return (mStyle & ITALIC) != 0; }
    public int getWeight() { return husk.Gfx.tfWeight(native_instance); }
    public String getSystemFontFamilyName() { return mFamily; }

    public static Typeface create(String family, int style) {
        String key = family + "/" + style;
        synchronized (sCache) {
            Typeface t = sCache.get(key);
            if (t == null) { t = new Typeface(husk.Gfx.tfCreate(family, style), style); t.mFamily = family; sCache.put(key, t); }
            return t;
        }
    }
    public static Typeface create(Typeface family, int style) {
        if (family == null) family = DEFAULT;
        if (family.mStyle == style) return family;
        return new Typeface(husk.Gfx.tfDerive(family.native_instance, style, 0), style);
    }
    public static Typeface create(Typeface family, int weight, boolean italic) {
        if (family == null) family = DEFAULT;
        int style = (weight >= 600 ? BOLD : 0) | (italic ? ITALIC : 0);
        return new Typeface(husk.Gfx.tfDerive(family.native_instance, style, weight), style);
    }
    public static Typeface defaultFromStyle(int style) { return style == NORMAL ? DEFAULT : create(DEFAULT, style); }
    public static Typeface createFromAsset(android.content.res.AssetManager mgr, String path) {
        byte[] d = husk.Native.readAsset(path);
        if (d == null) throw new RuntimeException("Font asset not found " + path);
        long h = husk.Gfx.tfFromData(d);
        if (h == 0) throw new RuntimeException("Font asset not readable " + path);
        return new Typeface(h, NORMAL);
    }
    public static Typeface createFromFile(java.io.File f) { return createFromFile(f.getPath()); }
    public static Typeface createFromFile(String path) {
        try {
            byte[] d = java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(path));
            long h = husk.Gfx.tfFromData(d);
            if (h != 0) return new Typeface(h, NORMAL);
        } catch (java.io.IOException e) {}
        throw new RuntimeException("Font not found " + path);
    }
    /** Husk: a face from font bytes (resources' fonts). */
    public static Typeface huskFromData(byte[] d) { long h = d == null ? 0 : husk.Gfx.tfFromData(d); return h == 0 ? DEFAULT : new Typeface(h, NORMAL); }
    @Override public boolean equals(Object o) { return o instanceof Typeface && ((Typeface) o).native_instance == native_instance; }
    @Override public int hashCode() { return (int) native_instance; }

    public static final class Builder {
        private byte[] data; private int weight = -1; private boolean italic; private String fallback;
        public Builder(java.io.File f) { try { data = java.nio.file.Files.readAllBytes(f.toPath()); } catch (java.io.IOException e) {} }
        public Builder(android.content.res.AssetManager m, String path) { data = husk.Native.readAsset(path); }
        public Builder setWeight(int w) { weight = w; return this; }
        public Builder setItalic(boolean i) { italic = i; return this; }
        public Builder setFallback(String f) { fallback = f; return this; }
        public Builder setFontVariationSettings(String s) { return this; }
        public Typeface build() {
            Typeface t = data != null ? huskFromData(data) : create(fallback, NORMAL);
            return weight > 0 || italic ? create(t, weight > 0 ? weight : 400, italic) : t;
        }
    }
}
