package android.webkit;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public abstract class WebViewDatabase {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    protected static final java.lang.String LOGTAG = "webviewdatabase";
    public WebViewDatabase() {}
    public static android.webkit.WebViewDatabase getInstance(android.content.Context p0) { synchronized (WebViewDatabase.class) { if (sInstance == null) sInstance = new Husk(); return sInstance; } }
    public abstract void clearFormData();
    public abstract void clearHttpAuthUsernamePassword();
    public abstract void clearUsernamePassword();
    public abstract java.lang.String[] getHttpAuthUsernamePassword(java.lang.String p0, java.lang.String p1);
    public abstract boolean hasFormData();
    public abstract boolean hasHttpAuthUsernamePassword();
    public abstract boolean hasUsernamePassword();
    public abstract void setHttpAuthUsernamePassword(java.lang.String p0, java.lang.String p1, java.lang.String p2, java.lang.String p3);
    static final class Husk extends WebViewDatabase {
        public void clearFormData() {}
        public void clearHttpAuthUsernamePassword() {}
        public void clearUsernamePassword() {}
        public java.lang.String[] getHttpAuthUsernamePassword(java.lang.String p0, java.lang.String p1) { return null; }
        public boolean hasFormData() { return false; }
        public boolean hasHttpAuthUsernamePassword() { return false; }
        public boolean hasUsernamePassword() { return false; }
        public void setHttpAuthUsernamePassword(java.lang.String p0, java.lang.String p1, java.lang.String p2, java.lang.String p3) {}
    }
    private static WebViewDatabase sInstance;
}
