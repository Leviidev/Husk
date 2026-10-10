package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.util.AttributeSet;
import android.view.View;

public class ImageView extends View {
    public enum ScaleType { MATRIX(0), FIT_XY(1), FIT_START(2), FIT_CENTER(3), FIT_END(4), CENTER(5), CENTER_CROP(6), CENTER_INSIDE(7); final int nativeInt; ScaleType(int n) { nativeInt = n; } }
    private static final ScaleType[] sScaleTypeArray = { ScaleType.MATRIX, ScaleType.FIT_XY, ScaleType.FIT_START, ScaleType.FIT_CENTER, ScaleType.FIT_END, ScaleType.CENTER, ScaleType.CENTER_CROP, ScaleType.CENTER_INSIDE };
    private Drawable mDrawable;
    private Uri mUri;
    private int mResource;
    private ScaleType mScaleType = ScaleType.FIT_CENTER;
    private Matrix mMatrix = new Matrix(), mDrawMatrix;
    private boolean mAdjustViewBounds, mCropToPadding, mBaselineAlignBottom;
    private int mMaxWidth = Integer.MAX_VALUE, mMaxHeight = Integer.MAX_VALUE, mBaseline = -1, mDrawableWidth = -1, mDrawableHeight = -1, mAlpha = 255, mLevel;
    private ColorStateList mTint;
    private PorterDuff.Mode mTintMode = PorterDuff.Mode.SRC_IN;
    private ColorFilter mColorFilter;
    private int[] mState;
    private boolean mMergeState;

    public ImageView(Context c) { this(c, null); }
    public ImageView(Context c, AttributeSet a) { this(c, a, 0); }
    public ImageView(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public ImageView(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(c, attrs, defStyleAttr, defStyleRes);
        if (attrs == null && defStyleAttr == 0 && defStyleRes == 0) return;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.ImageView, defStyleAttr, defStyleRes);
        Drawable d = a.getDrawable(husk.S.ImageView_src);
        mBaselineAlignBottom = a.getBoolean(husk.S.ImageView_baselineAlignBottom, false);
        mBaseline = a.getDimensionPixelSize(husk.S.ImageView_baseline, -1);
        setAdjustViewBounds(a.getBoolean(husk.S.ImageView_adjustViewBounds, false));
        setMaxWidth(a.getDimensionPixelSize(husk.S.ImageView_maxWidth, Integer.MAX_VALUE));
        setMaxHeight(a.getDimensionPixelSize(husk.S.ImageView_maxHeight, Integer.MAX_VALUE));
        int st = a.getInt(husk.S.ImageView_scaleType, -1);
        if (st >= 0 && st < sScaleTypeArray.length) setScaleType(sScaleTypeArray[st]);
        if (a.hasValue(husk.S.ImageView_tint)) mTint = a.getColorStateList(husk.S.ImageView_tint);
        int tm = a.getInt(husk.S.ImageView_tintMode, -1);
        if (tm == 3) mTintMode = PorterDuff.Mode.SRC_OVER; else if (tm == 9) mTintMode = PorterDuff.Mode.SRC_ATOP; else if (tm == 14) mTintMode = PorterDuff.Mode.MULTIPLY; else if (tm == 15) mTintMode = PorterDuff.Mode.SCREEN; else if (tm == 16) mTintMode = PorterDuff.Mode.ADD;
        mCropToPadding = a.getBoolean(husk.S.ImageView_cropToPadding, false);
        a.recycle();
        if (d != null) setImageDrawable(d);
    }
    @Override protected boolean verifyDrawable(Drawable dr) { return mDrawable == dr || super.verifyDrawable(dr); }
    @Override public void jumpDrawablesToCurrentState() { super.jumpDrawablesToCurrentState(); if (mDrawable != null) mDrawable.jumpToCurrentState(); }
    @Override public void invalidateDrawable(Drawable dr) { if (dr == mDrawable) { if (dr != null) { int w = dr.getIntrinsicWidth(), h = dr.getIntrinsicHeight(); if (w != mDrawableWidth || h != mDrawableHeight) { mDrawableWidth = w; mDrawableHeight = h; configureBounds(); } } invalidate(); } else super.invalidateDrawable(dr); }
    @Override public boolean hasOverlappingRendering() { return getBackground() != null && getBackground().getCurrent() != null; }
    public boolean getAdjustViewBounds() { return mAdjustViewBounds; }
    public void setAdjustViewBounds(boolean a) { mAdjustViewBounds = a; if (a) setScaleType(ScaleType.FIT_CENTER); }
    public int getMaxWidth() { return mMaxWidth; }
    public void setMaxWidth(int w) { mMaxWidth = w; }
    public int getMaxHeight() { return mMaxHeight; }
    public void setMaxHeight(int h) { mMaxHeight = h; }
    public Drawable getDrawable() { return mDrawable; }
    public void setImageResource(int resId) {
        if (mUri == null && mResource == resId && mDrawable != null) return;
        mResource = resId; mUri = null;
        Drawable d = null;
        if (resId != 0) { try { d = getContext().getDrawable(resId); } catch (Exception e) { android.util.Log.w("ImageView", "Unable to find resource: " + resId, e); } }
        updateDrawable(d);
        requestLayout(); invalidate();
    }
    public void setImageURI(Uri uri) {
        mResource = 0; mUri = uri;
        Drawable d = null;
        if (uri != null) { try (java.io.InputStream in = getContext().getContentResolver().openInputStream(uri)) { d = Drawable.createFromStream(in, null); } catch (Exception e) { android.util.Log.w("ImageView", "Unable to open content: " + uri, e); } }
        updateDrawable(d);
        requestLayout(); invalidate();
    }
    public void setImageDrawable(Drawable d) {
        if (mDrawable != d) {
            mResource = 0; mUri = null;
            int ow = mDrawableWidth, oh = mDrawableHeight;
            updateDrawable(d);
            if (ow != mDrawableWidth || oh != mDrawableHeight) requestLayout();
            invalidate();
        }
    }
    public void setImageIcon(android.graphics.drawable.Icon icon) { setImageDrawable(icon == null ? null : icon.loadDrawable(getContext())); }
    public void setImageBitmap(Bitmap bm) { setImageDrawable(bm == null ? null : new BitmapDrawable(getResources(), bm)); }
    public void setImageState(int[] state, boolean merge) { mState = state; mMergeState = merge; if (mDrawable != null) { refreshDrawableState(); resizeFromDrawable(); } }
    public void setImageLevel(int level) { mLevel = level; if (mDrawable != null) { mDrawable.setLevel(level); resizeFromDrawable(); } }
    public void setImageTintList(ColorStateList t) { mTint = t; applyTint(); }
    public ColorStateList getImageTintList() { return mTint; }
    public void setImageTintMode(PorterDuff.Mode m) { mTintMode = m; applyTint(); }
    public PorterDuff.Mode getImageTintMode() { return mTintMode; }
    public void setImageTintBlendMode(BlendMode m) {}
    private void applyTint() { if (mDrawable != null && mTint != null) { mDrawable = mDrawable.mutate(); mDrawable.setTintList(mTint); mDrawable.setTintMode(mTintMode); if (mDrawable.isStateful()) mDrawable.setState(getDrawableState()); } invalidate(); }
    public final void setColorFilter(int color, PorterDuff.Mode mode) { setColorFilter(new PorterDuffColorFilter(color, mode)); }
    public final void setColorFilter(int color) { setColorFilter(color, PorterDuff.Mode.SRC_ATOP); }
    public final void clearColorFilter() { setColorFilter(null); }
    public void setColorFilter(ColorFilter cf) { if (mColorFilter != cf) { mColorFilter = cf; if (mDrawable != null) { mDrawable = mDrawable.mutate(); mDrawable.setColorFilter(cf); } invalidate(); } }
    public ColorFilter getColorFilter() { return mColorFilter; }
    public int getImageAlpha() { return mAlpha; }
    public void setImageAlpha(int a) { a &= 0xFF; if (mAlpha != a) { mAlpha = a; if (mDrawable != null) { mDrawable = mDrawable.mutate(); mDrawable.setAlpha(a); } invalidate(); } }
    @Deprecated public void setAlpha(int a) { setImageAlpha(a); }
    public boolean isOpaque() { return super.isOpaque() || mDrawable != null && mDrawable.getOpacity() == PixelFormat.OPAQUE && mAlpha == 255 && mColorFilter == null; }
    public void setScaleType(ScaleType st) { if (st == null) throw new NullPointerException(); if (mScaleType != st) { mScaleType = st; requestLayout(); invalidate(); } }
    public ScaleType getScaleType() { return mScaleType; }
    public Matrix getImageMatrix() { return mDrawMatrix == null ? new Matrix(Matrix.IDENTITY_MATRIX) : mDrawMatrix; }
    public void setImageMatrix(Matrix m) {
        if (m != null && m.isIdentity()) m = null;
        if (m == null && !mMatrix.isIdentity() || m != null && !mMatrix.equals(m)) { mMatrix.set(m); configureBounds(); invalidate(); }
    }
    public boolean getCropToPadding() { return mCropToPadding; }
    public void setCropToPadding(boolean c) { if (mCropToPadding != c) { mCropToPadding = c; requestLayout(); invalidate(); } }
    public void animateTransform(Matrix m) { setImageMatrix(m); }
    private void updateDrawable(Drawable d) {
        if (mDrawable != null) { mDrawable.setCallback(null); unscheduleDrawable(mDrawable); }
        mDrawable = d;
        if (d != null) {
            d.setCallback(this);
            if (d.isStateful()) d.setState(getDrawableState());
            d.setVisible(getVisibility() == VISIBLE, true);
            d.setLevel(mLevel);
            mDrawableWidth = d.getIntrinsicWidth(); mDrawableHeight = d.getIntrinsicHeight();
            if (mTint != null) applyTint();
            if (mColorFilter != null) d.setColorFilter(mColorFilter);
            if (mAlpha != 255) d.setAlpha(mAlpha);
            configureBounds();
        } else mDrawableWidth = mDrawableHeight = -1;
    }
    private void resizeFromDrawable() {
        Drawable d = mDrawable;
        if (d != null) { int w = d.getIntrinsicWidth(), h = d.getIntrinsicHeight(); if (w < 0) w = mDrawableWidth; if (h < 0) h = mDrawableHeight; if (w != mDrawableWidth || h != mDrawableHeight) { mDrawableWidth = w; mDrawableHeight = h; requestLayout(); } }
    }
    @Override public int getBaseline() { if (mBaselineAlignBottom) return getMeasuredHeight(); return mBaseline; }
    public void setBaseline(int b) { if (mBaseline != b) { mBaseline = b; requestLayout(); } }
    public void setBaselineAlignBottom(boolean a) { if (mBaselineAlignBottom != a) { mBaselineAlignBottom = a; requestLayout(); } }
    public boolean getBaselineAlignBottom() { return mBaselineAlignBottom; }
    @Override protected int[] onCreateDrawableState(int extra) {
        if (mState == null) return super.onCreateDrawableState(extra);
        if (!mMergeState) return mState;
        return mergeDrawableStates(super.onCreateDrawableState(extra + mState.length), mState);
    }
    @Override protected void drawableStateChanged() { super.drawableStateChanged(); if (mDrawable != null && mDrawable.isStateful() && mDrawable.setState(getDrawableState())) invalidateDrawable(mDrawable); }
    @Override protected void onMeasure(int ws, int hs) {
        resizeFromDrawable();
        int w, h;
        float desiredAspect = 0.0f;
        boolean resizeWidth = false, resizeHeight = false;
        int wm = MeasureSpec.getMode(ws), hm = MeasureSpec.getMode(hs);
        if (mDrawable == null) { mDrawableWidth = -1; mDrawableHeight = -1; w = h = 0; }
        else {
            w = mDrawableWidth; h = mDrawableHeight;
            if (w <= 0) w = 1;
            if (h <= 0) h = 1;
            if (mAdjustViewBounds) {
                resizeWidth = wm != MeasureSpec.EXACTLY; resizeHeight = hm != MeasureSpec.EXACTLY;
                desiredAspect = (float) w / (float) h;
            }
        }
        int pl = getPaddingLeft(), pr = getPaddingRight(), pt = getPaddingTop(), pb = getPaddingBottom();
        int widthSize, heightSize;
        if (resizeWidth || resizeHeight) {
            widthSize = resolveAdjustedSize(w + pl + pr, mMaxWidth, ws);
            heightSize = resolveAdjustedSize(h + pt + pb, mMaxHeight, hs);
            if (desiredAspect != 0.0f) {
                float actualAspect = (float) (widthSize - pl - pr) / (heightSize - pt - pb);
                if (Math.abs(actualAspect - desiredAspect) > 0.0000001) {
                    boolean done = false;
                    if (resizeWidth) { int nw = (int) (desiredAspect * (heightSize - pt - pb)) + pl + pr; if (!resizeHeight) widthSize = resolveAdjustedSize(nw, mMaxWidth, ws); if (nw <= widthSize) { widthSize = nw; done = true; } }
                    if (!done && resizeHeight) { int nh = (int) ((widthSize - pl - pr) / desiredAspect) + pt + pb; if (!resizeWidth) heightSize = resolveAdjustedSize(nh, mMaxHeight, hs); if (nh <= heightSize) heightSize = nh; }
                }
            }
        } else {
            w += pl + pr; h += pt + pb;
            w = Math.max(w, getSuggestedMinimumWidth()); h = Math.max(h, getSuggestedMinimumHeight());
            widthSize = resolveSizeAndState(w, ws, 0); heightSize = resolveSizeAndState(h, hs, 0);
        }
        setMeasuredDimension(widthSize, heightSize);
    }
    private int resolveAdjustedSize(int desired, int max, int spec) {
        int mode = MeasureSpec.getMode(spec), size = MeasureSpec.getSize(spec);
        switch (mode) {
        case MeasureSpec.UNSPECIFIED: return Math.min(desired, max);
        case MeasureSpec.AT_MOST: return Math.min(Math.min(desired, size), max);
        default: return size;
        }
    }
    @Override protected boolean setFrame(int l, int t, int r, int b) { boolean changed = super.setFrame(l, t, r, b); configureBounds(); return changed; }
    private void configureBounds() {
        if (mDrawable == null) return;
        int dwidth = mDrawableWidth, dheight = mDrawableHeight;
        int vwidth = getWidth() - getPaddingLeft() - getPaddingRight(), vheight = getHeight() - getPaddingTop() - getPaddingBottom();
        boolean fits = (dwidth < 0 || vwidth == dwidth) && (dheight < 0 || vheight == dheight);
        if (dwidth <= 0 || dheight <= 0 || ScaleType.FIT_XY == mScaleType) { mDrawable.setBounds(0, 0, vwidth, vheight); mDrawMatrix = null; return; }
        mDrawable.setBounds(0, 0, dwidth, dheight);
        if (ScaleType.MATRIX == mScaleType) { mDrawMatrix = mMatrix.isIdentity() ? null : mMatrix; }
        else if (fits) mDrawMatrix = null;
        else if (ScaleType.CENTER == mScaleType) { mDrawMatrix = mMatrix; mDrawMatrix.setTranslate(Math.round((vwidth - dwidth) * 0.5f), Math.round((vheight - dheight) * 0.5f)); }
        else if (ScaleType.CENTER_CROP == mScaleType) {
            mDrawMatrix = mMatrix;
            float scale, dx = 0, dy = 0;
            if (dwidth * vheight > vwidth * dheight) { scale = (float) vheight / dheight; dx = (vwidth - dwidth * scale) * 0.5f; }
            else { scale = (float) vwidth / dwidth; dy = (vheight - dheight * scale) * 0.5f; }
            mDrawMatrix.setScale(scale, scale);
            mDrawMatrix.postTranslate(Math.round(dx), Math.round(dy));
        } else if (ScaleType.CENTER_INSIDE == mScaleType) {
            mDrawMatrix = mMatrix;
            float scale = dwidth <= vwidth && dheight <= vheight ? 1.0f : Math.min((float) vwidth / dwidth, (float) vheight / dheight);
            float dx = Math.round((vwidth - dwidth * scale) * 0.5f), dy = Math.round((vheight - dheight * scale) * 0.5f);
            mDrawMatrix.setScale(scale, scale);
            mDrawMatrix.postTranslate(dx, dy);
        } else {
            RectF src = new RectF(0, 0, dwidth, dheight), dst = new RectF(0, 0, vwidth, vheight);
            mDrawMatrix = mMatrix;
            Matrix.ScaleToFit stf = mScaleType == ScaleType.FIT_START ? Matrix.ScaleToFit.START : mScaleType == ScaleType.FIT_END ? Matrix.ScaleToFit.END : Matrix.ScaleToFit.CENTER;
            mDrawMatrix.setRectToRect(src, dst, stf);
        }
    }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        if (mDrawable == null || mDrawableWidth == 0 || mDrawableHeight == 0) return;
        int pl = getPaddingLeft(), pt = getPaddingTop();
        if (mDrawMatrix == null && pt == 0 && pl == 0) { mDrawable.draw(c); return; }
        int save = c.save();
        if (mCropToPadding) { int sx = getScrollX(), sy = getScrollY(); c.clipRect(sx + pl, sy + pt, sx + getRight() - getLeft() - getPaddingRight(), sy + getBottom() - getTop() - getPaddingBottom()); }
        c.translate(pl, pt);
        if (mDrawMatrix != null) c.concat(mDrawMatrix);
        mDrawable.draw(c);
        c.restoreToCount(save);
    }
    @Override public void setVisibility(int v) { super.setVisibility(v); if (mDrawable != null) mDrawable.setVisible(v == VISIBLE, false); }
    @Override public void onVisibilityAggregated(boolean v) { super.onVisibilityAggregated(v); if (mDrawable != null) mDrawable.setVisible(v, false); }
    @Override public CharSequence getAccessibilityClassName() { return ImageView.class.getName(); }
    public void setSelected(boolean s) { super.setSelected(s); resizeFromDrawable(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    public android.graphics.BlendMode getImageTintBlendMode() { return null; }
    public boolean isDefaultFocusHighlightNeeded(android.graphics.drawable.Drawable p0, android.graphics.drawable.Drawable p1) { return false; }
    public void onPopulateAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public java.lang.Runnable setImageIconAsync(android.graphics.drawable.Icon p0) { return null; }
    public java.lang.Runnable setImageResourceAsync(int p0) { return null; }
    public java.lang.Runnable setImageURIAsync(android.net.Uri p0) { return null; }
    public void setXfermode(android.graphics.Xfermode p0) {}
    // ---- end of generated members
}
