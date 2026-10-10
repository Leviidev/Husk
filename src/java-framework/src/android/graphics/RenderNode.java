package android.graphics;

/**
 * A display list with its own position and transform, as Compose's graphics layers and View's hardware layers use it. Here a
 * node's content is drawn into a bitmap of its size when it is recorded, and Canvas.drawRenderNode draws that bitmap where the
 * node is, transformed (translation, scale, rotation about the pivot, the animation matrix) and with its alpha.
 */
public final class RenderNode {
    public static final int USAGE_BACKGROUND = 1;
    public static final int USAGE_UNKNOWN = 0;
    private static long sNextId = 1;
    private final String mName;
    private final long mId;
    private int mLeft, mTop, mRight, mBottom;
    private float mAlpha = 1f, mTx, mTy, mTz, mSx = 1f, mSy = 1f, mRx, mRy, mRz, mPx, mPy, mCamera = 8f, mElevation;
    private boolean mPivotSet, mClipToBounds = true, mClipToOutline, mOverlapping = true, mCompositing, mForceDark = true, mProjBackwards, mProjReceiver;
    private int mAmbient = 0xff000000, mSpot = 0xff000000, mUsage;
    private Matrix mAnimation, mStatic;
    private Rect mClipRect;
    private Outline mOutline;
    private Paint mLayerPaint;
    private RenderEffect mEffect;
    private Bitmap mBitmap;
    private RecordingCanvas mCanvas;
    private boolean mHasDisplayList;
    public long mNativeRenderNode;

    public RenderNode(String name) { mName = name; synchronized (RenderNode.class) { mId = sNextId++; } mNativeRenderNode = mId; }
    public static RenderNode create(String name, Object animationHost) { return new RenderNode(name); }
    public static RenderNode adopt(long nativePtr) { return new RenderNode("adopted"); }

    // ---- recording
    public RecordingCanvas beginRecording(int width, int height) {
        if (mCanvas != null) throw new IllegalStateException("Recording currently in progress - missing #endRecording() call?");
        int w = Math.max(1, width), h = Math.max(1, height);
        if (mBitmap == null || mBitmap.getWidth() != w || mBitmap.getHeight() != h) mBitmap = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
        else mBitmap.eraseColor(0);
        mCanvas = new RecordingCanvas(this, mBitmap);
        return mCanvas;
    }
    public RecordingCanvas beginRecording() { return beginRecording(getWidth(), getHeight()); }
    public RecordingCanvas start(int width, int height) { return beginRecording(width, height); }
    public void endRecording() {
        if (mCanvas == null) throw new IllegalStateException("No recording in progress, forgot to call #beginRecording()?");
        mCanvas.huskRelease();
        mCanvas = null;
        mHasDisplayList = true;
    }
    public void end(RecordingCanvas canvas) { endRecording(); }
    public boolean hasDisplayList() { return mHasDisplayList; }
    public void discardDisplayList() { mHasDisplayList = false; mBitmap = null; }
    public boolean isAttached() { return false; }
    public long getUniqueId() { return mId; }
    public long computeApproximateMemoryUsage() { return mBitmap == null ? 0 : (long) mBitmap.getByteCount(); }
    public long computeApproximateMemoryAllocated() { return computeApproximateMemoryUsage(); }
    public void output() {}
    public void endAllAnimators() {}
    public void forceEndAnimators() {}
    public void setUsageHint(int usage) { mUsage = usage; }
    public void setIsTextureView() {}
    public void registerVectorDrawableAnimator(android.view.NativeVectorDrawableAnimator a) {}

    /** Husk: the node drawn on a canvas, as drawRenderNode does it. */
    void huskDraw(Canvas c) {
        if (!mHasDisplayList || mBitmap == null || mAlpha <= 0f) return;
        int save = c.save();
        c.translate(mLeft, mTop);
        if (!hasIdentityMatrix()) { Matrix m = new Matrix(); getMatrix(m); c.concat(m); }
        if (mClipRect != null) c.clipRect(mClipRect);
        Paint p = null;
        if (mAlpha < 1f || mLayerPaint != null) {
            p = mLayerPaint != null ? new Paint(mLayerPaint) : new Paint();
            p.setAlpha(Math.round(mAlpha * (mLayerPaint != null ? mLayerPaint.getAlpha() : 255)));
        }
        c.drawBitmap(mBitmap, 0, 0, p);
        c.restoreToCount(save);
    }

    // ---- position
    public boolean setPosition(int left, int top, int right, int bottom) {
        boolean changed = left != mLeft || top != mTop || right != mRight || bottom != mBottom;
        mLeft = left; mTop = top; mRight = right; mBottom = bottom;
        return changed;
    }
    public boolean setPosition(Rect r) { return setPosition(r.left, r.top, r.right, r.bottom); }
    public boolean setLeftTopRightBottom(int l, int t, int r, int b) { return setPosition(l, t, r, b); }
    public boolean setLeft(int v) { boolean c = v != mLeft; mLeft = v; return c; }
    public boolean setTop(int v) { boolean c = v != mTop; mTop = v; return c; }
    public boolean setRight(int v) { boolean c = v != mRight; mRight = v; return c; }
    public boolean setBottom(int v) { boolean c = v != mBottom; mBottom = v; return c; }
    public int getLeft() { return mLeft; }
    public int getTop() { return mTop; }
    public int getRight() { return mRight; }
    public int getBottom() { return mBottom; }
    public int getWidth() { return mRight - mLeft; }
    public int getHeight() { return mBottom - mTop; }
    public boolean offsetLeftAndRight(int d) { if (d == 0) return false; mLeft += d; mRight += d; return true; }
    public boolean offsetTopAndBottom(int d) { if (d == 0) return false; mTop += d; mBottom += d; return true; }

    // ---- transform
    public boolean hasIdentityMatrix() {
        return mTx == 0 && mTy == 0 && mSx == 1 && mSy == 1 && mRx == 0 && mRy == 0 && mRz == 0 && (mAnimation == null || mAnimation.isIdentity()) && (mStatic == null || mStatic.isIdentity());
    }
    public void getMatrix(Matrix out) {
        out.reset();
        float px = mPivotSet ? mPx : getWidth() / 2f, py = mPivotSet ? mPy : getHeight() / 2f;
        if (mStatic != null) out.set(mStatic);
        out.preTranslate(mTx, mTy);
        if (mRx != 0 || mRy != 0) {
            Camera cam = new Camera();
            cam.setLocation(0, 0, -Math.abs(mCamera) / 72f);
            cam.rotateX(mRx); cam.rotateY(mRy);
            Matrix m3 = new Matrix();
            cam.getMatrix(m3);
            m3.preTranslate(-px, -py); m3.postTranslate(px, py);
            out.preConcat(m3);
        }
        out.preRotate(mRz, px, py);
        out.preScale(mSx, mSy, px, py);
        if (mAnimation != null) out.postConcat(mAnimation);
    }
    public void getInverseMatrix(Matrix out) { getMatrix(out); out.invert(out); }
    public boolean setAnimationMatrix(Matrix m) { mAnimation = m == null ? null : new Matrix(m); return true; }
    public Matrix getAnimationMatrix() { return mAnimation; }
    public boolean setStaticMatrix(Matrix m) { mStatic = m == null ? null : new Matrix(m); return true; }
    public boolean setTranslationX(float v) { boolean c = v != mTx; mTx = v; return c; }
    public boolean setTranslationY(float v) { boolean c = v != mTy; mTy = v; return c; }
    public boolean setTranslationZ(float v) { boolean c = v != mTz; mTz = v; return c; }
    public float getTranslationX() { return mTx; }
    public float getTranslationY() { return mTy; }
    public float getTranslationZ() { return mTz; }
    public boolean setScaleX(float v) { boolean c = v != mSx; mSx = v; return c; }
    public boolean setScaleY(float v) { boolean c = v != mSy; mSy = v; return c; }
    public float getScaleX() { return mSx; }
    public float getScaleY() { return mSy; }
    public boolean setRotationX(float v) { boolean c = v != mRx; mRx = v; return c; }
    public boolean setRotationY(float v) { boolean c = v != mRy; mRy = v; return c; }
    public boolean setRotationZ(float v) { boolean c = v != mRz; mRz = v; return c; }
    public float getRotationX() { return mRx; }
    public float getRotationY() { return mRy; }
    public float getRotationZ() { return mRz; }
    public boolean setPivotX(float v) { mPx = v; mPivotSet = true; return true; }
    public boolean setPivotY(float v) { mPy = v; mPivotSet = true; return true; }
    public float getPivotX() { return mPivotSet ? mPx : getWidth() / 2f; }
    public float getPivotY() { return mPivotSet ? mPy : getHeight() / 2f; }
    public boolean isPivotExplicitlySet() { return mPivotSet; }
    public boolean resetPivot() { boolean c = mPivotSet; mPivotSet = false; return c; }
    public boolean setCameraDistance(float d) { mCamera = d; return true; }
    public float getCameraDistance() { return mCamera; }

    // ---- appearance
    public boolean setAlpha(float a) { boolean c = a != mAlpha; mAlpha = Math.max(0f, Math.min(1f, a)); return c; }
    public float getAlpha() { return mAlpha; }
    public boolean setHasOverlappingRendering(boolean b) { mOverlapping = b; return true; }
    public boolean hasOverlappingRendering() { return mOverlapping; }
    public boolean setElevation(float e) { mElevation = e; return true; }
    public float getElevation() { return mElevation; }
    public boolean setAmbientShadowColor(int c) { mAmbient = c; return true; }
    public int getAmbientShadowColor() { return mAmbient; }
    public boolean setSpotShadowColor(int c) { mSpot = c; return true; }
    public int getSpotShadowColor() { return mSpot; }
    public boolean hasShadow() { return false; }
    public boolean setClipToBounds(boolean b) { mClipToBounds = b; return true; }
    public boolean getClipToBounds() { return mClipToBounds; }
    public boolean setClipToOutline(boolean b) { mClipToOutline = b; return true; }
    public boolean getClipToOutline() { return mClipToOutline; }
    public boolean setOutline(Outline o) { mOutline = o; return true; }
    public boolean setClipRect(Rect r) { mClipRect = r == null ? null : new Rect(r); return true; }
    public boolean setRevealClip(boolean shouldClip, float x, float y, float radius) { return true; }
    public boolean setUseCompositingLayer(boolean forceToLayer, Paint paint) { mCompositing = forceToLayer; mLayerPaint = paint; return true; }
    public boolean getUseCompositingLayer() { return mCompositing; }
    public boolean setLayerType(int type) { return true; }
    public boolean setLayerPaint(Paint paint) { mLayerPaint = paint; return true; }
    public boolean setRenderEffect(RenderEffect e) { mEffect = e; return true; }
    public boolean setBackdropRenderEffect(RenderEffect e) { return true; }
    public boolean setForceDarkAllowed(boolean b) { mForceDark = b; return true; }
    public boolean isForceDarkAllowed() { return mForceDark; }
    public boolean setProjectBackwards(boolean b) { mProjBackwards = b; return true; }
    public boolean setProjectionReceiver(boolean b) { mProjReceiver = b; return true; }
    public boolean stretch(float vecX, float vecY, float maxStretchX, float maxStretchY) { return true; }
    public boolean clearStretch() { return true; }
    @Override public String toString() { return "RenderNode(" + mName + " " + mLeft + "," + mTop + " " + getWidth() + "x" + getHeight() + ")"; }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public interface AnimationHost {
        boolean isAttached();
        void registerAnimatingRenderNode(android.graphics.RenderNode p0, android.animation.Animator p1);
        void registerVectorDrawableAnimator(android.view.NativeVectorDrawableAnimator p0);
    }
    public interface PositionUpdateListener {
        static boolean callApplyStretch(java.lang.ref.WeakReference p0, long p1, float p2, float p3, float p4, float p5, float p6, float p7, float p8, float p9, float p10, float p11) { return false; }
        static boolean callPositionChanged(java.lang.ref.WeakReference p0, long p1, int p2, int p3, int p4, int p5) { return false; }
        static boolean callPositionChanged2(java.lang.ref.WeakReference p0, long p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, int p10, int p11) { return false; }
        static boolean callPositionLost(java.lang.ref.WeakReference p0, long p1) { return false; }
        default void applyStretch(long p0, float p1, float p2, float p3, float p4, float p5, float p6, float p7, float p8, float p9, float p10) {}
        void positionChanged(long p0, int p1, int p2, int p3, int p4);
        default void positionChanged(long p0, int p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, int p10) {}
        void positionLost(long p0);
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final int USAGE_NAVIGATION_BAR_BACKGROUND = 4;
    public static android.graphics.RenderNode create(java.lang.String p0, android.graphics.RenderNode.AnimationHost p1) { return null; }
    public void addPositionUpdateListener(android.graphics.RenderNode.PositionUpdateListener p0) {}
    public void removePositionUpdateListener(android.graphics.RenderNode.PositionUpdateListener p0) {}
    // ---- end of generated members
}
