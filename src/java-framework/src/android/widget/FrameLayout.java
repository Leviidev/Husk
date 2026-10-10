package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import java.util.ArrayList;

public class FrameLayout extends ViewGroup {
    private static final int DEFAULT_CHILD_GRAVITY = Gravity.TOP | Gravity.START;
    public static class LayoutParams extends MarginLayoutParams {
        public static final int UNSPECIFIED_GRAVITY = -1;
        public int gravity = UNSPECIFIED_GRAVITY;
        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            TypedArray a = c.obtainStyledAttributes(attrs, husk.S.FrameLayout_Layout);
            gravity = a.getInt(husk.S.FrameLayout_Layout_layout_gravity, UNSPECIFIED_GRAVITY);
            a.recycle();
        }
        public LayoutParams(int w, int h) { super(w, h); }
        public LayoutParams(int w, int h, int gravity) { super(w, h); this.gravity = gravity; }
        public LayoutParams(ViewGroup.LayoutParams o) { super(o); }
        public LayoutParams(MarginLayoutParams o) { super(o); }
        public LayoutParams(LayoutParams o) { super(o); gravity = o.gravity; }
    }
    private boolean mMeasureAllChildren;
    private final ArrayList<View> mMatchParentChildren = new ArrayList<>(1);
    public FrameLayout(Context c) { super(c); }
    public FrameLayout(Context c, AttributeSet a) { this(c, a, 0); }
    public FrameLayout(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public FrameLayout(Context c, AttributeSet a, int defStyleAttr, int defStyleRes) {
        super(c, a, defStyleAttr, defStyleRes);
        if (a != null || defStyleAttr != 0) {
            TypedArray t = c.obtainStyledAttributes(a, husk.S.FrameLayout, defStyleAttr, defStyleRes);
            mMeasureAllChildren = t.getBoolean(husk.S.FrameLayout_measureAllChildren, false);
            t.recycle();
        }
    }
    public void setMeasureAllChildren(boolean m) { mMeasureAllChildren = m; }
    public boolean getMeasureAllChildren() { return mMeasureAllChildren; }
    public boolean getConsiderGoneChildrenWhenMeasuring() { return mMeasureAllChildren; }
    @Override public void setForegroundGravity(int g) { super.setForegroundGravity(g); requestLayout(); }
    @Override protected LayoutParams generateDefaultLayoutParams() { return new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT); }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new LayoutParams(getContext(), a); }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof LayoutParams; }
    @Override protected ViewGroup.LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) {
        if (p instanceof LayoutParams) return new LayoutParams((LayoutParams) p);
        if (p instanceof MarginLayoutParams) return new LayoutParams((MarginLayoutParams) p);
        return new LayoutParams(p);
    }
    @Override public CharSequence getAccessibilityClassName() { return FrameLayout.class.getName(); }
    int getPaddingLeftWithForeground() { return getPaddingLeft(); }
    int getPaddingRightWithForeground() { return getPaddingRight(); }
    @Override protected void onMeasure(int ws, int hs) {
        int count = getChildCount();
        boolean measureMatchParent = MeasureSpec.getMode(ws) != MeasureSpec.EXACTLY || MeasureSpec.getMode(hs) != MeasureSpec.EXACTLY;
        mMatchParentChildren.clear();
        int maxH = 0, maxW = 0, state = 0;
        for (int i = 0; i < count; i++) {
            View c = getChildAt(i);
            if (mMeasureAllChildren || c.getVisibility() != GONE) {
                measureChildWithMargins(c, ws, 0, hs, 0);
                LayoutParams lp = (LayoutParams) c.getLayoutParams();
                maxW = Math.max(maxW, c.getMeasuredWidth() + lp.leftMargin + lp.rightMargin);
                maxH = Math.max(maxH, c.getMeasuredHeight() + lp.topMargin + lp.bottomMargin);
                state = combineMeasuredStates(state, c.getMeasuredState());
                if (measureMatchParent && (lp.width == LayoutParams.MATCH_PARENT || lp.height == LayoutParams.MATCH_PARENT)) mMatchParentChildren.add(c);
            }
        }
        maxW += getPaddingLeft() + getPaddingRight();
        maxH += getPaddingTop() + getPaddingBottom();
        maxH = Math.max(maxH, getSuggestedMinimumHeight());
        maxW = Math.max(maxW, getSuggestedMinimumWidth());
        if (getForeground() != null) { maxH = Math.max(maxH, getForeground().getMinimumHeight()); maxW = Math.max(maxW, getForeground().getMinimumWidth()); }
        setMeasuredDimension(resolveSizeAndState(maxW, ws, state), resolveSizeAndState(maxH, hs, state << MEASURED_HEIGHT_STATE_SHIFT));
        if (mMatchParentChildren.size() > 1) {
            for (View c : mMatchParentChildren) {
                MarginLayoutParams lp = (MarginLayoutParams) c.getLayoutParams();
                int cws = lp.width == LayoutParams.MATCH_PARENT
                    ? MeasureSpec.makeMeasureSpec(Math.max(0, getMeasuredWidth() - getPaddingLeft() - getPaddingRight() - lp.leftMargin - lp.rightMargin), MeasureSpec.EXACTLY)
                    : getChildMeasureSpec(ws, getPaddingLeft() + getPaddingRight() + lp.leftMargin + lp.rightMargin, lp.width);
                int chs = lp.height == LayoutParams.MATCH_PARENT
                    ? MeasureSpec.makeMeasureSpec(Math.max(0, getMeasuredHeight() - getPaddingTop() - getPaddingBottom() - lp.topMargin - lp.bottomMargin), MeasureSpec.EXACTLY)
                    : getChildMeasureSpec(hs, getPaddingTop() + getPaddingBottom() + lp.topMargin + lp.bottomMargin, lp.height);
                c.measure(cws, chs);
            }
        }
    }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) { layoutChildren(l, t, r, b, false); }
    void layoutChildren(int left, int top, int right, int bottom, boolean forceLeftGravity) {
        int count = getChildCount();
        int pl = getPaddingLeft(), pr = right - left - getPaddingRight(), pt = getPaddingTop(), pb = bottom - top - getPaddingBottom();
        for (int i = 0; i < count; i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) c.getLayoutParams();
            int w = c.getMeasuredWidth(), h = c.getMeasuredHeight();
            int gravity = lp.gravity == -1 ? DEFAULT_CHILD_GRAVITY : lp.gravity;
            int abs = Gravity.getAbsoluteGravity(gravity, getLayoutDirection());
            int cl, ct;
            switch (abs & Gravity.HORIZONTAL_GRAVITY_MASK) {
            case Gravity.CENTER_HORIZONTAL: cl = pl + (pr - pl - w) / 2 + lp.leftMargin - lp.rightMargin; break;
            case Gravity.RIGHT: if (!forceLeftGravity) { cl = pr - w - lp.rightMargin; break; }
            default: cl = pl + lp.leftMargin;
            }
            switch (gravity & Gravity.VERTICAL_GRAVITY_MASK) {
            case Gravity.CENTER_VERTICAL: ct = pt + (pb - pt - h) / 2 + lp.topMargin - lp.bottomMargin; break;
            case Gravity.BOTTOM: ct = pb - h - lp.bottomMargin; break;
            default: ct = pt + lp.topMargin;
            }
            c.layout(cl, ct, cl + w, ct + h);
        }
    }
    @Override public boolean shouldDelayChildPressedState() { return false; }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InspectionCompanion implements android.view.inspector.InspectionCompanion {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public InspectionCompanion() {}
        public void mapProperties(android.view.inspector.PropertyMapper p0) {}
        public void readProperties(android.widget.FrameLayout p0, android.view.inspector.PropertyReader p1) {}
        public void readProperties(java.lang.Object p0, android.view.inspector.PropertyReader p1) {}
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    // ---- end of generated members
}
