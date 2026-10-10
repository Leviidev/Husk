package android.graphics;
/** Recorded as a bitmap: beginRecording hands a canvas over one, draw() draws it. */
public class Picture {
    private Bitmap mBitmap; private Canvas mCanvas; private int w, h;
    public Canvas beginRecording(int w, int h) { this.w = w; this.h = h; mBitmap = Bitmap.createBitmap(Math.max(1, w), Math.max(1, h), Bitmap.Config.ARGB_8888); mCanvas = new Canvas(mBitmap); return mCanvas; }
    public void endRecording() { if (mCanvas != null) mCanvas.huskRelease(); mCanvas = null; }
    public int getWidth() { return w; }
    public int getHeight() { return h; }
    public boolean requiresHardwareAcceleration() { return false; }
    public void draw(Canvas c) { if (mBitmap != null) c.drawBitmap(mBitmap, 0, 0, null); }
}
