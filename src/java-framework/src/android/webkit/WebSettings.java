package android.webkit;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public abstract class WebSettings {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final long ENABLE_SIMPLIFIED_DARK_MODE = 214741472L;
    public static final long ENABLE_USER_AGENT_REDUCTION = 371034303L;
    public static final int FORCE_DARK_AUTO = 1;
    public static final int FORCE_DARK_OFF = 0;
    public static final int FORCE_DARK_ON = 2;
    public static final int LOAD_CACHE_ELSE_NETWORK = 1;
    public static final int LOAD_CACHE_ONLY = 3;
    public static final int LOAD_DEFAULT = -1;
    public static final int LOAD_NORMAL = 0;
    public static final int LOAD_NO_CACHE = 2;
    public static final int MENU_ITEM_NONE = 0;
    public static final int MENU_ITEM_PROCESS_TEXT = 4;
    public static final int MENU_ITEM_SHARE = 1;
    public static final int MENU_ITEM_WEB_SEARCH = 2;
    public static final int MIXED_CONTENT_ALWAYS_ALLOW = 0;
    public static final int MIXED_CONTENT_COMPATIBILITY_MODE = 2;
    public static final int MIXED_CONTENT_NEVER_ALLOW = 1;
    public WebSettings() {}
    public static java.lang.String getDefaultUserAgent(android.content.Context p0) { return HuskWebSettings.DEFAULT_UA; }
    public abstract boolean enableSmoothTransition();
    public abstract boolean getAcceptThirdPartyCookies();
    public abstract boolean getAllowContentAccess();
    public abstract boolean getAllowFileAccess();
    public abstract boolean getAllowFileAccessFromFileURLs();
    public abstract boolean getAllowUniversalAccessFromFileURLs();
    public abstract boolean getBlockNetworkImage();
    public abstract boolean getBlockNetworkLoads();
    public abstract boolean getBuiltInZoomControls();
    public abstract int getCacheMode();
    public abstract java.lang.String getCursiveFontFamily();
    public abstract boolean getDatabaseEnabled();
    public abstract java.lang.String getDatabasePath();
    public abstract int getDefaultFixedFontSize();
    public abstract int getDefaultFontSize();
    public abstract java.lang.String getDefaultTextEncodingName();
    public abstract android.webkit.WebSettings.ZoomDensity getDefaultZoom();
    public abstract int getDisabledActionModeMenuItems();
    public abstract boolean getDisplayZoomControls();
    public abstract boolean getDomStorageEnabled();
    public abstract java.lang.String getFantasyFontFamily();
    public abstract java.lang.String getFixedFontFamily();
    public int getForceDark() { return (huskProps.get("ForceDark") instanceof Integer ? (Integer) huskProps.get("ForceDark") : 0); }
    public abstract boolean getJavaScriptCanOpenWindowsAutomatically();
    public abstract boolean getJavaScriptEnabled();
    public abstract android.webkit.WebSettings.LayoutAlgorithm getLayoutAlgorithm();
    public abstract boolean getLightTouchEnabled();
    public abstract boolean getLoadWithOverviewMode();
    public abstract boolean getLoadsImagesAutomatically();
    public abstract boolean getMediaPlaybackRequiresUserGesture();
    public abstract int getMinimumFontSize();
    public abstract int getMinimumLogicalFontSize();
    public abstract int getMixedContentMode();
    public abstract boolean getNavDump();
    public abstract boolean getOffscreenPreRaster();
    public abstract android.webkit.WebSettings.PluginState getPluginState();
    public abstract boolean getPluginsEnabled();
    public java.lang.String getPluginsPath() { return (java.lang.String) huskProps.get("PluginsPath"); }
    public abstract boolean getSafeBrowsingEnabled();
    public abstract java.lang.String getSansSerifFontFamily();
    public abstract boolean getSaveFormData();
    public abstract boolean getSavePassword();
    public abstract java.lang.String getSerifFontFamily();
    public abstract java.lang.String getStandardFontFamily();
    public android.webkit.WebSettings.TextSize getTextSize() {
        int z = getTextZoom();
        for (TextSize t : TextSize.values()) if (t.value == z) return t;
        return TextSize.NORMAL;
    }
    public abstract int getTextZoom();
    public boolean getUseDoubleTree() { return (huskProps.get("UseDoubleTree") instanceof Boolean ? (Boolean) huskProps.get("UseDoubleTree") : false); }
    public abstract boolean getUseWebViewBackgroundForOverscrollBackground();
    public abstract boolean getUseWideViewPort();
    public abstract int getUserAgent();
    public abstract java.lang.String getUserAgentString();
    public abstract boolean getVideoOverlayForEmbeddedEncryptedVideoEnabled();
    public boolean isAlgorithmicDarkeningAllowed() { return (huskProps.get("AlgorithmicDarkeningAllowed") instanceof Boolean ? (Boolean) huskProps.get("AlgorithmicDarkeningAllowed") : false); }
    public abstract void setAcceptThirdPartyCookies(boolean p0);
    public void setAlgorithmicDarkeningAllowed(boolean p0) { huskProps.put("AlgorithmicDarkeningAllowed", Boolean.valueOf(p0)); }
    public abstract void setAllowContentAccess(boolean p0);
    public abstract void setAllowFileAccess(boolean p0);
    public abstract void setAllowFileAccessFromFileURLs(boolean p0);
    public abstract void setAllowUniversalAccessFromFileURLs(boolean p0);
    public void setAppCacheEnabled(boolean p0) { huskProps.put("AppCacheEnabled", Boolean.valueOf(p0)); }
    public void setAppCacheMaxSize(long p0) { huskProps.put("AppCacheMaxSize", Long.valueOf(p0)); }
    public void setAppCachePath(java.lang.String p0) { huskProps.put("AppCachePath", p0); }
    public abstract void setBlockNetworkImage(boolean p0);
    public abstract void setBlockNetworkLoads(boolean p0);
    public abstract void setBuiltInZoomControls(boolean p0);
    public abstract void setCacheMode(int p0);
    public abstract void setCursiveFontFamily(java.lang.String p0);
    public abstract void setDatabaseEnabled(boolean p0);
    public abstract void setDatabasePath(java.lang.String p0);
    public abstract void setDefaultFixedFontSize(int p0);
    public abstract void setDefaultFontSize(int p0);
    public abstract void setDefaultTextEncodingName(java.lang.String p0);
    public abstract void setDefaultZoom(android.webkit.WebSettings.ZoomDensity p0);
    public abstract void setDisabledActionModeMenuItems(int p0);
    public abstract void setDisplayZoomControls(boolean p0);
    public abstract void setDomStorageEnabled(boolean p0);
    public abstract void setEnableSmoothTransition(boolean p0);
    public abstract void setFantasyFontFamily(java.lang.String p0);
    public abstract void setFixedFontFamily(java.lang.String p0);
    public void setForceDark(int p0) { huskProps.put("ForceDark", Integer.valueOf(p0)); }
    public abstract void setGeolocationDatabasePath(java.lang.String p0);
    public abstract void setGeolocationEnabled(boolean p0);
    public abstract void setJavaScriptCanOpenWindowsAutomatically(boolean p0);
    public abstract void setJavaScriptEnabled(boolean p0);
    public abstract void setLayoutAlgorithm(android.webkit.WebSettings.LayoutAlgorithm p0);
    public abstract void setLightTouchEnabled(boolean p0);
    public abstract void setLoadWithOverviewMode(boolean p0);
    public abstract void setLoadsImagesAutomatically(boolean p0);
    public abstract void setMediaPlaybackRequiresUserGesture(boolean p0);
    public abstract void setMinimumFontSize(int p0);
    public abstract void setMinimumLogicalFontSize(int p0);
    public abstract void setMixedContentMode(int p0);
    public abstract void setNavDump(boolean p0);
    public abstract void setNeedInitialFocus(boolean p0);
    public abstract void setOffscreenPreRaster(boolean p0);
    public abstract void setPluginState(android.webkit.WebSettings.PluginState p0);
    public abstract void setPluginsEnabled(boolean p0);
    public void setPluginsPath(java.lang.String p0) { huskProps.put("PluginsPath", p0); }
    public abstract void setRenderPriority(android.webkit.WebSettings.RenderPriority p0);
    public abstract void setSafeBrowsingEnabled(boolean p0);
    public abstract void setSansSerifFontFamily(java.lang.String p0);
    public abstract void setSaveFormData(boolean p0);
    public abstract void setSavePassword(boolean p0);
    public abstract void setSerifFontFamily(java.lang.String p0);
    public abstract void setStandardFontFamily(java.lang.String p0);
    public abstract void setSupportMultipleWindows(boolean p0);
    public abstract void setSupportZoom(boolean p0);
    public void setTextSize(android.webkit.WebSettings.TextSize p0) { setTextZoom(p0.value); }
    public abstract void setTextZoom(int p0);
    public void setUseDoubleTree(boolean p0) { huskProps.put("UseDoubleTree", Boolean.valueOf(p0)); }
    public abstract void setUseWebViewBackgroundForOverscrollBackground(boolean p0);
    public abstract void setUseWideViewPort(boolean p0);
    public abstract void setUserAgent(int p0);
    public abstract void setUserAgentString(java.lang.String p0);
    public abstract void setVideoOverlayForEmbeddedEncryptedVideoEnabled(boolean p0);
    public abstract boolean supportMultipleWindows();
    public abstract boolean supportZoom();
    public enum LayoutAlgorithm {
        NARROW_COLUMNS, NORMAL, SINGLE_COLUMN, TEXT_AUTOSIZING;
    }
    public enum PluginState {
        OFF, ON, ON_DEMAND;
    }
    public enum RenderPriority {
        HIGH, LOW, NORMAL;
    }
    public enum TextSize {
        SMALLEST(50), SMALLER(75), NORMAL(100), LARGER(150), LARGEST(200);
        TextSize(int size) { value = size; }
        int value;
    }
    public enum ZoomDensity {
        FAR, MEDIUM, CLOSE;
        public int getValue() { return this == FAR ? 150 : this == CLOSE ? 75 : 100; }
    }
}
