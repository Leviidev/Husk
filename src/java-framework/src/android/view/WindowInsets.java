package android.view;

import android.graphics.Insets;
import android.graphics.Rect;

public final class WindowInsets {
    public static final WindowInsets CONSUMED = new WindowInsets(Insets.NONE, Insets.NONE, true);
    static { CONSUMED.mCutoutConsumed = true; }
    public static final class Type {
        public static int statusBars() { return 1; } public static int navigationBars() { return 2; } public static int captionBar() { return 4; }
        public static int ime() { return 8; } public static int systemGestures() { return 16; } public static int mandatorySystemGestures() { return 32; }
        public static int tappableElement() { return 64; } public static int displayCutout() { return 128; } public static int systemBars() { return 7; }
        public static int systemOverlays() { return 256; }
    }
    public static final class Side { public static final int LEFT = 1, TOP = 2, RIGHT = 4, BOTTOM = 8; public static int all() { return 15; } }
    private final Insets mSystem, mIme;
    private final boolean mConsumed;
    private DisplayCutout mCutout;
    /* as on a phone with a display cutout: consuming the system window insets leaves the cutout, so the insets still reach the
     * children of a view that fits system windows (with nothing left in them); only consumeDisplayCutout ends that */
    private boolean mCutoutConsumed;
    public WindowInsets(Insets system, Insets ime, boolean consumed) { mSystem = system; mIme = ime; mConsumed = consumed; }
    public WindowInsets(WindowInsets o) { this(o.mSystem, o.mIme, o.mConsumed); mCutout = o.mCutout; mCutoutConsumed = o.mCutoutConsumed; }
    public WindowInsets(Rect r) { this(Insets.of(r), Insets.NONE, false); }
    /** Husk: the window's insets (the status bar and home indicator areas, the keyboard). */
    public static WindowInsets huskOf(int l, int t, int r, int b, int imeBottom, DisplayCutout cutout) { WindowInsets i = new WindowInsets(Insets.of(l, t, r, b), Insets.of(0, 0, 0, imeBottom), false); i.mCutout = cutout; return i; }
    public int getSystemWindowInsetLeft() { return mConsumed ? 0 : mSystem.left; }
    public int getSystemWindowInsetTop() { return mConsumed ? 0 : mSystem.top; }
    public int getSystemWindowInsetRight() { return mConsumed ? 0 : mSystem.right; }
    public int getSystemWindowInsetBottom() { return mConsumed ? 0 : Math.max(mSystem.bottom, mIme.bottom); }
    public Insets getSystemWindowInsets() { return mConsumed ? Insets.NONE : Insets.of(mSystem.left, mSystem.top, mSystem.right, getSystemWindowInsetBottom()); }
    public int getStableInsetLeft() { return mSystem.left; } public int getStableInsetTop() { return mSystem.top; }
    public int getStableInsetRight() { return mSystem.right; } public int getStableInsetBottom() { return mSystem.bottom; }
    public Insets getStableInsets() { return mSystem; }
    public boolean hasSystemWindowInsets() { return !mConsumed && (mSystem.left | mSystem.top | mSystem.right | mSystem.bottom | mIme.bottom) != 0; }
    public boolean hasInsets() { return hasSystemWindowInsets(); }
    public boolean hasStableInsets() { return (mSystem.left | mSystem.top | mSystem.right | mSystem.bottom) != 0; }
    public boolean isConsumed() { return mConsumed && mCutoutConsumed; }
    public boolean isRound() { return false; }
    public WindowInsets consumeSystemWindowInsets() { WindowInsets i = new WindowInsets(mSystem, mIme, true); i.mCutout = mCutout; i.mCutoutConsumed = mCutoutConsumed; return i; }
    public WindowInsets consumeStableInsets() { return this; }
    public WindowInsets consumeDisplayCutout() { WindowInsets i = new WindowInsets(this); i.mCutout = null; i.mCutoutConsumed = true; return i; }
    public WindowInsets replaceSystemWindowInsets(int l, int t, int r, int b) { WindowInsets i = new WindowInsets(Insets.of(l, t, r, b), Insets.NONE, false); i.mCutout = mCutout; i.mCutoutConsumed = mCutoutConsumed; return i; }
    public WindowInsets replaceSystemWindowInsets(Rect r) { return replaceSystemWindowInsets(r.left, r.top, r.right, r.bottom); }
    public WindowInsets inset(int l, int t, int r, int b) { return replaceSystemWindowInsets(Math.max(0, getSystemWindowInsetLeft() - l), Math.max(0, getSystemWindowInsetTop() - t), Math.max(0, getSystemWindowInsetRight() - r), Math.max(0, getSystemWindowInsetBottom() - b)); }
    public WindowInsets inset(Insets i) { return inset(i.left, i.top, i.right, i.bottom); }
    public Insets getInsets(int types) {
        if (mConsumed) return Insets.NONE;
        int l = 0, t = 0, r = 0, b = 0;
        if ((types & 1) != 0) t = mSystem.top;
        if ((types & 2) != 0) { b = mSystem.bottom; l = mSystem.left; r = mSystem.right; }
        if ((types & 8) != 0) b = Math.max(b, mIme.bottom);
        if ((types & 128) != 0 && mCutout != null) { t = Math.max(t, mCutout.getSafeInsetTop()); }
        if ((types & (16 | 32 | 64)) != 0) { t = Math.max(t, mSystem.top); b = Math.max(b, mSystem.bottom); }
        return Insets.of(l, t, r, b);
    }
    public Insets getInsetsIgnoringVisibility(int types) { return getInsets(types); }
    public boolean isVisible(int types) { return (types & 8) == 0 || mIme.bottom > 0; }
    public DisplayCutout getDisplayCutout() { return mCutout; }
    public Insets getSystemGestureInsets() { return getInsets(Type.systemGestures()); }
    public Insets getMandatorySystemGestureInsets() { return getInsets(Type.mandatorySystemGestures()); }
    public Insets getTappableElementInsets() { return getInsets(Type.tappableElement()); }
    public android.view.RoundedCorner getRoundedCorner(int p) { return null; }
    @Override public String toString() { return "WindowInsets{systemWindowInsets=" + mSystem + " ime=" + mIme + (mConsumed ? " consumed" : "") + "}"; }
    public static final class Builder {
        private Insets sys = Insets.NONE, ime = Insets.NONE; private DisplayCutout cut;
        public Builder() {}
        public Builder(WindowInsets w) { sys = w.mSystem; ime = w.mIme; cut = w.mCutout; }
        public Builder setSystemWindowInsets(Insets i) { sys = i; return this; }
        public Builder setStableInsets(Insets i) { return this; }
        public Builder setInsets(int types, Insets i) { if ((types & 8) != 0) ime = i; else sys = i; return this; }
        public Builder setInsetsIgnoringVisibility(int types, Insets i) { return this; }
        public Builder setVisible(int types, boolean v) { return this; }
        public Builder setDisplayCutout(DisplayCutout c) { cut = c; return this; }
        public Builder setSystemGestureInsets(Insets i) { return this; }
        public Builder setMandatorySystemGestureInsets(Insets i) { return this; }
        public Builder setTappableElementInsets(Insets i) { return this; }
        public WindowInsets build() { WindowInsets w = new WindowInsets(sys, ime, false); w.mCutout = cut; return w; }
    }
}
