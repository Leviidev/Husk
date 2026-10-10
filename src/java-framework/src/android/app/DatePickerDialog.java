package android.app;

import android.content.Context;
import android.content.DialogInterface;
import android.widget.DatePicker;

public class DatePickerDialog extends AlertDialog implements DialogInterface.OnClickListener, DatePicker.OnDateChangedListener {
    public interface OnDateSetListener { void onDateSet(DatePicker view, int year, int month, int dayOfMonth); }
    private final DatePicker mDatePicker;
    private OnDateSetListener mDateSetListener;
    public DatePickerDialog(Context c) { this(c, 0, null, java.util.Calendar.getInstance(), -1, -1, -1); }
    public DatePickerDialog(Context c, int themeResId) { this(c, themeResId, null, java.util.Calendar.getInstance(), -1, -1, -1); }
    public DatePickerDialog(Context c, OnDateSetListener l, int year, int month, int day) { this(c, 0, l, null, year, month, day); }
    public DatePickerDialog(Context c, int themeResId, OnDateSetListener l, int year, int month, int day) { this(c, themeResId, l, null, year, month, day); }
    private DatePickerDialog(Context c, int themeResId, OnDateSetListener l, java.util.Calendar cal, int year, int month, int day) {
        super(c, resolveTheme(c, themeResId));
        mDatePicker = new DatePicker(getContext());
        if (cal != null) { year = cal.get(java.util.Calendar.YEAR); month = cal.get(java.util.Calendar.MONTH); day = cal.get(java.util.Calendar.DAY_OF_MONTH); }
        mDatePicker.init(year, month, day, this);
        mDateSetListener = l;
        setView(mDatePicker);
        setButton(BUTTON_POSITIVE, getContext().getString(android.R.string.ok), this);
        setButton(BUTTON_NEGATIVE, getContext().getString(android.R.string.cancel), this);
    }
    public void onDateChanged(DatePicker v, int y, int m, int d) { mDatePicker.init(y, m, d, this); }
    public void setOnDateSetListener(OnDateSetListener l) { mDateSetListener = l; }
    public void onClick(DialogInterface d, int which) { if (which == BUTTON_POSITIVE && mDateSetListener != null) mDateSetListener.onDateSet(mDatePicker, mDatePicker.getYear(), mDatePicker.getMonth(), mDatePicker.getDayOfMonth()); else if (which == BUTTON_NEGATIVE) cancel(); }
    public DatePicker getDatePicker() { return mDatePicker; }
    public void updateDate(int y, int m, int d) { mDatePicker.updateDate(y, m, d); }
}
