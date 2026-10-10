package android.graphics;

/** Drawing into a Bitmap with CoreGraphics (husk.Gfx). The view system draws the window through one of these each frame. */
public class Canvas {
    public static final int ALL_SAVE_FLAG = 0x1F, MATRIX_SAVE_FLAG = 1, CLIP_SAVE_FLAG = 2, HAS_ALPHA_LAYER_SAVE_FLAG = 4, FULL_COLOR_LAYER_SAVE_FLAG = 8,
        CLIP_TO_LAYER_SAVE_FLAG = 16;
    public enum EdgeType { BW, AA }
    public enum VertexMode { TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN }

    private long mNative;
    private Bitmap mBitmap;
    private int mDensity = Bitmap.DENSITY_NONE;
    private final float[] mAff = new float[6];
    private final int[] mClip = new int[4];
    private static final Paint sBitmapPaint = null;

    public Canvas() {}
    public Canvas(Bitmap b) { setBitmap(b); }
    public long huskNative() { return mNative; }

    public void setBitmap(Bitmap b) {
        if (mNative != 0) { husk.Gfx.cvFree(mNative); mNative = 0; }
        mBitmap = b;
        if (b != null) {
            if (!b.isMutable() && false) throw new IllegalStateException("Immutable bitmap passed to Canvas constructor");
            mNative = husk.Gfx.cvNew(b.huskNative());
            mDensity = b.getDensity();
        }
    }
    /** Husk: done with this canvas (its native state is freed; the bitmap is kept). */
    public void huskRelease() { setBitmap(null); }
    @Override protected void finalize() throws Throwable { if (mNative != 0) husk.Gfx.cvFree(mNative); mNative = 0; super.finalize(); }

    public boolean isHardwareAccelerated() { return false; }
    public boolean isOpaque() { return false; }
    public int getWidth() { return mBitmap == null ? 0 : mBitmap.getWidth(); }
    public int getHeight() { return mBitmap == null ? 0 : mBitmap.getHeight(); }
    public int getDensity() { return mDensity; }
    public void setDensity(int d) { mDensity = d; }
    public void setScreenDensity(int d) {}
    public int getMaximumBitmapWidth() { return 16384; }
    public int getMaximumBitmapHeight() { return 16384; }
    public void enableZ() {}
    public void disableZ() {}
    public DrawFilter getDrawFilter() { return null; }
    public void setDrawFilter(DrawFilter f) {}

    private void touched() { if (mBitmap != null) mBitmap.huskTouched(); }

    // ---- state
    public int save() { return mNative == 0 ? 1 : husk.Gfx.cvSave(mNative); }
    public int save(int flags) { return save(); }
    public int saveLayer(RectF b, Paint p, int flags) { return saveLayer(b, p); }
    public int saveLayer(RectF b, Paint p) { return b == null ? saveLayerAlpha(0, 0, 0, 0, p == null ? 255 : p.getAlpha()) : saveLayerAlpha(b.left, b.top, b.right, b.bottom, p == null ? 255 : p.getAlpha()); }
    public int saveLayer(float l, float t, float r, float b, Paint p, int flags) { return saveLayerAlpha(l, t, r, b, p == null ? 255 : p.getAlpha()); }
    public int saveLayer(float l, float t, float r, float b, Paint p) { return saveLayer(l, t, r, b, p, ALL_SAVE_FLAG); }
    public int saveLayerAlpha(RectF b, int alpha, int flags) { return saveLayerAlpha(b, alpha); }
    public int saveLayerAlpha(RectF b, int alpha) { return b == null ? saveLayerAlpha(0, 0, 0, 0, alpha) : saveLayerAlpha(b.left, b.top, b.right, b.bottom, alpha); }
    public int saveLayerAlpha(float l, float t, float r, float b, int alpha, int flags) { return saveLayerAlpha(l, t, r, b, alpha); }
    public int saveLayerAlpha(float l, float t, float r, float b, int alpha) { return mNative == 0 ? 1 : husk.Gfx.cvSaveLayer(mNative, l, t, r, b, Math.max(0, Math.min(255, alpha))); }
    public void restore() { if (mNative != 0) husk.Gfx.cvRestore(mNative); }
    public int getSaveCount() { return mNative == 0 ? 1 : husk.Gfx.cvSaveCount(mNative); }
    public void restoreToCount(int n) { if (mNative != 0) husk.Gfx.cvRestoreTo(mNative, Math.max(1, n)); }

    // ---- matrix
    private void concat6(float a, float b, float c, float d, float e, float f) {
        if (mNative == 0) return;
        mAff[0] = a; mAff[1] = b; mAff[2] = c; mAff[3] = d; mAff[4] = e; mAff[5] = f;
        husk.Gfx.cvMatrix(mNative, mAff, false);
    }
    public void translate(float dx, float dy) { if (dx != 0 || dy != 0) concat6(1, 0, dx, 0, 1, dy); }
    public void scale(float sx, float sy) { if (sx != 1 || sy != 1) concat6(sx, 0, 0, 0, sy, 0); }
    public final void scale(float sx, float sy, float px, float py) { translate(px, py); scale(sx, sy); translate(-px, -py); }
    public void rotate(float deg) { if (deg != 0) { double r = Math.toRadians(deg); float c = (float) Math.cos(r), s = (float) Math.sin(r); concat6(c, -s, 0, s, c, 0); } }
    public final void rotate(float deg, float px, float py) { translate(px, py); rotate(deg); translate(-px, -py); }
    public void skew(float sx, float sy) { concat6(1, sx, 0, sy, 1, 0); }
    public void concat(Matrix m) { if (m != null && !m.isIdentity() && mNative != 0) husk.Gfx.cvMatrix(mNative, m.huskAffine(), false); }
    public void setMatrix(Matrix m) { if (mNative != 0) husk.Gfx.cvMatrix(mNative, (m == null ? new Matrix() : m).huskAffine(), true); }
    public void getMatrix(Matrix m) {
        if (mNative == 0) { m.reset(); return; }
        husk.Gfx.cvGetMatrix(mNative, mAff);
        m.setValues(new float[] { mAff[0], mAff[1], mAff[2], mAff[3], mAff[4], mAff[5], 0, 0, 1 });
    }
    public final Matrix getMatrix() { Matrix m = new Matrix(); getMatrix(m); return m; }

    // ---- clip
    public boolean clipRect(float l, float t, float r, float b) { return mNative == 0 || husk.Gfx.cvClipRect(mNative, l, t, r, b, 1); }
    public boolean clipRect(int l, int t, int r, int b) { return clipRect((float) l, t, r, b); }
    public boolean clipRect(RectF r) { return clipRect(r.left, r.top, r.right, r.bottom); }
    public boolean clipRect(Rect r) { return clipRect((float) r.left, r.top, r.right, r.bottom); }
    public boolean clipRect(RectF r, Region.Op op) { return op == Region.Op.DIFFERENCE ? clipOutRect(r) : clipRect(r); }
    public boolean clipRect(Rect r, Region.Op op) { return op == Region.Op.DIFFERENCE ? clipOutRect(r) : clipRect(r); }
    public boolean clipRect(float l, float t, float r, float b, Region.Op op) { return op == Region.Op.DIFFERENCE ? clipOutRect(l, t, r, b) : clipRect(l, t, r, b); }
    public boolean clipOutRect(float l, float t, float r, float b) { return mNative == 0 || husk.Gfx.cvClipRect(mNative, l, t, r, b, 0); }
    public boolean clipOutRect(int l, int t, int r, int b) { return clipOutRect((float) l, t, r, b); }
    public boolean clipOutRect(RectF r) { return clipOutRect(r.left, r.top, r.right, r.bottom); }
    public boolean clipOutRect(Rect r) { return clipOutRect((float) r.left, r.top, r.right, r.bottom); }
    public boolean clipPath(Path p) { return mNative == 0 || husk.Gfx.cvClipPath(mNative, p.ops, p.n, p.huskFill(), 1); }
    public boolean clipPath(Path p, Region.Op op) { return op == Region.Op.DIFFERENCE ? clipOutPath(p) : clipPath(p); }
    public boolean clipOutPath(Path p) { return mNative == 0 || husk.Gfx.cvClipPath(mNative, p.ops, p.n, p.huskFill(), 0); }
    public boolean getClipBounds(Rect r) {
        boolean ok = mNative != 0 && husk.Gfx.cvClipBounds(mNative, mClip);
        if (r != null) r.set(mClip[0], mClip[1], mClip[2], mClip[3]);
        return ok;
    }
    public final Rect getClipBounds() { Rect r = new Rect(); getClipBounds(r); return r; }
    public boolean quickReject(RectF r, EdgeType t) { return quickReject(r.left, r.top, r.right, r.bottom); }
    public boolean quickReject(RectF r) { return quickReject(r.left, r.top, r.right, r.bottom); }
    public boolean quickReject(Path p, EdgeType t) { return false; }
    public boolean quickReject(Path p) { return false; }
    public boolean quickReject(float l, float t, float r, float b, EdgeType e) { return quickReject(l, t, r, b); }
    public boolean quickReject(float l, float t, float r, float b) {
        // only when the matrix is a translation and scale: the clip bounds are in the current coordinates
        if (mNative == 0 || !husk.Gfx.cvClipBounds(mNative, mClip)) return true;
        return r <= mClip[0] || l >= mClip[2] || b <= mClip[1] || t >= mClip[3];
    }

    // ---- drawing
    public void drawColor(int c) { if (mNative != 0) { husk.Gfx.cvDrawColor(mNative, c, 3); touched(); } }
    public void drawColor(long c) { drawColor(Color.toArgb(c)); }
    public void drawColor(int c, PorterDuff.Mode m) { if (mNative != 0) { husk.Gfx.cvDrawColor(mNative, c, m.nativeInt); touched(); } }
    public void drawColor(int c, BlendMode m) { if (mNative != 0) { husk.Gfx.cvDrawColor(mNative, c, m.n); touched(); } }
    public void drawRGB(int r, int g, int b) { drawColor(Color.rgb(r, g, b)); }
    public void drawARGB(int a, int r, int g, int b) { drawColor(Color.argb(a, r, g, b)); }
    public void drawPaint(Paint p) {
        if (mNative == 0) return;
        husk.Gfx.cvClipBounds(mNative, mClip);
        husk.Gfx.cvDrawRect(mNative, mClip[0], mClip[1], mClip[2], mClip[3], p.mP, p.huskShader());
        touched();
    }
    public void drawRect(float l, float t, float r, float b, Paint p) { if (mNative != 0) { husk.Gfx.cvDrawRect(mNative, l, t, r, b, p.mP, p.huskShader()); touched(); } }
    public void drawRect(RectF r, Paint p) { drawRect(r.left, r.top, r.right, r.bottom, p); }
    public void drawRect(Rect r, Paint p) { drawRect(r.left, r.top, r.right, r.bottom, p); }
    public void drawRoundRect(float l, float t, float r, float b, float rx, float ry, Paint p) { if (mNative != 0) { husk.Gfx.cvDrawRRect(mNative, l, t, r, b, rx, ry, p.mP, p.huskShader()); touched(); } }
    public void drawRoundRect(RectF r, float rx, float ry, Paint p) { drawRoundRect(r.left, r.top, r.right, r.bottom, rx, ry, p); }
    public void drawDoubleRoundRect(RectF outer, float orx, float ory, RectF inner, float irx, float iry, Paint p) {
        Path path = new Path();
        path.addRoundRect(outer, orx, ory, Path.Direction.CW);
        path.addRoundRect(inner, irx, iry, Path.Direction.CCW);
        path.setFillType(Path.FillType.EVEN_ODD);
        drawPath(path, p);
    }
    public void drawOval(float l, float t, float r, float b, Paint p) { if (mNative != 0) { husk.Gfx.cvDrawOval(mNative, l, t, r, b, p.mP, p.huskShader()); touched(); } }
    public void drawOval(RectF r, Paint p) { drawOval(r.left, r.top, r.right, r.bottom, p); }
    public void drawCircle(float cx, float cy, float radius, Paint p) { drawOval(cx - radius, cy - radius, cx + radius, cy + radius, p); }
    public void drawArc(float l, float t, float r, float b, float start, float sweep, boolean useCenter, Paint p) {
        if (mNative != 0) { husk.Gfx.cvDrawArc(mNative, l, t, r, b, start, sweep, useCenter, p.mP, p.huskShader()); touched(); }
    }
    public void drawArc(RectF o, float start, float sweep, boolean useCenter, Paint p) { drawArc(o.left, o.top, o.right, o.bottom, start, sweep, useCenter, p); }
    public void drawPath(Path path, Paint p) { if (mNative != 0 && path.n > 0) { husk.Gfx.cvDrawPath(mNative, path.ops, path.n, path.huskFill(), p.mP, p.huskShader()); touched(); } }
    public void drawLine(float x0, float y0, float x1, float y1, Paint p) { drawLines(new float[] { x0, y0, x1, y1 }, 0, 4, p); }
    public void drawLines(float[] pts, int offset, int count, Paint p) { if (mNative != 0) { husk.Gfx.cvDrawLines(mNative, pts, offset, count, p.mP, p.huskShader(), false); touched(); } }
    public void drawLines(float[] pts, Paint p) { drawLines(pts, 0, pts.length, p); }
    public void drawPoint(float x, float y, Paint p) { drawPoints(new float[] { x, y }, 0, 2, p); }
    public void drawPoints(float[] pts, int offset, int count, Paint p) { if (mNative != 0) { husk.Gfx.cvDrawLines(mNative, pts, offset, count, p.mP, p.huskShader(), true); touched(); } }
    public void drawPoints(float[] pts, Paint p) { drawPoints(pts, 0, pts.length, p); }

    // ---- bitmaps
    private static int[] paintOf(Paint p) { return p == null ? null : p.mP; }
    public void drawBitmap(Bitmap b, float left, float top, Paint p) {
        if (mNative == 0 || b == null || b.isRecycled()) return;
        float w = b.getWidth(), h = b.getHeight();
        if (mDensity != 0 && b.getDensity() != 0 && mDensity != b.getDensity()) { w = b.getScaledWidth(mDensity); h = b.getScaledHeight(mDensity); }
        husk.Gfx.cvDrawBitmap(mNative, b.huskNative(), 0, 0, b.getWidth(), b.getHeight(), left, top, left + w, top + h, paintOf(p));
        touched();
    }
    public void drawBitmap(Bitmap b, Rect src, RectF dst, Paint p) {
        if (mNative == 0 || b == null || b.isRecycled()) return;
        float sl = 0, st = 0, sr = b.getWidth(), sb = b.getHeight();
        if (src != null) { sl = src.left; st = src.top; sr = src.right; sb = src.bottom; }
        husk.Gfx.cvDrawBitmap(mNative, b.huskNative(), sl, st, sr, sb, dst.left, dst.top, dst.right, dst.bottom, paintOf(p));
        touched();
    }
    public void drawBitmap(Bitmap b, Rect src, Rect dst, Paint p) { drawBitmap(b, src, new RectF(dst), p); }
    public void drawBitmap(Bitmap b, Matrix m, Paint p) {
        if (mNative == 0 || b == null || b.isRecycled()) return;
        husk.Gfx.cvDrawBitmapM(mNative, b.huskNative(), (m == null ? new Matrix() : m).huskAffine(), paintOf(p));
        touched();
    }
    public void drawBitmap(int[] colors, int offset, int stride, float x, float y, int w, int h, boolean alpha, Paint p) {
        if (w <= 0 || h <= 0) return;
        Bitmap b = Bitmap.createBitmap(colors, offset, stride, w, h, Bitmap.Config.ARGB_8888);
        drawBitmap(b, x, y, p);
        b.recycle();
    }
    public void drawBitmap(int[] colors, int offset, int stride, int x, int y, int w, int h, boolean alpha, Paint p) { drawBitmap(colors, offset, stride, (float) x, (float) y, w, h, alpha, p); }
    public void drawBitmapMesh(Bitmap b, int mw, int mh, float[] verts, int vo, int[] colors, int co, Paint p) { drawBitmap(b, 0, 0, p); }
    public void drawPicture(Picture pic) { if (pic != null) pic.draw(this); }
    public void drawPicture(Picture pic, RectF dst) { save(); translate(dst.left, dst.top); if (pic.getWidth() > 0 && pic.getHeight() > 0) scale(dst.width() / pic.getWidth(), dst.height() / pic.getHeight()); drawPicture(pic); restore(); }
    public void drawPicture(Picture pic, Rect dst) { drawPicture(pic, new RectF(dst)); }
    public void drawVertices(VertexMode mode, int vc, float[] verts, int vo, float[] tex, int to, int[] colors, int co, short[] idx, int io, int ic, Paint p) {}
    public void drawRenderNode(android.graphics.RenderNode n) {}

    // ---- text
    public void drawText(String s, float x, float y, Paint p) { drawText(s, 0, s.length(), x, y, p); }
    public void drawText(String s, int start, int end, float x, float y, Paint p) {
        if (mNative == 0 || s == null || end <= start) return;
        husk.Gfx.cvDrawText(mNative, s, start, end, x, y, p.mP, p.huskFace(), p.huskShader());
        touched();
    }
    public void drawText(CharSequence s, int start, int end, float x, float y, Paint p) { drawText(s.toString(), start, end, x, y, p); }
    public void drawText(char[] t, int index, int count, float x, float y, Paint p) { drawText(new String(t, index, count), x, y, p); }
    public void drawTextRun(CharSequence s, int start, int end, int cs, int ce, float x, float y, boolean rtl, Paint p) { drawText(s.toString(), start, end, x, y, p); }
    public void drawTextRun(char[] t, int index, int count, int cs, int cc, float x, float y, boolean rtl, Paint p) { drawText(t, index, count, x, y, p); }
    public void drawTextOnPath(String s, Path path, float h, float v, Paint p) { drawText(s, h, v, p); }
    public void drawTextOnPath(char[] t, int index, int count, Path path, float h, float v, Paint p) { drawText(t, index, count, h, v, p); }
    public void drawPosText(char[] t, int index, int count, float[] pos, Paint p) { for (int i = 0; i < count; i++) drawText(t, index + i, 1, pos[2 * i], pos[2 * i + 1], p); }
    public void drawPosText(String s, float[] pos, Paint p) { drawPosText(s.toCharArray(), 0, s.length(), pos, p); }
    public void drawGlyphs(int[] ids, int io, float[] pos, int po, int n, android.graphics.text.Font font, Paint p) {}
}
