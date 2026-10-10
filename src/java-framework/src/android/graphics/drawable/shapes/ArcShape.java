package android.graphics.drawable.shapes;
import android.graphics.*;
public class ArcShape extends RectShape {
    private final float a, s;
    public ArcShape(float start, float sweep) { a = start; s = sweep; }
    @Override public void draw(Canvas c, Paint p) { c.drawArc(rect(), a, s, true, p); }
}
