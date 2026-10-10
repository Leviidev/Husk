package android.graphics.drawable.shapes;
import android.graphics.*;
public class RectShape extends Shape {
    private final RectF mRect = new RectF();
    @Override public void draw(Canvas c, Paint p) { c.drawRect(mRect, p); }
    @Override protected void onResize(float w, float h) { mRect.set(0, 0, w, h); }
    protected final RectF rect() { return mRect; }
}
