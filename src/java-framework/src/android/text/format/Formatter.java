package android.text.format;
public final class Formatter {
    public static String formatFileSize(android.content.Context c, long n) { return fmt(n, 1000); }
    public static String formatShortFileSize(android.content.Context c, long n) { return fmt(n, 1000); }
    private static String fmt(long n, int base) {
        String[] u = { "B", "kB", "MB", "GB", "TB", "PB" };
        double v = n; int i = 0;
        while (Math.abs(v) >= 900 && i < u.length - 1) { v /= base; i++; }
        return (v < 10 && i > 0 ? String.format("%.2f", v) : v < 100 && i > 0 ? String.format("%.1f", v) : String.format("%.0f", v)) + " " + u[i];
    }
    @Deprecated public static String formatIpAddress(int ip) { return (ip & 255) + "." + ((ip >> 8) & 255) + "." + ((ip >> 16) & 255) + "." + ((ip >> 24) & 255); }
}
