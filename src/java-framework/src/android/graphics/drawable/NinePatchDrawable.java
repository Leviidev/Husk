package android.graphics.drawable;

import android.content.res.Resources;
import android.graphics.*;

public class NinePatchDrawable extends Drawable {
    private final NinePatch mPatch;
    private final Paint mPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final Rect mPadding;
    public NinePatchDrawable(Resources r, Bitmap b, byte[] chunk, Rect padding, String src) { mPatch = new NinePatch(b, chunk, src); mPadding = padding != null ? new Rect(padding) : new Rect(mPatch.padding); }
    public NinePatchDrawable(NinePatch p) { mPatch = p; mPadding = new Rect(p.padding); }
    @Override public void draw(Canvas c) { mPaint.setColorFilter(huskTintFilter()); mPatch.draw(c, getBounds(), mPaint); }
    @Override public boolean getPadding(Rect p) { p.set(mPadding); return (p.left | p.top | p.right | p.bottom) != 0; }
    @Override public int getIntrinsicWidth() { return mPatch.getWidth(); }
    @Override public int getIntrinsicHeight() { return mPatch.getHeight(); }
    @Override public void setAlpha(int a) { mPaint.setAlpha(a); invalidateSelf(); }
    @Override public int getAlpha() { return mPaint.getAlpha(); }
    public Paint getPaint() { return mPaint; }
}
