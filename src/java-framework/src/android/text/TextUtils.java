package android.text;
public class TextUtils {
    public static boolean isEmpty(CharSequence s) { return s == null || s.length() == 0; }
    public static boolean equals(CharSequence a, CharSequence b) { return a == b || (a != null && b != null && a.toString().equals(b.toString())); }
    public static String join(CharSequence d, Iterable<?> t) { StringBuilder b = new StringBuilder(); boolean f = true; for (Object o : t) { if (!f) b.append(d); b.append(o); f = false; } return b.toString(); }
    public static String join(CharSequence d, Object[] t) { return join(d, java.util.Arrays.asList(t)); }
    public static String[] split(String s, String re) { return s.isEmpty() ? new String[0] : s.split(re, -1); }
}
