package android.graphics;
/** A nine-patch: stretchable regions from its chunk (res_png9 compiled form). */
public class NinePatch {
    private final Bitmap mBitmap; final int[] xDivs, yDivs; public final Rect padding = new Rect();
    public NinePatch(Bitmap b, byte[] chunk) { this(b, chunk, null); }
    public NinePatch(Bitmap b, byte[] chunk, String src) {
        mBitmap = b;
        int[][] d = parse(chunk, padding);
        xDivs = d[0]; yDivs = d[1];
    }
    public static boolean isNinePatchChunk(byte[] c) { return c != null && c.length >= 32 && c[0] != 0; }
    /** The serialized Res_png_9patch: wasDeserialized, numXDivs, numYDivs, numColors, 2 x int offsets, padding l r t b, color offset, then the divs. */
    static int[][] parse(byte[] c, Rect pad) {
        if (c == null || c.length < 32) return new int[][] { new int[0], new int[0] };
        java.nio.ByteBuffer b = java.nio.ByteBuffer.wrap(c).order(java.nio.ByteOrder.nativeOrder());
        b.get(); int nx = b.get() & 255, ny = b.get() & 255; b.get();
        b.getInt(); b.getInt();
        int pl = b.getInt(), pr = b.getInt(), pt = b.getInt(), pb = b.getInt();
        if (pad != null) pad.set(pl, pt, pr, pb);
        b.getInt();
        int[] xs = new int[nx], ys = new int[ny];
        for (int i = 0; i < nx && b.remaining() >= 4; i++) xs[i] = b.getInt();
        for (int i = 0; i < ny && b.remaining() >= 4; i++) ys[i] = b.getInt();
        return new int[][] { xs, ys };
    }
    public Bitmap getBitmap() { return mBitmap; }
    public int getWidth() { return mBitmap.getWidth(); }
    public int getHeight() { return mBitmap.getHeight(); }
    public final boolean hasAlpha() { return true; }
    public void setPaint(Paint p) {}
    public void draw(Canvas c, RectF dst) { draw(c, dst, null); }
    public void draw(Canvas c, Rect dst) { draw(c, new RectF(dst), null); }
    public void draw(Canvas c, Rect dst, Paint p) { draw(c, new RectF(dst), p); }
    public void draw(Canvas c, RectF dst, Paint p) {
        int w = mBitmap.getWidth(), h = mBitmap.getHeight();
        float[] sx = segs(xDivs, w), sy = segs(yDivs, h);
        float[] dx = place(xDivs, sx, w, dst.left, dst.width()), dy = place(yDivs, sy, h, dst.top, dst.height());
        Rect s = new Rect(); RectF d = new RectF();
        for (int j = 0; j + 1 < sy.length; j++) for (int i = 0; i + 1 < sx.length; i++) {
            s.set((int) sx[i], (int) sy[j], (int) sx[i + 1], (int) sy[j + 1]);
            d.set(dx[i], dy[j], dx[i + 1], dy[j + 1]);
            if (s.width() > 0 && s.height() > 0 && d.width() > 0 && d.height() > 0) c.drawBitmap(mBitmap, s, d, p);
        }
    }
    private static float[] segs(int[] divs, int size) {
        float[] o = new float[divs.length + 2];
        o[0] = 0; for (int i = 0; i < divs.length; i++) o[i + 1] = Math.min(size, Math.max(0, divs[i])); o[o.length - 1] = size;
        return o;
    }
    /** The destination edges: fixed segments keep their size, stretchable (odd) ones share what is left. */
    private static float[] place(int[] divs, float[] s, int size, float start, float len) {
        float fixed = 0, stretch = 0;
        for (int i = 0; i + 1 < s.length; i++) { float l = s[i + 1] - s[i]; if ((i & 1) == 1) stretch += l; else fixed += l; }
        float k = stretch > 0 ? Math.max(0, len - fixed) / stretch : 0, shrink = fixed > len && fixed > 0 ? len / fixed : 1;
        float[] o = new float[s.length];
        o[0] = start;
        for (int i = 0; i + 1 < s.length; i++) { float l = s[i + 1] - s[i]; o[i + 1] = o[i] + ((i & 1) == 1 ? l * k : l * shrink); }
        return o;
    }
}
