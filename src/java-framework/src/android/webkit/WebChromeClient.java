package android.webkit;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class WebChromeClient {
    public WebChromeClient() {}
    public android.graphics.Bitmap getDefaultVideoPoster() { return null; }
    public android.view.View getVideoLoadingProgressView() { return null; }
    public void getVisitedHistory(android.webkit.ValueCallback p0) {}
    public void onCloseWindow(android.webkit.WebView p0) {}
    public void onConsoleMessage(java.lang.String p0, int p1, java.lang.String p2) {}
    public boolean onConsoleMessage(android.webkit.ConsoleMessage p0) { onConsoleMessage(p0.message(), p0.lineNumber(), p0.sourceId()); return false; }
    public boolean onCreateWindow(android.webkit.WebView p0, boolean p1, boolean p2, android.os.Message p3) { return false; }
    public void onExceededDatabaseQuota(java.lang.String p0, java.lang.String p1, long p2, long p3, long p4, android.webkit.WebStorage.QuotaUpdater p5) {}
    public void onGeolocationPermissionsHidePrompt() {}
    public void onGeolocationPermissionsShowPrompt(java.lang.String p0, android.webkit.GeolocationPermissions.Callback p1) {}
    public void onHideCustomView() {}
    public boolean onJsAlert(android.webkit.WebView p0, java.lang.String p1, java.lang.String p2, android.webkit.JsResult p3) { return false; }
    public boolean onJsBeforeUnload(android.webkit.WebView p0, java.lang.String p1, java.lang.String p2, android.webkit.JsResult p3) { return false; }
    public boolean onJsConfirm(android.webkit.WebView p0, java.lang.String p1, java.lang.String p2, android.webkit.JsResult p3) { return false; }
    public boolean onJsPrompt(android.webkit.WebView p0, java.lang.String p1, java.lang.String p2, java.lang.String p3, android.webkit.JsPromptResult p4) { return false; }
    public boolean onJsTimeout() { return false; }
    public void onPermissionRequest(android.webkit.PermissionRequest p0) {}
    public void onPermissionRequestCanceled(android.webkit.PermissionRequest p0) {}
    public void onProgressChanged(android.webkit.WebView p0, int p1) {}
    public void onReachedMaxAppCacheSize(long p0, long p1, android.webkit.WebStorage.QuotaUpdater p2) {}
    public void onReceivedIcon(android.webkit.WebView p0, android.graphics.Bitmap p1) {}
    public void onReceivedTitle(android.webkit.WebView p0, java.lang.String p1) {}
    public void onReceivedTouchIconUrl(android.webkit.WebView p0, java.lang.String p1, boolean p2) {}
    public void onRequestFocus(android.webkit.WebView p0) {}
    public void onShowCustomView(android.view.View p0, int p1, android.webkit.WebChromeClient.CustomViewCallback p2) {}
    public void onShowCustomView(android.view.View p0, android.webkit.WebChromeClient.CustomViewCallback p1) {}
    public boolean onShowFileChooser(android.webkit.WebView p0, android.webkit.ValueCallback p1, android.webkit.WebChromeClient.FileChooserParams p2) { return false; }
    public void openFileChooser(android.webkit.ValueCallback p0, java.lang.String p1, java.lang.String p2) {}
    public interface CustomViewCallback {
        void onCustomViewHidden();
    }
    public static abstract class FileChooserParams {
        public static final long ENABLE_FILE_SYSTEM_ACCESS = 364980165L;
        public static final int MODE_OPEN = 0;
        public static final int MODE_OPEN_FOLDER = 2;
        public static final int MODE_OPEN_MULTIPLE = 1;
        public static final int MODE_SAVE = 3;
        public static final int PERMISSION_MODE_READ = 0;
        public static final int PERMISSION_MODE_READ_WRITE = 1;
        public FileChooserParams() {}
        public static android.net.Uri[] parseResult(int resultCode, android.content.Intent intent) {
            if (resultCode != android.app.Activity.RESULT_OK || intent == null) return null;
            if (intent.getClipData() != null) {
                android.content.ClipData c = intent.getClipData();
                android.net.Uri[] out = new android.net.Uri[c.getItemCount()];
                for (int i = 0; i < out.length; i++) out[i] = c.getItemAt(i).getUri();
                return out;
            }
            return intent.getData() != null ? new android.net.Uri[] { intent.getData() } : null;
        }
        public abstract android.content.Intent createIntent();
        public abstract java.lang.String[] getAcceptTypes();
        public abstract java.lang.String getFilenameHint();
        public abstract int getMode();
        public int getPermissionMode() { return PERMISSION_MODE_READ; }
        public abstract java.lang.CharSequence getTitle();
        public abstract boolean isCaptureEnabled();
    }
}
