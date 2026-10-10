package android.net;

public class Uri {
    public static final Uri EMPTY = new Uri("");
    private final String s;
    private Uri(String s) { this.s = s; }
    public static Uri parse(String s) { return new Uri(s); }
    public static Uri fromFile(java.io.File f) { return new Uri("file://" + f.getAbsolutePath()); }
    public static String encode(String s) { try { return java.net.URLEncoder.encode(s, "UTF-8").replace("+", "%20"); } catch (Exception e) { return s; } }
    public static String decode(String s) { try { return java.net.URLDecoder.decode(s, "UTF-8"); } catch (Exception e) { return s; } }
    public String getScheme() { int i = s.indexOf(':'); return i > 0 ? s.substring(0, i) : null; }
    public String getPath() {
        try { return new java.net.URI(s).getPath(); } catch (Exception e) { return s.startsWith("file://") ? s.substring(7) : s; }
    }
    public String getHost() { try { return new java.net.URI(s).getHost(); } catch (Exception e) { return null; } }
    public String getQueryParameter(String k) {
        int q = s.indexOf('?');
        if (q < 0) return null;
        for (String p : s.substring(q + 1).split("&")) { int e = p.indexOf('='); if (e > 0 && p.substring(0, e).equals(k)) return decode(p.substring(e + 1)); }
        return null;
    }
    public String getLastPathSegment() { String p = getPath(); if (p == null) return null; int i = p.lastIndexOf('/'); return i >= 0 ? p.substring(i + 1) : p; }
    @Override public String toString() { return s; }
    @Override public boolean equals(Object o) { return o instanceof Uri && ((Uri) o).s.equals(s); }
    @Override public int hashCode() { return s.hashCode(); }
    public int compareTo(Uri o) { return s.compareTo(o.s); }
}
