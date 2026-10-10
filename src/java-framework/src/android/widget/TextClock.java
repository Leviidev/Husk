package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import java.util.Calendar;
import java.util.TimeZone;

public class TextClock extends TextView {
    public static final CharSequence DEFAULT_FORMAT_12_HOUR = "h:mm a", DEFAULT_FORMAT_24_HOUR = "H:mm";
    private CharSequence mFormat12 = DEFAULT_FORMAT_12_HOUR, mFormat24 = DEFAULT_FORMAT_24_HOUR;
    private String mTimeZone;
    private boolean mAttached;
    private final Runnable mTicker = new Runnable() { public void run() { onTimeChanged(); long now = System.currentTimeMillis(); postDelayed(mTicker, 1000 - (now % 1000)); } };
    public TextClock(Context c) { this(c, null); }
    public TextClock(Context c, AttributeSet a) { this(c, a, 0); }
    public TextClock(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public TextClock(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.TextClock, s, r);
        CharSequence f = a.getText(husk.S.TextClock_format12Hour); if (f != null) mFormat12 = f;
        f = a.getText(husk.S.TextClock_format24Hour); if (f != null) mFormat24 = f;
        mTimeZone = a.getString(husk.S.TextClock_timeZone);
        a.recycle();
        onTimeChanged();
    }
    public CharSequence getFormat12Hour() { return mFormat12; } public void setFormat12Hour(CharSequence f) { mFormat12 = f; onTimeChanged(); }
    public CharSequence getFormat24Hour() { return mFormat24; } public void setFormat24Hour(CharSequence f) { mFormat24 = f; onTimeChanged(); }
    public void setContentDescriptionFormat12Hour(CharSequence f) {} public void setContentDescriptionFormat24Hour(CharSequence f) {}
    public boolean is24HourModeEnabled() { return android.text.format.DateFormat.is24HourFormat(getContext()); }
    public String getTimeZone() { return mTimeZone; } public void setTimeZone(String tz) { mTimeZone = tz; onTimeChanged(); }
    public CharSequence getFormat() { return is24HourModeEnabled() ? mFormat24 : mFormat12; }
    public void refreshTime() { onTimeChanged(); }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); if (!mAttached) { mAttached = true; mTicker.run(); } }
    @Override protected void onDetachedFromWindow() { super.onDetachedFromWindow(); if (mAttached) { removeCallbacks(mTicker); mAttached = false; } }
    private void onTimeChanged() { Calendar c = mTimeZone != null ? Calendar.getInstance(TimeZone.getTimeZone(mTimeZone)) : Calendar.getInstance(); setText(android.text.format.DateFormat.format(getFormat(), c)); }
}
