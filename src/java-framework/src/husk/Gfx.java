package husk;

/** android.graphics' natives: CoreGraphics and CoreText on the Husk side (husk-tl-dvm-graphics.c). Handles are native pointers. */
public final class Gfx {
    private Gfx() {}
    public static native long bmNew(int w, int h);
    public static native void bmFree(long bm);
    public static native void bmErase(long bm, int argb);
    public static native int bmGetPixel(long bm, int x, int y);
    public static native void bmSetPixel(long bm, int x, int y, int argb);
    public static native void bmPixels(long bm, int[] px, int offset, int stride, int x, int y, int w, int h, boolean set);
    public static native void bmRaw(long bm, long address, boolean set);
    public static native long bmAddress(long bm);
    public static native long bmDecode(byte[] data, int offset, int length, int sample, int[] outWH, boolean boundsOnly);
    public static native byte[] bmCompress(long bm, int format, int quality);
    public static native long bmCopy(long bm, int x, int y, int w, int h, float[] matrix, boolean filter);

    public static native long cvNew(long bm);
    public static native void cvFree(long cv);
    public static native int cvSave(long cv);
    public static native int cvSaveLayer(long cv, float l, float t, float r, float b, int alpha);
    public static native void cvRestore(long cv);
    public static native void cvRestoreTo(long cv, int count);
    public static native int cvSaveCount(long cv);
    public static native void cvMatrix(long cv, float[] affine, boolean set);
    public static native void cvGetMatrix(long cv, float[] affine);
    public static native boolean cvClipRect(long cv, float l, float t, float r, float b, int op);
    public static native boolean cvClipPath(long cv, float[] path, int n, int fill, int op);
    public static native boolean cvClipBounds(long cv, int[] out);
    public static native void cvDrawColor(long cv, int color, int mode);
    public static native void cvDrawRect(long cv, float l, float t, float r, float b, int[] paint, long shader);
    public static native void cvDrawRRect(long cv, float l, float t, float r, float b, float rx, float ry, int[] paint, long shader);
    public static native void cvDrawOval(long cv, float l, float t, float r, float b, int[] paint, long shader);
    public static native void cvDrawArc(long cv, float l, float t, float r, float b, float start, float sweep, boolean center, int[] paint, long shader);
    public static native void cvDrawPath(long cv, float[] path, int n, int fill, int[] paint, long shader);
    public static native void cvDrawLines(long cv, float[] pts, int offset, int count, int[] paint, long shader, boolean points);
    public static native void cvDrawBitmap(long cv, long bm, float sl, float st, float sr, float sb, float dl, float dt, float dr, float db, int[] paint);
    public static native void cvDrawBitmapM(long cv, long bm, float[] affine, int[] paint);
    public static native void cvDrawText(long cv, String s, int start, int end, float x, float y, int[] paint, long typeface, long shader);

    public static native float txMeasure(String s, int start, int end, int[] paint, long typeface);
    public static native float txMetrics(int[] paint, long typeface, float[] out);
    public static native void txBounds(String s, int start, int end, int[] paint, long typeface, int[] out);
    public static native int txBreak(String s, int start, int end, float maxWidth, int[] paint, long typeface, boolean words);
    public static native void txWidths(String s, int start, int end, int[] paint, long typeface, float[] out);

    public static native long tfCreate(String family, int style);
    public static native long tfDefault();
    public static native long tfDerive(long tf, int style, int weight);
    public static native long tfFromData(byte[] data);
    public static native int tfWeight(long tf);

    public static native long shLinear(float x0, float y0, float x1, float y1, int[] colors, float[] pos, int tile);
    public static native long shRadial(float cx, float cy, float r, int[] colors, float[] pos, int tile);
    public static native long shSweep(float cx, float cy, int[] colors, float[] pos);
    public static native long shBitmap(long bm, int tile);
    public static native void shMatrix(long sh, float[] affine);
    public static native void shFree(long sh);

    public static native void pathBounds(float[] path, int n, float[] out);
    public static native boolean pathContains(float[] path, int n, int fill, float x, float y);
    public static native float pathLength(float[] path, int n, float[] out);

    /** The window's views, drawn: Husk's render thread puts them on screen over the app's GL. 0: nothing to show. */
    public static native void present(long bm);
}
