package android.graphics.drawable;

import android.content.res.ColorStateList;
import android.graphics.*;

/** The content layers, with a light press highlight in the ripple's colour while pressed. */
public class RippleDrawable extends LayerDrawable {
    public static final int RADIUS_AUTO = -1;
    private ColorStateList mColor;
    private boolean mPressed;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private Drawable mMask;
    public RippleDrawable(ColorStateList color, Drawable content, Drawable mask) {
        super(content != null ? new Drawable[] { content } : new Drawable[0]);
        mColor = color; mMask = mask;
    }
    public void setColor(ColorStateList c) { mColor = c; invalidateSelf(); }
    public void setRadius(int r) {}
    public int getRadius() { return RADIUS_AUTO; }
    public void setEffectColor(ColorStateList c) {}
    @Override public boolean isStateful() { return true; }
    @Override protected boolean onStateChange(int[] s) {
        super.onStateChange(s);
        boolean p = false, en = true;
        for (int v : s) { if (v == android.R.attr.state_pressed) p = true; }
        if (p != mPressed) { mPressed = p; invalidateSelf(); return true; }
        return false;
    }
    @Override public void draw(Canvas c) {
        super.draw(c);
        if (mPressed && mColor != null) {
            int col = mColor.getDefaultColor();
            mPaint.setColor((col & 0xFFFFFF) | (Math.min(0x40, col >>> 24) << 24));
            Rect b = getBounds();
            if (mMask != null || getNumberOfLayers() > 0) c.drawRect(b, mPaint);
            else c.drawCircle(b.exactCenterX(), b.exactCenterY(), Math.max(b.width(), b.height()) / 2f, mPaint);
        }
    }
}
