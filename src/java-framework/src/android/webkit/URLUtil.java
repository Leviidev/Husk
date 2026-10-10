package android.webkit;

import java.util.Locale;

public final class URLUtil {
    static final String ASSET_BASE = "file:///android_asset/";
    static final String RESOURCE_BASE = "file:///android_res/";
    static final String FILE_BASE = "file:";
    static final String PROXY_BASE = "file:///cookieless_proxy/";
    static final String CONTENT_BASE = "content:";
    public URLUtil() {}
    private static boolean starts(String url, String p) { return url != null && url.length() >= p.length() && url.regionMatches(true, 0, p, 0, p.length()); }
    public static String guessUrl(String inUrl) {
        if (inUrl == null) return null;
        String s = inUrl.trim();
        if (s.isEmpty() || s.startsWith("javascript:")) return s;
        if (s.endsWith(".") && s.indexOf(' ') < 0) s = s.substring(0, s.length() - 1);
        if (s.contains("://") || starts(s, "about:") || starts(s, "data:") || starts(s, "file:") || starts(s, "content:")) return s;
        if (s.indexOf('.') < 0 && !s.startsWith("localhost")) return s;
        return "http://" + s;
    }
    public static String composeSearchUrl(String inQuery, String template, String queryPlaceHolder) {
        int i = template.indexOf(queryPlaceHolder);
        if (i < 0) return null;
        try { return template.substring(0, i) + java.net.URLEncoder.encode(inQuery, "utf-8") + template.substring(i + queryPlaceHolder.length()); }
        catch (java.io.UnsupportedEncodingException e) { return null; }
    }
    public static byte[] decode(byte[] url) throws IllegalArgumentException {
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream(url.length);
        for (int i = 0; i < url.length; i++) {
            byte b = url[i];
            if (b == '%') {
                if (url.length - i <= 2) throw new IllegalArgumentException("Invalid format");
                b = (byte) (Character.digit(url[i + 1], 16) * 16 + Character.digit(url[i + 2], 16));
                i += 2;
            }
            out.write(b);
        }
        return out.toByteArray();
    }
    public static boolean isAssetUrl(String url) { return url != null && url.startsWith(ASSET_BASE); }
    public static boolean isResourceUrl(String url) { return url != null && url.startsWith(RESOURCE_BASE); }
    @Deprecated public static boolean isCookielessProxyUrl(String url) { return url != null && url.startsWith(PROXY_BASE); }
    public static boolean isFileUrl(String url) { return url != null && url.startsWith(FILE_BASE) && !url.startsWith(ASSET_BASE) && !url.startsWith(PROXY_BASE); }
    public static boolean isAboutUrl(String url) { return starts(url, "about:"); }
    public static boolean isDataUrl(String url) { return starts(url, "data:"); }
    public static boolean isJavaScriptUrl(String url) { return starts(url, "javascript:"); }
    public static boolean isHttpUrl(String url) { return starts(url, "http://"); }
    public static boolean isHttpsUrl(String url) { return starts(url, "https://"); }
    public static boolean isNetworkUrl(String url) { return url != null && !url.isEmpty() && (isHttpUrl(url) || isHttpsUrl(url)); }
    public static boolean isContentUrl(String url) { return starts(url, CONTENT_BASE); }
    public static boolean isValidUrl(String url) {
        return url != null && !url.isEmpty() && (isAssetUrl(url) || isResourceUrl(url) || isFileUrl(url) || isAboutUrl(url) || isHttpUrl(url) || isHttpsUrl(url) || isJavaScriptUrl(url) || isContentUrl(url));
    }
    public static String stripAnchor(String url) { int i = url.indexOf('#'); return i != -1 ? url.substring(0, i) : url; }
    public static String guessFileName(String url, String contentDisposition, String mimeType) {
        String name = null, ext = null;
        if (contentDisposition != null) {
            java.util.regex.Matcher m = java.util.regex.Pattern.compile("attachment;\\s*filename\\s*=\\s*(\"?)([^\"]*)\\1\\s*$", java.util.regex.Pattern.CASE_INSENSITIVE).matcher(contentDisposition);
            if (m.find()) { name = m.group(2); int s = name.lastIndexOf('/') + 1; if (s > 0) name = name.substring(s); }
        }
        if (name == null) {
            String decoded = android.net.Uri.decode(url);
            if (decoded != null) {
                int q = decoded.indexOf('?'); if (q > 0) decoded = decoded.substring(0, q);
                if (!decoded.endsWith("/")) { int i = decoded.lastIndexOf('/') + 1; if (i > 0) name = decoded.substring(i); }
            }
        }
        if (name == null || name.isEmpty()) name = "downloadfile";
        int dot = name.indexOf('.');
        if (dot < 0) {
            if (mimeType != null) { ext = MimeTypeMap.getSingleton().getExtensionFromMimeType(mimeType); if (ext != null) ext = "." + ext; }
            if (ext == null) ext = mimeType != null && mimeType.toLowerCase(Locale.ROOT).startsWith("text/") ? (mimeType.equalsIgnoreCase("text/html") ? ".htm" : ".txt") : ".bin";
        } else {
            if (mimeType != null) {
                String typeFromExt = MimeTypeMap.getSingleton().getMimeTypeFromExtension(name.substring(name.lastIndexOf('.') + 1));
                if (typeFromExt != null && !typeFromExt.equalsIgnoreCase(mimeType)) { ext = MimeTypeMap.getSingleton().getExtensionFromMimeType(mimeType); if (ext != null) ext = "." + ext; }
            }
            if (ext == null) ext = name.substring(dot);
            name = name.substring(0, dot);
        }
        return name + ext;
    }
}
