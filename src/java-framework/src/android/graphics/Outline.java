package android.graphics;
public final class Outline {
    public Rect mRect; public float mRadius; public float mAlpha = 1; public Path mPath;
    public Outline() {}
    public Outline(Outline o) { set(o); }
    public void set(Outline o) { mRect = o.mRect == null ? null : new Rect(o.mRect); mRadius = o.mRadius; mAlpha = o.mAlpha; mPath = o.mPath; }
    public void setEmpty() { mRect = null; mPath = null; }
    public boolean isEmpty() { return mRect == null && mPath == null; }
    public boolean canClip() { return true; }
    public void setAlpha(float a) { mAlpha = a; }
    public float getAlpha() { return mAlpha; }
    public void setRect(int l, int t, int r, int b) { mRect = new Rect(l, t, r, b); mRadius = 0; mPath = null; }
    public void setRect(Rect r) { setRect(r.left, r.top, r.right, r.bottom); }
    public void setRoundRect(int l, int t, int r, int b, float radius) { mRect = new Rect(l, t, r, b); mRadius = radius; mPath = null; }
    public void setRoundRect(Rect r, float radius) { setRoundRect(r.left, r.top, r.right, r.bottom, radius); }
    public void setOval(int l, int t, int r, int b) { setRoundRect(l, t, r, b, Math.min(r - l, b - t) / 2f); }
    public void setOval(Rect r) { setOval(r.left, r.top, r.right, r.bottom); }
    public void setPath(Path p) { mPath = p; mRect = null; }
    public void setConvexPath(Path p) { setPath(p); }
    public boolean getRect(Rect out) { if (mRect == null) return false; out.set(mRect); return true; }
    public float getRadius() { return mRadius; }
    public void offset(int dx, int dy) { if (mRect != null) mRect.offset(dx, dy); }
}
