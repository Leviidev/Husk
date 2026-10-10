package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.Gravity;

/** Hour and minute wheels, with AM/PM in 12-hour mode. */
public class TimePicker extends FrameLayout {
    public interface OnTimeChangedListener { void onTimeChanged(TimePicker view, int hourOfDay, int minute); }
    private final NumberPicker mHour, mMinute, mAmPm;
    private boolean mIs24;
    private int mHourOfDay, mMinuteValue;
    private OnTimeChangedListener mListener;
    public TimePicker(Context c) { this(c, null); }
    public TimePicker(Context c, AttributeSet a) { this(c, a, android.R.attr.timePickerStyle); }
    public TimePicker(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public TimePicker(Context c, AttributeSet a, int s, int r) {
        super(c, a, s, r);
        java.util.Calendar now = java.util.Calendar.getInstance();
        mHourOfDay = now.get(java.util.Calendar.HOUR_OF_DAY); mMinuteValue = now.get(java.util.Calendar.MINUTE);
        mIs24 = android.text.format.DateFormat.is24HourFormat(c);
        LinearLayout row = new LinearLayout(c);
        row.setOrientation(LinearLayout.HORIZONTAL); row.setGravity(Gravity.CENTER);
        mHour = new NumberPicker(c); mMinute = new NumberPicker(c); mAmPm = new NumberPicker(c);
        mMinute.setMinValue(0); mMinute.setMaxValue(59); mMinute.setFormatter(v -> String.format("%02d", v));
        mAmPm.setMinValue(0); mAmPm.setMaxValue(1); mAmPm.setDisplayedValues(new java.text.DateFormatSymbols().getAmPmStrings()); mAmPm.setWrapSelectorWheel(false);
        NumberPicker.OnValueChangeListener l = (p, o, n) -> {
            if (p == mMinute) mMinuteValue = n;
            else if (p == mAmPm) mHourOfDay = mHourOfDay % 12 + (n == 1 ? 12 : 0);
            else mHourOfDay = mIs24 ? n : (n % 12) + (mHourOfDay >= 12 ? 12 : 0);
            if (mListener != null) mListener.onTimeChanged(this, mHourOfDay, mMinuteValue);
        };
        mHour.setOnValueChangedListener(l); mMinute.setOnValueChangedListener(l); mAmPm.setOnValueChangedListener(l);
        row.addView(mHour); TextView colon = new TextView(c); colon.setText(":"); colon.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 18); row.addView(colon); row.addView(mMinute); row.addView(mAmPm);
        addView(row, new FrameLayout.LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, Gravity.CENTER));
        sync();
    }
    private void sync() {
        if (mIs24) { mHour.setMinValue(0); mHour.setMaxValue(23); mHour.setFormatter(v -> String.format("%02d", v)); mHour.setValue(mHourOfDay); mAmPm.setVisibility(GONE); }
        else { mHour.setMinValue(1); mHour.setMaxValue(12); mHour.setFormatter(null); int h = mHourOfDay % 12; mHour.setValue(h == 0 ? 12 : h); mAmPm.setVisibility(VISIBLE); mAmPm.setValue(mHourOfDay >= 12 ? 1 : 0); }
        mMinute.setValue(mMinuteValue);
    }
    public void setHour(int h) { mHourOfDay = Math.max(0, Math.min(23, h)); sync(); if (mListener != null) mListener.onTimeChanged(this, mHourOfDay, mMinuteValue); }
    public int getHour() { return mHourOfDay; }
    public void setMinute(int m) { mMinuteValue = Math.max(0, Math.min(59, m)); sync(); if (mListener != null) mListener.onTimeChanged(this, mHourOfDay, mMinuteValue); }
    public int getMinute() { return mMinuteValue; }
    @Deprecated public void setCurrentHour(Integer h) { setHour(h); } @Deprecated public Integer getCurrentHour() { return getHour(); }
    @Deprecated public void setCurrentMinute(Integer m) { setMinute(m); } @Deprecated public Integer getCurrentMinute() { return getMinute(); }
    public void setIs24HourView(Boolean is24) { mIs24 = is24 != null && is24; sync(); }
    public boolean is24HourView() { return mIs24; }
    public void setOnTimeChangedListener(OnTimeChangedListener l) { mListener = l; }
    public boolean validateInput() { return true; }
    @Override public void setEnabled(boolean e) { super.setEnabled(e); mHour.setEnabled(e); mMinute.setEnabled(e); mAmPm.setEnabled(e); }
    @Override public CharSequence getAccessibilityClassName() { return TimePicker.class.getName(); }
}
