package android.view;
public final class WindowMetrics {
    public android.graphics.Rect getBounds() { return new android.graphics.Rect(0, 0, husk.Native.screenWidth(), husk.Native.screenHeight()); }
    public WindowInsets getWindowInsets() { return new WindowInsets(); }
}
