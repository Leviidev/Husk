package android.text.format;
public class DateFormat {
    public static boolean is24HourFormat(android.content.Context c) { return true; }
    public static java.text.DateFormat getTimeFormat(android.content.Context c) { return new java.text.SimpleDateFormat("HH:mm"); }
    public static java.text.DateFormat getDateFormat(android.content.Context c) { return java.text.DateFormat.getDateInstance(java.text.DateFormat.SHORT); }
    public static java.text.DateFormat getLongDateFormat(android.content.Context c) { return java.text.DateFormat.getDateInstance(java.text.DateFormat.LONG); }
    public static java.text.DateFormat getMediumDateFormat(android.content.Context c) { return java.text.DateFormat.getDateInstance(java.text.DateFormat.MEDIUM); }
    public static String getBestDateTimePattern(java.util.Locale l, String skeleton) { return skeleton; }
    public static char[] getDateFormatOrder(android.content.Context c) { return new char[] { 'd', 'M', 'y' }; }
    public static CharSequence format(CharSequence fmt, long ms) { return format(fmt, new java.util.Date(ms)); }
    public static CharSequence format(CharSequence fmt, java.util.Date d) { java.util.Calendar c = java.util.Calendar.getInstance(); c.setTime(d); return format(fmt, c); }
    public static CharSequence format(CharSequence fmt, java.util.Calendar c) {
        String f = fmt.toString().replace("kk", "HH").replace("k", "H");
        try { java.text.SimpleDateFormat s = new java.text.SimpleDateFormat(f); s.setTimeZone(c.getTimeZone()); return s.format(c.getTime()); } catch (IllegalArgumentException e) { return f; }
    }
}
