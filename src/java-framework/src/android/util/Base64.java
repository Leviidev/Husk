package android.util;
public class Base64 {
    public static final int DEFAULT = 0, NO_PADDING = 1, NO_WRAP = 2, CRLF = 4, URL_SAFE = 8, NO_CLOSE = 16;
    public static byte[] decode(String s, int flags) { return decode(s.getBytes(java.nio.charset.StandardCharsets.US_ASCII), flags); }
    public static byte[] decode(byte[] in, int flags) { return decode(in, 0, in.length, flags); }
    public static byte[] decode(byte[] in, int off, int len, int flags) {
        StringBuilder b = new StringBuilder(len);
        for (int i = off; i < off + len; i++) { char c = (char) (in[i] & 255); if (c == '-') c = '+'; else if (c == '_') c = '/'; if (Character.isWhitespace(c)) continue; b.append(c); }
        while (b.length() % 4 != 0) b.append('=');
        try { return java.util.Base64.getDecoder().decode(b.toString()); } catch (IllegalArgumentException e) { throw new IllegalArgumentException("bad base-64"); }
    }
    public static String encodeToString(byte[] in, int flags) { return new String(encode(in, flags), java.nio.charset.StandardCharsets.US_ASCII); }
    public static String encodeToString(byte[] in, int off, int len, int flags) { return new String(encode(in, off, len, flags), java.nio.charset.StandardCharsets.US_ASCII); }
    public static byte[] encode(byte[] in, int flags) { return encode(in, 0, in.length, flags); }
    public static byte[] encode(byte[] in, int off, int len, int flags) {
        byte[] src = java.util.Arrays.copyOfRange(in, off, off + len);
        java.util.Base64.Encoder e = (flags & URL_SAFE) != 0 ? java.util.Base64.getUrlEncoder() : java.util.Base64.getEncoder();
        if ((flags & NO_PADDING) != 0) e = e.withoutPadding();
        String s = e.encodeToString(src);
        if ((flags & NO_WRAP) == 0) {
            StringBuilder b = new StringBuilder();
            for (int i = 0; i < s.length(); i += 76) { b.append(s, i, Math.min(s.length(), i + 76)); b.append((flags & CRLF) != 0 ? "\r\n" : "\n"); }
            s = b.toString();
        }
        return s.getBytes(java.nio.charset.StandardCharsets.US_ASCII);
    }
}
