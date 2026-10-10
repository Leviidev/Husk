package android.webkit;

import java.util.HashMap;
import java.util.Locale;

/** File extensions to MIME types and back: the common types an app's web content and downloads use. */
public class MimeTypeMap {
    private static final MimeTypeMap sMimeTypeMap = new MimeTypeMap();
    private static final HashMap<String, String> EXT = new HashMap<>(), MIME = new HashMap<>();
    private static void add(String mime, String... exts) {
        for (String e : exts) { if (!EXT.containsKey(e)) EXT.put(e, mime); }
        if (!MIME.containsKey(mime)) MIME.put(mime, exts[0]);
    }
    static {
        add("text/html", "html", "htm", "shtml"); add("text/css", "css"); add("text/javascript", "js", "mjs");
        add("application/javascript", "js"); add("application/json", "json", "map"); add("text/plain", "txt", "text", "log", "ini", "conf");
        add("text/xml", "xml"); add("application/xml", "xml"); add("application/xhtml+xml", "xhtml"); add("text/csv", "csv"); add("text/markdown", "md");
        add("image/png", "png"); add("image/jpeg", "jpg", "jpeg", "jpe"); add("image/gif", "gif"); add("image/webp", "webp"); add("image/svg+xml", "svg", "svgz");
        add("image/bmp", "bmp"); add("image/x-icon", "ico"); add("image/vnd.microsoft.icon", "ico"); add("image/heic", "heic"); add("image/avif", "avif");
        add("audio/mpeg", "mp3"); add("audio/ogg", "ogg", "oga"); add("audio/wav", "wav"); add("audio/x-wav", "wav"); add("audio/aac", "aac"); add("audio/mp4", "m4a");
        add("audio/flac", "flac"); add("audio/midi", "mid", "midi"); add("audio/webm", "weba"); add("audio/opus", "opus");
        add("video/mp4", "mp4", "m4v"); add("video/webm", "webm"); add("video/ogg", "ogv"); add("video/quicktime", "mov"); add("video/3gpp", "3gp"); add("video/x-matroska", "mkv");
        add("font/woff", "woff"); add("font/woff2", "woff2"); add("font/ttf", "ttf"); add("font/otf", "otf"); add("application/vnd.ms-fontobject", "eot");
        add("application/wasm", "wasm"); add("application/pdf", "pdf"); add("application/zip", "zip"); add("application/gzip", "gz");
        add("application/vnd.android.package-archive", "apk"); add("application/octet-stream", "bin");
        add("application/msword", "doc"); add("application/vnd.openxmlformats-officedocument.wordprocessingml.document", "docx");
        add("application/vnd.ms-excel", "xls"); add("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "xlsx");
        add("application/vnd.ms-powerpoint", "ppt"); add("application/vnd.openxmlformats-officedocument.presentationml.presentation", "pptx");
        add("application/rtf", "rtf"); add("application/epub+zip", "epub"); add("application/x-tar", "tar"); add("application/x-7z-compressed", "7z");
        add("application/manifest+json", "webmanifest"); add("text/calendar", "ics"); add("text/vcard", "vcf");
    }
    private MimeTypeMap() {}
    /** As the platform's mime module does at boot: libcore's MimeMap (URLConnection.guessContentTypeFromName and friends) knows these. */
    public static void huskInstallDefault() {
        try {
            final libcore.content.type.MimeMap.Builder b = libcore.content.type.MimeMap.builder();
            HashMap<String, java.util.ArrayList<String>> byMime = new HashMap<>();
            for (java.util.Map.Entry<String, String> e : EXT.entrySet()) byMime.computeIfAbsent(e.getValue(), k -> new java.util.ArrayList<>()).add(e.getKey());
            for (java.util.Map.Entry<String, String> e : MIME.entrySet()) {
                // the preferred extension first, so guessExtensionFromMimeType gives it
                java.util.ArrayList<String> exts = byMime.computeIfAbsent(e.getKey(), k -> new java.util.ArrayList<>());
                exts.remove(e.getValue()); exts.add(0, e.getValue());
            }
            for (java.util.Map.Entry<String, java.util.ArrayList<String>> e : byMime.entrySet()) b.addMimeMapping("?" + e.getKey(), prefixed(e.getValue()));
            final libcore.content.type.MimeMap map = b.build();
            libcore.content.type.MimeMap.setDefaultSupplier(() -> map);
        } catch (Throwable t) {
            android.util.Log.w("MimeTypeMap", "could not install the MIME types: " + t);
        }
    }
    private static java.util.List<String> prefixed(java.util.List<String> exts) {
        java.util.ArrayList<String> out = new java.util.ArrayList<>();
        for (String e : exts) out.add("?" + e);          // "?": keep an existing mapping (the first wins)
        return out;
    }
    public static MimeTypeMap getSingleton() { return sMimeTypeMap; }
    public static String getFileExtensionFromUrl(String url) {
        if (url == null || url.isEmpty()) return "";
        int i = url.lastIndexOf('#'); if (i > 0) url = url.substring(0, i);
        i = url.lastIndexOf('?'); if (i > 0) url = url.substring(0, i);
        i = url.lastIndexOf('/'); String name = i >= 0 ? url.substring(i + 1) : url;
        if (!name.isEmpty() && java.util.regex.Pattern.matches("[a-zA-Z_0-9\\.\\-\\(\\)\\%]+", name)) {
            int dot = name.lastIndexOf('.');
            if (dot >= 0) return name.substring(dot + 1);
        }
        return "";
    }
    public boolean hasMimeType(String mimeType) { return mimeType != null && MIME.containsKey(mimeType.toLowerCase(Locale.ROOT)); }
    public String getMimeTypeFromExtension(String extension) { return extension == null ? null : EXT.get(extension.toLowerCase(Locale.ROOT)); }
    public boolean hasExtension(String extension) { return extension != null && EXT.containsKey(extension.toLowerCase(Locale.ROOT)); }
    public String getExtensionFromMimeType(String mimeType) { return mimeType == null ? null : MIME.get(mimeType.toLowerCase(Locale.ROOT)); }
}
