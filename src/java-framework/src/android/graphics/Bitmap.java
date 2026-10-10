package android.graphics;

/** Pixels in native memory (husk.Gfx): premultiplied RGBA, rows top first. Every config is kept as ARGB_8888. */
public final class Bitmap implements android.os.Parcelable {
    public static final int DENSITY_NONE = 0;
    public enum Config { ALPHA_8, RGB_565, ARGB_4444, ARGB_8888, RGBA_F16, HARDWARE, RGBA_1010102 }
    public enum CompressFormat { JPEG, PNG, WEBP, WEBP_LOSSY, WEBP_LOSSLESS }

    private long mNative;
    private int mWidth, mHeight;
    private Config mConfig;
    private boolean mMutable, mRecycled, mHasAlpha = true, mPremultiplied = true;
    private int mDensity = android.util.DisplayMetrics.DENSITY_DEVICE_STABLE;
    private int mGeneration;

    Bitmap(long nat, int w, int h, Config c, boolean mutable) { mNative = nat; mWidth = w; mHeight = h; mConfig = c == null ? Config.ARGB_8888 : c; mMutable = mutable; }
    /** The native bitmap. */
    public long huskNative() { return mNative; }
    /** Changes each time it is drawn into, for caches. */
    public int getGenerationId() { return mGeneration; }
    void huskTouched() { mGeneration++; }

    public static Bitmap createBitmap(int w, int h, Config c) {
        if (w <= 0 || h <= 0) throw new IllegalArgumentException("width and height must be > 0");
        long n = husk.Gfx.bmNew(w, h);
        if (n == 0) throw new OutOfMemoryError("Bitmap " + w + "x" + h);
        return new Bitmap(n, w, h, c, true);
    }
    public static Bitmap createBitmap(int w, int h, Config c, boolean hasAlpha) { Bitmap b = createBitmap(w, h, c); b.mHasAlpha = hasAlpha; return b; }
    public static Bitmap createBitmap(android.util.DisplayMetrics d, int w, int h, Config c) { return createBitmap(w, h, c); }
    public static Bitmap createBitmap(int w, int h, Config c, boolean hasAlpha, ColorSpace cs) { return createBitmap(w, h, c, hasAlpha); }
    public static Bitmap createBitmap(int[] colors, int offset, int stride, int w, int h, Config c) {
        Bitmap b = createBitmap(w, h, c);
        b.setPixels(colors, offset, stride, 0, 0, w, h);
        b.mMutable = false;
        return b;
    }
    public static Bitmap createBitmap(int[] colors, int w, int h, Config c) { return createBitmap(colors, 0, w, w, h, c); }
    public static Bitmap createBitmap(android.util.DisplayMetrics d, int[] colors, int w, int h, Config c) { return createBitmap(colors, w, h, c); }
    public static Bitmap createBitmap(Bitmap src) { return createBitmap(src, 0, 0, src.mWidth, src.mHeight); }
    public static Bitmap createBitmap(Bitmap src, int x, int y, int w, int h) { return createBitmap(src, x, y, w, h, null, false); }
    public static Bitmap createBitmap(Bitmap src, int x, int y, int w, int h, Matrix m, boolean filter) {
        if (x < 0 || y < 0 || x + w > src.mWidth || y + h > src.mHeight || w <= 0 || h <= 0) throw new IllegalArgumentException("x + width must be <= bitmap.width()");
        long n = husk.Gfx.bmCopy(src.mNative, x, y, w, h, m == null || m.isIdentity() ? null : m.huskAffine(), filter);
        if (n == 0) throw new OutOfMemoryError();
        float[] wh = { w, h };
        int nw = w, nh = h;
        if (m != null && !m.isIdentity()) { RectF r = new RectF(0, 0, w, h); m.mapRect(r); nw = Math.max(1, Math.round(r.width())); nh = Math.max(1, Math.round(r.height())); }
        return new Bitmap(n, nw, nh, src.mConfig, false);
    }
    public static Bitmap createScaledBitmap(Bitmap src, int dw, int dh, boolean filter) {
        if (dw == src.mWidth && dh == src.mHeight) return src;
        Matrix m = new Matrix();
        m.setScale(dw / (float) src.mWidth, dh / (float) src.mHeight);
        return createBitmap(src, 0, 0, src.mWidth, src.mHeight, m, filter);
    }
    public Bitmap copy(Config c, boolean mutable) {
        Bitmap b = createBitmap(this, 0, 0, mWidth, mHeight);
        b.mMutable = mutable; b.mConfig = c == null ? mConfig : c;
        return b;
    }
    public Bitmap extractAlpha() { return copy(Config.ALPHA_8, true); }
    public Bitmap extractAlpha(Paint p, int[] offset) { if (offset != null) { offset[0] = offset[1] = 0; } return extractAlpha(); }

    public final int getWidth() { return mWidth; }
    public final int getHeight() { return mHeight; }
    public final Config getConfig() { return mConfig; }
    public final boolean isMutable() { return mMutable; }
    public final boolean isRecycled() { return mRecycled; }
    public final boolean hasAlpha() { return mHasAlpha; }
    public void setHasAlpha(boolean b) { mHasAlpha = b; }
    public final boolean hasMipMap() { return false; }
    public final void setHasMipMap(boolean b) {}
    public final boolean isPremultiplied() { return mPremultiplied; }
    public final void setPremultiplied(boolean b) { mPremultiplied = b; }
    public int getDensity() { return mDensity; }
    public void setDensity(int d) { mDensity = d; }
    public int getScaledWidth(int density) { return mDensity == 0 || density == 0 ? mWidth : (mWidth * density + mDensity / 2) / mDensity; }
    public int getScaledHeight(int density) { return mDensity == 0 || density == 0 ? mHeight : (mHeight * density + mDensity / 2) / mDensity; }
    public int getScaledWidth(Canvas c) { return mWidth; }
    public int getScaledHeight(Canvas c) { return mHeight; }
    public int getScaledWidth(android.util.DisplayMetrics m) { return getScaledWidth(m.densityDpi); }
    public int getScaledHeight(android.util.DisplayMetrics m) { return getScaledHeight(m.densityDpi); }
    public final int getRowBytes() { return mWidth * 4; }
    public final int getByteCount() { return mWidth * mHeight * 4; }
    public final int getAllocationByteCount() { return getByteCount(); }
    public ColorSpace getColorSpace() { return ColorSpace.get(ColorSpace.Named.SRGB); }
    public void setConfig(Config c) { mConfig = c; }
    public void reconfigure(int w, int h, Config c) {}
    public void setWidth(int w) {}
    public void setHeight(int h) {}
    public void prepareToDraw() {}
    public boolean sameAs(Bitmap o) { if (o == null || o.mWidth != mWidth || o.mHeight != mHeight) return false; int[] a = new int[mWidth * mHeight], b = new int[a.length]; getPixels(a, 0, mWidth, 0, 0, mWidth, mHeight); o.getPixels(b, 0, mWidth, 0, 0, mWidth, mHeight); return java.util.Arrays.equals(a, b); }

    public void recycle() { if (!mRecycled && mNative != 0) { husk.Gfx.bmFree(mNative); mNative = 0; mRecycled = true; } }
    @Override protected void finalize() throws Throwable { recycle(); super.finalize(); }

    public void eraseColor(int c) { husk.Gfx.bmErase(mNative, c); mGeneration++; }
    public void eraseColor(long c) { eraseColor(Color.toArgb(c)); }
    public int getPixel(int x, int y) { return husk.Gfx.bmGetPixel(mNative, x, y); }
    public void setPixel(int x, int y, int c) { husk.Gfx.bmSetPixel(mNative, x, y, c); mGeneration++; }
    public void getPixels(int[] px, int offset, int stride, int x, int y, int w, int h) { if (w > 0 && h > 0) husk.Gfx.bmPixels(mNative, px, offset, stride, x, y, w, h, false); }
    public void setPixels(int[] px, int offset, int stride, int x, int y, int w, int h) { if (w > 0 && h > 0) husk.Gfx.bmPixels(mNative, px, offset, stride, x, y, w, h, true); mGeneration++; }
    private static java.lang.reflect.Field sAddress;
    /** A direct buffer's memory, for copying pixels straight into it (0 when it is not a direct one). */
    static long huskAddress(java.nio.Buffer b) {
        if (!b.isDirect()) return 0;
        try { if (sAddress == null) { sAddress = java.nio.Buffer.class.getDeclaredField("address"); sAddress.setAccessible(true); } return sAddress.getLong(b); } catch (Exception e) { return 0; }
    }
    /** Husk: the raw (premultiplied RGBA, top row first) pixels into a buffer at its position. */
    public void huskCopyRgba(java.nio.ByteBuffer dst) {
        long a = huskAddress(dst);
        if (a != 0 && dst.remaining() >= mWidth * mHeight * 4) { husk.Gfx.bmRaw(mNative, a + dst.position(), false); return; }
        copyPixelsToBuffer(dst);
    }
    public void copyPixelsToBuffer(java.nio.Buffer dst) {
        long addr = huskAddress(dst);
        if (addr != 0 && getConfig() != Config.RGB_565 && dst.remaining() * (dst instanceof java.nio.ByteBuffer ? 1 : dst instanceof java.nio.ShortBuffer ? 2 : 4) >= mWidth * mHeight * 4) {
            int unit = dst instanceof java.nio.ByteBuffer ? 1 : dst instanceof java.nio.ShortBuffer ? 2 : 4;
            husk.Gfx.bmRaw(mNative, addr + (long) dst.position() * unit, false);
            dst.position(dst.position() + mWidth * mHeight * 4 / unit);
            return;
        }
        java.nio.ByteBuffer b = dst instanceof java.nio.ByteBuffer ? (java.nio.ByteBuffer) dst : null;
        int n = getByteCount();
        byte[] tmp = new byte[n];
        int[] px = new int[mWidth * mHeight];
        getPixels(px, 0, mWidth, 0, 0, mWidth, mHeight);
        for (int i = 0; i < px.length; i++) { int c = px[i]; int a = c >>> 24; tmp[4 * i] = (byte) (((c >> 16) & 255) * a / 255); tmp[4 * i + 1] = (byte) (((c >> 8) & 255) * a / 255); tmp[4 * i + 2] = (byte) ((c & 255) * a / 255); tmp[4 * i + 3] = (byte) a; }
        if (b != null) b.put(tmp); else if (dst instanceof java.nio.IntBuffer) { java.nio.ByteBuffer.wrap(tmp).order(java.nio.ByteOrder.nativeOrder()).asIntBuffer().get(new int[0]); }
    }
    public void copyPixelsFromBuffer(java.nio.Buffer src) {
        if (!(src instanceof java.nio.ByteBuffer)) return;
        java.nio.ByteBuffer b = (java.nio.ByteBuffer) src;
        int[] px = new int[mWidth * mHeight];
        for (int i = 0; i < px.length && b.remaining() >= 4; i++) {
            int r = b.get() & 255, g = b.get() & 255, bl = b.get() & 255, a = b.get() & 255;
            if (a != 0 && a != 255) { r = Math.min(255, r * 255 / a); g = Math.min(255, g * 255 / a); bl = Math.min(255, bl * 255 / a); }
            px[i] = a << 24 | r << 16 | g << 8 | bl;
        }
        setPixels(px, 0, mWidth, 0, 0, mWidth, mHeight);
    }
    public boolean compress(CompressFormat fmt, int quality, java.io.OutputStream out) {
        byte[] d = husk.Gfx.bmCompress(mNative, fmt == CompressFormat.JPEG ? 0 : 1, quality);
        if (d == null) return false;
        try { out.write(d); return true; } catch (java.io.IOException e) { return false; }
    }
    public byte[] getNinePatchChunk() { return null; }
    public Bitmap asShared() { return this; }
    public int describeContents() { return 0; }
}
