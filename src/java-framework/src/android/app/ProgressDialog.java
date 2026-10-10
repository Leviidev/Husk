package android.app;

import android.content.Context;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

@Deprecated
public class ProgressDialog extends AlertDialog {
    public static final int STYLE_SPINNER = 0, STYLE_HORIZONTAL = 1;
    private ProgressBar mProgress;
    private TextView mMessageView;
    private int mStyle = STYLE_SPINNER, mMax = 100, mValue, mSecondary;
    private boolean mIndeterminate;
    private CharSequence mMessage;
    public ProgressDialog(Context c) { super(c); }
    public ProgressDialog(Context c, int theme) { super(c, theme); }
    public static ProgressDialog show(Context c, CharSequence title, CharSequence message) { return show(c, title, message, false); }
    public static ProgressDialog show(Context c, CharSequence title, CharSequence message, boolean indeterminate) { return show(c, title, message, indeterminate, false, null); }
    public static ProgressDialog show(Context c, CharSequence title, CharSequence message, boolean indeterminate, boolean cancelable) { return show(c, title, message, indeterminate, cancelable, null); }
    public static ProgressDialog show(Context c, CharSequence title, CharSequence message, boolean indeterminate, boolean cancelable, OnCancelListener l) {
        ProgressDialog d = new ProgressDialog(c);
        d.setTitle(title); d.setMessage(message); d.setIndeterminate(indeterminate); d.setCancelable(cancelable); d.setOnCancelListener(l);
        d.show();
        return d;
    }
    @Override protected void onCreate(Bundle s) {
        Context c = getContext();
        float dp = c.getResources().getDisplayMetrics().density;
        LinearLayout row = new LinearLayout(c);
        row.setOrientation(mStyle == STYLE_HORIZONTAL ? LinearLayout.VERTICAL : LinearLayout.HORIZONTAL);
        row.setGravity(Gravity.CENTER_VERTICAL);
        row.setPadding((int) (24 * dp), (int) (16 * dp), (int) (24 * dp), (int) (16 * dp));
        mProgress = mStyle == STYLE_HORIZONTAL ? new ProgressBar(c, null, android.R.attr.progressBarStyleHorizontal) : new ProgressBar(c);
        mProgress.setMax(mMax); mProgress.setProgress(mValue); mProgress.setSecondaryProgress(mSecondary);
        if (mStyle == STYLE_HORIZONTAL) mProgress.setIndeterminate(mIndeterminate);
        mMessageView = new TextView(c);
        mMessageView.setText(mMessage);
        mMessageView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        if (mStyle == STYLE_HORIZONTAL) { row.addView(mMessageView); row.addView(mProgress, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)); }
        else { row.addView(mProgress, new LinearLayout.LayoutParams((int) (40 * dp), (int) (40 * dp))); mMessageView.setPadding((int) (16 * dp), 0, 0, 0); row.addView(mMessageView); }
        setView(row);
        super.onCreate(s);
    }
    public void setProgress(int v) { mValue = v; if (mProgress != null) mProgress.setProgress(v); }
    public void setSecondaryProgress(int v) { mSecondary = v; if (mProgress != null) mProgress.setSecondaryProgress(v); }
    public int getProgress() { return mValue; }
    public int getSecondaryProgress() { return mSecondary; }
    public int getMax() { return mMax; }
    public void setMax(int m) { mMax = m; if (mProgress != null) mProgress.setMax(m); }
    public void incrementProgressBy(int d) { setProgress(mValue + d); }
    public void incrementSecondaryProgressBy(int d) { setSecondaryProgress(mSecondary + d); }
    public void setProgressDrawable(android.graphics.drawable.Drawable d) {}
    public void setIndeterminateDrawable(android.graphics.drawable.Drawable d) {}
    public void setIndeterminate(boolean i) { mIndeterminate = i; if (mProgress != null && mStyle == STYLE_HORIZONTAL) mProgress.setIndeterminate(i); }
    public boolean isIndeterminate() { return mIndeterminate; }
    @Override public void setMessage(CharSequence m) { mMessage = m; if (mMessageView != null) mMessageView.setText(m); }
    public void setProgressStyle(int s) { mStyle = s; }
    public void setProgressNumberFormat(String f) {}
    public void setProgressPercentFormat(java.text.NumberFormat f) {}
}
