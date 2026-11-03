package android.webkit;

import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import java.lang.reflect.Array;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.util.concurrent.Callable;

/**
 * The WebView end of androidx.webkit (WebViewCompat, WebViewFeature). androidx talks to a WebView's "support library glue" only
 * through InvocationHandlers standing for its boundary interfaces (org.chromium.support_lib_boundary.*, which the app carries); a
 * handler here answers by method name. Features it does not list stay unsupported, so androidx never calls into them; anything else
 * asked answers its return type's zero.
 */
public final class HuskSupportLib {
    private static final String TAG = "WebView";
    private static final String BOUNDARY = "org.chromium.support_lib_boundary.";
    private static final String[] FEATURES = { "WEB_MESSAGE_LISTENER", "DOCUMENT_START_SCRIPT", "DOCUMENT_START_SCRIPT:1", "MULTI_PROCESS",
        "GET_WEB_VIEW_CLIENT", "GET_WEB_CHROME_CLIENT",
        /* WebSettingsCompat: kept per WebSettings and read back (settings()); the page is not darkened or pre-rastered for them */
        "OFF_SCREEN_PRERASTER", "SAFE_BROWSING_ENABLE", "DISABLED_ACTION_MODE_MENU_ITEMS", "FORCE_DARK", "FORCE_DARK_BEHAVIOR",
        "ALGORITHMIC_DARKENING", "REQUESTED_WITH_HEADER_ALLOW_LIST", "ENTERPRISE_AUTHENTICATION_APP_LINK_POLICY", "ATTRIBUTION_BEHAVIOR" };
    private static final Object NONE = new Object();
    private static final Handler sMain = new Handler(Looper.getMainLooper());
    private static final boolean TRACE = System.getenv("TL_WEB_CONSOLE") != null;     /* listener traffic in the log too */
    private static String clip(String s) { return s != null && s.length() > 300 ? s.substring(0, 300) + "..." : s; }

    /** A boundary interface's implementation: call() answers the methods it knows, NONE for the rest. */
    private abstract static class Boundary implements InvocationHandler {
        @Override public final Object invoke(Object proxy, Method m, Object[] a) throws Throwable {
            String n = m.getName();
            if (a == null) a = new Object[0];
            if (n.equals("hashCode") && a.length == 0) return System.identityHashCode(this);
            if (n.equals("equals") && a.length == 1) return a[0] == proxy;
            if (n.equals("toString") && a.length == 0) return getClass().getName();
            Object r = call(n, a);
            return r != NONE ? r : zero(m.getReturnType());
        }
        abstract Object call(String n, Object[] a) throws Throwable;
    }
    private static final InvocationHandler EMPTY = new Boundary() { Object call(String n, Object[] a) { return NONE; } };

    private static Object zero(Class<?> t) {
        if (t == InvocationHandler.class) return EMPTY;
        if (!t.isPrimitive()) return t.isArray() ? Array.newInstance(t.getComponentType(), 0) : null;
        if (t == boolean.class) return Boolean.FALSE;
        if (t == int.class) return 0;
        if (t == long.class) return 0L;
        if (t == float.class) return 0f;
        if (t == double.class) return 0d;
        if (t == short.class) return (short) 0;
        if (t == byte.class) return (byte) 0;
        if (t == char.class) return (char) 0;
        return null;                                            /* void */
    }

    public static InvocationHandler factory() {
        return new Boundary() {
            Object call(String n, Object[] a) {
                switch (n) {
                case "getSupportedFeatures": return FEATURES.clone();
                case "createWebView": return provider((WebView) a[0]);
                case "getStatics": return STATICS;
                case "getWebkitToCompatConverter": return CONVERTER;
                }
                return NONE;
            }
        };
    }


    /* WebSettings -> its WebSettingsBoundaryInterface: setX stores, getX/isX reads back (AOSP's defaults before a set) */
    private static final java.util.Map<WebSettings, InvocationHandler> sSettings = new java.util.WeakHashMap<>();
    private static final InvocationHandler CONVERTER = new Boundary() {
        Object call(String n, Object[] a) {
            if (n.equals("convertSettings") && a.length == 1 && a[0] instanceof WebSettings) {
                synchronized (sSettings) { InvocationHandler h = sSettings.get(a[0]); if (h == null) sSettings.put((WebSettings) a[0], h = settings()); return h; }
            }
            return NONE;
        }
    };
    private static InvocationHandler settings() {
        final java.util.HashMap<String, Object> v = new java.util.HashMap<>();
        v.put("SafeBrowsingEnabled", Boolean.TRUE); v.put("ForceDark", 1); v.put("ForceDarkBehavior", 2);
        v.put("EnterpriseAuthenticationAppLinkPolicyEnabled", Boolean.TRUE); v.put("AttributionBehavior", 1);
        return (proxy, m, a) -> {
            String n = m.getName();
            if (a == null) a = new Object[0];
            if (n.equals("hashCode") && a.length == 0) return System.identityHashCode(v);
            if (n.equals("equals") && a.length == 1) return a[0] == proxy;
            if (n.equals("toString") && a.length == 0) return "HuskWebSettingsCompat";
            synchronized (v) {
                if (n.startsWith("set") && a.length == 1) { v.put(n.substring(3), a[0]); return null; }
                String k = n.startsWith("get") ? n.substring(3) : n.startsWith("is") ? n.substring(2) : null;
                Class<?> t = m.getReturnType();
                if (k != null && a.length == 0) {
                    Object r = v.get(k);
                    if (r != null) return r;
                    if (java.util.Set.class.isAssignableFrom(t)) return new java.util.HashSet<>();
                    if (java.util.Map.class.isAssignableFrom(t)) return new java.util.HashMap<>();
                }
                return zero(t);
            }
        };
    }

    private static final InvocationHandler STATICS = new Boundary() {
        Object call(String n, Object[] a) {
            switch (n) {
            case "isMultiProcessEnabled": return false;
            case "getVariationsHeader": return "";
            case "getSafeBrowsingPrivacyPolicyUrl": return Uri.parse("https://www.google.com/chrome/privacy/");
            case "initSafeBrowsing": answer(a.length > 1 ? a[1] : null, Boolean.FALSE); return null;
            case "setSafeBrowsingAllowlist": case "setSafeBrowsingWhitelist": answer(a.length > 1 ? a[1] : null, Boolean.TRUE); return null;
            }
            return NONE;
        }
    };
    @SuppressWarnings("unchecked")
    private static void answer(Object callback, Object value) {
        if (callback instanceof ValueCallback) sMain.post(() -> ((ValueCallback<Object>) callback).onReceiveValue(value));
    }

    private static InvocationHandler provider(final WebView w) {
        return new Boundary() {
            Object call(String n, Object[] a) {
                switch (n) {
                case "addWebMessageListener": w.huskAddWebMessageListener((String) a[0], (String[]) a[1], (InvocationHandler) a[2]); return null;
                case "removeWebMessageListener": w.huskRemoveWebMessageListener((String) a[0]); return null;
                case "addDocumentStartJavaScript": {
                    final Runnable remove = w.huskAddDocumentStartScript((String) a[0], (String[]) a[1]);
                    return new Boundary() { Object call(String m, Object[] b) { if (m.equals("remove")) { remove.run(); return null; } return NONE; } };
                }
                case "getWebViewClient": return w.getWebViewClient();
                case "getWebChromeClient": return w.getWebChromeClient();
                }
                return NONE;
            }
        };
    }

    /** A page's postMessage, to the app's WebMessageListener (on the main thread). */
    static void deliver(WebView w, final String name, InvocationHandler listener, final String data, Uri origin, boolean mainFrame) {
        if (TRACE) Log.d(TAG, "listener " + name + " page -> app (" + origin + "): " + clip(data));
        InvocationHandler message = new Boundary() {
            Object call(String n, Object[] a) {
                switch (n) {
                case "getData": return data;
                case "getMessagePayload": return payload(data);
                case "getPorts": return new InvocationHandler[0];
                case "getSupportedFeatures": return new String[0];
                }
                return NONE;
            }
        };
        InvocationHandler reply = new Boundary() {
            private Object mPeer;
            Object call(String n, Object[] a) throws Exception {
                switch (n) {
                case "getOrCreatePeer": synchronized (this) { if (mPeer == null) mPeer = ((Callable<?>) a[0]).call(); return mPeer; }
                case "postMessage": if (TRACE) Log.d(TAG, "listener " + name + " app -> page: " + clip((String) a[0])); w.huskReplyMessage(name, (String) a[0]); return null;
                case "postMessageWithPayload": {
                    Object s = callBack((InvocationHandler) a[0], "WebMessagePayloadBoundaryInterface", "getAsString", new Class<?>[0]);
                    w.huskReplyMessage(name, s instanceof String ? (String) s : null);
                    return null;
                }
                }
                return NONE;
            }
        };
        callBack(listener, "WebMessageListenerBoundaryInterface", "onPostMessage",
                 new Class<?>[] { WebView.class, InvocationHandler.class, Uri.class, boolean.class, InvocationHandler.class },
                 w, message, origin, mainFrame, reply);
    }
    private static InvocationHandler payload(final String s) {
        return new Boundary() {
            Object call(String n, Object[] a) {
                switch (n) {
                case "getType": return 0;                       /* a string */
                case "getAsString": return s;
                case "getSupportedFeatures": return new String[0];
                }
                return NONE;
            }
        };
    }

    /** A method of one of the app's boundary interfaces, called on the handler androidx made for it. */
    private static Object callBack(InvocationHandler h, String iface, String method, Class<?>[] types, Object... args) {
        if (h == null) return null;
        try {
            Class<?> c = Class.forName(BOUNDARY + iface, false, h.getClass().getClassLoader());
            return h.invoke(null, c.getMethod(method, types), args);
        } catch (Throwable t) {
            Log.e(TAG, "androidx.webkit: " + iface + "." + method + " failed", t);
            return null;
        }
    }

    /** Whether an origin is one of a listener's or script's rules: "*", or scheme://host[:port] with an optional "*." subdomain wildcard. */
    static boolean originAllowed(String[] rules, Uri origin) {
        if (rules == null) return false;
        String scheme = origin.getScheme(), host = origin.getHost();
        int port = origin.getPort();
        if (port < 0) port = "https".equalsIgnoreCase(scheme) ? 443 : "http".equalsIgnoreCase(scheme) ? 80 : -1;
        for (String r : rules) {
            if (r == null) continue;
            if (r.equals("*")) return true;
            Uri u = Uri.parse(r);
            if (u.getScheme() == null || scheme == null || !u.getScheme().equalsIgnoreCase(scheme)) continue;
            String rh = u.getHost();
            if (rh == null || host == null) { if (rh == null && host == null) return true; continue; }
            int rp = u.getPort() >= 0 ? u.getPort() : "https".equalsIgnoreCase(u.getScheme()) ? 443 : "http".equalsIgnoreCase(u.getScheme()) ? 80 : -1;
            if (rp != port) continue;
            rh = rh.toLowerCase(); String h = host.toLowerCase();
            if (rh.equals(h) || (rh.startsWith("*.") && h.endsWith(rh.substring(1)))) return true;
        }
        return false;
    }
}
