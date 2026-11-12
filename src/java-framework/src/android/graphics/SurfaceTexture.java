package android.graphics;

/**
 * A stream of frames from a producer (a Surface made on it: MediaCodec's video, lockCanvas drawing, an EGL window surface) to a
 * consumer. Husk keeps the frames in software: a back buffer the producer draws, and the front one the consumer (a TextureView)
 * shows. GL consumers (updateTexImage into an external texture) get no pixels yet.
 */
public class SurfaceTexture {
    public interface OnFrameAvailableListener { void onFrameAvailable(SurfaceTexture t); }
    public interface OnSetFrameRateListener { void onSetFrameRate(SurfaceTexture t, float rate, int compat, int strategy); }
    public static class OutOfResourcesException extends Exception { public OutOfResourcesException() {} public OutOfResourcesException(String s) { super(s); } }
    private int mTex;
    private final Object mLock = new Object();
    private final Object mRgbaLock = new Object();
    private int mW, mH;
    private Bitmap mFront, mBack;
    private Canvas mBackCanvas;
    private boolean mLocked;
    private volatile boolean mReleased;
    private int mNativeProducers;
    private long mGeneration;
    private long mTimestamp;
    private OnFrameAvailableListener mListener;
    private android.os.Handler mHandler;
    /** Internal CPU image consumer. Each callback owns an immutable snapshot of a real posted frame. */
    public interface HuskFrameConsumer { void onFrame(Bitmap bitmap, long timestamp); }
    private HuskFrameConsumer mFrameConsumer;
    public void huskSetFrameConsumer(HuskFrameConsumer consumer) { synchronized (mLock) { if (!mReleased) mFrameConsumer = consumer; } }
    /** The TextureView showing this texture, told when a frame arrives. */
    public android.view.View huskView;
    public SurfaceTexture(int texName) { this(texName, false); }
    public SurfaceTexture(int texName, boolean singleBufferMode) { mTex = texName; }
    public SurfaceTexture(boolean singleBufferMode) { mTex = 0; }
    public void setOnFrameAvailableListener(OnFrameAvailableListener l) { setOnFrameAvailableListener(l, null); }
    public void setOnFrameAvailableListener(OnFrameAvailableListener l, android.os.Handler h) {
        synchronized (mLock) {
            if (mReleased) return;
            mListener = l;
            mHandler = l == null ? null : h != null ? h : new android.os.Handler(android.os.Looper.myLooper() != null ? android.os.Looper.myLooper() : android.os.Looper.getMainLooper());
        }
    }
    public void setOnSetFrameRateListener(OnSetFrameRateListener l, android.os.Handler h) {}
    public void setDefaultBufferSize(int w, int h) { synchronized (mLock) { mW = w; mH = h; } }
    /** The buffer size producers draw at (the consumer's size when it never set one). */
    public int[] huskSize() { synchronized (mLock) { return new int[] { mW, mH }; } }
    public Canvas huskLock(Rect dirty) {
        synchronized (mLock) {
            if (mReleased) return null;
            while (mLocked) { try { mLock.wait(); } catch (InterruptedException e) { return null; } if (mReleased) return null; }
            int w = mW > 0 ? mW : 1, h = mH > 0 ? mH : 1;
            if (mBack == null || mBack.getWidth() != w || mBack.getHeight() != h) {
                if (mBackCanvas != null) mBackCanvas.huskRelease();
                if (mBack != null) mBack.recycle();
                mBack = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
                mBackCanvas = new Canvas(mBack);
            }
            mLocked = true;
            mBackCanvas.save();
            if (dirty != null) mBackCanvas.clipRect(dirty);
            return mBackCanvas;
        }
    }
    public void huskPost(Canvas c) {
        HuskFrameConsumer consumer;
        Bitmap snapshot;
        long timestamp;
        long generation;
        OnFrameAvailableListener listener;
        android.os.Handler handler;
        synchronized (mLock) {
            if (!mLocked) return;
            mBackCanvas.restore();
            // A Canvas owns a saved, y-down CoreGraphics transform on its bitmap.
            // Release it before this bitmap becomes a front buffer or gets a new
            // Canvas next time; otherwise reused buffers alternate orientation.
            mBackCanvas.huskRelease();
            mBackCanvas = null;
            if (mReleased) {
                mLocked = false;
                if (mNativeProducers == 0) recycleLocked();
                mLock.notifyAll();
                return;
            }
            Bitmap f = mFront; mFront = mBack; mBack = f;
            if (mBack != null && (mBack.getWidth() != mFront.getWidth() || mBack.getHeight() != mFront.getHeight())) { mBack.recycle(); mBack = null; }
            mBackCanvas = mBack != null ? new Canvas(mBack) : null;
            mLocked = false; mTimestamp = System.nanoTime();
            consumer = mFrameConsumer;
            snapshot = consumer != null ? mFront.copy(Bitmap.Config.ARGB_8888, false) : null;
            timestamp = mTimestamp;
            generation = mGeneration;
            listener = mListener; handler = mHandler;
            mLock.notifyAll();
        }
        // Do not invoke the consumer under mLock: closing an ImageReader may release this texture.
        if (consumer != null) {
            synchronized (mLock) {
                if (mReleased || generation != mGeneration || consumer != mFrameConsumer) {
                    snapshot.recycle(); snapshot = null;
                }
            }
            if (snapshot != null) consumer.onFrame(snapshot, timestamp);
        }
        android.view.View v = huskView;
        if (v != null) v.postInvalidate();
        if (listener != null && handler != null) handler.post(() -> {
            synchronized (mLock) {
                if (mReleased || generation != mGeneration || listener != mListener) return;
            }
            listener.onFrameAvailable(this);
        });
    }
    public boolean huskValid() { return !mReleased; }
    private Bitmap mStage;
    /** A native producer's frame (husk-tl-egl.c's TextureView windows, ANativeWindow_lock): w x h RGBA at addr, bottom row first when flip. */
    public void huskPostRgba(long addr, int w, int h, boolean flip) {
        // Serialise the staging bitmap before taking the Canvas producer lock.
        // release() never takes mRgbaLock and can defer cleanup without waiting
        // for an application callback running on this producer's thread.
        synchronized (mRgbaLock) {
            synchronized (mLock) { if (mReleased) return; mNativeProducers++; }
            try {
                if (mStage == null || mStage.getWidth() != w || mStage.getHeight() != h) {
                    if (mStage != null) mStage.recycle();
                    mStage = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
                }
                mStage.huskSetRgba(addr);
                Canvas c = huskLock(null);
                if (c == null) return;
                try {
                    c.save();
                    if (flip) c.scale(1, -1, c.getWidth() / 2f, c.getHeight() / 2f);
                    c.drawColor(0, PorterDuff.Mode.CLEAR);
                    c.drawBitmap(mStage, null, new Rect(0, 0, c.getWidth(), c.getHeight()), null);
                    c.restore();
                } finally { huskPost(c); }
            } finally {
                synchronized (mLock) {
                    mNativeProducers--;
                    if (mReleased && !mLocked && mNativeProducers == 0) recycleLocked();
                    mLock.notifyAll();
                }
            }
        }
    }
    /** The latest frame (null before the first). */
    public Bitmap huskFront() { synchronized (mLock) { return mReleased ? null : mFront; } }
    /** Internal: draw a borrowed front buffer while release/reuse cannot recycle it. */
    public Bitmap huskDraw(Canvas canvas, Rect destination, Paint paint) {
        synchronized (mLock) {
            if (mReleased || mFront == null) return null;
            canvas.drawBitmap(mFront, null, destination, paint);
            return mFront; // identity only; callers must not access its pixels outside the lock
        }
    }
    public void updateTexImage() { if (mReleased) throw new IllegalStateException("updateTexImage on a released SurfaceTexture"); }
    public void releaseTexImage() {}
    public void detachFromGLContext() { mTex = 0; }
    public void attachToGLContext(int tex) { mTex = tex; }
    public void getTransformMatrix(float[] m) { android.opengl.Matrix.setIdentityM(m, 0); }
    public long getTimestamp() { synchronized (mLock) { return mTimestamp; } }
    public int getDataSpace() { return 0; }
    public boolean isSingleBuffered() { return false; }
    private void recycleLocked() {
        if (mBackCanvas != null) { mBackCanvas.huskRelease(); mBackCanvas = null; }
        if (mFront != null) { mFront.recycle(); mFront = null; }
        if (mBack != null) { mBack.recycle(); mBack = null; }
        if (mStage != null) { mStage.recycle(); mStage = null; }
    }
    public void release() {
        synchronized (mLock) {
            if (mReleased) return;
            mReleased = true; mGeneration++;
            mFrameConsumer = null; mListener = null; mHandler = null; huskView = null;
            if (!mLocked && mNativeProducers == 0) recycleLocked();
            mLock.notifyAll();
        }
    }
    public boolean isReleased() { return mReleased; }
}
