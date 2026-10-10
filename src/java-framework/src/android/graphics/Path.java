package android.graphics;

/** A path, kept as the op stream husk.Gfx draws (see husk-tl-dvm-graphics.c: OP_*). */
public class Path {
    public enum FillType { WINDING, EVEN_ODD, INVERSE_WINDING, INVERSE_EVEN_ODD }
    public enum Direction { CW, CCW }
    public enum Op { DIFFERENCE, INTERSECT, UNION, XOR, REVERSE_DIFFERENCE }
    static final int MOVE = 0, LINE = 1, QUAD = 2, CUBIC = 3, CLOSE = 4, ARC = 5, RECT = 6, OVAL = 7, RRECT = 8, TRANSFORM = 9,
        RMOVE = 10, RLINE = 11, RQUAD = 12, RCUBIC = 13, RRECT8 = 14;

    float[] ops = new float[32];
    int n;
    private FillType fill = FillType.WINDING;
    private float lastX, lastY;
    private boolean any;

    public Path() {}
    public Path(Path src) { set(src); }

    private void put(int op, float... v) {
        if (n + 1 + v.length > ops.length) ops = java.util.Arrays.copyOf(ops, Math.max(ops.length * 2, n + 1 + v.length + 16));
        ops[n++] = op;
        for (float f : v) ops[n++] = f;
        any = true;
    }
    public void set(Path src) { if (src == this) return; ops = src.ops.clone(); n = src.n; fill = src.fill; lastX = src.lastX; lastY = src.lastY; any = src.any; }
    public void reset() { n = 0; any = false; lastX = lastY = 0; }
    public void rewind() { reset(); }
    public boolean isEmpty() { return !any; }
    public boolean isConvex() { return false; }
    public boolean isRect(RectF r) { return false; }
    public boolean isInverseFillType() { return fill == FillType.INVERSE_WINDING || fill == FillType.INVERSE_EVEN_ODD; }
    public void toggleInverseFillType() {}
    public FillType getFillType() { return fill; }
    public void setFillType(FillType f) { fill = f; }
    public void incReserve(int extra) {}
    public void moveTo(float x, float y) { put(MOVE, x, y); lastX = x; lastY = y; }
    public void rMoveTo(float dx, float dy) { put(RMOVE, dx, dy); lastX += dx; lastY += dy; }
    public void lineTo(float x, float y) { put(LINE, x, y); lastX = x; lastY = y; }
    public void rLineTo(float dx, float dy) { put(RLINE, dx, dy); lastX += dx; lastY += dy; }
    public void quadTo(float x1, float y1, float x2, float y2) { put(QUAD, x1, y1, x2, y2); lastX = x2; lastY = y2; }
    public void rQuadTo(float dx1, float dy1, float dx2, float dy2) { put(RQUAD, dx1, dy1, dx2, dy2); lastX += dx2; lastY += dy2; }
    public void conicTo(float x1, float y1, float x2, float y2, float w) { quadTo(x1, y1, x2, y2); }
    public void rConicTo(float dx1, float dy1, float dx2, float dy2, float w) { rQuadTo(dx1, dy1, dx2, dy2); }
    public void cubicTo(float x1, float y1, float x2, float y2, float x3, float y3) { put(CUBIC, x1, y1, x2, y2, x3, y3); lastX = x3; lastY = y3; }
    public void rCubicTo(float x1, float y1, float x2, float y2, float x3, float y3) { put(RCUBIC, x1, y1, x2, y2, x3, y3); lastX += x3; lastY += y3; }
    public void arcTo(RectF o, float start, float sweep, boolean forceMoveTo) { arcTo(o.left, o.top, o.right, o.bottom, start, sweep, forceMoveTo); }
    public void arcTo(RectF o, float start, float sweep) { arcTo(o, start, sweep, false); }
    public void arcTo(float l, float t, float r, float b, float start, float sweep, boolean forceMoveTo) {
        put(ARC, l, t, r, b, start, sweep, forceMoveTo ? 1 : 0);
        double a = Math.toRadians(start + sweep);
        lastX = (float) ((l + r) / 2 + (r - l) / 2 * Math.cos(a)); lastY = (float) ((t + b) / 2 + (b - t) / 2 * Math.sin(a));
    }
    public void close() { put(CLOSE); }
    public void addRect(RectF r, Direction d) { addRect(r.left, r.top, r.right, r.bottom, d); }
    public void addRect(float l, float t, float r, float b, Direction d) { put(RECT, l, t, r, b, d == Direction.CCW ? 1 : 0); }
    public void addOval(RectF o, Direction d) { addOval(o.left, o.top, o.right, o.bottom, d); }
    public void addOval(float l, float t, float r, float b, Direction d) { put(OVAL, l, t, r, b, d == Direction.CCW ? 1 : 0); }
    public void addCircle(float x, float y, float radius, Direction d) { addOval(x - radius, y - radius, x + radius, y + radius, d); }
    public void addArc(RectF o, float start, float sweep) { addArc(o.left, o.top, o.right, o.bottom, start, sweep); }
    public void addArc(float l, float t, float r, float b, float start, float sweep) { put(ARC, l, t, r, b, start, sweep, 1); }
    public void addRoundRect(RectF r, float rx, float ry, Direction d) { addRoundRect(r.left, r.top, r.right, r.bottom, rx, ry, d); }
    public void addRoundRect(float l, float t, float r, float b, float rx, float ry, Direction d) { put(RRECT, l, t, r, b, rx, ry, d == Direction.CCW ? 1 : 0); }
    public void addRoundRect(RectF r, float[] radii, Direction d) { addRoundRect(r.left, r.top, r.right, r.bottom, radii, d); }
    public void addRoundRect(float l, float t, float r, float b, float[] radii, Direction d) {
        put(RRECT8, l, t, r, b, radii[0], radii[1], radii[2], radii[3], radii[4], radii[5], radii[6], radii[7], d == Direction.CCW ? 1 : 0);
    }
    public void addPath(Path src) { if (src.n == 0) return; float[] o = java.util.Arrays.copyOf(src.ops, src.n); appendRaw(o); }
    public void addPath(Path src, float dx, float dy) { Path p = new Path(src); p.offset(dx, dy); addPath(p); }
    public void addPath(Path src, Matrix m) { Path p = new Path(src); p.transform(m); addPath(p); }
    private void appendRaw(float[] o) {
        if (n + o.length > ops.length) ops = java.util.Arrays.copyOf(ops, n + o.length + 16);
        System.arraycopy(o, 0, ops, n, o.length); n += o.length; any = true;
    }
    public void offset(float dx, float dy) { transform(translate(dx, dy)); }
    public void offset(float dx, float dy, Path dst) { if (dst != null) { dst.set(this); dst.offset(dx, dy); } else offset(dx, dy); }
    private static Matrix translate(float dx, float dy) { Matrix m = new Matrix(); m.setTranslate(dx, dy); return m; }
    public void transform(Matrix m) { if (m == null || m.isIdentity() || n == 0) return; put(TRANSFORM, m.m[0], m.m[1], m.m[2], m.m[3], m.m[4], m.m[5]); float[] p = { lastX, lastY }; m.mapPoints(p); lastX = p[0]; lastY = p[1]; }
    public void transform(Matrix m, Path dst) { if (dst != null) { dst.set(this); dst.transform(m); } else transform(m); }
    public void setLastPoint(float x, float y) { lastX = x; lastY = y; }
    public void computeBounds(RectF bounds, boolean exact) {
        if (n == 0) { bounds.set(0, 0, 0, 0); return; }
        float[] o = new float[4];
        husk.Gfx.pathBounds(ops, n, o);
        bounds.set(o[0], o[1], o[2], o[3]);
    }
    public boolean op(Path p, Op op) { if (op == Op.UNION) { addPath(p); return true; } return false; }
    public boolean op(Path a, Path b, Op op) { set(a); return op(b, op); }
    public float[] approximate(float acceptableError) { return new float[0]; }
    /** For Canvas and Region: the stream and its length. */
    public int huskFill() { return fill == FillType.EVEN_ODD || fill == FillType.INVERSE_EVEN_ODD ? 1 : 0; }
}
