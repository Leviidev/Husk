package android.graphics;

/** A 3x3 transform, as Android keeps it: MSCALE_X, MSKEW_X, MTRANS_X / MSKEW_Y, MSCALE_Y, MTRANS_Y / MPERSP_0..2. */
public class Matrix {
    public static final int MSCALE_X = 0, MSKEW_X = 1, MTRANS_X = 2, MSKEW_Y = 3, MSCALE_Y = 4, MTRANS_Y = 5, MPERSP_0 = 6, MPERSP_1 = 7, MPERSP_2 = 8;
    public enum ScaleToFit { FILL, START, CENTER, END }
    public static final Matrix IDENTITY_MATRIX = new Matrix();

    final float[] m = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

    public Matrix() {}
    public Matrix(Matrix src) { if (src != null) set(src); }

    public boolean isIdentity() { return m[0] == 1 && m[1] == 0 && m[2] == 0 && m[3] == 0 && m[4] == 1 && m[5] == 0 && m[6] == 0 && m[7] == 0 && m[8] == 1; }
    public boolean isAffine() { return m[6] == 0 && m[7] == 0 && m[8] == 1; }
    public boolean rectStaysRect() { return (m[1] == 0 && m[3] == 0) || (m[0] == 0 && m[4] == 0); }
    public void set(Matrix src) { if (src == null) reset(); else System.arraycopy(src.m, 0, m, 0, 9); }
    public void reset() { m[0] = 1; m[1] = 0; m[2] = 0; m[3] = 0; m[4] = 1; m[5] = 0; m[6] = 0; m[7] = 0; m[8] = 1; }
    public void getValues(float[] v) { System.arraycopy(m, 0, v, 0, 9); }
    public void setValues(float[] v) { System.arraycopy(v, 0, m, 0, 9); }
    /** The affine part, as husk.Gfx takes it. */
    public float[] huskAffine() { return new float[] { m[0], m[1], m[2], m[3], m[4], m[5] }; }

    private static void mul(float[] out, float[] a, float[] b) {
        float[] r = new float[9];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) r[3 * i + j] = a[3 * i] * b[j] + a[3 * i + 1] * b[3 + j] + a[3 * i + 2] * b[6 + j];
        System.arraycopy(r, 0, out, 0, 9);
    }
    private static float[] t(float dx, float dy) { return new float[] { 1, 0, dx, 0, 1, dy, 0, 0, 1 }; }
    private static float[] s(float sx, float sy, float px, float py) { return new float[] { sx, 0, px - sx * px, 0, sy, py - sy * py, 0, 0, 1 }; }
    private static float[] r(float deg, float px, float py) {
        double a = Math.toRadians(deg); float c = (float) Math.cos(a), sn = (float) Math.sin(a);
        return new float[] { c, -sn, px - c * px + sn * py, sn, c, py - sn * px - c * py, 0, 0, 1 };
    }
    private static float[] k(float kx, float ky, float px, float py) { return new float[] { 1, kx, -kx * py, ky, 1, -ky * px, 0, 0, 1 }; }

    public void setTranslate(float dx, float dy) { System.arraycopy(t(dx, dy), 0, m, 0, 9); }
    public void setScale(float sx, float sy, float px, float py) { System.arraycopy(s(sx, sy, px, py), 0, m, 0, 9); }
    public void setScale(float sx, float sy) { setScale(sx, sy, 0, 0); }
    public void setRotate(float d, float px, float py) { System.arraycopy(r(d, px, py), 0, m, 0, 9); }
    public void setRotate(float d) { setRotate(d, 0, 0); }
    public void setSinCos(float sin, float cos, float px, float py) { float[] v = { cos, -sin, px - cos * px + sin * py, sin, cos, py - sin * px - cos * py, 0, 0, 1 }; System.arraycopy(v, 0, m, 0, 9); }
    public void setSinCos(float sin, float cos) { setSinCos(sin, cos, 0, 0); }
    public void setSkew(float kx, float ky, float px, float py) { System.arraycopy(k(kx, ky, px, py), 0, m, 0, 9); }
    public void setSkew(float kx, float ky) { setSkew(kx, ky, 0, 0); }
    public boolean setConcat(Matrix a, Matrix b) { mul(m, a.m, b.m); return true; }
    public boolean preTranslate(float dx, float dy) { mul(m, m, t(dx, dy)); return true; }
    public boolean preScale(float sx, float sy, float px, float py) { mul(m, m, s(sx, sy, px, py)); return true; }
    public boolean preScale(float sx, float sy) { return preScale(sx, sy, 0, 0); }
    public boolean preRotate(float d, float px, float py) { mul(m, m, r(d, px, py)); return true; }
    public boolean preRotate(float d) { return preRotate(d, 0, 0); }
    public boolean preSkew(float kx, float ky, float px, float py) { mul(m, m, k(kx, ky, px, py)); return true; }
    public boolean preSkew(float kx, float ky) { return preSkew(kx, ky, 0, 0); }
    public boolean preConcat(Matrix o) { mul(m, m, o.m); return true; }
    public boolean postTranslate(float dx, float dy) { mul(m, t(dx, dy), m); return true; }
    public boolean postScale(float sx, float sy, float px, float py) { mul(m, s(sx, sy, px, py), m); return true; }
    public boolean postScale(float sx, float sy) { return postScale(sx, sy, 0, 0); }
    public boolean postRotate(float d, float px, float py) { mul(m, r(d, px, py), m); return true; }
    public boolean postRotate(float d) { return postRotate(d, 0, 0); }
    public boolean postSkew(float kx, float ky, float px, float py) { mul(m, k(kx, ky, px, py), m); return true; }
    public boolean postSkew(float kx, float ky) { return postSkew(kx, ky, 0, 0); }
    public boolean postConcat(Matrix o) { mul(m, o.m, m); return true; }

    public boolean invert(Matrix inverse) {
        float a = m[0], b = m[1], c = m[2], d = m[3], e = m[4], f = m[5], g = m[6], h = m[7], i = m[8];
        float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
        float det = a * A + b * B + c * C;
        if (det == 0 || Float.isNaN(det)) return false;
        if (inverse != null) {
            float[] v = { A, -(b * i - c * h), b * f - c * e, B, a * i - c * g, -(a * f - c * d), C, -(a * h - b * g), a * e - b * d };
            for (int k = 0; k < 9; k++) v[k] /= det;
            System.arraycopy(v, 0, inverse.m, 0, 9);
        }
        return true;
    }
    public void mapPoints(float[] dst, int dstIndex, float[] src, int srcIndex, int pointCount) {
        for (int p = 0; p < pointCount; p++) {
            float x = src[srcIndex + 2 * p], y = src[srcIndex + 2 * p + 1];
            float w = m[6] * x + m[7] * y + m[8]; if (w == 0) w = 1;
            dst[dstIndex + 2 * p] = (m[0] * x + m[1] * y + m[2]) / w;
            dst[dstIndex + 2 * p + 1] = (m[3] * x + m[4] * y + m[5]) / w;
        }
    }
    public void mapPoints(float[] dst, float[] src) { mapPoints(dst, 0, src, 0, src.length / 2); }
    public void mapPoints(float[] pts) { mapPoints(pts, 0, pts, 0, pts.length / 2); }
    public void mapVectors(float[] dst, int dstIndex, float[] src, int srcIndex, int n) {
        for (int p = 0; p < n; p++) {
            float x = src[srcIndex + 2 * p], y = src[srcIndex + 2 * p + 1];
            dst[dstIndex + 2 * p] = m[0] * x + m[1] * y; dst[dstIndex + 2 * p + 1] = m[3] * x + m[4] * y;
        }
    }
    public void mapVectors(float[] v) { mapVectors(v, 0, v, 0, v.length / 2); }
    public void mapVectors(float[] dst, float[] src) { mapVectors(dst, 0, src, 0, src.length / 2); }
    public boolean mapRect(RectF dst, RectF src) {
        float[] p = { src.left, src.top, src.right, src.top, src.right, src.bottom, src.left, src.bottom };
        mapPoints(p);
        float l = Math.min(Math.min(p[0], p[2]), Math.min(p[4], p[6])), r = Math.max(Math.max(p[0], p[2]), Math.max(p[4], p[6]));
        float t = Math.min(Math.min(p[1], p[3]), Math.min(p[5], p[7])), b = Math.max(Math.max(p[1], p[3]), Math.max(p[5], p[7]));
        dst.set(l, t, r, b);
        return rectStaysRect();
    }
    public boolean mapRect(RectF r) { return mapRect(r, r); }
    public float mapRadius(float radius) { float[] v = { radius, 0, 0, radius }; mapVectors(v); return (float) Math.sqrt(Math.hypot(v[0], v[1]) * Math.hypot(v[2], v[3])); }
    public boolean setRectToRect(RectF src, RectF dst, ScaleToFit stf) {
        if (src.isEmpty()) { reset(); return false; }
        float sx = dst.width() / src.width(), sy = dst.height() / src.height();
        float tx, ty;
        if (stf == ScaleToFit.FILL) { tx = dst.left - src.left * sx; ty = dst.top - src.top * sy; }
        else {
            float sc = Math.min(sx, sy); sx = sy = sc;
            float dw = src.width() * sc, dh = src.height() * sc;
            float ox = 0, oy = 0;
            if (stf == ScaleToFit.CENTER) { ox = (dst.width() - dw) / 2; oy = (dst.height() - dh) / 2; }
            else if (stf == ScaleToFit.END) { ox = dst.width() - dw; oy = dst.height() - dh; }
            tx = dst.left + ox - src.left * sc; ty = dst.top + oy - src.top * sc;
        }
        float[] v = { sx, 0, tx, 0, sy, ty, 0, 0, 1 };
        System.arraycopy(v, 0, m, 0, 9);
        return true;
    }
    public boolean setPolyToPoly(float[] src, int srcIndex, float[] dst, int dstIndex, int pointCount) {
        if (pointCount == 0) { reset(); return true; }
        if (pointCount == 1) { setTranslate(dst[dstIndex] - src[srcIndex], dst[dstIndex + 1] - src[srcIndex + 1]); return true; }
        // two or more points: a similarity from the first two (enough for the uses seen)
        float sx0 = src[srcIndex], sy0 = src[srcIndex + 1], sx1 = src[srcIndex + 2], sy1 = src[srcIndex + 3];
        float dx0 = dst[dstIndex], dy0 = dst[dstIndex + 1], dx1 = dst[dstIndex + 2], dy1 = dst[dstIndex + 3];
        float svx = sx1 - sx0, svy = sy1 - sy0, dvx = dx1 - dx0, dvy = dy1 - dy0;
        float sl = svx * svx + svy * svy; if (sl == 0) return false;
        float a = (svx * dvx + svy * dvy) / sl, b = (svx * dvy - svy * dvx) / sl;
        float[] v = { a, -b, dx0 - (a * sx0 - b * sy0), b, a, dy0 - (b * sx0 + a * sy0), 0, 0, 1 };
        System.arraycopy(v, 0, m, 0, 9);
        return true;
    }
    @Override public boolean equals(Object o) { return o instanceof Matrix && java.util.Arrays.equals(m, ((Matrix) o).m); }
    @Override public int hashCode() { return java.util.Arrays.hashCode(m); }
    @Override public String toString() { return "Matrix" + java.util.Arrays.toString(m); }
    public String toShortString() { return java.util.Arrays.toString(m); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void dump(java.io.PrintWriter p0) {}
    public long ni() { return 0L; }
    // ---- end of generated members
}
