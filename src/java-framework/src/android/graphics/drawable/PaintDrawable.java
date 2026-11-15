package android.graphics.drawable;

import android.graphics.drawable.shapes.RoundRectShape;

/** A ShapeDrawable of one colour: a rectangle, or a rounded one once corner radii are set. */
public class PaintDrawable extends ShapeDrawable {
    public PaintDrawable() { super(); }
    public PaintDrawable(int color) { super(); getPaint().setColor(color); }
    public void setCornerRadius(float r) {
        float[] radii = null;
        if (r > 0) { radii = new float[8]; java.util.Arrays.fill(radii, r); }
        setCornerRadii(radii);
    }
    public void setCornerRadii(float[] radii) {
        if (radii == null) { if (getShape() != null) setShape(null); }
        else setShape(new RoundRectShape(radii, null, null));
        invalidateSelf();
    }
    protected boolean inflateTag(String name, android.content.res.Resources r, org.xmlpull.v1.XmlPullParser p, android.util.AttributeSet a) { return false; }
    @Override public ConstantState getConstantState() {
        final PaintDrawable src = this;
        return new ConstantState() {
            @Override public Drawable newDrawable() { PaintDrawable p = new PaintDrawable(); p.huskCopyShape(src); return p; }
            @Override public int getChangingConfigurations() { return src.getChangingConfigurations(); }
        };
    }
}
