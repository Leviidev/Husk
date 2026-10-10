package android.graphics.drawable.shapes;
import android.graphics.*;
public class PathShape extends Shape {
    private final Path mPath; private final float sw, sh; private float kx = 1, ky = 1;
    public PathShape(Path p, float stdWidth, float stdHeight) { mPath = p; sw = stdWidth; sh = stdHeight; }
    @Override protected void onResize(float w, float h) { kx = w / sw; ky = h / sh; }
    @Override public void draw(Canvas c, Paint p) { c.save(); c.scale(kx, ky); c.drawPath(mPath, p); c.restore(); }
}
