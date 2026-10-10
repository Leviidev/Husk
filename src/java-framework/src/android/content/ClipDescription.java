package android.content;
public class ClipDescription implements android.os.Parcelable {
    public static final String MIMETYPE_TEXT_PLAIN = "text/plain", MIMETYPE_TEXT_HTML = "text/html", MIMETYPE_TEXT_URILIST = "text/uri-list", MIMETYPE_TEXT_INTENT = "text/vnd.android.intent", MIMETYPE_UNKNOWN = "application/octet-stream", EXTRA_IS_SENSITIVE = "android.content.extra.IS_SENSITIVE";
    private final CharSequence mLabel; private final String[] mTypes; private android.os.PersistableBundle mExtras;
    public ClipDescription(CharSequence label, String[] types) { mLabel = label; mTypes = types; }
    public ClipDescription(ClipDescription o) { mLabel = o.mLabel; mTypes = o.mTypes; }
    public CharSequence getLabel() { return mLabel; } public int getMimeTypeCount() { return mTypes.length; } public String getMimeType(int i) { return mTypes[i]; }
    public boolean hasMimeType(String t) { for (String m : mTypes) if (compareMimeTypes(m, t)) return true; return false; }
    public String[] filterMimeTypes(String t) { java.util.ArrayList<String> l = new java.util.ArrayList<>(); for (String m : mTypes) if (compareMimeTypes(m, t)) l.add(m); return l.isEmpty() ? null : l.toArray(new String[0]); }
    public static boolean compareMimeTypes(String a, String b) { if (a.equals(b) || "*/*".equals(b)) return true; int s = b.indexOf("/*"); return s > 0 && a.startsWith(b.substring(0, s + 1)); }
    public android.os.PersistableBundle getExtras() { return mExtras; } public void setExtras(android.os.PersistableBundle e) { mExtras = e; }
    public long getTimestamp() { return 0; } public boolean isStyledText() { return false; }
    public int describeContents() { return 0; }
}
