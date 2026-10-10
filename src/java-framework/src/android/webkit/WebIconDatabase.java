package android.webkit;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public abstract class WebIconDatabase {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public WebIconDatabase() {}
    public static android.webkit.WebIconDatabase getInstance() { synchronized (WebIconDatabase.class) { if (sInstance == null) sInstance = new Husk(); return sInstance; } }
    public abstract void bulkRequestIconForPageUrl(android.content.ContentResolver p0, java.lang.String p1, android.webkit.WebIconDatabase.IconListener p2);
    public abstract void close();
    public abstract void open(java.lang.String p0);
    public abstract void releaseIconForPageUrl(java.lang.String p0);
    public abstract void removeAllIcons();
    public abstract void requestIconForPageUrl(java.lang.String p0, android.webkit.WebIconDatabase.IconListener p1);
    public abstract void retainIconForPageUrl(java.lang.String p0);
    public interface IconListener {
        void onReceivedIcon(java.lang.String p0, android.graphics.Bitmap p1);
    }
    static final class Husk extends WebIconDatabase {
        public void bulkRequestIconForPageUrl(android.content.ContentResolver p0, java.lang.String p1, android.webkit.WebIconDatabase.IconListener p2) {}
        public void close() {}
        public void open(java.lang.String p0) {}
        public void releaseIconForPageUrl(java.lang.String p0) {}
        public void removeAllIcons() {}
        public void requestIconForPageUrl(java.lang.String p0, android.webkit.WebIconDatabase.IconListener p1) {}
        public void retainIconForPageUrl(java.lang.String p0) {}
    }
    private static WebIconDatabase sInstance;
}
