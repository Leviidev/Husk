package android.view;
public final class WindowMetrics {
    private final android.graphics.Rect mBounds; private final WindowInsets mInsets;
    public WindowMetrics(android.graphics.Rect bounds, WindowInsets insets) { mBounds = bounds; mInsets = insets; }
    public WindowMetrics(android.graphics.Rect bounds, WindowInsets insets, float density) { this(bounds, insets); }
    public android.graphics.Rect getBounds() { return mBounds; }
    public WindowInsets getWindowInsets() { return mInsets; }
    public float getDensity() { return husk.Native.density(); }
}
