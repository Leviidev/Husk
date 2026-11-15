package android.graphics.drawable.shapes;
import android.graphics.*;
public class RectShape extends Shape {
    private RectF mRect = new RectF();
    @Override public void draw(Canvas c, Paint p) { c.drawRect(mRect, p); }
    @Override protected void onResize(float w, float h) { mRect.set(0, 0, w, h); }
    protected final RectF rect() { return mRect; }
    @Override public RectShape clone() throws CloneNotSupportedException {
        RectShape copy = (RectShape) super.clone();
        copy.mRect = new RectF(mRect);
        return copy;
    }
}
