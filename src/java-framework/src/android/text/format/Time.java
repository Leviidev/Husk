package android.text.format;
public class Time {
    public static final String TIMEZONE_UTC = "UTC";
    public int second, minute, hour, monthDay, month, year, weekDay, yearDay, isDst; public long gmtoff; public String timezone; public boolean allDay;
    public Time() { this(java.util.TimeZone.getDefault().getID()); }
    public Time(String tz) { timezone = tz; }
    public void setToNow() { set(System.currentTimeMillis()); }
    public void set(long millis) { java.util.Calendar c = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone(timezone)); c.setTimeInMillis(millis); second = c.get(java.util.Calendar.SECOND); minute = c.get(java.util.Calendar.MINUTE); hour = c.get(java.util.Calendar.HOUR_OF_DAY); monthDay = c.get(java.util.Calendar.DAY_OF_MONTH); month = c.get(java.util.Calendar.MONTH); year = c.get(java.util.Calendar.YEAR); weekDay = c.get(java.util.Calendar.DAY_OF_WEEK) - 1; yearDay = c.get(java.util.Calendar.DAY_OF_YEAR) - 1; }
    public long toMillis(boolean ignoreDst) { java.util.Calendar c = java.util.Calendar.getInstance(java.util.TimeZone.getTimeZone(timezone)); c.set(year, month, monthDay, hour, minute, second); c.set(java.util.Calendar.MILLISECOND, 0); return c.getTimeInMillis(); }
    public String format(String f) { return new java.text.SimpleDateFormat(f.replace("%Y", "yyyy").replace("%m", "MM").replace("%d", "dd").replace("%H", "HH").replace("%M", "mm").replace("%S", "ss")).format(new java.util.Date(toMillis(false))); }
}
