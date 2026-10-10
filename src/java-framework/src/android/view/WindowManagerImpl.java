package android.view;
public final class WindowManagerImpl implements WindowManager {
    private final Display display = new Display();
    public Display getDefaultDisplay() { return display; }
    public WindowMetrics getCurrentWindowMetrics() { return new WindowMetrics(); }
    public WindowMetrics getMaximumWindowMetrics() { return new WindowMetrics(); }
    public void addView(View v, ViewGroup.LayoutParams p) {}
    public void updateViewLayout(View v, ViewGroup.LayoutParams p) {}
    public void removeView(View v) {}
}
