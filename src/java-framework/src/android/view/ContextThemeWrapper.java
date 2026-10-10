package android.view;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;

public class ContextThemeWrapper extends android.content.ContextWrapper {
    private int mThemeResource;
    private Resources.Theme mTheme;
    private LayoutInflater mInflater;
    private Configuration mOverrideConfiguration;
    private Resources mResources;
    public ContextThemeWrapper() { super(null); }
    public ContextThemeWrapper(Context base, int themeResId) { super(base); mThemeResource = themeResId; }
    public ContextThemeWrapper(Context base, Resources.Theme theme) { super(base); mTheme = theme; }
    @Override protected void attachBaseContext(Context base) { super.attachBaseContext(base); }
    public void applyOverrideConfiguration(Configuration c) {
        if (mResources != null) throw new IllegalStateException("getResources() or getAssets() has already been called");
        if (mOverrideConfiguration != null) throw new IllegalStateException("Override configuration has already been set");
        mOverrideConfiguration = new Configuration(c);
    }
    public Configuration getOverrideConfiguration() { return mOverrideConfiguration; }
    @Override public android.content.res.AssetManager getAssets() { return getResources().getAssets(); }
    @Override public Resources getResources() {
        if (mResources == null) {
            if (mOverrideConfiguration == null) mResources = super.getResources();
            else mResources = createConfigurationContext(mOverrideConfiguration).getResources();
        }
        return mResources;
    }
    @Override public void setTheme(int resid) { if (mThemeResource != resid) { mThemeResource = resid; initializeTheme(); } }
    public int getThemeResId() { return mThemeResource; }
    @Override public Resources.Theme getTheme() {
        if (mTheme != null) return mTheme;
        if (mThemeResource == 0) mThemeResource = husk.ContextImpl.defaultTheme();
        initializeTheme();
        return mTheme;
    }
    @Override public Object getSystemService(String name) {
        if (LAYOUT_INFLATER_SERVICE.equals(name)) {
            if (mInflater == null) mInflater = LayoutInflater.from(getBaseContext()).cloneInContext(this);
            return mInflater;
        }
        return getBaseContext().getSystemService(name);
    }
    protected void onApplyThemeResource(Resources.Theme theme, int resid, boolean first) { theme.applyStyle(resid, true); }
    private void initializeTheme() {
        boolean first = mTheme == null;
        if (first) {
            mTheme = getResources().newTheme();
            Resources.Theme base = getBaseContext() != null ? getBaseContext().getTheme() : null;
            if (base != null) mTheme.setTo(base);
        }
        onApplyThemeResource(mTheme, mThemeResource, first);
    }
}
