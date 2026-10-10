package android.widget;

import android.content.Context;
import android.util.AttributeSet;

/** A date chooser (here the wheel picker). */
public class CalendarView extends FrameLayout {
    public interface OnDateChangeListener { void onSelectedDayChange(CalendarView view, int year, int month, int dayOfMonth); }
    private final DatePicker mPicker;
    private OnDateChangeListener mListener;
    public CalendarView(Context c) { this(c, null); }
    public CalendarView(Context c, AttributeSet a) { this(c, a, 0); }
    public CalendarView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public CalendarView(Context c, AttributeSet a, int s, int r) {
        super(c, a, s, r);
        mPicker = new DatePicker(c);
        mPicker.setOnDateChangedListener((v, y, m, d) -> { if (mListener != null) mListener.onSelectedDayChange(this, y, m, d); });
        addView(mPicker);
    }
    public void setOnDateChangeListener(OnDateChangeListener l) { mListener = l; }
    public long getDate() { java.util.Calendar c = java.util.Calendar.getInstance(); c.set(mPicker.getYear(), mPicker.getMonth(), mPicker.getDayOfMonth()); return c.getTimeInMillis(); }
    public void setDate(long d) { setDate(d, false, false); }
    public void setDate(long d, boolean animate, boolean center) { java.util.Calendar c = java.util.Calendar.getInstance(); c.setTimeInMillis(d); mPicker.updateDate(c.get(java.util.Calendar.YEAR), c.get(java.util.Calendar.MONTH), c.get(java.util.Calendar.DAY_OF_MONTH)); }
    public long getMinDate() { return mPicker.getMinDate(); } public void setMinDate(long d) { mPicker.setMinDate(d); }
    public long getMaxDate() { return mPicker.getMaxDate(); } public void setMaxDate(long d) { mPicker.setMaxDate(d); }
    public void setFirstDayOfWeek(int d) {} public int getFirstDayOfWeek() { return mPicker.getFirstDayOfWeek(); }
    public void setShowWeekNumber(boolean s) {} public boolean getShowWeekNumber() { return false; }
    public void setDateTextAppearance(int r) {} public void setWeekDayTextAppearance(int r) {}
}
