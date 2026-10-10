package android.app;
public class AlarmManager {
    public static final int RTC_WAKEUP = 0, RTC = 1, ELAPSED_REALTIME_WAKEUP = 2, ELAPSED_REALTIME = 3;
    public static final long INTERVAL_FIFTEEN_MINUTES = 900000, INTERVAL_HALF_HOUR = 1800000, INTERVAL_HOUR = 3600000, INTERVAL_HALF_DAY = 43200000, INTERVAL_DAY = 86400000;
    public static final String ACTION_NEXT_ALARM_CLOCK_CHANGED = "android.app.action.NEXT_ALARM_CLOCK_CHANGED", ACTION_SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED = "android.app.action.SCHEDULE_EXACT_ALARM_PERMISSION_STATE_CHANGED";
    public interface OnAlarmListener { void onAlarm(); }
    public static class AlarmClockInfo implements android.os.Parcelable { public AlarmClockInfo(long t, PendingIntent p) {} public long getTriggerTime() { return 0; } public int describeContents() { return 0; } }
    private final android.os.Handler mHandler = new android.os.Handler(android.os.Looper.getMainLooper());
    private long delay(int type, long at) { long now = type == RTC || type == RTC_WAKEUP ? System.currentTimeMillis() : android.os.SystemClock.elapsedRealtime(); return Math.max(0, at - now); }
    public void set(int type, long at, PendingIntent op) { mHandler.postDelayed(() -> { try { op.send(); } catch (PendingIntent.CanceledException e) {} }, delay(type, at)); }
    public void set(int type, long at, String tag, OnAlarmListener l, android.os.Handler h) { mHandler.postDelayed(l::onAlarm, delay(type, at)); }
    public void setExact(int type, long at, PendingIntent op) { set(type, at, op); }
    public void setExact(int type, long at, String tag, OnAlarmListener l, android.os.Handler h) { set(type, at, tag, l, h); }
    public void setExactAndAllowWhileIdle(int type, long at, PendingIntent op) { set(type, at, op); }
    public void setAndAllowWhileIdle(int type, long at, PendingIntent op) { set(type, at, op); }
    public void setWindow(int type, long start, long len, PendingIntent op) { set(type, start, op); }
    public void setRepeating(int type, long at, long interval, PendingIntent op) { set(type, at, op); }
    public void setInexactRepeating(int type, long at, long interval, PendingIntent op) { set(type, at, op); }
    public void setAlarmClock(AlarmClockInfo i, PendingIntent op) {}
    public void cancel(PendingIntent op) {}
    public void cancel(OnAlarmListener l) {}
    public void cancelAll() {}
    public boolean canScheduleExactAlarms() { return true; }
    public AlarmClockInfo getNextAlarmClock() { return null; }
    public void setTimeZone(String tz) {}
}
