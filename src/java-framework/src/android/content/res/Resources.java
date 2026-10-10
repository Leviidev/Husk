package android.content.res;

import android.util.DisplayMetrics;

/** Resources: only what does not need the APK's compiled resource table (metrics, configuration, assets). */
public class Resources {
    public static class NotFoundException extends RuntimeException { public NotFoundException() {} public NotFoundException(String s) { super(s); } }
    public final class Theme { public void applyStyle(int id, boolean force) {} }
    private final AssetManager assets;
    private final DisplayMetrics metrics = new DisplayMetrics();
    private final Configuration config = new Configuration();
    public Resources(AssetManager a) { assets = a; metrics.setToDefaults(); }
    public static Resources getSystem() { return new Resources(new AssetManager()); }
    public final AssetManager getAssets() { return assets; }
    public DisplayMetrics getDisplayMetrics() { metrics.setToDefaults(); return metrics; }
    public Configuration getConfiguration() { return config; }
    public void updateConfiguration(Configuration c, DisplayMetrics m) {}
    public int getIdentifier(String name, String type, String pkg) { return 0; }
    public String getString(int id) { return ""; }
    public String getString(int id, Object... args) { return ""; }
    public CharSequence getText(int id) { return ""; }
    public String[] getStringArray(int id) { return new String[0]; }
    public int getInteger(int id) { return 0; }
    public boolean getBoolean(int id) { return false; }
    public float getDimension(int id) { return 0; }
    public int getDimensionPixelSize(int id) { return 0; }
    public int getColor(int id) { return 0; }
    public String getResourceName(int id) { throw new NotFoundException(); }
    public java.io.InputStream openRawResource(int id) { throw new NotFoundException("resource " + id); }
    public final Theme newTheme() { return new Theme(); }
}
