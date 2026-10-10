package android.graphics;
public final class Rect {
    public int left, top, right, bottom;
    public Rect() {} public Rect(int l, int t, int r, int b) { set(l, t, r, b); } public Rect(Rect o) { set(o.left, o.top, o.right, o.bottom); }
    public void set(int l, int t, int r, int b) { left = l; top = t; right = r; bottom = b; }
    public int width() { return right - left; } public int height() { return bottom - top; }
    public boolean contains(int x, int y) { return x >= left && x < right && y >= top && y < bottom; }
    public boolean isEmpty() { return left >= right || top >= bottom; }
}
