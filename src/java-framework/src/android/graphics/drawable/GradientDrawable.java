package android.graphics.drawable;

import android.content.res.ColorStateList;
import android.graphics.*;

public class GradientDrawable extends Drawable {
    public static final int RECTANGLE = 0, OVAL = 1, LINE = 2, RING = 3;
    public static final int LINEAR_GRADIENT = 0, RADIAL_GRADIENT = 1, SWEEP_GRADIENT = 2;
    public enum Orientation { TOP_BOTTOM, TR_BL, RIGHT_LEFT, BR_TL, BOTTOM_TOP, BL_TR, LEFT_RIGHT, TL_BR }

    private int mShape = RECTANGLE, mGradient = LINEAR_GRADIENT, mAlpha = 255;
    private Orientation mOrientation = Orientation.TOP_BOTTOM;
    private int[] mColors;
    private float[] mOffsets;
    private ColorStateList mSolid, mStrokeColor;
    private float mRadius, mGradRadius = 0.5f, mCenterX = 0.5f, mCenterY = 0.5f, mStrokeWidth, mDashWidth, mDashGap;
    private float[] mRadii;
    private int mWidth = -1, mHeight = -1;
    private Rect mPadding;
    private float mInnerRadiusRatio = 3, mThicknessRatio = 9;
    private int mInnerRadius = -1, mThickness = -1;
    private boolean mUseLevel;
    private final Paint mFill = new Paint(Paint.ANTI_ALIAS_FLAG), mStroke = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mRect = new RectF();
    private Shader mShader;
    private boolean mShaderDirty = true;

    public GradientDrawable() {}
    public GradientDrawable(Orientation o, int[] colors) { mOrientation = o; setColors(colors); }

    public void setShape(int s) { mShape = s; mShaderDirty = true; invalidateSelf(); }
    public int getShape() { return mShape; }
    public void setGradientType(int t) { mGradient = t; mShaderDirty = true; invalidateSelf(); }
    public int getGradientType() { return mGradient; }
    public void setGradientCenter(float x, float y) { mCenterX = x; mCenterY = y; mShaderDirty = true; invalidateSelf(); }
    public float getGradientCenterX() { return mCenterX; }
    public float getGradientCenterY() { return mCenterY; }
    public void setGradientRadius(float r) { mGradRadius = r; mShaderDirty = true; invalidateSelf(); }
    public float getGradientRadius() { return mGradRadius; }
    public void setUseLevel(boolean u) { mUseLevel = u; }
    public boolean getUseLevel() { return mUseLevel; }
    public Orientation getOrientation() { return mOrientation; }
    public void setOrientation(Orientation o) { mOrientation = o; mShaderDirty = true; invalidateSelf(); }
    public void setColors(int[] c) { mColors = c; mOffsets = null; mSolid = null; mShaderDirty = true; invalidateSelf(); }
    public void setColors(int[] c, float[] offsets) { mColors = c; mOffsets = offsets; mSolid = null; mShaderDirty = true; invalidateSelf(); }
    public int[] getColors() { return mColors == null ? null : mColors.clone(); }
    public void setColor(int c) { mSolid = ColorStateList.valueOf(c); mColors = null; mShaderDirty = true; invalidateSelf(); }
    public void setColor(ColorStateList c) { mSolid = c; mColors = null; mShaderDirty = true; invalidateSelf(); }
    public ColorStateList getColor() { return mSolid; }
    public void setCornerRadius(float r) { mRadius = Math.max(0, r); mRadii = null; invalidateSelf(); }
    public float getCornerRadius() { return mRadius; }
    public void setCornerRadii(float[] r) { mRadii = r; invalidateSelf(); }
    public float[] getCornerRadii() { return mRadii == null ? null : mRadii.clone(); }
    public void setStroke(int width, int color) { setStroke(width, ColorStateList.valueOf(color), 0, 0); }
    public void setStroke(int width, ColorStateList color) { setStroke(width, color, 0, 0); }
    public void setStroke(int width, int color, float dashWidth, float dashGap) { setStroke(width, ColorStateList.valueOf(color), dashWidth, dashGap); }
    public void setStroke(int width, ColorStateList color, float dashWidth, float dashGap) { mStrokeWidth = width; mStrokeColor = color; mDashWidth = dashWidth; mDashGap = dashGap; invalidateSelf(); }
    public void setSize(int w, int h) { mWidth = w; mHeight = h; invalidateSelf(); }
    public void setPadding(int l, int t, int r, int b) { mPadding = new Rect(l, t, r, b); }
    public void setInnerRadius(int r) { mInnerRadius = r; }
    public void setInnerRadiusRatio(float r) { mInnerRadiusRatio = r; }
    public void setThickness(int t) { mThickness = t; }
    public void setThicknessRatio(float r) { mThicknessRatio = r; }
    public void setDither(boolean d) {}
    @Override public int getIntrinsicWidth() { return mWidth; }
    @Override public int getIntrinsicHeight() { return mHeight; }
    @Override public boolean getPadding(Rect p) { if (mPadding != null) { p.set(mPadding); return true; } return super.getPadding(p); }
    @Override public void setAlpha(int a) { mAlpha = a; invalidateSelf(); }
    @Override public int getAlpha() { return mAlpha; }
    @Override public boolean isStateful() { return (mSolid != null && mSolid.isStateful()) || (mStrokeColor != null && mStrokeColor.isStateful()) || super.isStateful(); }
    @Override protected boolean onStateChange(int[] s) { invalidateSelf(); return true; }
    @Override protected void onBoundsChange(Rect b) { mShaderDirty = true; }
    @Override protected boolean onLevelChange(int l) { if (mUseLevel) { mShaderDirty = true; invalidateSelf(); return true; } return false; }
    @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    @Override public void getOutline(Outline o) {
        Rect b = getBounds();
        if (mShape == OVAL) o.setOval(b);
        else o.setRoundRect(b, mRadius);
        o.setAlpha(mAlpha / 255f);
    }
    private static int alpha(int c, int a) { return (c & 0xFFFFFF) | ((c >>> 24) * a / 255) << 24; }

    private void buildShader() {
        mShaderDirty = false;
        mShader = null;
        if (mColors == null || mColors.length < 1) return;
        Rect b = getBounds();
        float l = b.left, t = b.top, r = b.right, bo = b.bottom;
        int[] cs = mColors.length == 1 ? new int[] { mColors[0], mColors[0] } : mColors;
        float[] pos = mOffsets;
        if (mGradient == LINEAR_GRADIENT) {
            float x0, y0, x1, y1;
            float lv = mUseLevel ? getLevel() / 10000f : 1;
            switch (mOrientation) {
            case TOP_BOTTOM: x0 = l; y0 = t; x1 = x0; y1 = t + (bo - t) * lv; break;
            case TR_BL: x0 = r; y0 = t; x1 = r - (r - l) * lv; y1 = t + (bo - t) * lv; break;
            case RIGHT_LEFT: x0 = r; y0 = t; x1 = r - (r - l) * lv; y1 = y0; break;
            case BR_TL: x0 = r; y0 = bo; x1 = r - (r - l) * lv; y1 = bo - (bo - t) * lv; break;
            case BOTTOM_TOP: x0 = l; y0 = bo; x1 = x0; y1 = bo - (bo - t) * lv; break;
            case BL_TR: x0 = l; y0 = bo; x1 = l + (r - l) * lv; y1 = bo - (bo - t) * lv; break;
            case LEFT_RIGHT: x0 = l; y0 = t; x1 = l + (r - l) * lv; y1 = y0; break;
            default: x0 = l; y0 = t; x1 = l + (r - l) * lv; y1 = t + (bo - t) * lv; break;
            }
            mShader = new LinearGradient(x0, y0, x1, y1, cs, pos, Shader.TileMode.CLAMP);
        } else if (mGradient == RADIAL_GRADIENT) {
            float rad = mGradRadius <= 1 ? mGradRadius * Math.min(r - l, bo - t) : mGradRadius;
            if (mUseLevel) rad *= getLevel() / 10000f;
            mShader = new RadialGradient(l + (r - l) * mCenterX, t + (bo - t) * mCenterY, Math.max(rad, 0.001f), cs, pos, Shader.TileMode.CLAMP);
        } else {
            mShader = new SweepGradient(l + (r - l) * mCenterX, t + (bo - t) * mCenterY, cs, pos);
        }
    }

    @Override public void draw(Canvas c) {
        Rect b = getBounds();
        if (b.isEmpty()) return;
        if (mShaderDirty) buildShader();
        int[] st = getState();
        ColorFilter cf = huskTintFilter();
        boolean haveFill = mShader != null || mSolid != null;
        float sw = mStrokeWidth > 0 && mStrokeColor != null ? mStrokeWidth : 0;
        mRect.set(b.left + sw / 2, b.top + sw / 2, b.right - sw / 2, b.bottom - sw / 2);
        if (haveFill) {
            mFill.setShader(mShader);
            mFill.setColor(mShader != null ? alpha(0xFF000000, mAlpha) : alpha(mSolid.getColorForState(st, mSolid.getDefaultColor()), mAlpha));
            mFill.setColorFilter(cf);
            shape(c, mFill);
        }
        if (sw > 0) {
            mStroke.setStyle(Paint.Style.STROKE);
            mStroke.setStrokeWidth(sw);
            mStroke.setColor(alpha(mStrokeColor.getColorForState(st, mStrokeColor.getDefaultColor()), mAlpha));
            mStroke.setColorFilter(cf);
            mStroke.setPathEffect(mDashWidth > 0 ? new DashPathEffect(new float[] { mDashWidth, mDashGap }, 0) : null);
            shape(c, mStroke);
        }
    }
    private void shape(Canvas c, Paint p) {
        switch (mShape) {
        case OVAL: c.drawOval(mRect, p); break;
        case LINE: { float y = mRect.centerY(); c.drawLine(mRect.left, y, mRect.right, y, p); break; }
        case RING: {
            float cx = mRect.centerX(), cy = mRect.centerY();
            float th = mThickness >= 0 ? mThickness : getBounds().width() / mThicknessRatio;
            float ir = mInnerRadius >= 0 ? mInnerRadius : getBounds().width() / mInnerRadiusRatio;
            Path path = new Path();
            path.addCircle(cx, cy, ir + th, Path.Direction.CW);
            path.addCircle(cx, cy, ir, Path.Direction.CCW);
            path.setFillType(Path.FillType.EVEN_ODD);
            if (p.getStyle() == Paint.Style.FILL) c.drawPath(path, p);
            break; }
        default:
            if (mRadii != null) { Path path = new Path(); path.addRoundRect(mRect, mRadii, Path.Direction.CW); c.drawPath(path, p); }
            else if (mRadius > 0) { float rr = Math.min(mRadius, Math.min(mRect.width(), mRect.height()) / 2); c.drawRoundRect(mRect, rr, rr, p); }
            else c.drawRect(mRect, p);
        }
    }
    @Override public Drawable mutate() { return this; }
    @Override public ConstantState getConstantState() {
        final GradientDrawable src = this;
        return new ConstantState() { public Drawable newDrawable() { GradientDrawable g = new GradientDrawable(); g.copyFrom(src); return g; } };
    }
    private void copyFrom(GradientDrawable o) {
        mShape = o.mShape; mGradient = o.mGradient; mAlpha = o.mAlpha; mOrientation = o.mOrientation; mColors = o.mColors; mOffsets = o.mOffsets;
        mSolid = o.mSolid; mStrokeColor = o.mStrokeColor; mRadius = o.mRadius; mGradRadius = o.mGradRadius; mCenterX = o.mCenterX; mCenterY = o.mCenterY;
        mStrokeWidth = o.mStrokeWidth; mDashWidth = o.mDashWidth; mDashGap = o.mDashGap; mRadii = o.mRadii; mWidth = o.mWidth; mHeight = o.mHeight;
        mPadding = o.mPadding; mInnerRadius = o.mInnerRadius; mThickness = o.mThickness; mInnerRadiusRatio = o.mInnerRadiusRatio; mThicknessRatio = o.mThicknessRatio;
        mUseLevel = o.mUseLevel; mShaderDirty = true;
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final int ARC = 4;
    public static boolean sWrapNegativeAngleMeasurements;
    public void clearMutated() {}
    public int getInnerRadius() { return 0; }
    public float getInnerRadiusRatio() { return 0f; }
    public int getStrokeCap() { return (huskFill.get("StrokeCap") instanceof Integer ? (Integer) huskFill.get("StrokeCap") : 0); }
    public int getThickness() { return 0; }
    public float getThicknessRatio() { return 0f; }
    public void setAntiAlias(boolean p0) {}
    public void setStrokeCap(int p0) { huskFill.put("StrokeCap", Integer.valueOf(p0)); }
    public void setXfermode(android.graphics.Xfermode p0) {}
    // ---- end of generated members
}
