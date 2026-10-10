package android.view;

public class ViewConfiguration {
    private final float d;
    private ViewConfiguration(float density) { d = density; }
    public static ViewConfiguration get(android.content.Context c) { return new ViewConfiguration(husk.Native.density()); }
    public static int getScrollBarSize() { return 4; }
    public int getScaledScrollBarSize() { return (int) (4 * d + 0.5f); }
    public static int getScrollBarFadeDuration() { return 250; }
    public static int getScrollDefaultDelay() { return 300; }
    public static int getFadingEdgeLength() { return 12; }
    public int getScaledFadingEdgeLength() { return (int) (12 * d + 0.5f); }
    public static int getPressedStateDuration() { return 64; }
    public static int getLongPressTimeout() { return 400; }
    public static int getMultiPressTimeout() { return 300; }
    public static int getKeyRepeatTimeout() { return 400; }
    public static int getKeyRepeatDelay() { return 50; }
    public static int getTapTimeout() { return 100; }
    public static int getJumpTapTimeout() { return 500; }
    public static int getDoubleTapTimeout() { return 300; }
    public static int getDoubleTapMinTime() { return 40; }
    public static int getEdgeSlop() { return 12; }
    public int getScaledEdgeSlop() { return (int) (12 * d + 0.5f); }
    public static int getTouchSlop() { return 8; }
    public int getScaledTouchSlop() { return (int) (8 * d + 0.5f); }
    public int getScaledHoverSlop() { return (int) (4 * d + 0.5f); }
    public int getScaledPagingTouchSlop() { return (int) (16 * d + 0.5f); }
    public int getScaledDoubleTapSlop() { return (int) (100 * d + 0.5f); }
    public static int getWindowTouchSlop() { return 16; }
    public int getScaledWindowTouchSlop() { return (int) (16 * d + 0.5f); }
    public static int getMinimumFlingVelocity() { return 50; }
    public int getScaledMinimumFlingVelocity() { return (int) (50 * d + 0.5f); }
    public static int getMaximumFlingVelocity() { return 8000; }
    public int getScaledMaximumFlingVelocity() { return (int) (8000 * d + 0.5f); }
    public int getScaledMinimumScalingSpan() { return (int) (170 * d + 0.5f); }
    public float getScaledHorizontalScrollFactor() { return 64 * d; }
    public float getScaledVerticalScrollFactor() { return 64 * d; }
    public static long getZoomControlsTimeout() { return 3000; }
    public static long getGlobalActionKeyTimeout() { return 500; }
    public static float getScrollFriction() { return 0.015f; }
    public int getScaledOverscrollDistance() { return 0; }
    public int getScaledOverflingDistance() { return (int) (6 * d + 0.5f); }
    public static int getMaximumDrawingCacheSize() { return 480 * 800 * 4; }
    public int getScaledMaximumDrawingCacheSize() { return getMaximumDrawingCacheSize(); }
    public boolean hasPermanentMenuKey() { return false; }
    public boolean shouldShowMenuShortcutsWhenKeyboardPresent() { return false; }
    public static long getDefaultActionModeHideDuration() { return 2000; }
    public static int getHoverTapTimeout() { return 150; }
    public static int getHoverTapSlop() { return 20; }
}
