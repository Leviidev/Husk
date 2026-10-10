package android.webkit;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Picture;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Message;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsoluteLayout;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

/**
 * A web page in the app's window, drawn by WKWebView under the screen (see husk.Web): the view leaves a clear hole in the window
 * where WebKit's page shows through, and tells the host where that is whenever it moves.
 *
 * The app's own content is served through the huskapp scheme: file:///android_asset/x is huskapp://android_asset/x, other files
 * huskapp://file/path, and an http(s) origin whose first page the app's shouldInterceptRequest answers (Capacitor's https://localhost)
 * becomes huskapp://host. Every request on those comes back here, to shouldInterceptRequest and then the APK's assets or the
 * file system; anything else is fetched by the host. The URLs the app sees are always the Android ones.
 */
@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class WebView extends AbsoluteLayout implements android.view.ViewTreeObserver.OnGlobalFocusChangeListener, android.view.ViewGroup.OnHierarchyChangeListener {
    public static final int RENDERER_PRIORITY_BOUND = 1;
    public static final int RENDERER_PRIORITY_IMPORTANT = 2;
    public static final int RENDERER_PRIORITY_WAIVED = 0;
    public static final String SCHEME_GEO = "geo:0,0?q=";
    public static final String SCHEME_MAILTO = "mailto:";
    public static final String SCHEME_TEL = "tel:";
    private static final String TAG = "WebView";

    private static final HashMap<Integer, WebView> sViews = new HashMap<>();
    private static int sNextId = 1;
    private static final ExecutorService sIo = Executors.newFixedThreadPool(4, r -> { Thread t = new Thread(r, "Chrome_IOThread"); t.setDaemon(true); return t; });
    private static final ExecutorService sBridge = Executors.newSingleThreadExecutor(r -> { Thread t = new Thread(r, "JavaBridge"); t.setDaemon(true); return t; });

    private final int mId;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final HuskWebSettings mSettings;
    private volatile WebViewClient mClient = new WebViewClient();
    private volatile WebChromeClient mChrome;
    private DownloadListener mDownload;
    private FindListener mFindListener;
    private PictureListener mPictureListener;
    private WebViewRenderProcessClient mRenderClient;
    private final Map<String, Object> mInterfaces = new LinkedHashMap<>();
    private final Map<String, String> mLocalSchemes = new HashMap<>();          // host -> http / https, origins the app serves
    private final Map<String, WebResourceResponse> mPrimed = new HashMap<>();   // the first page of such an origin, already answered
    private final HashMap<Integer, ValueCallback<String>> mEvalCallbacks = new HashMap<>();
    private int mNextCallback = 1;
    private volatile String mUrl, mOriginalUrl, mTitle;
    private volatile int mProgress = 100;
    private volatile boolean mCanBack, mCanForward, mDestroyed, mPaused;
    private final ArrayList<String> mHistory = new ArrayList<>();
    private int mFrameX = -1, mFrameY, mFrameW, mFrameH;
    private boolean mFrameVisible;
    private boolean mSettingsPosted;
    private final Paint mClear = new Paint();

    public WebView(Context context) { this(context, null); }
    public WebView(Context context, AttributeSet attrs) { this(context, attrs, android.R.attr.webViewStyle); }
    public WebView(Context context, AttributeSet attrs, int defStyleAttr) { this(context, attrs, defStyleAttr, 0); }
    public WebView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
        mClear.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        setWillNotDraw(false);
        setFocusable(true);
        setFocusableInTouchMode(true);
        synchronized (sViews) { mId = sNextId++; sViews.put(mId, this); }
        mSettings = new HuskWebSettings(this::settingsChanged);
        husk.Web.create(mId);
        pushSettings();
    }
    public WebView(Context context, AttributeSet attrs, int defStyleAttr, boolean privateBrowsing) { this(context, attrs, defStyleAttr, 0); }
    protected WebView(Context context, AttributeSet attrs, int defStyleAttr, Map<String, Object> javaScriptInterfaces, boolean privateBrowsing) {
        this(context, attrs, defStyleAttr, 0);
        if (javaScriptInterfaces != null) for (Map.Entry<String, Object> e : javaScriptInterfaces.entrySet()) addJavascriptInterface(e.getValue(), e.getKey());
    }
    protected WebView(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes, Map<String, Object> javaScriptInterfaces, boolean privateBrowsing) {
        this(context, attrs, defStyleAttr, defStyleRes);
        if (javaScriptInterfaces != null) for (Map.Entry<String, Object> e : javaScriptInterfaces.entrySet()) addJavascriptInterface(e.getValue(), e.getKey());
    }

    /** @hide */ public static WebView huskById(int id) { synchronized (sViews) { return sViews.get(id); } }

    // ---- static API
    public static void setWebContentsDebuggingEnabled(boolean enabled) {}
    public static void setDataDirectorySuffix(String suffix) {}
    public static void disableWebView() {}
    public static void clearClientCertPreferences(Runnable onCleared) { if (onCleared != null) onCleared.run(); }
    public static void startSafeBrowsing(Context context, ValueCallback<Boolean> callback) { if (callback != null) callback.onReceiveValue(false); }
    public static void setSafeBrowsingWhitelist(List<String> hosts, ValueCallback<Boolean> callback) { if (callback != null) callback.onReceiveValue(true); }
    public static Uri getSafeBrowsingPrivacyPolicyUrl() { return Uri.parse("https://policies.google.com/privacy"); }
    /** The "system WebView" apps check the version of (Capacitor wants 60+): Chrome's package, at the version the user agent names. */
    public static android.content.pm.PackageInfo getCurrentWebViewPackage() {
        android.content.pm.PackageInfo p = new android.content.pm.PackageInfo();
        p.packageName = "com.google.android.webview";
        p.versionName = "124.0.6367.179";
        p.versionCode = 636717900;
        p.applicationInfo = new android.content.pm.ApplicationInfo();
        p.applicationInfo.packageName = p.packageName;
        p.applicationInfo.enabled = true;
        return p;
    }
    public static ClassLoader getWebViewClassLoader() { return WebView.class.getClassLoader(); }
    public static String findAddress(String addr) { return null; }
    public static void enableSlowWholeDocumentDraw() {}
    public static void enablePlatformNotifications() {}
    public static void disablePlatformNotifications() {}
    public static void freeMemoryForTests() {}
    public static PluginList getPluginList() { return null; }

    // ---- settings and clients
    public WebSettings getSettings() { return mSettings; }
    public void setWebViewClient(WebViewClient client) { mClient = client != null ? client : new WebViewClient(); }
    public WebViewClient getWebViewClient() { return mClient; }
    public void setWebChromeClient(WebChromeClient client) { mChrome = client; }
    public WebChromeClient getWebChromeClient() { return mChrome; }
    public void setDownloadListener(DownloadListener listener) { mDownload = listener; }
    public void setFindListener(FindListener listener) { mFindListener = listener; }
    @Deprecated public void setPictureListener(PictureListener listener) { mPictureListener = listener; }
    public void setWebViewRenderProcessClient(java.util.concurrent.Executor executor, WebViewRenderProcessClient client) { mRenderClient = client; }
    public void setWebViewRenderProcessClient(WebViewRenderProcessClient client) { mRenderClient = client; }
    public WebViewRenderProcessClient getWebViewRenderProcessClient() { return mRenderClient; }
    public WebViewRenderProcess getWebViewRenderProcess() { return null; }
    private void settingsChanged() {
        synchronized (this) { if (mSettingsPosted) return; mSettingsPosted = true; }
        mHandler.post(() -> { synchronized (this) { mSettingsPosted = false; } pushSettings(); });
    }
    private void pushSettings() {
        if (mDestroyed) return;
        husk.Web.settings(mId, mSettings.getJavaScriptEnabled(), mSettings.getUserAgentString(), mSettings.getTextZoom(), mSettings.getMediaPlaybackRequiresUserGesture());
    }
    @Override public void setBackgroundColor(int color) { if (!mDestroyed) husk.Web.background(mId, color); }

    // ---- loading
    public void loadUrl(String url) { loadUrl(url, null); }
    public void loadUrl(String url, Map<String, String> additionalHttpHeaders) {
        if (mDestroyed || url == null) return;
        Log.d(TAG, "loadUrl " + url);
        if (url.startsWith("javascript:")) { evaluateJavascript(Uri.decode(url.substring(11)), null); return; }
        url = normalize(url);
        if (mOriginalUrl == null) mOriginalUrl = url;
        mUrl = url;
        String[] h = null;
        if (additionalHttpHeaders != null) {
            h = new String[additionalHttpHeaders.size() * 2];
            int i = 0;
            for (Map.Entry<String, String> e : additionalHttpHeaders.entrySet()) { h[i++] = e.getKey(); h[i++] = e.getValue(); }
        }
        final String[] headers = h;
        final String target = url;
        if (isHttp(url) && !isLocal(url)) {
            // the app may serve this origin itself (shouldInterceptRequest): ask before WebKit goes to the network
            sIo.execute(() -> {
                WebResourceResponse r = intercept(target, "GET", true, additionalHttpHeaders);
                Log.d(TAG, target + (r != null ? ": the app serves this origin" : ": from the network"));
                synchronized (this) {
                    if (r != null) { Uri u = Uri.parse(target); mLocalSchemes.put(u.getHost(), u.getScheme()); mPrimed.put(toHost(target), r); }
                }
                husk.Web.load(mId, toHost(target), headers, null);
            });
            return;
        }
        husk.Web.load(mId, toHost(url), headers, null);
    }
    public void postUrl(String url, byte[] postData) {
        if (mDestroyed) return;
        mUrl = url;
        husk.Web.load(mId, toHost(url), null, postData);
    }
    public void loadData(String data, String mimeType, String encoding) {
        if (mDestroyed) return;
        Log.d(TAG, "loadData " + mimeType);
        byte[] bytes = "base64".equalsIgnoreCase(encoding) ? android.util.Base64.decode(data, android.util.Base64.DEFAULT) : (data == null ? "" : data).getBytes(java.nio.charset.StandardCharsets.UTF_8);
        mUrl = "data:" + (mimeType == null ? "text/html" : mimeType) + "," + (data == null ? "" : data.substring(0, Math.min(data.length(), 64)));
        husk.Web.loadData(mId, bytes, mimeType == null || mimeType.isEmpty() ? "text/html" : mimeType, "utf-8", null);
    }
    public void loadDataWithBaseURL(String baseUrl, String data, String mimeType, String encoding, String historyUrl) {
        if (mDestroyed) return;
        Log.d(TAG, "loadDataWithBaseURL " + baseUrl);
        byte[] bytes = (data == null ? "" : data).getBytes(java.nio.charset.StandardCharsets.UTF_8);
        String base = baseUrl == null || baseUrl.startsWith("data:") ? null : baseUrl;
        mUrl = historyUrl != null ? historyUrl : base != null ? base : "about:blank";
        husk.Web.loadData(mId, bytes, mimeType == null || mimeType.isEmpty() ? "text/html" : mimeType, "utf-8", base == null ? null : toHost(base));
    }
    public void evaluateJavascript(String script, ValueCallback<String> resultCallback) {
        if (mDestroyed || script == null) return;
        int cb = 0;
        if (resultCallback != null) synchronized (mEvalCallbacks) { cb = mNextCallback++; mEvalCallbacks.put(cb, resultCallback); }
        husk.Web.eval(mId, script, cb);
    }
    public void stopLoading() { if (!mDestroyed) husk.Web.go(mId, 3); }
    public void reload() { if (!mDestroyed) husk.Web.go(mId, 2); }
    public boolean canGoBack() { return mCanBack; }
    public void goBack() { if (!mDestroyed) { mCanBack = false; husk.Web.go(mId, 0); } }
    public boolean canGoForward() { return mCanForward; }
    public void goForward() { if (!mDestroyed) husk.Web.go(mId, 1); }
    public boolean canGoBackOrForward(int steps) { return steps == 0 || (steps < 0 ? steps == -1 && mCanBack : steps == 1 && mCanForward); }
    public void goBackOrForward(int steps) { if (steps < 0) for (int i = 0; i < -steps; i++) goBack(); else for (int i = 0; i < steps; i++) goForward(); }
    public boolean pageUp(boolean top) { evaluateJavascript(top ? "window.scrollTo(0,0)" : "window.scrollBy(0,-window.innerHeight/2)", null); return true; }
    public boolean pageDown(boolean bottom) { evaluateJavascript(bottom ? "window.scrollTo(0,document.body.scrollHeight)" : "window.scrollBy(0,window.innerHeight/2)", null); return true; }
    public void clearHistory() { mHistory.clear(); }
    public void clearCache(boolean includeDiskFiles) {}
    public void clearFormData() {}
    public void clearSslPreferences() {}
    public void clearMatches() {}
    public void clearView() {}
    public String getUrl() { return mUrl; }
    public String getOriginalUrl() { return mOriginalUrl; }
    public String getTitle() { return mTitle; }
    public Bitmap getFavicon() { return null; }
    public String getTouchIconUrl() { return null; }
    public int getProgress() { return mProgress; }
    public int getContentHeight() { return getHeight(); }
    public int getContentWidth() { return getWidth(); }
    public float getScale() { return getResources().getDisplayMetrics().density; }
    public void setInitialScale(int scaleInPercent) {}
    public void invokeZoomPicker() {}
    public boolean canZoomIn() { return false; }
    public boolean canZoomOut() { return false; }
    public void zoomBy(float zoomFactor) {}
    public boolean zoomIn() { return false; }
    public boolean zoomOut() { return false; }
    public View getZoomControls() { return null; }
    public void flingScroll(int vx, int vy) {}
    public HitTestResult getHitTestResult() { return new HitTestResult(); }
    public void requestFocusNodeHref(Message hrefMsg) { if (hrefMsg != null) hrefMsg.sendToTarget(); }
    public void requestImageRef(Message msg) { if (msg != null) msg.sendToTarget(); }
    public void documentHasImages(Message response) { if (response != null) { response.arg1 = 0; response.sendToTarget(); } }
    public android.net.http.SslCertificate getCertificate() { return null; }
    @Deprecated public void setCertificate(android.net.http.SslCertificate certificate) {}
    @Deprecated public void savePassword(String host, String username, String password) {}
    @Deprecated public void setHttpAuthUsernamePassword(String host, String realm, String username, String password) {}
    @Deprecated public String[] getHttpAuthUsernamePassword(String host, String realm) { return null; }
    public void setNetworkAvailable(boolean networkUp) {}
    public WebBackForwardList saveState(Bundle outState) { return copyBackForwardList(); }
    public WebBackForwardList restoreState(Bundle inState) { return null; }
    public void saveWebArchive(String filename) {}
    public void saveWebArchive(String basename, boolean autoname, ValueCallback<String> callback) { if (callback != null) callback.onReceiveValue(null); }
    @Deprecated public boolean savePicture(Bundle b, File dest) { return false; }
    @Deprecated public boolean restorePicture(Bundle b, File src) { return false; }
    @Deprecated public Picture capturePicture() { return new Picture(); }
    public android.print.PrintDocumentAdapter createPrintDocumentAdapter() { return null; }
    public android.print.PrintDocumentAdapter createPrintDocumentAdapter(String documentName) { return null; }
    public int findAll(String find) { findAllAsync(find); return 0; }
    public void findAllAsync(String find) { if (mFindListener != null) mFindListener.onFindResultReceived(0, 0, true); }
    public void findNext(boolean forward) {}
    public boolean showFindDialog(String text, boolean showIme) { return false; }
    public void pauseTimers() {}
    public void resumeTimers() {}
    public void onPause() { mPaused = true; }
    public void onResume() { mPaused = false; }
    public boolean isPaused() { return mPaused; }
    public boolean isPrivateBrowsingEnabled() { return false; }
    public void freeMemory() {}
    public void setMapTrackballToArrowKeys(boolean setMap) {}
    public void setHorizontalScrollbarOverlay(boolean overlay) {}
    public void setVerticalScrollbarOverlay(boolean overlay) {}
    public boolean overlayHorizontalScrollbar() { return true; }
    public boolean overlayVerticalScrollbar() { return false; }
    public int getVisibleTitleHeight() { return 0; }
    public void setRendererPriorityPolicy(int rendererRequestedPriority, boolean waivedWhenNotVisible) {}
    public int getRendererRequestedPriority() { return RENDERER_PRIORITY_IMPORTANT; }
    public boolean getRendererPriorityWaivedWhenNotVisible() { return false; }
    public void setTextClassifier(android.view.textclassifier.TextClassifier tc) {}
    public android.view.textclassifier.TextClassifier getTextClassifier() { return null; }
    public Looper getWebViewLooper() { return Looper.getMainLooper(); }
    public void postVisualStateCallback(long requestId, VisualStateCallback callback) { mHandler.post(() -> callback.onComplete(requestId)); }
    public WebMessagePort[] createWebMessageChannel() { return new WebMessagePort[0]; }
    public void postWebMessage(WebMessage message, Uri targetOrigin) {
        evaluateJavascript("window.dispatchEvent(new MessageEvent('message',{data:" + JSONObject.quote(message.getData()) + "}))", null);
    }
    public WebBackForwardList copyBackForwardList() {
        final String url = mUrl;
        return new WebBackForwardList() {
            public WebHistoryItem getCurrentItem() { return url == null ? null : item(url); }
            public int getCurrentIndex() { return url == null ? -1 : 0; }
            public WebHistoryItem getItemAtIndex(int index) { return index == 0 && url != null ? item(url) : null; }
            public int getSize() { return url == null ? 0 : 1; }
            protected WebBackForwardList clone() { return this; }
        };
    }
    private WebHistoryItem item(final String url) {
        final String title = mTitle;
        return new WebHistoryItem() {
            public int getId() { return 0; }
            public String getUrl() { return url; }
            public String getOriginalUrl() { return url; }
            public String getTitle() { return title; }
            public Bitmap getFavicon() { return null; }
            protected WebHistoryItem clone() { return this; }
        };
    }
    public void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        synchronized (sViews) { sViews.remove(mId); }
        husk.Web.destroy(mId);
    }

    // ---- addJavascriptInterface: prompt() bridge
    public void addJavascriptInterface(Object object, String name) {
        if (object == null || name == null || mDestroyed) return;
        synchronized (mInterfaces) { mInterfaces.put(name, object); }
        StringBuilder js = new StringBuilder("(function(){var n=").append(JSONObject.quote(name)).append(",o={};");
        for (String m : bridgedMethods(object.getClass()))
            js.append("o[").append(JSONObject.quote(m)).append("]=function(){return window.__husk.call(n,").append(JSONObject.quote(m)).append(",arguments)};");
        js.append("try{Object.defineProperty(window,n,{value:o,configurable:true,writable:true})}catch(e){window[n]=o}})();");
        husk.Web.userScript(mId, js.toString());
    }
    public void removeJavascriptInterface(String name) {
        synchronized (mInterfaces) { mInterfaces.remove(name); }
        evaluateJavascript("try{delete window[" + JSONObject.quote(name) + "]}catch(e){}", null);
    }
    private static boolean annotationsRequired() { return husk.Manifest.targetSdk >= 17; }
    private static Boolean sAnnotationsWork;
    /** Whether the runtime reports annotations at all (JavascriptInterface's own @Retention): without them every public method is bridged. */
    private static boolean annotationsWork() {
        if (sAnnotationsWork == null) {
            boolean ok;
            try { ok = JavascriptInterface.class.isAnnotationPresent(java.lang.annotation.Retention.class); } catch (Throwable t) { ok = false; }
            sAnnotationsWork = ok;
        }
        return sAnnotationsWork;
    }
    private static boolean bridged(Method m) {
        if (!Modifier.isPublic(m.getModifiers()) || m.getDeclaringClass() == Object.class) return false;
        if (!annotationsRequired() || !annotationsWork()) return true;
        try { return m.isAnnotationPresent(JavascriptInterface.class); } catch (Throwable t) { return true; }
    }
    private static List<String> bridgedMethods(Class<?> c) {
        HashSet<String> seen = new HashSet<>();
        ArrayList<String> out = new ArrayList<>();
        for (Method m : c.getMethods()) if (bridged(m) && seen.add(m.getName())) out.add(m.getName());
        return out;
    }
    private void bridgeCall(int call, String json) {
        String result;
        try {
            JSONObject req = new JSONObject(json);
            Object target;
            synchronized (mInterfaces) { target = mInterfaces.get(req.getString("o")); }
            String name = req.getString("m");
            JSONArray args = req.optJSONArray("a");
            int n = args == null ? 0 : args.length();
            Method found = null;
            if (target != null) for (Method m : target.getClass().getMethods()) if (m.getName().equals(name) && m.getParameterTypes().length == n && bridged(m)) { found = m; break; }
            if (found == null) { result = "{\"e\":\"Method not found\"}"; }
            else {
                Class<?>[] pt = found.getParameterTypes();
                Object[] av = new Object[n];
                for (int i = 0; i < n; i++) av[i] = convert(args.isNull(i) ? null : args.get(i), pt[i]);
                Object r;
                try { r = found.invoke(target, av); }
                catch (InvocationTargetException e) { Log.e(TAG, "Java exception in a JavaScript interface method", e.getCause()); r = null; json = null; }
                if (json == null) result = "{\"e\":\"Java exception was raised during method invocation\"}";
                else if (found.getReturnType() == void.class || r == null) result = "{}";
                else if (r instanceof String) result = "{\"v\":" + JSONObject.quote((String) r) + "}";
                else if (r instanceof Number || r instanceof Boolean) result = "{\"v\":" + JSONObject.wrap(r) + "}";
                else if (r instanceof Character) result = "{\"v\":" + JSONObject.quote(r.toString()) + "}";
                else result = "{}";
            }
        } catch (JSONException | IllegalAccessException | IllegalArgumentException e) {
            Log.w(TAG, "bridge call failed: " + e);
            result = "{}";
        }
        husk.Web.reply(call, result);
    }
    private static Object convert(Object v, Class<?> t) {
        if (t == String.class) return v == null ? null : v instanceof String ? v : "undefined".equals(v) ? null : String.valueOf(v);
        if (t == boolean.class || t == Boolean.class) return v instanceof Boolean ? v : Boolean.FALSE;
        double d = v instanceof Number ? ((Number) v).doubleValue() : 0;
        if (t == int.class || t == Integer.class) return (int) d;
        if (t == long.class || t == Long.class) return (long) d;
        if (t == double.class || t == Double.class) return d;
        if (t == float.class || t == Float.class) return (float) d;
        if (t == short.class || t == Short.class) return (short) d;
        if (t == byte.class || t == Byte.class) return (byte) d;
        if (t == char.class || t == Character.class) return (char) d;
        return v == null || v == JSONObject.NULL ? null : t.isInstance(v) ? v : null;
    }

    // ---- the app's own content: URL mapping and requests
    /** As Chromium has it: an http(s) URL with no path has the path "/". */
    private static String normalize(String url) {
        if (!isHttp(url)) return url;
        int start = url.indexOf("://") + 3;
        int end = start;
        while (end < url.length() && "/?#".indexOf(url.charAt(end)) < 0) end++;
        return end < url.length() && url.charAt(end) == '/' ? url : url.substring(0, end) + "/" + url.substring(end);
    }
    private static boolean isHttp(String url) { return url.regionMatches(true, 0, "http://", 0, 7) || url.regionMatches(true, 0, "https://", 0, 8); }
    private synchronized boolean isLocal(String url) {
        Uri u = Uri.parse(url);
        String s = mLocalSchemes.get(u.getHost());
        return s != null && s.equalsIgnoreCase(u.getScheme());
    }
    private synchronized String toHost(String url) {
        if (url.startsWith("file:///android_asset/")) return "huskapp://android_asset/" + url.substring(22);
        if (url.startsWith("file:///android_res/")) return "huskapp://android_res/" + url.substring(20);
        if (url.startsWith("file://")) return "huskapp://file" + url.substring(7);
        if (url.startsWith("file:/")) return "huskapp://file" + url.substring(5);
        if (isHttp(url) && isLocal(url)) return "huskapp" + url.substring(url.indexOf("://"));
        return url;
    }
    private synchronized String fromHost(String url) {
        if (url == null || !url.startsWith("huskapp://")) return url;
        String rest = url.substring(10);
        if (rest.startsWith("android_asset/")) return "file:///" + rest;
        if (rest.startsWith("android_res/")) return "file:///" + rest;
        if (rest.startsWith("file/")) return "file://" + rest.substring(4);
        int slash = rest.indexOf('/');
        String host = slash < 0 ? rest : rest.substring(0, slash);
        int colon = host.indexOf(':');
        String scheme = mLocalSchemes.get(colon < 0 ? host : host.substring(0, colon));
        return (scheme != null ? scheme : "https") + "://" + rest;
    }
    private WebResourceRequest request(String url, String method, boolean main, boolean gesture, Map<String, String> headers) {
        final Uri u = Uri.parse(url);
        final Map<String, String> h = headers != null ? headers : new HashMap<>();
        return new WebResourceRequest() {
            public Uri getUrl() { return u; }
            public boolean isForMainFrame() { return main; }
            public boolean isRedirect() { return false; }
            public boolean hasGesture() { return gesture; }
            public String getMethod() { return method; }
            public Map<String, String> getRequestHeaders() { return h; }
        };
    }
    private WebResourceResponse intercept(String url, String method, boolean main, Map<String, String> headers) {
        try { return mClient.shouldInterceptRequest(this, request(url, method, main, false, headers)); }
        catch (RuntimeException e) { Log.e(TAG, "shouldInterceptRequest threw for " + url, e); return null; }
    }
    private static byte[] readAll(InputStream in) throws IOException {
        if (in == null) return new byte[0];
        try (InputStream s = in) {
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            byte[] buf = new byte[16384];
            for (int n; (n = s.read(buf)) > 0; ) out.write(buf, 0, n);
            return out.toByteArray();
        }
    }
    private static String mimeOf(String path) {
        String ext = MimeTypeMap.getFileExtensionFromUrl(path);
        String m = ext.isEmpty() ? null : MimeTypeMap.getSingleton().getMimeTypeFromExtension(ext);
        return m != null ? m : "application/octet-stream";
    }
    private void serve(int call, String hostUrl, String method, boolean main, Map<String, String> headers) {
        String url = fromHost(hostUrl);
        WebResourceResponse r;
        synchronized (this) { r = main ? mPrimed.remove(hostUrl) : null; }
        if (r == null) r = intercept(url, method, main, headers);
        try {
            if (r != null) {
                Map<String, String> rh = r.getResponseHeaders();
                String[] h = null;
                if (rh != null) { h = new String[rh.size() * 2]; int i = 0; for (Map.Entry<String, String> e : rh.entrySet()) { h[i++] = e.getKey(); h[i++] = e.getValue(); } }
                byte[] body = readAll(r.getData());
                int status = r.getData() == null && r.getStatusCode() == 200 ? 404 : r.getStatusCode();
                husk.Web.respond(call, status, r.getReasonPhrase(), r.getMimeType(), r.getEncoding(), h, body);
                return;
            }
            String path = Uri.parse(url).getPath();
            if (url.startsWith("file:///android_asset/")) {
                byte[] b;
                try { b = readAll(getContext().getAssets().open(Uri.decode(url.substring(22).replaceAll("[?#].*$", "")))); }
                catch (IOException e) { husk.Web.respond(call, 404, "Not Found", "text/plain", null, null, new byte[0]); return; }
                husk.Web.respond(call, 200, "OK", mimeOf(path), null, null, b);
                return;
            }
            if (url.startsWith("file:///android_res/")) { husk.Web.respond(call, 404, "Not Found", "text/plain", null, null, new byte[0]); return; }
            if (url.startsWith("file:")) {
                File f = new File(path == null ? "" : path);
                if (!f.isFile()) { husk.Web.respond(call, 404, "Not Found", "text/plain", null, null, new byte[0]); return; }
                husk.Web.respond(call, 200, "OK", mimeOf(f.getName()), null, null, readAll(new FileInputStream(f)));
                return;
            }
            husk.Web.respond(call, 0, url, null, null, null, null);       // the network's: the host fetches it
        } catch (IOException | RuntimeException e) {
            Log.w(TAG, "serving " + url + " failed: " + e);
            husk.Web.respond(call, -1, null, null, null, null, null);
        }
    }

    // ---- events from the host
    /** @hide */
    public void huskEvent(String[] f) {
        switch (f[0]) {
        case "s": {
            String url = fromHost(f[2]);
            mUrl = url; mProgress = 10;
            mClient.onPageStarted(this, url, null);
            break;
        }
        case "m": mClient.onPageCommitVisible(this, fromHost(f[2])); mClient.doUpdateVisitedHistory(this, fromHost(f[2]), false); break;
        case "f": {
            String url = fromHost(f[2]);
            if (!url.isEmpty()) mUrl = url;
            mProgress = 100;
            if (mChrome != null) mChrome.onProgressChanged(this, 100);
            mClient.onPageFinished(this, mUrl);
            break;
        }
        case "t": mTitle = f[2]; if (mChrome != null) mChrome.onReceivedTitle(this, f[2]); break;
        case "p": {
            int p = Integer.parseInt(f[2]);
            if (p == mProgress) break;
            mProgress = p;
            if (mChrome != null && p < 100) mChrome.onProgressChanged(this, p);
            break;
        }
        case "h": mCanBack = "1".equals(f[2]); mCanForward = "1".equals(f[3]); if (!f[4].isEmpty()) mUrl = fromHost(f[4]); break;
        case "E": {
            final int code = errorCode(Integer.parseInt(f[2]));
            final String desc = f[3];
            mClient.onReceivedError(this, request(fromHost(f[4]), "GET", true, false, null), new WebResourceError() {
                public CharSequence getDescription() { return desc; }
                public int getErrorCode() { return code; }
            });
            break;
        }
        case "g": {
            boolean handled = mClient.onRenderProcessGone(this, new RenderProcessGoneDetail() {
                public boolean didCrash() { return true; }
                public int rendererPriorityAtExit() { return RENDERER_PRIORITY_IMPORTANT; }
            });
            if (!handled) reload();
            break;
        }
        case "x": if (mChrome != null) mChrome.onCloseWindow(this); break;
        case "o": navigation(Integer.parseInt(f[2]), fromHost(f[3]), "1".equals(f[4]), "1".equals(f[5]), f[6], "1".equals(f[7])); break;
        case "A": case "C": case "P": dialog(f[0], Integer.parseInt(f[2]), fromHost(f[3]), f[4], f.length > 5 ? f[5] : null); break;
        case "j": { final int call = Integer.parseInt(f[2]); final String json = f[3]; sBridge.execute(() -> bridgeCall(call, json)); break; }
        case "c": console(Integer.parseInt(f[2]), f[3], f[4], f.length > 5 ? parseInt(f[5]) : 0); break;
        case "v": {
            ValueCallback<String> cb;
            synchronized (mEvalCallbacks) { cb = mEvalCallbacks.remove(Integer.parseInt(f[2])); }
            if (cb != null) cb.onReceiveValue(f[3]);
            break;
        }
        case "r": {
            final int call = Integer.parseInt(f[2]);
            final String url = f[3], method = f[4];
            final boolean main = "1".equals(f[5]);
            final Map<String, String> h = new HashMap<>();
            for (int i = 6; i + 1 < f.length; i += 2) h.put(f[i], f[i + 1]);
            sIo.execute(() -> serve(call, url, method, main, h));
            break;
        }
        }
    }
    private static int parseInt(String s) { try { return Integer.parseInt(s); } catch (NumberFormatException e) { return 0; } }
    private static int errorCode(int ns) {
        switch (ns) {
        case -1003: case -1006: return WebViewClient.ERROR_HOST_LOOKUP;
        case -1001: return WebViewClient.ERROR_TIMEOUT;
        case -1004: case -1005: case -1009: return WebViewClient.ERROR_CONNECT;
        case -1002: return WebViewClient.ERROR_UNSUPPORTED_SCHEME;
        case -1000: return WebViewClient.ERROR_BAD_URL;
        case -1007: return WebViewClient.ERROR_REDIRECT_LOOP;
        case -1011: return WebViewClient.ERROR_IO;
        case -1012: case -1013: return WebViewClient.ERROR_AUTHENTICATION;
        case -1100: case -1101: return WebViewClient.ERROR_FILE_NOT_FOUND;
        case -1102: return WebViewClient.ERROR_FILE;
        default: return ns <= -1200 && ns >= -1206 ? WebViewClient.ERROR_FAILED_SSL_HANDSHAKE : WebViewClient.ERROR_UNKNOWN;
        }
    }
    private void navigation(int call, String url, boolean main, boolean gesture, String method, boolean web) {
        boolean override = false;
        try { override = mClient.shouldOverrideUrlLoading(this, request(url, method, main, gesture, null)); }
        catch (RuntimeException e) { Log.e(TAG, "shouldOverrideUrlLoading threw for " + url, e); }
        if (!override && main && isHttp(url) && isLocal(url) && !url.startsWith("huskapp:")) {
            // a link to an origin the app serves: load it through the huskapp scheme instead
            husk.Web.reply(call, "\u0001nav-cancel");
            husk.Web.load(mId, toHost(url), null, null);
            return;
        }
        if (!override && !web) Log.w(TAG, "nothing handles " + url);
        husk.Web.reply(call, override || !web ? "\u0001nav-cancel" : "\u0001nav-allow");
    }
    private void console(int level, String message, String source, int line) {
        ConsoleMessage.MessageLevel l = level == 4 ? ConsoleMessage.MessageLevel.ERROR : level == 3 ? ConsoleMessage.MessageLevel.WARNING
                                      : level == 1 ? ConsoleMessage.MessageLevel.DEBUG : level == 2 ? ConsoleMessage.MessageLevel.LOG : ConsoleMessage.MessageLevel.LOG;
        boolean handled = mChrome != null && mChrome.onConsoleMessage(new ConsoleMessage(message, fromHost(source), line, l));
        if (!handled) Log.println(level >= 4 ? Log.ERROR : level == 3 ? Log.WARN : Log.INFO, "chromium", "[INFO:CONSOLE(" + line + ")] \"" + message + "\", source: " + fromHost(source) + " (" + line + ")");
    }
    private void dialog(String kind, int call, String url, String message, String def) {
        final boolean[] answered = new boolean[1];
        JsResult.ResultReceiver rr = res -> {
            if (answered[0]) return;
            answered[0] = true;
            if (kind.equals("A")) husk.Web.reply(call, "\u0001alert");
            else if (kind.equals("C")) husk.Web.reply(call, res.getResult() ? "\u0001yes" : "\u0001no");
            else husk.Web.reply(call, res.getResult() ? ((JsPromptResult) res).getStringResult() : null);
        };
        JsResult result = kind.equals("P") ? new JsPromptResult(rr) : new JsResult(rr);
        boolean handled = false;
        if (mChrome != null) {
            try {
                handled = kind.equals("A") ? mChrome.onJsAlert(this, url, message, result)
                        : kind.equals("C") ? mChrome.onJsConfirm(this, url, message, result)
                        : mChrome.onJsPrompt(this, url, message, def, (JsPromptResult) result);
            } catch (RuntimeException e) { Log.e(TAG, "a JavaScript dialog handler threw", e); }
        }
        if (handled) return;
        // Android's own dialog
        String host = Uri.parse(url).getHost();
        String title = host == null || host.isEmpty() || url.startsWith("huskapp:") || url.startsWith("file:") ? "JavaScript" : "The page at \"" + host + "\" says:";
        try {
            android.app.AlertDialog.Builder b = new android.app.AlertDialog.Builder(getContext()).setTitle(title).setMessage(message);
            final android.widget.EditText edit = kind.equals("P") ? new android.widget.EditText(getContext()) : null;
            if (edit != null) { edit.setText(def == null ? "" : def); edit.setSingleLine(true); b.setView(edit); }
            b.setPositiveButton(android.R.string.ok, (d, w) -> { if (edit != null) ((JsPromptResult) result).confirm(edit.getText().toString()); else result.confirm(); });
            if (!kind.equals("A")) b.setNegativeButton(android.R.string.cancel, (d, w) -> result.cancel());
            b.setOnCancelListener(d -> result.cancel());
            b.show();
        } catch (RuntimeException e) {
            Log.w(TAG, "could not show a JavaScript dialog: " + e);
            result.cancel();
        }
    }

    // ---- where the page shows
    private boolean mFrameTouchable;
    private void updateFrame() { updateFrame(false); }
    private void updateFrame(boolean force) {
        if (mDestroyed) return;
        boolean visible = isAttachedToWindow() && isShown() && getWindowVisibility() == VISIBLE && getWidth() > 0 && getHeight() > 0;
        int[] loc = new int[2];
        if (visible) getLocationOnScreen(loc);
        // a dialog, popup or menu over the page takes the touches: the page only gets them while its window is the top one
        husk.ViewRoot root = visible ? husk.ViewRoot.of(getRootView()) : null;
        boolean touchable = root != null && root == husk.ViewRoot.topTouchable();
        if (!force && visible == mFrameVisible && touchable == mFrameTouchable && (!visible || (loc[0] == mFrameX && loc[1] == mFrameY && getWidth() == mFrameW && getHeight() == mFrameH))) return;
        mFrameVisible = visible; mFrameTouchable = touchable; mFrameX = loc[0]; mFrameY = loc[1]; mFrameW = getWidth(); mFrameH = getHeight();
        husk.Web.frame(mId, loc[0], loc[1], getWidth(), getHeight(), visible, touchable);
    }
    /** @hide A window came or went: every page says again whether it takes touches. */
    public static void huskWindowsChanged() {
        ArrayList<WebView> all;
        synchronized (sViews) { all = new ArrayList<>(sViews.values()); }
        for (WebView w : all) w.updateFrame(true);
    }
    @Override protected void onDraw(Canvas canvas) {
        canvas.drawRect(0, 0, getWidth(), getHeight(), mClear);
        updateFrame();
    }
    @Override protected void onSizeChanged(int w, int h, int ow, int oh) { super.onSizeChanged(w, h, ow, oh); post(this::updateFrame); }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) { super.onLayout(changed, l, t, r, b); updateFrame(); }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); post(this::updateFrame); }
    @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); updateFrame(); }
    @Override protected void onVisibilityChanged(View changedView, int visibility) { super.onVisibilityChanged(changedView, visibility); updateFrame(); }
    @Override protected void onWindowVisibilityChanged(int visibility) { super.onWindowVisibilityChanged(visibility); updateFrame(); }
    @Override protected void onScrollChanged(int l, int t, int oldl, int oldt) { super.onScrollChanged(l, t, oldl, oldt); updateFrame(); }
    @Override public boolean onTouchEvent(MotionEvent event) { return true; }       // touches in the page's frame go to WebKit itself
    @Override public boolean onCheckIsTextEditor() { return true; }
    @Override public boolean shouldDelayChildPressedState() { return true; }
    public void onGlobalFocusChanged(View oldFocus, View newFocus) {}
    public void onChildViewAdded(View parent, View child) {}
    public void onChildViewRemoved(View p, View child) {}
    @Override public CharSequence getAccessibilityClassName() { return WebView.class.getName(); }

    // ---- nested API types
    public interface FindListener { void onFindResultReceived(int activeMatchOrdinal, int numberOfMatches, boolean isDoneCounting); }
    @Deprecated public interface PictureListener { @Deprecated void onNewPicture(WebView view, Picture picture); }
    public static abstract class VisualStateCallback { public abstract void onComplete(long requestId); }
    public class WebViewTransport {
        private WebView mWebview;
        public synchronized void setWebView(WebView webview) { mWebview = webview; }
        public synchronized WebView getWebView() { return mWebview; }
    }
    public static class HitTestResult {
        public static final int UNKNOWN_TYPE = 0;
        @Deprecated public static final int ANCHOR_TYPE = 1;
        public static final int PHONE_TYPE = 2;
        public static final int GEO_TYPE = 3;
        public static final int EMAIL_TYPE = 4;
        public static final int IMAGE_TYPE = 5;
        @Deprecated public static final int IMAGE_ANCHOR_TYPE = 6;
        public static final int SRC_ANCHOR_TYPE = 7;
        public static final int SRC_IMAGE_ANCHOR_TYPE = 8;
        public static final int EDIT_TEXT_TYPE = 9;
        private int mType = UNKNOWN_TYPE;
        private String mExtra;
        public HitTestResult() {}
        public void setType(int type) { mType = type; }
        public void setExtra(String extra) { mExtra = extra; }
        public int getType() { return mType; }
        public String getExtra() { return mExtra; }
    }
}
