package android.text.style;
import android.graphics.*;
import android.graphics.drawable.Drawable;
public abstract class DynamicDrawableSpan extends ReplacementSpan {
    public static final int ALIGN_BOTTOM = 0, ALIGN_BASELINE = 1, ALIGN_CENTER = 2;
    protected final int mVerticalAlignment;
    public DynamicDrawableSpan() { this(ALIGN_BOTTOM); }
    protected DynamicDrawableSpan(int a) { mVerticalAlignment = a; }
    public int getVerticalAlignment() { return mVerticalAlignment; }
    public abstract Drawable getDrawable();
    @Override public int getSize(Paint paint, CharSequence text, int start, int end, Paint.FontMetricsInt fm) {
        Drawable d = getDrawable(); Rect r = d.getBounds();
        if (fm != null) { fm.ascent = -r.bottom; fm.descent = 0; fm.top = fm.ascent; fm.bottom = 0; }
        return r.right;
    }
    @Override public void draw(Canvas c, CharSequence text, int start, int end, float x, int top, int y, int bottom, Paint paint) {
        Drawable d = getDrawable(); c.save();
        int transY = bottom - d.getBounds().bottom;
        if (mVerticalAlignment == ALIGN_BASELINE) transY -= paint.getFontMetricsInt().descent;
        else if (mVerticalAlignment == ALIGN_CENTER) transY = top + (bottom - top) / 2 - d.getBounds().height() / 2;
        c.translate(x, transY); d.draw(c); c.restore();
    }
}
