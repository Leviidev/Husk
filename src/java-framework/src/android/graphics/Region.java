package android.graphics;
/** A region as its bounding rectangle. */
public class Region {
    public enum Op { DIFFERENCE, INTERSECT, UNION, XOR, REVERSE_DIFFERENCE, REPLACE }
    private final Rect r = new Rect();
    public Region() {}
    public Region(Rect r) { set(r); }
    public Region(int l, int t, int rr, int b) { r.set(l, t, rr, b); }
    public Region(Region o) { r.set(o.r); }
    public void setEmpty() { r.setEmpty(); }
    public boolean set(Region o) { r.set(o.r); return !r.isEmpty(); }
    public boolean set(Rect o) { r.set(o); return !r.isEmpty(); }
    public boolean set(int l, int t, int rr, int b) { r.set(l, t, rr, b); return !r.isEmpty(); }
    public boolean setPath(Path p, Region clip) { RectF b = new RectF(); p.computeBounds(b, true); b.roundOut(r); r.intersect(clip.r); return !r.isEmpty(); }
    public boolean isEmpty() { return r.isEmpty(); }
    public boolean isRect() { return true; }
    public boolean isComplex() { return false; }
    public Rect getBounds() { return new Rect(r); }
    public boolean getBounds(Rect o) { o.set(r); return !r.isEmpty(); }
    public boolean contains(int x, int y) { return r.contains(x, y); }
    public boolean quickContains(Rect o) { return r.contains(o); }
    public boolean quickReject(Rect o) { return !Rect.intersects(r, o); }
    public void translate(int dx, int dy) { r.offset(dx, dy); }
    public boolean op(Rect o, Op op) { if (op == Op.INTERSECT) return r.intersect(o); if (op == Op.UNION) { r.union(o); return true; } if (op == Op.REPLACE) { r.set(o); return true; } return !r.isEmpty(); }
    public boolean op(Region o, Op op) { return op(o.r, op); }
    public boolean op(int l, int t, int rr, int b, Op op) { return op(new Rect(l, t, rr, b), op); }
    public boolean union(Rect o) { r.union(o); return true; }
}
