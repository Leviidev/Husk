package android.net;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** A URI as Android's: scheme, authority, path, query and fragment, kept encoded and decoded when asked for. */
public abstract class Uri implements Comparable<Uri>, android.os.Parcelable {
    public static final Uri EMPTY = new StringUri("");
    private static final char[] HEX = "0123456789ABCDEF".toCharArray();
    private Uri() {}
    public abstract boolean isHierarchical();
    public boolean isOpaque() { return !isHierarchical(); }
    public abstract boolean isRelative();
    public boolean isAbsolute() { return !isRelative(); }
    public abstract String getScheme();
    public abstract String getSchemeSpecificPart();
    public abstract String getEncodedSchemeSpecificPart();
    public abstract String getAuthority();
    public abstract String getEncodedAuthority();
    public abstract String getUserInfo();
    public abstract String getEncodedUserInfo();
    public abstract String getHost();
    public abstract int getPort();
    public abstract String getPath();
    public abstract String getEncodedPath();
    public abstract String getQuery();
    public abstract String getEncodedQuery();
    public abstract String getFragment();
    public abstract String getEncodedFragment();
    public abstract List<String> getPathSegments();
    public abstract String getLastPathSegment();
    public abstract Builder buildUpon();
    @Override public boolean equals(Object o) { return o instanceof Uri && toString().equals(o.toString()); }
    @Override public int hashCode() { return toString().hashCode(); }
    public int compareTo(Uri o) { return toString().compareTo(o.toString()); }
    public abstract String toString();
    public String toSafeString() { return toString(); }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) { p.writeString(toString()); }
    public static final android.os.Parcelable.Creator<Uri> CREATOR = new android.os.Parcelable.Creator<Uri>() { public Uri createFromParcel(android.os.Parcel p) { return parse(p.readString()); } public Uri[] newArray(int n) { return new Uri[n]; } };
    public Uri normalizeScheme() { String s = getScheme(); if (s == null) return this; String l = s.toLowerCase(java.util.Locale.ROOT); return s.equals(l) ? this : buildUpon().scheme(l).build(); }
    public Set<String> getQueryParameterNames() {
        String q = getEncodedQuery();
        if (q == null) return Collections.emptySet();
        LinkedHashSet<String> names = new LinkedHashSet<>();
        int start = 0;
        do { int next = q.indexOf('&', start); int end = next == -1 ? q.length() : next; int sep = q.indexOf('=', start); if (sep > end || sep == -1) sep = end; names.add(decode(q.substring(start, sep))); start = end + 1; } while (start < q.length());
        return Collections.unmodifiableSet(names);
    }
    public List<String> getQueryParameters(String key) {
        String q = getEncodedQuery();
        if (q == null) return Collections.emptyList();
        String ek = encode(key, null);
        ArrayList<String> vals = new ArrayList<>();
        int start = 0;
        do { int next = q.indexOf('&', start); int end = next != -1 ? next : q.length(); int sep = q.indexOf('=', start); if (sep > end || sep == -1) sep = end;
            if (sep - start == ek.length() && q.regionMatches(start, ek, 0, ek.length())) vals.add(sep == end ? "" : decode(q.substring(sep + 1, end)));
            if (next != -1) start = next + 1; else break; } while (true);
        return Collections.unmodifiableList(vals);
    }
    public String getQueryParameter(String key) {
        String q = getEncodedQuery();
        if (q == null) return null;
        String ek = encode(key, null);
        int start = 0;
        do { int next = q.indexOf('&', start); int end = next != -1 ? next : q.length(); int sep = q.indexOf('=', start); if (sep > end || sep == -1) sep = end;
            if (sep - start == ek.length() && q.regionMatches(start, ek, 0, ek.length())) return sep == end ? "" : decode(q.substring(sep + 1, end)).replace('+', ' ').equals(decode(q.substring(sep + 1, end))) ? decode(q.substring(sep + 1, end)) : decode(q.substring(sep + 1, end).replace('+', ' '));
            if (next != -1) start = next + 1; else break; } while (true);
        return null;
    }
    public boolean getBooleanQueryParameter(String k, boolean d) { String v = getQueryParameter(k); if (v == null) return d; v = v.toLowerCase(java.util.Locale.ROOT); return !"false".equals(v) && !"0".equals(v); }

    public static Uri parse(String s) { return new StringUri(s); }
    public static Uri fromFile(java.io.File f) { return new Builder().scheme("file").authority("").path(f.getAbsolutePath()).build(); }
    public static Uri fromParts(String scheme, String ssp, String fragment) { return new StringUri(scheme + ":" + encode(ssp, "@:/?=&") + (fragment != null ? "#" + encode(fragment, null) : "")); }
    public static Uri withAppendedPath(Uri base, String seg) { return base.buildUpon().appendEncodedPath(seg).build(); }
    public static String encode(String s) { return encode(s, null); }
    public static String encode(String s, String allow) {
        if (s == null) return null;
        StringBuilder b = null;
        byte[] bytes = null;
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            boolean ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || "_-!.~'()*".indexOf(c) >= 0 || (allow != null && allow.indexOf(c) >= 0);
            if (ok) { if (b != null) b.append(c); continue; }
            if (b == null) { b = new StringBuilder(); b.append(s, 0, i); }
            int j = i + 1;
            while (j < s.length()) { char d = s.charAt(j); boolean ok2 = (d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || (d >= '0' && d <= '9') || "_-!.~'()*".indexOf(d) >= 0 || (allow != null && allow.indexOf(d) >= 0); if (ok2) break; j++; }
            byte[] enc = s.substring(i, j).getBytes(java.nio.charset.StandardCharsets.UTF_8);
            for (byte x : enc) { b.append('%').append(HEX[(x & 0xf0) >> 4]).append(HEX[x & 0xf]); }
            i = j - 1;
        }
        return b == null ? s : b.toString();
    }
    public static String decode(String s) {
        if (s == null) return null;
        if (s.indexOf('%') < 0) return s;
        java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); ) {
            char c = s.charAt(i);
            if (c == '%' && i + 2 < s.length() + 0 && i + 2 <= s.length() - 1 + 1) {
                try { out.write(Integer.parseInt(s.substring(i + 1, i + 3), 16)); i += 3; continue; } catch (Exception e) {}
            }
            if (out.size() > 0) { b.append(new String(out.toByteArray(), java.nio.charset.StandardCharsets.UTF_8)); out.reset(); }
            b.append(c); i++;
        }
        if (out.size() > 0) b.append(new String(out.toByteArray(), java.nio.charset.StandardCharsets.UTF_8));
        return b.toString();
    }

    /** A URI from its string, parsed on demand. */
    private static final class StringUri extends Uri {
        private final String s;
        private int cachedSsi = -2, cachedFsi = -2;
        StringUri(String s) { if (s == null) throw new NullPointerException("uriString"); this.s = s; }
        private int ssi() { if (cachedSsi != -2) return cachedSsi; int len = s.length(); for (int i = 0; i < len; i++) { char c = s.charAt(i); if (c == ':') return cachedSsi = i; if (c == '/' || c == '?' || c == '#') return cachedSsi = -1; } return cachedSsi = -1; }
        private int fsi() { if (cachedFsi != -2) return cachedFsi; return cachedFsi = s.indexOf('#', Math.max(ssi(), 0)); }
        public boolean isHierarchical() { int i = ssi(); if (i == -1) return true; if (s.length() == i + 1) return false; return s.charAt(i + 1) == '/'; }
        public boolean isRelative() { return ssi() == -1; }
        public String getScheme() { int i = ssi(); return i == -1 ? null : s.substring(0, i); }
        public String getEncodedSchemeSpecificPart() { int st = ssi() + 1, f = fsi(); return f == -1 ? s.substring(st) : s.substring(st, f); }
        public String getSchemeSpecificPart() { return decode(getEncodedSchemeSpecificPart()); }
        public String getEncodedAuthority() {
            int len = s.length(), st = ssi() + 1;
            if (len > st + 1 && s.charAt(st) == '/' && s.charAt(st + 1) == '/') {
                int e = st + 2;
                while (e < len) { char c = s.charAt(e); if (c == '/' || c == '\\' || c == '?' || c == '#') break; e++; }
                return s.substring(st + 2, e);
            }
            return null;
        }
        public String getAuthority() { return decode(getEncodedAuthority()); }
        public String getEncodedUserInfo() { String a = getEncodedAuthority(); if (a == null) return null; int i = a.lastIndexOf('@'); return i == -1 ? null : a.substring(0, i); }
        public String getUserInfo() { return decode(getEncodedUserInfo()); }
        public String getHost() {
            String a = getEncodedAuthority(); if (a == null) return null;
            int u = a.lastIndexOf('@'), p = a.indexOf(':', u);
            String h = p == -1 ? a.substring(u + 1) : a.substring(u + 1, p);
            if (h.startsWith("[")) { int e = a.indexOf(']', u); if (e > 0) h = a.substring(u + 1, e + 1); }
            return decode(h);
        }
        public int getPort() { String a = getEncodedAuthority(); if (a == null) return -1; int u = a.lastIndexOf('@'), p = a.lastIndexOf(':'); if (p <= u || (a.indexOf(']') > p)) return -1; try { return Integer.parseInt(decode(a.substring(p + 1))); } catch (NumberFormatException e) { return -1; } }
        public String getEncodedPath() {
            int ssi = ssi();
            if (ssi > -1 && !isHierarchical()) return null;
            int st = ssi + 1, len = s.length();
            if (len > st + 1 && s.charAt(st) == '/' && s.charAt(st + 1) == '/') { st += 2; while (st < len) { char c = s.charAt(st); if (c == '?' || c == '#') return ""; if (c == '/' || c == '\\') break; st++; } }
            int e = st;
            while (e < len) { char c = s.charAt(e); if (c == '?' || c == '#') break; e++; }
            return s.substring(st, e);
        }
        public String getPath() { return decode(getEncodedPath()); }
        public String getEncodedQuery() { int q = s.indexOf('?', Math.max(ssi(), 0)), f = fsi(); if (q == -1 || (f != -1 && f < q)) return null; return f == -1 ? s.substring(q + 1) : s.substring(q + 1, f); }
        public String getQuery() { return decode(getEncodedQuery()); }
        public String getEncodedFragment() { int f = fsi(); return f == -1 ? null : s.substring(f + 1); }
        public String getFragment() { return decode(getEncodedFragment()); }
        public List<String> getPathSegments() {
            String p = getEncodedPath();
            if (p == null || p.isEmpty()) return Collections.emptyList();
            ArrayList<String> segs = new ArrayList<>();
            for (String seg : p.split("/")) if (!seg.isEmpty()) segs.add(decode(seg));
            return Collections.unmodifiableList(segs);
        }
        public String getLastPathSegment() { List<String> s = getPathSegments(); return s.isEmpty() ? null : s.get(s.size() - 1); }
        public Builder buildUpon() {
            if (isHierarchical()) return new Builder().scheme(getScheme()).encodedAuthority(getEncodedAuthority()).encodedPath(getEncodedPath()).encodedQuery(getEncodedQuery()).encodedFragment(getEncodedFragment());
            return new Builder().scheme(getScheme()).encodedOpaquePart(getEncodedSchemeSpecificPart()).encodedFragment(getEncodedFragment());
        }
        public String toString() { return s; }
    }

    public static final class Builder {
        private String scheme, opaque, authority, path, query, fragment;
        public Builder() {}
        public Builder scheme(String s) { scheme = s; return this; }
        public Builder opaquePart(String o) { opaque = encode(o, null); return this; }
        public Builder encodedOpaquePart(String o) { opaque = o; return this; }
        public Builder authority(String a) { opaque = null; authority = encode(a, "@:"); return this; }
        public Builder encodedAuthority(String a) { opaque = null; authority = a; return this; }
        public Builder path(String p) { opaque = null; path = encode(p, "/"); return this; }
        public Builder encodedPath(String p) { opaque = null; path = p; return this; }
        public Builder appendPath(String seg) { return appendEncodedPath(encode(seg, null)); }
        public Builder appendEncodedPath(String seg) {
            opaque = null;
            // as Android's PathPart.appendEncodedSegment, slashes and all ("" + "/" is "//": apps' URI matchers count on it)
            if (path == null || path.isEmpty()) path = "/" + seg;
            else path = path.endsWith("/") ? path + seg : path + "/" + seg;
            return this;
        }
        public Builder query(String q) { opaque = null; query = encode(q, "=&"); return this; }
        public Builder encodedQuery(String q) { opaque = null; query = q; return this; }
        public Builder fragment(String f) { fragment = encode(f, null); return this; }
        public Builder encodedFragment(String f) { fragment = f; return this; }
        public Builder appendQueryParameter(String k, String v) { opaque = null; String e = encode(k, null) + "=" + encode(v, null); query = query == null || query.isEmpty() ? e : query + "&" + e; return this; }
        public Builder clearQuery() { query = null; return this; }
        public Uri build() {
            StringBuilder b = new StringBuilder();
            if (scheme != null) b.append(scheme).append(':');
            if (opaque != null) b.append(opaque);
            else {
                if (authority != null) b.append("//").append(authority);
                if (path != null && !path.isEmpty()) { if (authority != null && !path.startsWith("/")) b.append('/'); b.append(path); }
                if (query != null) b.append('?').append(query);
            }
            if (fragment != null) b.append('#').append(fragment);
            return new StringUri(b.toString());
        }
        @Override public String toString() { return build().toString(); }
    }
}
