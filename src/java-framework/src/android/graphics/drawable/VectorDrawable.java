package android.graphics.drawable;

import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.*;
import android.util.AttributeSet;
import java.util.ArrayList;
import org.xmlpull.v1.XmlPullParser;

/** A vector drawable: groups of paths in a viewport, drawn scaled to the bounds (straight to the canvas, no cache bitmap). */
public class VectorDrawable extends Drawable {
    static class Node {}
    static final class Group extends Node {
        float rotate, px, py, sx = 1, sy = 1, tx, ty;
        final ArrayList<Node> kids = new ArrayList<>();
        String name;
    }
    static final class PathNode extends Node {
        Path path; boolean clip; String name;
        ColorStateList fill, stroke; Shader fillShader, strokeShader;
        float fillAlpha = 1, strokeAlpha = 1, strokeWidth, miter = 4, trimStart = 0, trimEnd = 1;
        Paint.Cap cap = Paint.Cap.BUTT; Paint.Join join = Paint.Join.MITER; boolean evenOdd;
    }
    private final Group mRoot = new Group();
    private float mVw, mVh;
    private int mW, mH, mAlpha = 255;
    private ColorStateList mTint;
    private PorterDuff.Mode mTintMode = PorterDuff.Mode.SRC_IN;
    private ColorFilter mFilter;
    private boolean mMirror;
    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);

    public VectorDrawable() {}
    @Override public int getIntrinsicWidth() { return mW; }
    @Override public int getIntrinsicHeight() { return mH; }
    @Override public void setAlpha(int a) { mAlpha = a; invalidateSelf(); }
    @Override public int getAlpha() { return mAlpha; }
    @Override public void setColorFilter(ColorFilter f) { mFilter = f; invalidateSelf(); }
    @Override public void setTintList(ColorStateList t) { mTint = t; invalidateSelf(); }
    @Override public void setTintMode(PorterDuff.Mode m) { mTintMode = m; invalidateSelf(); }
    @Override public boolean isStateful() { return (mTint != null && mTint.isStateful()) || anyStateful(mRoot); }
    private static boolean anyStateful(Group g) { for (Node n : g.kids) { if (n instanceof Group && anyStateful((Group) n)) return true; if (n instanceof PathNode) { PathNode p = (PathNode) n; if ((p.fill != null && p.fill.isStateful()) || (p.stroke != null && p.stroke.isStateful())) return true; } } return false; }
    @Override protected boolean onStateChange(int[] s) { invalidateSelf(); return true; }
    @Override public void setAutoMirrored(boolean m) { mMirror = m; }
    @Override public boolean isAutoMirrored() { return mMirror; }
    @Override public int getOpacity() { return PixelFormat.TRANSLUCENT; }
    @Override public ConstantState getConstantState() { final VectorDrawable src = this; return new ConstantState() { public Drawable newDrawable() { VectorDrawable v = new VectorDrawable(); v.copy(src); return v; } }; }
    private void copy(VectorDrawable o) { mRoot.kids.addAll(o.mRoot.kids); mVw = o.mVw; mVh = o.mVh; mW = o.mW; mH = o.mH; mAlpha = o.mAlpha; mTint = o.mTint; mTintMode = o.mTintMode; mMirror = o.mMirror; }
    @Override public Drawable mutate() { return this; }

    private ColorFilter mDrawFilter;
    @Override public void draw(Canvas c) {
        Rect b = getBounds();
        if (b.width() <= 0 || b.height() <= 0 || mVw <= 0 || mVh <= 0) return;
        ColorFilter cf = mFilter;
        if (cf == null && mTint != null) cf = new PorterDuffColorFilter(mTint.getColorForState(getState(), mTint.getDefaultColor()), mTintMode);
        mDrawFilter = cf;
        int save = c.save();
        c.translate(b.left, b.top);
        c.clipRect(0, 0, b.width(), b.height());
        if (mAlpha < 255) c.saveLayerAlpha(0, 0, b.width(), b.height(), mAlpha);
        c.scale(b.width() / mVw, b.height() / mVh);
        drawGroup(c, mRoot, getState());
        c.restoreToCount(save);
    }
    private void drawGroup(Canvas c, Group g, int[] state) {
        int save = c.save();
        c.translate(g.tx + g.px, g.ty + g.py);
        c.rotate(g.rotate);
        c.scale(g.sx, g.sy);
        c.translate(-g.px, -g.py);
        for (Node n : g.kids) {
            if (n instanceof Group) drawGroup(c, (Group) n, state);
            else {
                PathNode p = (PathNode) n;
                if (p.path == null) continue;
                if (p.clip) { c.clipPath(p.path); continue; }
                Path path = p.path;
                if (p.trimStart != 0 || p.trimEnd != 1) { if (p.trimEnd - p.trimStart <= 0) continue; }
                path.setFillType(p.evenOdd ? Path.FillType.EVEN_ODD : Path.FillType.WINDING);
                if (p.fill != null || p.fillShader != null) {
                    mPaint.reset(); mPaint.setAntiAlias(true); mPaint.setStyle(Paint.Style.FILL);
                    int col = p.fill != null ? p.fill.getColorForState(state, p.fill.getDefaultColor()) : 0xFF000000;
                    mPaint.setColor(applyAlpha(col, p.fillAlpha));
                    mPaint.setShader(p.fillShader);
                    mPaint.setColorFilter(mDrawFilter);
                    if ((col >>> 24) != 0 || p.fillShader != null) c.drawPath(path, mPaint);
                }
                if ((p.stroke != null || p.strokeShader != null) && p.strokeWidth > 0) {
                    mPaint.reset(); mPaint.setAntiAlias(true); mPaint.setStyle(Paint.Style.STROKE);
                    int col = p.stroke != null ? p.stroke.getColorForState(state, p.stroke.getDefaultColor()) : 0xFF000000;
                    mPaint.setColor(applyAlpha(col, p.strokeAlpha));
                    mPaint.setShader(p.strokeShader);
                    mPaint.setColorFilter(mDrawFilter);
                    mPaint.setStrokeWidth(p.strokeWidth); mPaint.setStrokeCap(p.cap); mPaint.setStrokeJoin(p.join); mPaint.setStrokeMiter(p.miter);
                    c.drawPath(path, mPaint);
                }
            }
        }
        c.restoreToCount(save);
    }
    private static int applyAlpha(int c, float a) { return (c & 0xFFFFFF) | (Math.round((c >>> 24) * Math.max(0, Math.min(1, a))) << 24); }

    /** From <vector> (the parser on it): its groups, paths and clip paths. */
    public static VectorDrawable huskInflate(Resources r, XmlPullParser p, Resources.Theme t, AttributeSet set) throws Exception {
        VectorDrawable v = new VectorDrawable();
        TypedArray a = t != null ? t.obtainStyledAttributes(set, husk.S.VectorDrawable, 0, 0) : r.obtainAttributes(set, husk.S.VectorDrawable);
        v.mW = a.getDimensionPixelSize(husk.S.VectorDrawable_width, 0);
        v.mH = a.getDimensionPixelSize(husk.S.VectorDrawable_height, 0);
        v.mVw = a.getFloat(husk.S.VectorDrawable_viewportWidth, v.mW);
        v.mVh = a.getFloat(husk.S.VectorDrawable_viewportHeight, v.mH);
        v.mTint = a.getColorStateList(husk.S.VectorDrawable_tint);
        int tm = a.getInt(husk.S.VectorDrawable_tintMode, -1);
        if (tm == 3) v.mTintMode = PorterDuff.Mode.SRC_OVER; else if (tm == 9) v.mTintMode = PorterDuff.Mode.SRC_ATOP; else if (tm == 14) v.mTintMode = PorterDuff.Mode.MULTIPLY; else if (tm == 15) v.mTintMode = PorterDuff.Mode.SCREEN;
        v.mAlpha = Math.round(a.getFloat(husk.S.VectorDrawable_alpha, 1) * 255);
        v.mMirror = a.getBoolean(husk.S.VectorDrawable_autoMirrored, false);
        a.recycle();
        ArrayList<Group> stack = new ArrayList<>();
        stack.add(v.mRoot);
        int depth = p.getDepth(), tt;
        while ((tt = p.next()) != XmlPullParser.END_DOCUMENT && !(tt == XmlPullParser.END_TAG && p.getDepth() == depth)) {
            Group top = stack.get(stack.size() - 1);
            if (tt == XmlPullParser.START_TAG) {
                AttributeSet s = android.util.Xml.asAttributeSet(p);
                String n = p.getName();
                if ("group".equals(n)) {
                    TypedArray g = obtain(r, t, s, husk.S.VectorDrawableGroup);
                    Group gr = new Group();
                    gr.rotate = g.getFloat(husk.S.VectorDrawableGroup_rotation, 0); gr.px = g.getFloat(husk.S.VectorDrawableGroup_pivotX, 0); gr.py = g.getFloat(husk.S.VectorDrawableGroup_pivotY, 0);
                    gr.sx = g.getFloat(husk.S.VectorDrawableGroup_scaleX, 1); gr.sy = g.getFloat(husk.S.VectorDrawableGroup_scaleY, 1);
                    gr.tx = g.getFloat(husk.S.VectorDrawableGroup_translateX, 0); gr.ty = g.getFloat(husk.S.VectorDrawableGroup_translateY, 0);
                    gr.name = g.getString(husk.S.VectorDrawableGroup_name);
                    g.recycle();
                    top.kids.add(gr);
                    stack.add(gr);
                } else if ("path".equals(n)) {
                    TypedArray g = obtain(r, t, s, husk.S.VectorDrawablePath);
                    PathNode pn = new PathNode();
                    pn.name = g.getString(husk.S.VectorDrawablePath_name);
                    pn.path = husk.PathData.parse(g.getString(husk.S.VectorDrawablePath_pathData));
                    pn.fill = colorOrNull(r, g, husk.S.VectorDrawablePath_fillColor, t);
                    pn.stroke = colorOrNull(r, g, husk.S.VectorDrawablePath_strokeColor, t);
                    pn.fillShader = gradientOrNull(r, g, husk.S.VectorDrawablePath_fillColor, t);
                    pn.strokeShader = gradientOrNull(r, g, husk.S.VectorDrawablePath_strokeColor, t);
                    pn.fillAlpha = g.getFloat(husk.S.VectorDrawablePath_fillAlpha, 1); pn.strokeAlpha = g.getFloat(husk.S.VectorDrawablePath_strokeAlpha, 1);
                    pn.strokeWidth = g.getFloat(husk.S.VectorDrawablePath_strokeWidth, 0);
                    pn.miter = g.getFloat(husk.S.VectorDrawablePath_strokeMiterLimit, 4);
                    int cap = g.getInt(husk.S.VectorDrawablePath_strokeLineCap, 0), join = g.getInt(husk.S.VectorDrawablePath_strokeLineJoin, 0);
                    pn.cap = cap == 1 ? Paint.Cap.ROUND : cap == 2 ? Paint.Cap.SQUARE : Paint.Cap.BUTT;
                    pn.join = join == 1 ? Paint.Join.ROUND : join == 2 ? Paint.Join.BEVEL : Paint.Join.MITER;
                    pn.trimStart = g.getFloat(husk.S.VectorDrawablePath_trimPathStart, 0); pn.trimEnd = g.getFloat(husk.S.VectorDrawablePath_trimPathEnd, 1);
                    pn.evenOdd = g.getInt(husk.S.VectorDrawablePath_fillType, 0) == 1;
                    g.recycle();
                    top.kids.add(pn);
                } else if ("clip-path".equals(n)) {
                    TypedArray g = obtain(r, t, s, husk.S.VectorDrawableClipPath);
                    PathNode pn = new PathNode();
                    pn.clip = true;
                    pn.path = husk.PathData.parse(g.getString(husk.S.VectorDrawableClipPath_pathData));
                    g.recycle();
                    top.kids.add(pn);
                }
            } else if (tt == XmlPullParser.END_TAG && "group".equals(p.getName()) && stack.size() > 1) stack.remove(stack.size() - 1);
        }
        return v;
    }
    private static TypedArray obtain(Resources r, Resources.Theme t, AttributeSet s, int[] attrs) { return t != null ? t.obtainStyledAttributes(s, attrs, 0, 0) : r.obtainAttributes(s, attrs); }
    private static ColorStateList colorOrNull(Resources r, TypedArray a, int i, Resources.Theme t) {
        android.util.TypedValue v = a.peekValue(i);
        if (v == null) return null;
        if (v.type >= android.util.TypedValue.TYPE_FIRST_INT && v.type <= android.util.TypedValue.TYPE_LAST_INT) return ColorStateList.valueOf(v.data);
        if (v.type == android.util.TypedValue.TYPE_STRING && v.string != null && v.string.toString().endsWith(".xml")) {
            try {
                android.content.res.XmlResourceParser x = r.huskXml(v.string.toString(), v.resourceId);
                int e; while ((e = x.next()) != XmlPullParser.START_TAG && e != XmlPullParser.END_DOCUMENT) {}
                if ("gradient".equals(x.getName())) return null;
            } catch (Exception e) { return null; }
            return a.getColorStateList(i);
        }
        return null;
    }
    /** A <gradient> colour resource (aapt:attr inline gradients compile to one): a shader in viewport units. */
    private static Shader gradientOrNull(Resources r, TypedArray a, int i, Resources.Theme t) {
        android.util.TypedValue v = a.peekValue(i);
        if (v == null || v.type != android.util.TypedValue.TYPE_STRING || v.string == null || !v.string.toString().endsWith(".xml")) return null;
        try {
            android.content.res.XmlResourceParser x = r.huskXml(v.string.toString(), v.resourceId);
            int e; while ((e = x.next()) != XmlPullParser.START_TAG && e != XmlPullParser.END_DOCUMENT) {}
            if (!"gradient".equals(x.getName())) return null;
            int[] attrs = { android.R.attr.startColor, android.R.attr.endColor, android.R.attr.type, android.R.attr.centerColor, android.R.attr.gradientRadius,
                            android.R.attr.centerX, android.R.attr.centerY, android.R.attr.startX, android.R.attr.startY, android.R.attr.endX, android.R.attr.endY, android.R.attr.tileMode };
            int[] sorted = attrs.clone(); java.util.Arrays.sort(sorted);
            TypedArray g = obtain(r, t, android.util.Xml.asAttributeSet(x), sorted);
            java.util.function.IntUnaryOperator ix = id -> java.util.Arrays.binarySearch(sorted, id);
            int type = g.getInt(ix.applyAsInt(android.R.attr.type), 0);
            ArrayList<Integer> cols = new ArrayList<>(); ArrayList<Float> offs = new ArrayList<>();
            int depth = x.getDepth(), tt;
            while ((tt = x.next()) != XmlPullParser.END_DOCUMENT && !(tt == XmlPullParser.END_TAG && x.getDepth() == depth)) {
                if (tt == XmlPullParser.START_TAG && "item".equals(x.getName())) {
                    int[] ia = { android.R.attr.color, android.R.attr.offset };
                    int[] is = ia.clone(); java.util.Arrays.sort(is);
                    TypedArray it = obtain(r, t, android.util.Xml.asAttributeSet(x), is);
                    cols.add(it.getColor(java.util.Arrays.binarySearch(is, android.R.attr.color), 0));
                    offs.add(it.getFloat(java.util.Arrays.binarySearch(is, android.R.attr.offset), 0));
                    it.recycle();
                }
            }
            int[] c; float[] o = null;
            if (!cols.isEmpty()) { c = new int[cols.size()]; o = new float[cols.size()]; for (int k = 0; k < c.length; k++) { c[k] = cols.get(k); o[k] = offs.get(k); } }
            else if (g.hasValue(ix.applyAsInt(android.R.attr.centerColor))) c = new int[] { g.getColor(ix.applyAsInt(android.R.attr.startColor), 0), g.getColor(ix.applyAsInt(android.R.attr.centerColor), 0), g.getColor(ix.applyAsInt(android.R.attr.endColor), 0) };
            else c = new int[] { g.getColor(ix.applyAsInt(android.R.attr.startColor), 0), g.getColor(ix.applyAsInt(android.R.attr.endColor), 0) };
            float sx = g.getFloat(ix.applyAsInt(android.R.attr.startX), 0), sy = g.getFloat(ix.applyAsInt(android.R.attr.startY), 0);
            float ex = g.getFloat(ix.applyAsInt(android.R.attr.endX), 0), ey = g.getFloat(ix.applyAsInt(android.R.attr.endY), 0);
            float cx = g.getFloat(ix.applyAsInt(android.R.attr.centerX), 0), cy = g.getFloat(ix.applyAsInt(android.R.attr.centerY), 0);
            float gr = g.getFloat(ix.applyAsInt(android.R.attr.gradientRadius), 0);
            int tile = g.getInt(ix.applyAsInt(android.R.attr.tileMode), 0);
            g.recycle();
            Shader.TileMode tm = tile == 1 ? Shader.TileMode.REPEAT : tile == 2 ? Shader.TileMode.MIRROR : Shader.TileMode.CLAMP;
            if (type == 1) return new RadialGradient(cx, cy, Math.max(gr, 0.001f), c, o, tm);
            if (type == 2) return new SweepGradient(cx, cy, c, o);
            return new LinearGradient(sx, sy, ex, ey, c, o, tm);
        } catch (Exception e) { return null; }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static android.graphics.drawable.VectorDrawable create(android.content.res.Resources p0, int p1) { return null; }
    public void clearMutated() {}
    public long getNativeTree() { return 0L; }
    public float getPixelSize() { return 0f; }
    public void setAntiAlias(boolean p0) {}
    // ---- end of generated members
}
