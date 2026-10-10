package android.graphics.drawable;

import android.graphics.*;

public class InsetDrawable extends DrawableWrapper {
    private final int l, t, r, b;
    public InsetDrawable(Drawable d, int inset) { this(d, inset, inset, inset, inset); }
    public InsetDrawable(Drawable d, float inset) { this(d, 0); }
    public InsetDrawable(Drawable d, int l, int t, int r, int b) { super(d); this.l = l; this.t = t; this.r = r; this.b = b; }
    @Override protected void onBoundsChange(Rect bo) { if (getDrawable() != null) getDrawable().setBounds(bo.left + l, bo.top + t, bo.right - r, bo.bottom - b); }
    @Override public int getIntrinsicWidth() { int w = super.getIntrinsicWidth(); return w < 0 ? -1 : w + l + r; }
    @Override public int getIntrinsicHeight() { int h = super.getIntrinsicHeight(); return h < 0 ? -1 : h + t + b; }
    @Override public boolean getPadding(Rect p) { boolean has = super.getPadding(p); p.left += l; p.top += t; p.right += r; p.bottom += b; return has || (l | t | r | b) != 0; }
}
