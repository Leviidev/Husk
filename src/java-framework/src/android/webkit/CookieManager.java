package android.webkit;

import java.util.LinkedHashMap;
import java.util.Map;

/** Cookies for the app's WebViews: kept per host, name by name. */
public abstract class CookieManager {
    private static CookieManager sInstance;
    private static boolean sFileSchemeCookies;
    public CookieManager() {}
    public static synchronized CookieManager getInstance() { if (sInstance == null) sInstance = new Husk(); return sInstance; }
    public static boolean allowFileSchemeCookies() { return sFileSchemeCookies; }
    public static void setAcceptFileSchemeCookies(boolean accept) { sFileSchemeCookies = accept; }
    @Override protected Object clone() throws CloneNotSupportedException { throw new CloneNotSupportedException("doesn't implement Cloneable"); }
    public abstract void setAcceptCookie(boolean accept);
    public abstract boolean acceptCookie();
    public abstract void setAcceptThirdPartyCookies(WebView webview, boolean accept);
    public abstract boolean acceptThirdPartyCookies(WebView webview);
    public abstract void setCookie(String url, String value);
    public abstract void setCookie(String url, String value, ValueCallback<Boolean> callback);
    public abstract String getCookie(String url);
    public String getCookie(String url, boolean privateBrowsing) { return getCookie(url); }
    public synchronized String getCookie(android.net.WebAddress uri) { return getCookie(uri.toString()); }
    @Deprecated public abstract void removeSessionCookie();
    public abstract void removeSessionCookies(ValueCallback<Boolean> callback);
    @Deprecated public abstract void removeAllCookie();
    public abstract void removeAllCookies(ValueCallback<Boolean> callback);
    public abstract boolean hasCookies();
    public boolean hasCookies(boolean privateBrowsing) { return hasCookies(); }
    @Deprecated public abstract void removeExpiredCookie();
    public abstract void flush();
    protected boolean allowFileSchemeCookiesImpl() { return sFileSchemeCookies; }
    protected void setAcceptFileSchemeCookiesImpl(boolean accept) { sFileSchemeCookies = accept; }

    static final class Husk extends CookieManager {
        private final Map<String, LinkedHashMap<String, String>> mJar = new java.util.HashMap<>();
        private boolean mAccept = true;
        private static String host(String url) {
            try { String h = android.net.Uri.parse(url).getHost(); return h == null ? "" : h.toLowerCase(java.util.Locale.ROOT); } catch (RuntimeException e) { return ""; }
        }
        public synchronized void setAcceptCookie(boolean accept) { mAccept = accept; }
        public synchronized boolean acceptCookie() { return mAccept; }
        public void setAcceptThirdPartyCookies(WebView w, boolean accept) {}
        public boolean acceptThirdPartyCookies(WebView w) { return false; }
        public void setCookie(String url, String value) { setCookie(url, value, null); }
        public void setCookie(String url, String value, ValueCallback<Boolean> cb) {
            boolean ok = false;
            synchronized (this) {
                if (value != null) {
                    String nv = value.split(";", 2)[0];
                    int eq = nv.indexOf('=');
                    String name = (eq < 0 ? "" : nv.substring(0, eq)).trim(), v = (eq < 0 ? nv : nv.substring(eq + 1)).trim();
                    String h = host(url);
                    for (String attr : value.split(";")) { String a = attr.trim(); if (a.regionMatches(true, 0, "domain=", 0, 7)) h = a.substring(7).replaceFirst("^\\.", "").toLowerCase(java.util.Locale.ROOT); }
                    mJar.computeIfAbsent(h, k -> new LinkedHashMap<>()).put(name, v);
                    ok = true;
                }
            }
            if (cb != null) cb.onReceiveValue(ok);
        }
        public synchronized String getCookie(String url) {
            String h = host(url);
            StringBuilder sb = new StringBuilder();
            for (Map.Entry<String, LinkedHashMap<String, String>> e : mJar.entrySet()) {
                if (!h.equals(e.getKey()) && !h.endsWith("." + e.getKey())) continue;
                for (Map.Entry<String, String> c : e.getValue().entrySet()) {
                    if (sb.length() > 0) sb.append("; ");
                    sb.append(c.getKey().isEmpty() ? c.getValue() : c.getKey() + "=" + c.getValue());
                }
            }
            return sb.length() == 0 ? null : sb.toString();
        }
        public synchronized void removeSessionCookie() {}
        public void removeSessionCookies(ValueCallback<Boolean> cb) { if (cb != null) cb.onReceiveValue(false); }
        public synchronized void removeAllCookie() { mJar.clear(); }
        public void removeAllCookies(ValueCallback<Boolean> cb) { boolean had; synchronized (this) { had = !mJar.isEmpty(); mJar.clear(); } if (cb != null) cb.onReceiveValue(had); }
        public synchronized boolean hasCookies() { return !mJar.isEmpty(); }
        public void removeExpiredCookie() {}
        public void flush() {}
    }
}
