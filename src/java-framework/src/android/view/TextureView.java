package android.view;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;

/**
 * A view showing a SurfaceTexture's frames (video players, camera previews, GL renderers that draw off-screen). The texture is
 * made once the view is attached and sized; each frame posted to it redraws the view, scaled to its bounds and then transformed
 * by setTransform, as Android composites the layer.
 */
public class TextureView extends View {
    public interface SurfaceTextureListener {
        void onSurfaceTextureAvailable(SurfaceTexture t, int w, int h);
        void onSurfaceTextureSizeChanged(SurfaceTexture t, int w, int h);
        boolean onSurfaceTextureDestroyed(SurfaceTexture t);
        void onSurfaceTextureUpdated(SurfaceTexture t);
    }
    private SurfaceTexture mTexture;
    private SurfaceTextureListener mListener;
    private boolean mOpaque = true, mAvailable;
    private final Matrix mMatrix = new Matrix();
    private Paint mLayerPaint;
    private final Paint mPaint = new Paint(Paint.FILTER_BITMAP_FLAG);
    private Bitmap mShown;
    private Surface mCanvasSurface;
    public TextureView(android.content.Context c) { super(c); }
    public TextureView(android.content.Context c, android.util.AttributeSet a) { super(c, a); }
    public TextureView(android.content.Context c, android.util.AttributeSet a, int s) { super(c, a, s); }
    public TextureView(android.content.Context c, android.util.AttributeSet a, int s, int r) { super(c, a, s, r); }

    public SurfaceTexture getSurfaceTexture() { return mTexture; }
    public void setSurfaceTexture(SurfaceTexture t) {
        if (t == null) throw new NullPointerException("surfaceTexture must not be null");
        if (t == mTexture) return;
        if (t.isReleased()) throw new IllegalArgumentException("Cannot setSurfaceTexture to a released SurfaceTexture");
        if (mTexture != null) mTexture.huskView = null;
        mTexture = t; t.huskView = this;
        if (getWidth() > 0 && getHeight() > 0) t.setDefaultBufferSize(getWidth(), getHeight());
        mAvailable = isAttachedToWindow() && getWidth() > 0;
        invalidate();
    }
    public SurfaceTextureListener getSurfaceTextureListener() { return mListener; }
    public void setSurfaceTextureListener(SurfaceTextureListener l) { mListener = l; }
    public boolean isAvailable() { return mTexture != null && mAvailable; }
    public boolean isOpaque() { return mOpaque; }
    public void setOpaque(boolean o) { mOpaque = o; invalidate(); }
    public void setTransform(Matrix m) { mMatrix.set(m); invalidate(); }
    public Matrix getTransform(Matrix m) { if (m == null) m = new Matrix(); m.set(mMatrix); return m; }
    public void setLayerPaint(Paint p) { mLayerPaint = p; invalidate(); }
    public void setLayerType(int type, Paint p) { mLayerPaint = p; }
    public int getLayerType() { return LAYER_TYPE_HARDWARE; }
    public void buildLayer() {}
    public CharSequence getAccessibilityClassName() { return TextureView.class.getName(); }

    private void ensureTexture() {
        int w = getWidth(), h = getHeight();
        if (!isAttachedToWindow() || w <= 0 || h <= 0) return;
        if (mTexture == null) { mTexture = new SurfaceTexture(false); mTexture.huskView = this; }
        if (!mAvailable) {
            mAvailable = true;
            int[] sz = mTexture.huskSize();
            if (sz[0] <= 0 || sz[1] <= 0) mTexture.setDefaultBufferSize(w, h);
            if (mListener != null) mListener.onSurfaceTextureAvailable(mTexture, w, h);
        }
    }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); post(this::ensureTexture); }
    @Override protected void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        if (mTexture != null && mAvailable) {
            mTexture.setDefaultBufferSize(w, h);
            if (mListener != null) mListener.onSurfaceTextureSizeChanged(mTexture, w, h);
        } else post(this::ensureTexture);
    }
    @Override protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        if (mTexture != null && mAvailable) {
            mAvailable = false;
            boolean release = mListener == null || mListener.onSurfaceTextureDestroyed(mTexture);
            if (release) { mTexture.release(); mTexture.huskView = null; mTexture = null; }
        }
        if (mCanvasSurface != null) { mCanvasSurface.release(); mCanvasSurface = null; }
        mShown = null;
    }
    @Override public void draw(Canvas c) {
        super.draw(c);
        SurfaceTexture t = mTexture;
        if (t != null) {
            int save = c.save();
            c.concat(mMatrix);
            Paint p = mLayerPaint != null ? mLayerPaint : mPaint;
            Bitmap f = t.huskDraw(c, new Rect(0, 0, getWidth(), getHeight()), p);
            c.restoreToCount(save);
            if (f != null && f != mShown) { mShown = f; if (mListener != null) mListener.onSurfaceTextureUpdated(t); }
        }
    }
    public Bitmap getBitmap() { return getWidth() > 0 && getHeight() > 0 ? getBitmap(getWidth(), getHeight()) : null; }
    public Bitmap getBitmap(int w, int h) { return isAvailable() && w > 0 && h > 0 ? getBitmap(Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)) : null; }
    public Bitmap getBitmap(Bitmap b) {
        if (b != null && mTexture != null) {
            Canvas c = new Canvas(b);
            try { mTexture.huskDraw(c, new Rect(0, 0, b.getWidth(), b.getHeight()), mPaint); }
            finally { c.huskRelease(); }
        }
        return b;
    }
    public Canvas lockCanvas() { return lockCanvas(null); }
    public Canvas lockCanvas(Rect dirty) {
        if (!isAvailable()) return null;
        if (mCanvasSurface == null) mCanvasSurface = new Surface(mTexture);
        return mCanvasSurface.lockCanvas(dirty);
    }
    public void unlockCanvasAndPost(Canvas c) { if (mCanvasSurface != null) mCanvasSurface.unlockCanvasAndPost(c); }
}
