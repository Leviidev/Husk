package android.widget;
public class SeekBar extends AbsSeekBar {
    public interface OnSeekBarChangeListener { void onProgressChanged(SeekBar s, int p, boolean fromUser); void onStartTrackingTouch(SeekBar s); void onStopTrackingTouch(SeekBar s); }
    private OnSeekBarChangeListener mListener;
    public SeekBar(android.content.Context c) { this(c, null); }
    public SeekBar(android.content.Context c, android.util.AttributeSet a) { this(c, a, android.R.attr.seekBarStyle); }
    public SeekBar(android.content.Context c, android.util.AttributeSet a, int s) { this(c, a, s, 0); }
    public SeekBar(android.content.Context c, android.util.AttributeSet a, int s, int r) { super(c, a, s, r); }
    @Override void onProgressRefresh(float scale, boolean fromUser, int progress) { super.onProgressRefresh(scale, fromUser, progress); if (mListener != null) mListener.onProgressChanged(this, progress, fromUser); }
    public void setOnSeekBarChangeListener(OnSeekBarChangeListener l) { mListener = l; }
    @Override void onStartTrackingTouch() { super.onStartTrackingTouch(); if (mListener != null) mListener.onStartTrackingTouch(this); }
    @Override void onStopTrackingTouch() { super.onStopTrackingTouch(); if (mListener != null) mListener.onStopTrackingTouch(this); }
    @Override public CharSequence getAccessibilityClassName() { return SeekBar.class.getName(); }
}
