package husk;

/**
 * WKWebView under the app's screen, for android.webkit.WebView. Calls queue onto the host's main thread; what the page does comes
 * back as events (input phase 9), "kind \u001f id \u001f fields", which deliver() hands to the WebView with that id. Waiters on the
 * host (navigation decisions, dialogs, bridged calls, the app's own resources) are answered with reply / respond by call id.
 */
public final class Web {
    private Web() {}
    public static native void create(int id);
    public static native void destroy(int id);
    public static native void frame(int id, int x, int y, int w, int h, boolean visible, boolean touchable);   // screen pixels
    public static native void load(int id, String url, String[] headers, byte[] postBody);
    public static native void loadData(int id, byte[] data, String mime, String encoding, String baseUrl);
    public static native void eval(int id, String js, int callback);                        // callback 0: no result wanted
    public static native void userScript(int id, String js);                                // in every page from its start, and now
    public static native void settings(int id, boolean javaScript, String userAgent, int textZoom, boolean mediaNeedsGesture);
    public static native void go(int id, int cmd);                                          // 0 back, 1 forward, 2 reload, 3 stop
    public static native void background(int id, int argb);
    public static native void reply(int call, String result);
    /** status 0: not the app's, the host fetches reason (the real URL); below 0: failed */
    public static native void respond(int call, int status, String reason, String mime, String encoding, String[] headers, byte[] body);
    public static native String[] poll();

    public static void deliver() {
        String[] ev = poll();
        if (ev == null) return;
        for (String e : ev) {
            String[] f = e.split("\u001f", -1);
            if (f.length < 2) continue;
            int id;
            try { id = Integer.parseInt(f[1]); } catch (NumberFormatException x) { continue; }
            android.webkit.WebView w = android.webkit.WebView.huskById(id);
            try {
                if (w != null) w.huskEvent(f);
                else orphan(f);
            } catch (RuntimeException x) {
                android.util.Log.e("WebView", "event " + f[0] + " threw", x);
                orphan(f);
            }
        }
    }
    /** An event for a WebView that is gone: let whatever waits on the host go. */
    private static void orphan(String[] f) {
        if (f.length < 3) return;
        int call;
        try { call = Integer.parseInt(f[2]); } catch (NumberFormatException x) { return; }
        switch (f[0]) {
        case "o": reply(call, "\u0001nav-allow"); break;
        case "A": reply(call, "\u0001alert"); break;
        case "C": reply(call, "\u0001no"); break;
        case "P": case "j": reply(call, null); break;
        case "r": respond(call, -1, null, null, null, null, null); break;
        }
    }
}
