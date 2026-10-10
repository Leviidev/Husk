package android.graphics.drawable;

import android.graphics.*;

public class ColorDrawable extends Drawable {
    private int mBase, mUse;
    private int mAlpha = 255;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    public ColorDrawable() { this(0); }
    public ColorDrawable(int c) { setColor(c); }
    public int getColor() { return mUse; }
    public void setColor(int c) { if (mBase != c || mUse == 0 && c != 0) { mBase = c; update(); invalidateSelf(); } }
    private void update() { int a = (mBase >>> 24) * (mAlpha + (mAlpha >> 7)) >> 8; mUse = (mBase & 0xFFFFFF) | (a << 24); }
    @Override public void setAlpha(int a) { mAlpha = a & 255; update(); invalidateSelf(); }
    @Override public int getAlpha() { return mUse >>> 24; }
    @Override public void draw(Canvas c) {
        ColorFilter f = huskTintFilter();
        if ((mUse >>> 24) == 0 && f == null) return;
        mPaint.setColor(mUse);
        mPaint.setColorFilter(f);
        c.drawRect(getBounds(), mPaint);
    }
    @Override public int getOpacity() { int a = mUse >>> 24; return a == 255 ? PixelFormat.OPAQUE : a == 0 ? PixelFormat.TRANSPARENT : PixelFormat.TRANSLUCENT; }
    @Override public void getOutline(Outline o) { o.setRect(getBounds()); o.setAlpha(getAlpha() / 255f); }
    @Override public ConstantState getConstantState() { final int c = mBase; return new ConstantState() { public Drawable newDrawable() { return new ColorDrawable(c); } }; }
}
