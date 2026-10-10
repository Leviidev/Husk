package android.graphics.drawable.shapes;
import android.graphics.*;
public abstract class Shape implements Cloneable {
    private float mW, mH;
    public final float getWidth() { return mW; }
    public final float getHeight() { return mH; }
    public abstract void draw(Canvas c, Paint p);
    public final void resize(float w, float h) { if (w < 0) w = 0; if (h < 0) h = 0; if (mW != w || mH != h) { mW = w; mH = h; onResize(w, h); } }
    public boolean hasAlpha() { return true; }
    protected void onResize(float w, float h) {}
    public void getOutline(Outline o) {}
    @Override public Shape clone() throws CloneNotSupportedException { return (Shape) super.clone(); }
}
