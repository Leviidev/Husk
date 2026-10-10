package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import java.util.ArrayDeque;
import java.util.ArrayList;

/** AOSP's RelativeLayout: children placed by rules against the parent and each other, measured in dependency order. */
public class RelativeLayout extends ViewGroup {
    public static final int TRUE = -1, LEFT_OF = 0, RIGHT_OF = 1, ABOVE = 2, BELOW = 3, ALIGN_BASELINE = 4, ALIGN_LEFT = 5, ALIGN_TOP = 6, ALIGN_RIGHT = 7, ALIGN_BOTTOM = 8,
        ALIGN_PARENT_LEFT = 9, ALIGN_PARENT_TOP = 10, ALIGN_PARENT_RIGHT = 11, ALIGN_PARENT_BOTTOM = 12, CENTER_IN_PARENT = 13, CENTER_HORIZONTAL = 14, CENTER_VERTICAL = 15,
        START_OF = 16, END_OF = 17, ALIGN_START = 18, ALIGN_END = 19, ALIGN_PARENT_START = 20, ALIGN_PARENT_END = 21;
    private static final int VERB_COUNT = 22;
    private static final int[] RULES_VERTICAL = { ABOVE, BELOW, ALIGN_BASELINE, ALIGN_TOP, ALIGN_BOTTOM };
    private static final int[] RULES_HORIZONTAL = { LEFT_OF, RIGHT_OF, ALIGN_LEFT, ALIGN_RIGHT, START_OF, END_OF, ALIGN_START, ALIGN_END };
    private static final int VALUE_NOT_SET = Integer.MIN_VALUE;

    public static class LayoutParams extends MarginLayoutParams {
        private final int[] mRules = new int[VERB_COUNT];
        private int mLeft, mTop, mRight, mBottom;
        public boolean alignWithParent;
        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            TypedArray a = c.obtainStyledAttributes(attrs, husk.S.RelativeLayout_Layout);
            for (int i = 0, n = a.getIndexCount(); i < n; i++) {
                int at = a.getIndex(i);
                switch (at) {
                case husk.S.RelativeLayout_Layout_layout_alignWithParentIfMissing: alignWithParent = a.getBoolean(at, false); break;
                case husk.S.RelativeLayout_Layout_layout_toLeftOf: mRules[LEFT_OF] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_toRightOf: mRules[RIGHT_OF] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_above: mRules[ABOVE] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_below: mRules[BELOW] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignBaseline: mRules[ALIGN_BASELINE] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignLeft: mRules[ALIGN_LEFT] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignTop: mRules[ALIGN_TOP] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignRight: mRules[ALIGN_RIGHT] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignBottom: mRules[ALIGN_BOTTOM] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignParentLeft: mRules[ALIGN_PARENT_LEFT] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_alignParentTop: mRules[ALIGN_PARENT_TOP] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_alignParentRight: mRules[ALIGN_PARENT_RIGHT] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_alignParentBottom: mRules[ALIGN_PARENT_BOTTOM] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_centerInParent: mRules[CENTER_IN_PARENT] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_centerHorizontal: mRules[CENTER_HORIZONTAL] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_centerVertical: mRules[CENTER_VERTICAL] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_toStartOf: mRules[START_OF] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_toEndOf: mRules[END_OF] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignStart: mRules[ALIGN_START] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignEnd: mRules[ALIGN_END] = a.getResourceId(at, 0); break;
                case husk.S.RelativeLayout_Layout_layout_alignParentStart: mRules[ALIGN_PARENT_START] = a.getBoolean(at, false) ? TRUE : 0; break;
                case husk.S.RelativeLayout_Layout_layout_alignParentEnd: mRules[ALIGN_PARENT_END] = a.getBoolean(at, false) ? TRUE : 0; break;
                }
            }
            a.recycle();
            resolve();
        }
        public LayoutParams(int w, int h) { super(w, h); }
        public LayoutParams(ViewGroup.LayoutParams p) { super(p); }
        public LayoutParams(MarginLayoutParams p) { super(p); }
        public LayoutParams(LayoutParams p) { super(p); alignWithParent = p.alignWithParent; System.arraycopy(p.mRules, 0, mRules, 0, VERB_COUNT); }
        /** Start and end rules as left and right (left to right only). */
        private void resolve() {
            if (mRules[START_OF] != 0) mRules[LEFT_OF] = mRules[START_OF];
            if (mRules[END_OF] != 0) mRules[RIGHT_OF] = mRules[END_OF];
            if (mRules[ALIGN_START] != 0) mRules[ALIGN_LEFT] = mRules[ALIGN_START];
            if (mRules[ALIGN_END] != 0) mRules[ALIGN_RIGHT] = mRules[ALIGN_END];
            if (mRules[ALIGN_PARENT_START] != 0) mRules[ALIGN_PARENT_LEFT] = mRules[ALIGN_PARENT_START];
            if (mRules[ALIGN_PARENT_END] != 0) mRules[ALIGN_PARENT_RIGHT] = mRules[ALIGN_PARENT_END];
        }
        public void addRule(int verb) { addRule(verb, TRUE); }
        public void addRule(int verb, int subject) { mRules[verb] = subject; resolve(); }
        public void removeRule(int verb) { mRules[verb] = 0; if (verb == START_OF) mRules[LEFT_OF] = 0; if (verb == END_OF) mRules[RIGHT_OF] = 0; if (verb == ALIGN_START) mRules[ALIGN_LEFT] = 0; if (verb == ALIGN_END) mRules[ALIGN_RIGHT] = 0; if (verb == ALIGN_PARENT_START) mRules[ALIGN_PARENT_LEFT] = 0; if (verb == ALIGN_PARENT_END) mRules[ALIGN_PARENT_RIGHT] = 0; }
        public int getRule(int verb) { return mRules[verb]; }
        public int[] getRules() { return mRules; }
        public int[] getRules(int layoutDirection) { return mRules; }
        // ---- generated by tools/compat/fillmembers.py (LayoutParams): the platform's members this class does not write (signatures only)
        protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
        // ---- end of generated members (LayoutParams)
    }
    private View mBaselineView;
    private int mGravity = Gravity.START | Gravity.TOP;
    private final Rect mContentBounds = new Rect(), mSelfBounds = new Rect();
    private int mIgnoreGravity;
    private View[] mSortedHorizontal, mSortedVertical;
    private boolean mDirtyHierarchy = true;
    public RelativeLayout(Context c) { super(c); }
    public RelativeLayout(Context c, AttributeSet a) { this(c, a, 0); }
    public RelativeLayout(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public RelativeLayout(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(c, attrs, defStyleAttr, defStyleRes);
        if (attrs == null && defStyleAttr == 0) return;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.RelativeLayout, defStyleAttr, defStyleRes);
        mIgnoreGravity = a.getResourceId(husk.S.RelativeLayout_ignoreGravity, View.NO_ID);
        mGravity = a.getInt(husk.S.RelativeLayout_gravity, mGravity);
        a.recycle();
    }
    @Override public boolean shouldDelayChildPressedState() { return false; }
    public void setIgnoreGravity(int id) { mIgnoreGravity = id; }
    public int getIgnoreGravity() { return mIgnoreGravity; }
    public int getGravity() { return mGravity; }
    public void setGravity(int g) { if (mGravity != g) { if ((g & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) == 0) g |= Gravity.START; if ((g & Gravity.VERTICAL_GRAVITY_MASK) == 0) g |= Gravity.TOP; mGravity = g; requestLayout(); } }
    public void setHorizontalGravity(int h) { int g = h & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK; if ((mGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) != g) { mGravity = (mGravity & ~Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) | g; requestLayout(); } }
    public void setVerticalGravity(int v) { int g = v & Gravity.VERTICAL_GRAVITY_MASK; if ((mGravity & Gravity.VERTICAL_GRAVITY_MASK) != g) { mGravity = (mGravity & ~Gravity.VERTICAL_GRAVITY_MASK) | g; requestLayout(); } }
    @Override public int getBaseline() { return mBaselineView != null ? mBaselineView.getBaseline() : super.getBaseline(); }
    @Override public void requestLayout() { super.requestLayout(); mDirtyHierarchy = true; }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new LayoutParams(getContext(), a); }
    @Override protected ViewGroup.LayoutParams generateDefaultLayoutParams() { return new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT); }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof LayoutParams; }
    @Override protected ViewGroup.LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) {
        if (p instanceof LayoutParams) return new LayoutParams((LayoutParams) p);
        if (p instanceof MarginLayoutParams) return new LayoutParams((MarginLayoutParams) p);
        return new LayoutParams(p);
    }
    @Override public CharSequence getAccessibilityClassName() { return RelativeLayout.class.getName(); }

    private void sortChildren() {
        int count = getChildCount();
        if (mSortedVertical == null || mSortedVertical.length != count) { mSortedVertical = new View[count]; mSortedHorizontal = new View[count]; }
        sort(mSortedVertical, RULES_VERTICAL);
        sort(mSortedHorizontal, RULES_HORIZONTAL);
    }
    /** Children in an order where each comes after the ones its rules (of these kinds) name. */
    private void sort(View[] out, int[] rules) {
        int count = getChildCount();
        SparseArray<View> byId = new SparseArray<>();
        for (int i = 0; i < count; i++) { View c = getChildAt(i); if (c.getId() != View.NO_ID) byId.put(c.getId(), c); }
        java.util.HashMap<View, ArrayList<View>> dependents = new java.util.HashMap<>();
        java.util.HashMap<View, Integer> deps = new java.util.HashMap<>();
        for (int i = 0; i < count; i++) {
            View c = getChildAt(i);
            int n = 0;
            int[] r = ((LayoutParams) c.getLayoutParams()).mRules;
            for (int verb : rules) {
                int id = r[verb];
                if (id <= 0) continue;
                View anchor = byId.get(id);
                if (anchor == null || anchor == c) continue;
                ArrayList<View> l = dependents.get(anchor);
                if (l == null) dependents.put(anchor, l = new ArrayList<>());
                if (!l.contains(c)) { l.add(c); n++; }
            }
            deps.put(c, n);
        }
        ArrayDeque<View> roots = new ArrayDeque<>();
        for (int i = 0; i < count; i++) { View c = getChildAt(i); if (deps.get(c) == 0) roots.add(c); }
        int index = 0;
        while (!roots.isEmpty()) {
            View v = roots.poll();
            out[index++] = v;
            ArrayList<View> l = dependents.get(v);
            if (l != null) for (View d : l) { int n = deps.get(d) - 1; deps.put(d, n); if (n == 0) roots.add(d); }
        }
        if (index < count) throw new IllegalStateException("Circular dependencies cannot exist in RelativeLayout");
    }
    @Override protected void onMeasure(int ws, int hs) {
        if (mDirtyHierarchy) { mDirtyHierarchy = false; sortChildren(); }
        int myWidth = -1, myHeight = -1, width = 0, height = 0;
        int wm = MeasureSpec.getMode(ws), hm = MeasureSpec.getMode(hs), wsz = MeasureSpec.getSize(ws), hsz = MeasureSpec.getSize(hs);
        if (wm != MeasureSpec.UNSPECIFIED) myWidth = wsz;
        if (hm != MeasureSpec.UNSPECIFIED) myHeight = hsz;
        if (wm == MeasureSpec.EXACTLY) width = myWidth;
        if (hm == MeasureSpec.EXACTLY) height = myHeight;
        View ignore = null;
        int gravity = mGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK;
        boolean horizontalGravity = gravity != Gravity.START && gravity != 0;
        gravity = mGravity & Gravity.VERTICAL_GRAVITY_MASK;
        boolean verticalGravity = gravity != Gravity.TOP && gravity != 0;
        int left = Integer.MAX_VALUE, top = Integer.MAX_VALUE, right = Integer.MIN_VALUE, bottom = Integer.MIN_VALUE;
        boolean offsetH = false, offsetV = false;
        if ((horizontalGravity || verticalGravity) && mIgnoreGravity != View.NO_ID) ignore = findViewById(mIgnoreGravity);
        boolean isWrapW = wm != MeasureSpec.EXACTLY, isWrapH = hm != MeasureSpec.EXACTLY;
        for (View child : mSortedHorizontal) {
            if (child.getVisibility() == GONE) continue;
            LayoutParams p = (LayoutParams) child.getLayoutParams();
            applyHorizontalSizeRules(p, myWidth, p.mRules);
            measureChildHorizontal(child, p, myWidth, myHeight);
            if (positionChildHorizontal(child, p, myWidth, isWrapW)) offsetH = true;
        }
        for (View child : mSortedVertical) {
            if (child.getVisibility() == GONE) continue;
            LayoutParams p = (LayoutParams) child.getLayoutParams();
            applyVerticalSizeRules(p, myHeight, child.getBaseline());
            measureChild(child, p, myWidth, myHeight);
            if (positionChildVertical(child, p, myHeight, isWrapH)) offsetV = true;
            if (isWrapW) width = Math.max(width, p.mRight + p.rightMargin);
            if (isWrapH) height = Math.max(height, p.mBottom + p.bottomMargin);
            if (child != ignore || verticalGravity) { left = Math.min(left, p.mLeft - p.leftMargin); top = Math.min(top, p.mTop - p.topMargin); }
            if (child != ignore || horizontalGravity) { right = Math.max(right, p.mRight + p.rightMargin); bottom = Math.max(bottom, p.mBottom + p.bottomMargin); }
        }
        View baselineView = null; LayoutParams baselineParams = null;
        for (View child : mSortedVertical) {
            if (child.getVisibility() == GONE) continue;
            LayoutParams cp = (LayoutParams) child.getLayoutParams();
            if (baselineView == null || baselineParams == null || compareLayoutPosition(cp, baselineParams) < 0) { baselineView = child; baselineParams = cp; }
        }
        mBaselineView = baselineView;
        if (isWrapW) {
            width += getPaddingRight();
            if (getLayoutParams() != null && getLayoutParams().width >= 0) width = Math.max(width, getLayoutParams().width);
            width = Math.max(width, getSuggestedMinimumWidth());
            width = resolveSize(width, ws);
            if (offsetH) {
                for (int i = 0; i < getChildCount(); i++) {
                    View child = getChildAt(i);
                    if (child.getVisibility() == GONE) continue;
                    LayoutParams p = (LayoutParams) child.getLayoutParams();
                    int[] r = p.mRules;
                    if (r[CENTER_IN_PARENT] != 0 || r[CENTER_HORIZONTAL] != 0) centerHorizontal(child, p, width);
                    else if (r[ALIGN_PARENT_RIGHT] != 0) { int cw = child.getMeasuredWidth(); p.mLeft = width - getPaddingRight() - cw; p.mRight = p.mLeft + cw; }
                }
            }
        }
        if (isWrapH) {
            height += getPaddingBottom();
            if (getLayoutParams() != null && getLayoutParams().height >= 0) height = Math.max(height, getLayoutParams().height);
            height = Math.max(height, getSuggestedMinimumHeight());
            height = resolveSize(height, hs);
            if (offsetV) {
                for (int i = 0; i < getChildCount(); i++) {
                    View child = getChildAt(i);
                    if (child.getVisibility() == GONE) continue;
                    LayoutParams p = (LayoutParams) child.getLayoutParams();
                    int[] r = p.mRules;
                    if (r[CENTER_IN_PARENT] != 0 || r[CENTER_VERTICAL] != 0) centerVertical(child, p, height);
                    else if (r[ALIGN_PARENT_BOTTOM] != 0) { int ch = child.getMeasuredHeight(); p.mTop = height - getPaddingBottom() - ch; p.mBottom = p.mTop + ch; }
                }
            }
        }
        if (horizontalGravity || verticalGravity) {
            Rect selfBounds = mSelfBounds;
            selfBounds.set(getPaddingLeft(), getPaddingTop(), width - getPaddingRight(), height - getPaddingBottom());
            Rect contentBounds = mContentBounds;
            Gravity.apply(mGravity, right - left, bottom - top, selfBounds, contentBounds);
            int hShift = contentBounds.left - left, vShift = contentBounds.top - top;
            if (hShift != 0 || vShift != 0) {
                for (int i = 0; i < getChildCount(); i++) {
                    View child = getChildAt(i);
                    if (child.getVisibility() == GONE || child == ignore) continue;
                    LayoutParams p = (LayoutParams) child.getLayoutParams();
                    if (horizontalGravity) { p.mLeft += hShift; p.mRight += hShift; }
                    if (verticalGravity) { p.mTop += vShift; p.mBottom += vShift; }
                }
            }
        }
        setMeasuredDimension(width, height);
    }
    private int compareLayoutPosition(LayoutParams a, LayoutParams b) { int t = a.mTop - b.mTop; return t != 0 ? t : a.mLeft - b.mLeft; }
    private void measureChild(View child, LayoutParams p, int myWidth, int myHeight) {
        int cws = getChildMeasureSpec(p.mLeft, p.mRight, p.width, p.leftMargin, p.rightMargin, getPaddingLeft(), getPaddingRight(), myWidth);
        int chs = getChildMeasureSpec(p.mTop, p.mBottom, p.height, p.topMargin, p.bottomMargin, getPaddingTop(), getPaddingBottom(), myHeight);
        child.measure(cws, chs);
    }
    private void measureChildHorizontal(View child, LayoutParams p, int myWidth, int myHeight) {
        int cws = getChildMeasureSpec(p.mLeft, p.mRight, p.width, p.leftMargin, p.rightMargin, getPaddingLeft(), getPaddingRight(), myWidth);
        int chs;
        if (myHeight < 0) chs = p.height >= 0 ? MeasureSpec.makeMeasureSpec(p.height, MeasureSpec.EXACTLY) : MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        else {
            int maxHeight = Math.max(0, myHeight - getPaddingTop() - getPaddingBottom() - p.topMargin - p.bottomMargin);
            int mode = p.height == LayoutParams.MATCH_PARENT ? MeasureSpec.EXACTLY : MeasureSpec.AT_MOST;
            chs = MeasureSpec.makeMeasureSpec(p.height >= 0 ? p.height : maxHeight, p.height >= 0 ? MeasureSpec.EXACTLY : mode);
        }
        child.measure(cws, chs);
    }
    private int getChildMeasureSpec(int childStart, int childEnd, int childSize, int startMargin, int endMargin, int startPadding, int endPadding, int mySize) {
        int childSpecMode = 0, childSpecSize = 0;
        boolean isUnspecified = mySize < 0;
        if (isUnspecified) {
            if (childStart != VALUE_NOT_SET && childEnd != VALUE_NOT_SET) { childSpecSize = Math.max(0, childEnd - childStart); childSpecMode = MeasureSpec.EXACTLY; }
            else if (childSize >= 0) { childSpecSize = childSize; childSpecMode = MeasureSpec.EXACTLY; }
            else { childSpecSize = 0; childSpecMode = MeasureSpec.UNSPECIFIED; }
            return MeasureSpec.makeMeasureSpec(childSpecSize, childSpecMode);
        }
        int tempStart = childStart, tempEnd = childEnd;
        if (tempStart == VALUE_NOT_SET) tempStart = startPadding + startMargin;
        if (tempEnd == VALUE_NOT_SET) tempEnd = mySize - endPadding - endMargin;
        int maxAvailable = tempEnd - tempStart;
        if (childStart != VALUE_NOT_SET && childEnd != VALUE_NOT_SET) { childSpecMode = isUnspecified ? MeasureSpec.UNSPECIFIED : MeasureSpec.EXACTLY; childSpecSize = Math.max(0, maxAvailable); }
        else {
            if (childSize >= 0) { childSpecMode = MeasureSpec.EXACTLY; childSpecSize = maxAvailable >= 0 ? Math.min(maxAvailable, childSize) : childSize; }
            else if (childSize == LayoutParams.MATCH_PARENT) { childSpecMode = isUnspecified ? MeasureSpec.UNSPECIFIED : MeasureSpec.EXACTLY; childSpecSize = Math.max(0, maxAvailable); }
            else if (childSize == LayoutParams.WRAP_CONTENT) { if (maxAvailable >= 0) { childSpecMode = MeasureSpec.AT_MOST; childSpecSize = maxAvailable; } else { childSpecMode = MeasureSpec.UNSPECIFIED; childSpecSize = 0; } }
        }
        return MeasureSpec.makeMeasureSpec(childSpecSize, childSpecMode);
    }
    private boolean positionChildHorizontal(View child, LayoutParams p, int myWidth, boolean wrap) {
        int[] r = p.mRules;
        if (p.mLeft == VALUE_NOT_SET && p.mRight != VALUE_NOT_SET) p.mLeft = p.mRight - child.getMeasuredWidth();
        else if (p.mLeft != VALUE_NOT_SET && p.mRight == VALUE_NOT_SET) p.mRight = p.mLeft + child.getMeasuredWidth();
        else if (p.mLeft == VALUE_NOT_SET && p.mRight == VALUE_NOT_SET) {
            if (r[CENTER_IN_PARENT] != 0 || r[CENTER_HORIZONTAL] != 0) {
                if (!wrap) centerHorizontal(child, p, myWidth);
                else { p.mLeft = getPaddingLeft() + p.leftMargin; p.mRight = p.mLeft + child.getMeasuredWidth(); }
                return true;
            } else { p.mLeft = getPaddingLeft() + p.leftMargin; p.mRight = p.mLeft + child.getMeasuredWidth(); }
        }
        return r[ALIGN_PARENT_RIGHT] != 0;
    }
    private boolean positionChildVertical(View child, LayoutParams p, int myHeight, boolean wrap) {
        int[] r = p.mRules;
        if (p.mTop == VALUE_NOT_SET && p.mBottom != VALUE_NOT_SET) p.mTop = p.mBottom - child.getMeasuredHeight();
        else if (p.mTop != VALUE_NOT_SET && p.mBottom == VALUE_NOT_SET) p.mBottom = p.mTop + child.getMeasuredHeight();
        else if (p.mTop == VALUE_NOT_SET && p.mBottom == VALUE_NOT_SET) {
            if (r[CENTER_IN_PARENT] != 0 || r[CENTER_VERTICAL] != 0) {
                if (!wrap) centerVertical(child, p, myHeight);
                else { p.mTop = getPaddingTop() + p.topMargin; p.mBottom = p.mTop + child.getMeasuredHeight(); }
                return true;
            } else { p.mTop = getPaddingTop() + p.topMargin; p.mBottom = p.mTop + child.getMeasuredHeight(); }
        }
        return r[ALIGN_PARENT_BOTTOM] != 0;
    }
    private void applyHorizontalSizeRules(LayoutParams p, int myWidth, int[] r) {
        LayoutParams anchor;
        p.mLeft = VALUE_NOT_SET; p.mRight = VALUE_NOT_SET;
        anchor = relatedParams(r, LEFT_OF);
        if (anchor != null) p.mRight = anchor.mLeft - (anchor.leftMargin + p.rightMargin);
        else if (p.alignWithParent && r[LEFT_OF] != 0 && myWidth >= 0) p.mRight = myWidth - getPaddingRight() - p.rightMargin;
        anchor = relatedParams(r, RIGHT_OF);
        if (anchor != null) p.mLeft = anchor.mRight + (anchor.rightMargin + p.leftMargin);
        else if (p.alignWithParent && r[RIGHT_OF] != 0) p.mLeft = getPaddingLeft() + p.leftMargin;
        anchor = relatedParams(r, ALIGN_LEFT);
        if (anchor != null) p.mLeft = anchor.mLeft + p.leftMargin;
        else if (p.alignWithParent && r[ALIGN_LEFT] != 0) p.mLeft = getPaddingLeft() + p.leftMargin;
        anchor = relatedParams(r, ALIGN_RIGHT);
        if (anchor != null) p.mRight = anchor.mRight - p.rightMargin;
        else if (p.alignWithParent && r[ALIGN_RIGHT] != 0 && myWidth >= 0) p.mRight = myWidth - getPaddingRight() - p.rightMargin;
        if (r[ALIGN_PARENT_LEFT] != 0) p.mLeft = getPaddingLeft() + p.leftMargin;
        if (r[ALIGN_PARENT_RIGHT] != 0 && myWidth >= 0) p.mRight = myWidth - getPaddingRight() - p.rightMargin;
    }
    private void applyVerticalSizeRules(LayoutParams p, int myHeight, int myBaseline) {
        int[] r = p.mRules;
        int baselineOffset = getRelatedViewBaselineOffset(r);
        if (baselineOffset != -1) {
            if (myBaseline != -1) baselineOffset -= myBaseline;
            p.mTop = baselineOffset; p.mBottom = VALUE_NOT_SET;
            return;
        }
        LayoutParams anchor;
        p.mTop = VALUE_NOT_SET; p.mBottom = VALUE_NOT_SET;
        anchor = relatedParams(r, ABOVE);
        if (anchor != null) p.mBottom = anchor.mTop - (anchor.topMargin + p.bottomMargin);
        else if (p.alignWithParent && r[ABOVE] != 0 && myHeight >= 0) p.mBottom = myHeight - getPaddingBottom() - p.bottomMargin;
        anchor = relatedParams(r, BELOW);
        if (anchor != null) p.mTop = anchor.mBottom + (anchor.bottomMargin + p.topMargin);
        else if (p.alignWithParent && r[BELOW] != 0) p.mTop = getPaddingTop() + p.topMargin;
        anchor = relatedParams(r, ALIGN_TOP);
        if (anchor != null) p.mTop = anchor.mTop + p.topMargin;
        else if (p.alignWithParent && r[ALIGN_TOP] != 0) p.mTop = getPaddingTop() + p.topMargin;
        anchor = relatedParams(r, ALIGN_BOTTOM);
        if (anchor != null) p.mBottom = anchor.mBottom - p.bottomMargin;
        else if (p.alignWithParent && r[ALIGN_BOTTOM] != 0 && myHeight >= 0) p.mBottom = myHeight - getPaddingBottom() - p.bottomMargin;
        if (r[ALIGN_PARENT_TOP] != 0) p.mTop = getPaddingTop() + p.topMargin;
        if (r[ALIGN_PARENT_BOTTOM] != 0 && myHeight >= 0) p.mBottom = myHeight - getPaddingBottom() - p.bottomMargin;
    }
    private View relatedView(int[] rules, int relation) {
        int id = rules[relation];
        if (id == 0) return null;
        View v = null;
        for (int i = 0; i < getChildCount(); i++) if (getChildAt(i).getId() == id) { v = getChildAt(i); break; }
        if (v == null) return null;
        // a gone view is skipped over to what it is itself placed against
        while (v.getVisibility() == GONE) {
            int[] r = ((LayoutParams) v.getLayoutParams()).mRules;
            int next = r[relation];
            if (next == 0) return null;
            View n = null;
            for (int i = 0; i < getChildCount(); i++) if (getChildAt(i).getId() == next) { n = getChildAt(i); break; }
            if (n == null || n == v) return null;
            v = n;
        }
        return v;
    }
    private LayoutParams relatedParams(int[] rules, int relation) { View v = relatedView(rules, relation); return v != null && v.getLayoutParams() instanceof LayoutParams ? (LayoutParams) v.getLayoutParams() : null; }
    private int getRelatedViewBaselineOffset(int[] rules) {
        View v = relatedView(rules, ALIGN_BASELINE);
        if (v != null) { int b = v.getBaseline(); if (b != -1 && v.getLayoutParams() instanceof LayoutParams) return ((LayoutParams) v.getLayoutParams()).mTop + b; }
        return -1;
    }
    private static void centerHorizontal(View child, LayoutParams p, int myWidth) { int w = child.getMeasuredWidth(), l = (myWidth - w) / 2; p.mLeft = l; p.mRight = l + w; }
    private static void centerVertical(View child, LayoutParams p, int myHeight) { int h = child.getMeasuredHeight(), t = (myHeight - h) / 2; p.mTop = t; p.mBottom = t + h; }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child.getVisibility() == GONE) continue;
            LayoutParams st = (LayoutParams) child.getLayoutParams();
            child.layout(st.mLeft, st.mTop, st.mRight, st.mBottom);
        }
    }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InspectionCompanion implements android.view.inspector.InspectionCompanion {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public InspectionCompanion() {}
        public void mapProperties(android.view.inspector.PropertyMapper p0) {}
        public void readProperties(android.widget.RelativeLayout p0, android.view.inspector.PropertyReader p1) {}
        public void readProperties(java.lang.Object p0, android.view.inspector.PropertyReader p1) {}
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public boolean dispatchPopulateAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) { return false; }
    // ---- end of generated members
}
