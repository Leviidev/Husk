package android.view;

import android.content.Context;
import android.graphics.*;
import android.util.AttributeSet;
import java.util.ArrayList;

/**
 * A surface under the window. For GL (GLSurfaceView) the view clears its area of the window so the render thread's frame shows
 * through; for software drawing lockCanvas gives a canvas on the view's buffer, and each posted frame is drawn in its place.
 * surfaceCreated / surfaceChanged come once the view has a size; surfaceDestroyed when it leaves the window.
 */
public class SurfaceView extends View {
    private final ArrayList<SurfaceHolder.Callback> mCallbacks = new ArrayList<>();
    private final Surface mSurface = new Surface();
    private final Object mLock = new Object();
    private Bitmap mFront, mBack;
    private Canvas mBackCanvas;
    private boolean mCreated, mLocked, mZOnTop, mFixed, mHasFrame;
    private int mFixedW, mFixedH, mFormat = PixelFormat.OPAQUE, mSurfaceW, mSurfaceH;
    private final Paint mClear = new Paint(), mBlit = new Paint(Paint.FILTER_BITMAP_FLAG);
    private final SurfaceHolder mHolder = new SurfaceHolder() {
        public void addCallback(Callback c) { synchronized (mCallbacks) { if (!mCallbacks.contains(c)) mCallbacks.add(c); } }
        public void removeCallback(Callback c) { synchronized (mCallbacks) { mCallbacks.remove(c); } }
        public boolean isCreating() { return false; }
        public void setType(int t) {}
        public void setFixedSize(int w, int h) { mFixed = true; mFixedW = w; mFixedH = h; post(() -> sizeChanged()); }
        public void setSizeFromLayout() { mFixed = false; post(() -> sizeChanged()); }
        public void setFormat(int f) { mFormat = f; }
        public void setKeepScreenOn(boolean b) { SurfaceView.this.setKeepScreenOn(b); }
        public Canvas lockCanvas() { return lock(null); }
        public Canvas lockCanvas(Rect dirty) { return lock(dirty); }
        public void unlockCanvasAndPost(Canvas c) { post(c); }
        public Rect getSurfaceFrame() { return new Rect(0, 0, mSurfaceW, mSurfaceH); }
        public Surface getSurface() { return mSurface; }
    };
    public SurfaceView(Context c) { this(c, null); }
    public SurfaceView(Context c, AttributeSet a) { this(c, a, 0); }
    public SurfaceView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public SurfaceView(Context c, AttributeSet a, int s, int r) {
        super(c, a, s, r);
        setWillNotDraw(false);
        mClear.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.CLEAR));
        mSurface.mProducer = new Surface.Producer() {
            public Canvas lock(Rect d) { return SurfaceView.this.lock(d); }
            public void post(Canvas c) { SurfaceView.this.post(c); }
            public boolean valid() { return mCreated; }
        };
    }
    public SurfaceHolder getHolder() { return mHolder; }
    public void setZOrderOnTop(boolean onTop) { mZOnTop = onTop; }
    public void setZOrderMediaOverlay(boolean o) {}
    public void setSecure(boolean s) {}
    public SurfaceControl getSurfaceControl() { return new SurfaceControl(); }
    public void setChildSurfacePackage(Object p) {}
    public android.os.IBinder getHostToken() { return null; }
    @Override protected void onMeasure(int ws, int hs) {
        int w = mFixed ? resolveSizeAndState(mFixedW, ws, 0) : getDefaultSize(0, ws);
        int h = mFixed ? resolveSizeAndState(mFixedH, hs, 0) : getDefaultSize(0, hs);
        setMeasuredDimension(w, h);
    }
    @Override protected void onSizeChanged(int w, int h, int ow, int oh) { super.onSizeChanged(w, h, ow, oh); sizeChanged(); }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); if (getWidth() > 0 && getHeight() > 0) post(this::sizeChanged); }
    @Override protected void onDetachedFromWindow() { destroy(); super.onDetachedFromWindow(); }
    @Override protected void onWindowVisibilityChanged(int v) { super.onWindowVisibilityChanged(v); if (v != VISIBLE) destroy(); else if (getWidth() > 0) post(this::sizeChanged); }
    private ArrayList<SurfaceHolder.Callback> callbacks() { synchronized (mCallbacks) { return new ArrayList<>(mCallbacks); } }
    private void sizeChanged() {
        if (!isAttachedToWindow()) return;
        int w = mFixed ? mFixedW : getWidth(), h = mFixed ? mFixedH : getHeight();
        if (w <= 0 || h <= 0) return;
        boolean created = false;
        if (!mCreated) { mCreated = true; created = true; for (SurfaceHolder.Callback c : callbacks()) c.surfaceCreated(mHolder); }
        if (created || w != mSurfaceW || h != mSurfaceH) {
            mSurfaceW = w; mSurfaceH = h;
            for (SurfaceHolder.Callback c : callbacks()) c.surfaceChanged(mHolder, mFormat, w, h);
            for (SurfaceHolder.Callback c : callbacks()) if (c instanceof SurfaceHolder.Callback2) ((SurfaceHolder.Callback2) c).surfaceRedrawNeeded(mHolder);
        }
    }
    private void destroy() {
        if (!mCreated) return;
        for (SurfaceHolder.Callback c : callbacks()) c.surfaceDestroyed(mHolder);
        synchronized (mLock) { mCreated = false; mHasFrame = false; mLock.notifyAll(); }
    }
    /** Software drawing: a canvas on the back buffer (kept from the last frame, as Android's is). */
    private Canvas lock(Rect dirty) {
        synchronized (mLock) {
            if (!mCreated) return null;
            while (mLocked) { try { mLock.wait(); } catch (InterruptedException e) { return null; } if (!mCreated) return null; }
            int w = mSurfaceW, h = mSurfaceH;
            if (w <= 0 || h <= 0) return null;
            if (mBack == null || mBack.getWidth() != w || mBack.getHeight() != h) {
                if (mBackCanvas != null) mBackCanvas.huskRelease();
                mBack = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
                mBackCanvas = new Canvas(mBack);
                if (mFront != null) mBackCanvas.drawBitmap(mFront, 0, 0, null);
            }
            mLocked = true;
            int save = mBackCanvas.save();
            if (dirty != null) { mBackCanvas.clipRect(dirty); }
            mSaveHusk = save;
            return mBackCanvas;
        }
    }
    private int mSaveHusk;
    private void post(Canvas c) {
        synchronized (mLock) {
            if (!mLocked) throw new IllegalStateException("Surface was not locked");
            mBackCanvas.restoreToCount(mSaveHusk);
            // swap: the posted buffer becomes the front, the next lock draws on a copy of it
            Bitmap f = mFront;
            mFront = mBack;
            if (f != null && f.getWidth() == mFront.getWidth() && f.getHeight() == mFront.getHeight()) { mBackCanvas.huskRelease(); mBack = f; mBackCanvas = new Canvas(mBack); mBackCanvas.drawBitmap(mFront, 0, 0, null); }
            else { mBack = null; if (mBackCanvas != null) mBackCanvas.huskRelease(); mBackCanvas = null; }
            mHasFrame = true;
            mLocked = false;
            mLock.notifyAll();
        }
        postInvalidate();
    }
    @Override public void draw(Canvas c) {
        super.draw(c);
    }
    @Override protected void onDraw(Canvas c) {
        synchronized (mLock) {
            if (mHasFrame && mFront != null) {
                if (mFront.getWidth() == getWidth() && mFront.getHeight() == getHeight()) c.drawBitmap(mFront, 0, 0, mBlit);
                else c.drawBitmap(mFront, null, new Rect(0, 0, getWidth(), getHeight()), mBlit);
                return;
            }
        }
        // a GL surface (or none drawn yet): let what is under the window show through
        if (!mZOnTop || this instanceof android.opengl.GLSurfaceView) c.drawRect(0, 0, getWidth(), getHeight(), mClear);
    }
    @Override public boolean gatherTransparentRegion(Region r) { return true; }
}
