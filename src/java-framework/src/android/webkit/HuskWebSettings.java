package android.webkit;

/** What a WebView's getSettings() returns: every setting kept, the ones WebKit honours passed on to it. */
@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
final class HuskWebSettings extends WebSettings {
    static final String DEFAULT_UA = "Mozilla/5.0 (Linux; Android 14; Husk; wv) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Chrome/124.0.0.0 Mobile Safari/537.36";
    private final java.util.HashMap<String, Object> mValues = new java.util.HashMap<>();
    private final Runnable mChanged;
    HuskWebSettings(Runnable changed) { mChanged = changed; }
    private Object val(String k, Object d) { Object v = mValues.get(k); return v != null ? v : d; }
    private void put(String k, Object v) { mValues.put(k, v); if (mChanged != null) mChanged.run(); }
    @Override public boolean enableSmoothTransition() { return (Boolean) val("EnableSmoothTransition", false); }
    @Override public boolean getAcceptThirdPartyCookies() { return (Boolean) val("AcceptThirdPartyCookies", false); }
    @Override public boolean getAllowContentAccess() { return (Boolean) val("AllowContentAccess", true); }
    @Override public boolean getAllowFileAccess() { return (Boolean) val("AllowFileAccess", false); }
    @Override public boolean getAllowFileAccessFromFileURLs() { return (Boolean) val("AllowFileAccessFromFileURLs", false); }
    @Override public boolean getAllowUniversalAccessFromFileURLs() { return (Boolean) val("AllowUniversalAccessFromFileURLs", false); }
    @Override public boolean getBlockNetworkImage() { return (Boolean) val("BlockNetworkImage", false); }
    @Override public boolean getBlockNetworkLoads() { return (Boolean) val("BlockNetworkLoads", false); }
    @Override public boolean getBuiltInZoomControls() { return (Boolean) val("BuiltInZoomControls", false); }
    @Override public int getCacheMode() { return (Integer) val("CacheMode", LOAD_DEFAULT); }
    @Override public java.lang.String getCursiveFontFamily() { return (java.lang.String) val("CursiveFontFamily", "cursive"); }
    @Override public boolean getDatabaseEnabled() { return (Boolean) val("DatabaseEnabled", false); }
    @Override public java.lang.String getDatabasePath() { return (java.lang.String) val("DatabasePath", ""); }
    @Override public int getDefaultFixedFontSize() { return (Integer) val("DefaultFixedFontSize", 13); }
    @Override public int getDefaultFontSize() { return (Integer) val("DefaultFontSize", 16); }
    @Override public java.lang.String getDefaultTextEncodingName() { return (java.lang.String) val("DefaultTextEncodingName", "UTF-8"); }
    @Override public android.webkit.WebSettings.ZoomDensity getDefaultZoom() { return (android.webkit.WebSettings.ZoomDensity) val("DefaultZoom", ZoomDensity.MEDIUM); }
    @Override public int getDisabledActionModeMenuItems() { return (Integer) val("DisabledActionModeMenuItems", MENU_ITEM_NONE); }
    @Override public boolean getDisplayZoomControls() { return (Boolean) val("DisplayZoomControls", true); }
    @Override public boolean getDomStorageEnabled() { return (Boolean) val("DomStorageEnabled", false); }
    @Override public java.lang.String getFantasyFontFamily() { return (java.lang.String) val("FantasyFontFamily", "fantasy"); }
    @Override public java.lang.String getFixedFontFamily() { return (java.lang.String) val("FixedFontFamily", "monospace"); }
    @Override public boolean getJavaScriptCanOpenWindowsAutomatically() { return (Boolean) val("JavaScriptCanOpenWindowsAutomatically", false); }
    @Override public boolean getJavaScriptEnabled() { return (Boolean) val("JavaScriptEnabled", false); }
    @Override public android.webkit.WebSettings.LayoutAlgorithm getLayoutAlgorithm() { return (android.webkit.WebSettings.LayoutAlgorithm) val("LayoutAlgorithm", LayoutAlgorithm.NARROW_COLUMNS); }
    @Override public boolean getLightTouchEnabled() { return (Boolean) val("LightTouchEnabled", false); }
    @Override public boolean getLoadWithOverviewMode() { return (Boolean) val("LoadWithOverviewMode", false); }
    @Override public boolean getLoadsImagesAutomatically() { return (Boolean) val("LoadsImagesAutomatically", true); }
    @Override public boolean getMediaPlaybackRequiresUserGesture() { return (Boolean) val("MediaPlaybackRequiresUserGesture", true); }
    @Override public int getMinimumFontSize() { return (Integer) val("MinimumFontSize", 8); }
    @Override public int getMinimumLogicalFontSize() { return (Integer) val("MinimumLogicalFontSize", 8); }
    @Override public int getMixedContentMode() { return (Integer) val("MixedContentMode", MIXED_CONTENT_NEVER_ALLOW); }
    @Override public boolean getNavDump() { return (Boolean) val("NavDump", false); }
    @Override public boolean getOffscreenPreRaster() { return (Boolean) val("OffscreenPreRaster", false); }
    @Override public android.webkit.WebSettings.PluginState getPluginState() { return (android.webkit.WebSettings.PluginState) val("PluginState", PluginState.OFF); }
    @Override public boolean getPluginsEnabled() { return (Boolean) val("PluginsEnabled", false); }
    @Override public boolean getSafeBrowsingEnabled() { return (Boolean) val("SafeBrowsingEnabled", true); }
    @Override public java.lang.String getSansSerifFontFamily() { return (java.lang.String) val("SansSerifFontFamily", "sans-serif"); }
    @Override public boolean getSaveFormData() { return (Boolean) val("SaveFormData", true); }
    @Override public boolean getSavePassword() { return (Boolean) val("SavePassword", false); }
    @Override public java.lang.String getSerifFontFamily() { return (java.lang.String) val("SerifFontFamily", "serif"); }
    @Override public java.lang.String getStandardFontFamily() { return (java.lang.String) val("StandardFontFamily", "sans-serif"); }
    @Override public int getTextZoom() { return (Integer) val("TextZoom", 100); }
    @Override public boolean getUseWebViewBackgroundForOverscrollBackground() { return (Boolean) val("UseWebViewBackgroundForOverscrollBackground", false); }
    @Override public boolean getUseWideViewPort() { return (Boolean) val("UseWideViewPort", false); }
    @Override public int getUserAgent() { return (Integer) val("UserAgent", 0); }
    @Override public java.lang.String getUserAgentString() { return (java.lang.String) val("UserAgentString", DEFAULT_UA); }
    @Override public boolean getVideoOverlayForEmbeddedEncryptedVideoEnabled() { return (Boolean) val("VideoOverlayForEmbeddedEncryptedVideoEnabled", false); }
    @Override public void setAcceptThirdPartyCookies(boolean v) { put("AcceptThirdPartyCookies", v); }
    @Override public void setAllowContentAccess(boolean v) { put("AllowContentAccess", v); }
    @Override public void setAllowFileAccess(boolean v) { put("AllowFileAccess", v); }
    @Override public void setAllowFileAccessFromFileURLs(boolean v) { put("AllowFileAccessFromFileURLs", v); }
    @Override public void setAllowUniversalAccessFromFileURLs(boolean v) { put("AllowUniversalAccessFromFileURLs", v); }
    @Override public void setBlockNetworkImage(boolean v) { put("BlockNetworkImage", v); }
    @Override public void setBlockNetworkLoads(boolean v) { put("BlockNetworkLoads", v); }
    @Override public void setBuiltInZoomControls(boolean v) { put("BuiltInZoomControls", v); }
    @Override public void setCacheMode(int v) { put("CacheMode", v); }
    @Override public void setCursiveFontFamily(java.lang.String v) { put("CursiveFontFamily", v); }
    @Override public void setDatabaseEnabled(boolean v) { put("DatabaseEnabled", v); }
    @Override public void setDatabasePath(java.lang.String v) { put("DatabasePath", v); }
    @Override public void setDefaultFixedFontSize(int v) { put("DefaultFixedFontSize", v); }
    @Override public void setDefaultFontSize(int v) { put("DefaultFontSize", v); }
    @Override public void setDefaultTextEncodingName(java.lang.String v) { put("DefaultTextEncodingName", v); }
    @Override public void setDefaultZoom(android.webkit.WebSettings.ZoomDensity v) { put("DefaultZoom", v); }
    @Override public void setDisabledActionModeMenuItems(int v) { put("DisabledActionModeMenuItems", v); }
    @Override public void setDisplayZoomControls(boolean v) { put("DisplayZoomControls", v); }
    @Override public void setDomStorageEnabled(boolean v) { put("DomStorageEnabled", v); }
    @Override public void setEnableSmoothTransition(boolean v) { put("EnableSmoothTransition", v); }
    @Override public void setFantasyFontFamily(java.lang.String v) { put("FantasyFontFamily", v); }
    @Override public void setFixedFontFamily(java.lang.String v) { put("FixedFontFamily", v); }
    @Override public void setGeolocationDatabasePath(java.lang.String v) { put("GeolocationDatabasePath", v); }
    @Override public void setGeolocationEnabled(boolean v) { put("GeolocationEnabled", v); }
    @Override public void setJavaScriptCanOpenWindowsAutomatically(boolean v) { put("JavaScriptCanOpenWindowsAutomatically", v); }
    @Override public void setJavaScriptEnabled(boolean v) { put("JavaScriptEnabled", v); }
    @Override public void setLayoutAlgorithm(android.webkit.WebSettings.LayoutAlgorithm v) { put("LayoutAlgorithm", v); }
    @Override public void setLightTouchEnabled(boolean v) { put("LightTouchEnabled", v); }
    @Override public void setLoadWithOverviewMode(boolean v) { put("LoadWithOverviewMode", v); }
    @Override public void setLoadsImagesAutomatically(boolean v) { put("LoadsImagesAutomatically", v); }
    @Override public void setMediaPlaybackRequiresUserGesture(boolean v) { put("MediaPlaybackRequiresUserGesture", v); }
    @Override public void setMinimumFontSize(int v) { put("MinimumFontSize", v); }
    @Override public void setMinimumLogicalFontSize(int v) { put("MinimumLogicalFontSize", v); }
    @Override public void setMixedContentMode(int v) { put("MixedContentMode", v); }
    @Override public void setNavDump(boolean v) { put("NavDump", v); }
    @Override public void setNeedInitialFocus(boolean v) { put("NeedInitialFocus", v); }
    @Override public void setOffscreenPreRaster(boolean v) { put("OffscreenPreRaster", v); }
    @Override public void setPluginState(android.webkit.WebSettings.PluginState v) { put("PluginState", v); }
    @Override public void setPluginsEnabled(boolean v) { put("PluginsEnabled", v); }
    @Override public void setRenderPriority(android.webkit.WebSettings.RenderPriority v) { put("RenderPriority", v); }
    @Override public void setSafeBrowsingEnabled(boolean v) { put("SafeBrowsingEnabled", v); }
    @Override public void setSansSerifFontFamily(java.lang.String v) { put("SansSerifFontFamily", v); }
    @Override public void setSaveFormData(boolean v) { put("SaveFormData", v); }
    @Override public void setSavePassword(boolean v) { put("SavePassword", v); }
    @Override public void setSerifFontFamily(java.lang.String v) { put("SerifFontFamily", v); }
    @Override public void setStandardFontFamily(java.lang.String v) { put("StandardFontFamily", v); }
    @Override public void setSupportMultipleWindows(boolean v) { put("SupportMultipleWindows", v); }
    @Override public void setSupportZoom(boolean v) { put("SupportZoom", v); }
    @Override public void setTextZoom(int v) { put("TextZoom", v); }
    @Override public void setUseWebViewBackgroundForOverscrollBackground(boolean v) { put("UseWebViewBackgroundForOverscrollBackground", v); }
    @Override public void setUseWideViewPort(boolean v) { put("UseWideViewPort", v); }
    @Override public void setUserAgent(int v) { put("UserAgent", v); }
    @Override public void setUserAgentString(java.lang.String v) { put("UserAgentString", v); }
    @Override public void setVideoOverlayForEmbeddedEncryptedVideoEnabled(boolean v) { put("VideoOverlayForEmbeddedEncryptedVideoEnabled", v); }
    @Override public boolean supportMultipleWindows() { return (Boolean) val("SupportMultipleWindows", false); }
    @Override public boolean supportZoom() { return (Boolean) val("SupportZoom", true); }
}
