package android.view;
import android.graphics.Rect;
public final class DisplayCutout {
    private final Rect mSafe;
    public DisplayCutout(Rect safeInsets, java.util.List<Rect> boundingRects) { mSafe = safeInsets == null ? new Rect() : new Rect(safeInsets); }
    public DisplayCutout(android.graphics.Insets safe, Rect l, Rect t, Rect r, Rect b) { mSafe = new Rect(safe.left, safe.top, safe.right, safe.bottom); }
    public int getSafeInsetTop() { return mSafe.top; } public int getSafeInsetBottom() { return mSafe.bottom; }
    public int getSafeInsetLeft() { return mSafe.left; } public int getSafeInsetRight() { return mSafe.right; }
    public java.util.List<Rect> getBoundingRects() { return new java.util.ArrayList<>(); }
    public Rect getBoundingRectTop() { return new Rect(); }
    public android.graphics.Insets getWaterfallInsets() { return android.graphics.Insets.NONE; }
}
