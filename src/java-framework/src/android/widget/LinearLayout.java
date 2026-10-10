package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;

public class LinearLayout extends ViewGroup {
    public static final int HORIZONTAL = 0, VERTICAL = 1, SHOW_DIVIDER_NONE = 0, SHOW_DIVIDER_BEGINNING = 1, SHOW_DIVIDER_MIDDLE = 2, SHOW_DIVIDER_END = 4;
    public static class LayoutParams extends MarginLayoutParams {
        public float weight;
        public int gravity = -1;
        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            TypedArray a = c.obtainStyledAttributes(attrs, husk.S.LinearLayout_Layout);
            weight = a.getFloat(husk.S.LinearLayout_Layout_layout_weight, 0);
            gravity = a.getInt(husk.S.LinearLayout_Layout_layout_gravity, -1);
            a.recycle();
        }
        public LayoutParams(int w, int h) { super(w, h); }
        public LayoutParams(int w, int h, float weight) { super(w, h); this.weight = weight; }
        public LayoutParams(ViewGroup.LayoutParams p) { super(p); }
        public LayoutParams(MarginLayoutParams p) { super(p); }
        public LayoutParams(LayoutParams p) { super(p); weight = p.weight; gravity = p.gravity; }
    }
    private boolean mBaselineAligned = true, mUseLargestChild;
    private int mBaselineAlignedChildIndex = -1, mBaselineChildTop, mOrientation, mGravity = Gravity.START | Gravity.TOP, mTotalLength;
    private float mWeightSum = -1;
    private int[] mMaxAscent, mMaxDescent;
    private Drawable mDivider;
    private int mDividerWidth, mDividerHeight, mShowDividers, mDividerPadding;

    public LinearLayout(Context c) { super(c); }
    public LinearLayout(Context c, AttributeSet a) { this(c, a, 0); }
    public LinearLayout(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public LinearLayout(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(c, attrs, defStyleAttr, defStyleRes);
        if (attrs == null && defStyleAttr == 0 && defStyleRes == 0) return;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.LinearLayout, defStyleAttr, defStyleRes);
        int o = a.getInt(husk.S.LinearLayout_orientation, -1);
        if (o >= 0) setOrientation(o);
        int g = a.getInt(husk.S.LinearLayout_gravity, -1);
        if (g >= 0) setGravity(g);
        mBaselineAligned = a.getBoolean(husk.S.LinearLayout_baselineAligned, true);
        mWeightSum = a.getFloat(husk.S.LinearLayout_weightSum, -1);
        mBaselineAlignedChildIndex = a.getInt(husk.S.LinearLayout_baselineAlignedChildIndex, -1);
        mUseLargestChild = a.getBoolean(husk.S.LinearLayout_measureWithLargestChild, false);
        mShowDividers = a.getInt(husk.S.LinearLayout_showDividers, SHOW_DIVIDER_NONE);
        mDividerPadding = a.getDimensionPixelSize(husk.S.LinearLayout_dividerPadding, 0);
        setDividerDrawable(a.getDrawable(husk.S.LinearLayout_divider));
        a.recycle();
    }
    public void setShowDividers(int s) { mShowDividers = s; requestLayout(); }
    public int getShowDividers() { return mShowDividers; }
    public Drawable getDividerDrawable() { return mDivider; }
    public void setDividerDrawable(Drawable d) {
        if (d == mDivider) return;
        mDivider = d;
        if (d != null) { mDividerWidth = d.getIntrinsicWidth(); mDividerHeight = d.getIntrinsicHeight(); } else mDividerWidth = mDividerHeight = 0;
        setWillNotDraw(d == null);
        requestLayout();
    }
    public void setDividerPadding(int p) { mDividerPadding = p; }
    public int getDividerPadding() { return mDividerPadding; }
    public int getDividerWidth() { return mDividerWidth; }
    public boolean isBaselineAligned() { return mBaselineAligned; }
    public void setBaselineAligned(boolean b) { mBaselineAligned = b; }
    public boolean isMeasureWithLargestChildEnabled() { return mUseLargestChild; }
    public void setMeasureWithLargestChildEnabled(boolean e) { mUseLargestChild = e; }
    public int getBaselineAlignedChildIndex() { return mBaselineAlignedChildIndex; }
    public void setBaselineAlignedChildIndex(int i) { mBaselineAlignedChildIndex = i; }
    public float getWeightSum() { return mWeightSum; }
    public void setWeightSum(float w) { mWeightSum = Math.max(0, w); }
    public void setOrientation(int o) { if (mOrientation != o) { mOrientation = o; requestLayout(); } }
    public int getOrientation() { return mOrientation; }
    public void setGravity(int g) {
        if (mGravity != g) {
            if ((g & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) == 0) g |= Gravity.START;
            if ((g & Gravity.VERTICAL_GRAVITY_MASK) == 0) g |= Gravity.TOP;
            mGravity = g; requestLayout();
        }
    }
    public int getGravity() { return mGravity; }
    public void setHorizontalGravity(int h) { int g = h & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK; if ((mGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) != g) { mGravity = (mGravity & ~Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) | g; requestLayout(); } }
    public void setVerticalGravity(int v) { int g = v & Gravity.VERTICAL_GRAVITY_MASK; if ((mGravity & Gravity.VERTICAL_GRAVITY_MASK) != g) { mGravity = (mGravity & ~Gravity.VERTICAL_GRAVITY_MASK) | g; requestLayout(); } }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new LayoutParams(getContext(), a); }
    @Override protected LayoutParams generateDefaultLayoutParams() {
        if (mOrientation == HORIZONTAL) return new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
        if (mOrientation == VERTICAL) return new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT);
        return null;
    }
    @Override protected ViewGroup.LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) {
        if (p instanceof LayoutParams) return new LayoutParams((LayoutParams) p);
        if (p instanceof MarginLayoutParams) return new LayoutParams((MarginLayoutParams) p);
        return new LayoutParams(p);
    }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof LayoutParams; }
    @Override public CharSequence getAccessibilityClassName() { return LinearLayout.class.getName(); }
    @Override public boolean shouldDelayChildPressedState() { return false; }
    @Override public int getBaseline() {
        if (mBaselineAlignedChildIndex < 0) return super.getBaseline();
        if (getChildCount() <= mBaselineAlignedChildIndex) throw new RuntimeException("mBaselineAlignedChildIndex of LinearLayout set to an index that is out of bounds.");
        View child = getChildAt(mBaselineAlignedChildIndex);
        int b = child.getBaseline();
        if (b == -1) { if (mBaselineAlignedChildIndex == 0) return -1; throw new RuntimeException("mBaselineAlignedChildIndex of LinearLayout points to a View that doesn't know how to get its baseline."); }
        int childTop = mBaselineChildTop;
        if (mOrientation == VERTICAL) {
            int major = mGravity & Gravity.VERTICAL_GRAVITY_MASK;
            if (major != Gravity.TOP) {
                if (major == Gravity.BOTTOM) childTop = getBottom() - getTop() - getPaddingBottom() - mTotalLength;
                else if (major == Gravity.CENTER_VERTICAL) childTop += ((getBottom() - getTop() - getPaddingTop() - getPaddingBottom()) - mTotalLength) / 2;
            }
        }
        return childTop + ((LayoutParams) child.getLayoutParams()).topMargin + b;
    }
    protected boolean hasDividerBeforeChildAt(int i) {
        if (i == getChildCount()) return (mShowDividers & SHOW_DIVIDER_END) != 0;
        boolean allGone = true;
        for (int k = i - 1; k >= 0; k--) if (getChildAt(k).getVisibility() != GONE) { allGone = false; break; }
        return allGone ? (mShowDividers & SHOW_DIVIDER_BEGINNING) != 0 : (mShowDividers & SHOW_DIVIDER_MIDDLE) != 0;
    }
    @Override protected void onMeasure(int ws, int hs) { if (mOrientation == VERTICAL) measureVertical(ws, hs); else measureHorizontal(ws, hs); }

    void measureVertical(int ws, int hs) {
        mTotalLength = 0;
        int maxWidth = 0, childState = 0, alternativeMaxWidth = 0, weightedMaxWidth = 0, largestChildHeight = Integer.MIN_VALUE, consumedExcessSpace = 0;
        boolean allFillParent = true, matchWidth = false, skippedMeasure = false;
        float totalWeight = 0;
        int count = getChildCount();
        int wm = MeasureSpec.getMode(ws), hm = MeasureSpec.getMode(hs);
        int baselineChildIndex = mBaselineAlignedChildIndex;
        for (int i = 0; i < count; ++i) {
            View child = getChildAt(i);
            if (child == null || child.getVisibility() == GONE) continue;
            if (hasDividerBeforeChildAt(i)) mTotalLength += mDividerHeight;
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            totalWeight += lp.weight;
            boolean useExcessSpace = lp.height == 0 && lp.weight > 0;
            if (hm == MeasureSpec.EXACTLY && useExcessSpace) {
                int totalLength = mTotalLength;
                mTotalLength = Math.max(totalLength, totalLength + lp.topMargin + lp.bottomMargin);
                skippedMeasure = true;
            } else {
                if (useExcessSpace) lp.height = LayoutParams.WRAP_CONTENT;
                int usedHeight = totalWeight == 0 ? mTotalLength : 0;
                measureChildWithMargins(child, ws, 0, hs, usedHeight);
                int childHeight = child.getMeasuredHeight();
                if (useExcessSpace) { lp.height = 0; consumedExcessSpace += childHeight; }
                int totalLength = mTotalLength;
                mTotalLength = Math.max(totalLength, totalLength + childHeight + lp.topMargin + lp.bottomMargin);
                if (mUseLargestChild) largestChildHeight = Math.max(childHeight, largestChildHeight);
            }
            if (baselineChildIndex >= 0 && baselineChildIndex == i + 1) mBaselineChildTop = mTotalLength;
            boolean matchWidthLocally = false;
            if (wm != MeasureSpec.EXACTLY && lp.width == LayoutParams.MATCH_PARENT) { matchWidth = true; matchWidthLocally = true; }
            int margin = lp.leftMargin + lp.rightMargin, measuredWidth = child.getMeasuredWidth() + margin;
            maxWidth = Math.max(maxWidth, measuredWidth);
            childState = combineMeasuredStates(childState, child.getMeasuredState());
            allFillParent = allFillParent && lp.width == LayoutParams.MATCH_PARENT;
            if (lp.weight > 0) weightedMaxWidth = Math.max(weightedMaxWidth, matchWidthLocally ? margin : measuredWidth);
            else alternativeMaxWidth = Math.max(alternativeMaxWidth, matchWidthLocally ? margin : measuredWidth);
        }
        if (mTotalLength > 0 && hasDividerBeforeChildAt(count)) mTotalLength += mDividerHeight;
        if (mUseLargestChild && (hm == MeasureSpec.AT_MOST || hm == MeasureSpec.UNSPECIFIED)) {
            mTotalLength = 0;
            for (int i = 0; i < count; ++i) {
                View child = getChildAt(i);
                if (child == null || child.getVisibility() == GONE) continue;
                LayoutParams lp = (LayoutParams) child.getLayoutParams();
                int totalLength = mTotalLength;
                mTotalLength = Math.max(totalLength, totalLength + largestChildHeight + lp.topMargin + lp.bottomMargin);
            }
        }
        mTotalLength += getPaddingTop() + getPaddingBottom();
        int heightSize = Math.max(mTotalLength, getSuggestedMinimumHeight());
        int heightSizeAndState = resolveSizeAndState(heightSize, hs, 0);
        heightSize = heightSizeAndState & MEASURED_SIZE_MASK;
        int remainingExcess = heightSize - mTotalLength + consumedExcessSpace;
        if (skippedMeasure || (remainingExcess != 0 && totalWeight > 0.0f)) {
            float remainingWeightSum = mWeightSum > 0.0f ? mWeightSum : totalWeight;
            mTotalLength = 0;
            for (int i = 0; i < count; ++i) {
                View child = getChildAt(i);
                if (child == null || child.getVisibility() == GONE) continue;
                LayoutParams lp = (LayoutParams) child.getLayoutParams();
                float childWeight = lp.weight;
                if (childWeight > 0) {
                    int share = (int) (childWeight * remainingExcess / remainingWeightSum);
                    remainingExcess -= share;
                    remainingWeightSum -= childWeight;
                    int childHeight;
                    if (mUseLargestChild && hm != MeasureSpec.EXACTLY) childHeight = largestChildHeight;
                    else if (lp.height == 0 && hm == MeasureSpec.EXACTLY) childHeight = share;
                    else childHeight = child.getMeasuredHeight() + share;
                    int chs = MeasureSpec.makeMeasureSpec(Math.max(0, childHeight), MeasureSpec.EXACTLY);
                    int cws = getChildMeasureSpec(ws, getPaddingLeft() + getPaddingRight() + lp.leftMargin + lp.rightMargin, lp.width);
                    child.measure(cws, chs);
                    childState = combineMeasuredStates(childState, child.getMeasuredState() & (MEASURED_STATE_MASK >> MEASURED_HEIGHT_STATE_SHIFT));
                }
                int margin = lp.leftMargin + lp.rightMargin, measuredWidth = child.getMeasuredWidth() + margin;
                maxWidth = Math.max(maxWidth, measuredWidth);
                boolean matchWidthLocally = wm != MeasureSpec.EXACTLY && lp.width == LayoutParams.MATCH_PARENT;
                alternativeMaxWidth = Math.max(alternativeMaxWidth, matchWidthLocally ? margin : measuredWidth);
                allFillParent = allFillParent && lp.width == LayoutParams.MATCH_PARENT;
                int totalLength = mTotalLength;
                mTotalLength = Math.max(totalLength, totalLength + child.getMeasuredHeight() + lp.topMargin + lp.bottomMargin);
            }
            mTotalLength += getPaddingTop() + getPaddingBottom();
        } else {
            alternativeMaxWidth = Math.max(alternativeMaxWidth, weightedMaxWidth);
            if (mUseLargestChild && hm != MeasureSpec.EXACTLY) {
                for (int i = 0; i < count; i++) {
                    View child = getChildAt(i);
                    if (child == null || child.getVisibility() == GONE) continue;
                    LayoutParams lp = (LayoutParams) child.getLayoutParams();
                    if (lp.weight > 0) child.measure(MeasureSpec.makeMeasureSpec(child.getMeasuredWidth(), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(largestChildHeight, MeasureSpec.EXACTLY));
                }
            }
        }
        if (!allFillParent && wm != MeasureSpec.EXACTLY) maxWidth = alternativeMaxWidth;
        maxWidth += getPaddingLeft() + getPaddingRight();
        maxWidth = Math.max(maxWidth, getSuggestedMinimumWidth());
        setMeasuredDimension(resolveSizeAndState(maxWidth, ws, childState), heightSizeAndState);
        if (matchWidth) forceUniformWidth(count, hs);
    }
    private void forceUniformWidth(int count, int hs) {
        int uniform = MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
        for (int i = 0; i < count; ++i) {
            View child = getChildAt(i);
            if (child == null || child.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            if (lp.width == LayoutParams.MATCH_PARENT) {
                int old = lp.height;
                lp.height = child.getMeasuredHeight();
                measureChildWithMargins(child, uniform, 0, hs, 0);
                lp.height = old;
            }
        }
    }
    void measureHorizontal(int ws, int hs) {
        mTotalLength = 0;
        int maxHeight = 0, childState = 0, alternativeMaxHeight = 0, weightedMaxHeight = 0, largestChildWidth = Integer.MIN_VALUE, usedExcessSpace = 0;
        boolean allFillParent = true, matchHeight = false, skippedMeasure = false;
        float totalWeight = 0;
        int count = getChildCount();
        int wm = MeasureSpec.getMode(ws), hm = MeasureSpec.getMode(hs);
        if (mMaxAscent == null || mMaxDescent == null) { mMaxAscent = new int[4]; mMaxDescent = new int[4]; }
        int[] maxAscent = mMaxAscent, maxDescent = mMaxDescent;
        maxAscent[0] = maxAscent[1] = maxAscent[2] = maxAscent[3] = -1;
        maxDescent[0] = maxDescent[1] = maxDescent[2] = maxDescent[3] = -1;
        boolean baselineAligned = mBaselineAligned, useLargestChild = mUseLargestChild, isExactly = wm == MeasureSpec.EXACTLY;
        for (int i = 0; i < count; ++i) {
            View child = getChildAt(i);
            if (child == null || child.getVisibility() == GONE) continue;
            if (hasDividerBeforeChildAt(i)) mTotalLength += mDividerWidth;
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            totalWeight += lp.weight;
            boolean useExcessSpace = lp.width == 0 && lp.weight > 0;
            if (wm == MeasureSpec.EXACTLY && useExcessSpace) {
                if (isExactly) mTotalLength += lp.leftMargin + lp.rightMargin;
                else { int total = mTotalLength; mTotalLength = Math.max(total, total + lp.leftMargin + lp.rightMargin); }
                if (baselineAligned) { int f = MeasureSpec.makeSafeMeasureSpec(MeasureSpec.getSize(ws), MeasureSpec.UNSPECIFIED); child.measure(f, MeasureSpec.makeSafeMeasureSpec(MeasureSpec.getSize(hs), MeasureSpec.UNSPECIFIED)); }
                else skippedMeasure = true;
            } else {
                if (useExcessSpace) lp.width = LayoutParams.WRAP_CONTENT;
                int usedWidth = totalWeight == 0 ? mTotalLength : 0;
                measureChildWithMargins(child, ws, usedWidth, hs, 0);
                int childWidth = child.getMeasuredWidth();
                if (useExcessSpace) { lp.width = 0; usedExcessSpace += childWidth; }
                if (isExactly) mTotalLength += childWidth + lp.leftMargin + lp.rightMargin;
                else { int total = mTotalLength; mTotalLength = Math.max(total, total + childWidth + lp.leftMargin + lp.rightMargin); }
                if (useLargestChild) largestChildWidth = Math.max(childWidth, largestChildWidth);
            }
            boolean matchHeightLocally = false;
            if (hm != MeasureSpec.EXACTLY && lp.height == LayoutParams.MATCH_PARENT) { matchHeight = true; matchHeightLocally = true; }
            int margin = lp.topMargin + lp.bottomMargin, childHeight = child.getMeasuredHeight() + margin;
            childState = combineMeasuredStates(childState, child.getMeasuredState());
            if (baselineAligned) {
                int childBaseline = child.getBaseline();
                if (childBaseline != -1) {
                    int gravity = (lp.gravity < 0 ? mGravity : lp.gravity) & Gravity.VERTICAL_GRAVITY_MASK;
                    int index = ((gravity >> Gravity.AXIS_Y_SHIFT) & ~Gravity.AXIS_SPECIFIED) >> 1;
                    maxAscent[index] = Math.max(maxAscent[index], childBaseline);
                    maxDescent[index] = Math.max(maxDescent[index], childHeight - childBaseline);
                }
            }
            maxHeight = Math.max(maxHeight, childHeight);
            allFillParent = allFillParent && lp.height == LayoutParams.MATCH_PARENT;
            if (lp.weight > 0) weightedMaxHeight = Math.max(weightedMaxHeight, matchHeightLocally ? margin : childHeight);
            else alternativeMaxHeight = Math.max(alternativeMaxHeight, matchHeightLocally ? margin : childHeight);
        }
        if (mTotalLength > 0 && hasDividerBeforeChildAt(count)) mTotalLength += mDividerWidth;
        if (maxAscent[1] != -1 || maxAscent[0] != -1 || maxAscent[2] != -1 || maxAscent[3] != -1) {
            int ascent = Math.max(maxAscent[3], Math.max(maxAscent[0], Math.max(maxAscent[1], maxAscent[2])));
            int descent = Math.max(maxDescent[3], Math.max(maxDescent[0], Math.max(maxDescent[1], maxDescent[2])));
            maxHeight = Math.max(maxHeight, ascent + descent);
        }
        if (useLargestChild && (wm == MeasureSpec.AT_MOST || wm == MeasureSpec.UNSPECIFIED)) {
            mTotalLength = 0;
            for (int i = 0; i < count; ++i) {
                View child = getChildAt(i);
                if (child == null || child.getVisibility() == GONE) continue;
                LayoutParams lp = (LayoutParams) child.getLayoutParams();
                if (isExactly) mTotalLength += largestChildWidth + lp.leftMargin + lp.rightMargin;
                else { int total = mTotalLength; mTotalLength = Math.max(total, total + largestChildWidth + lp.leftMargin + lp.rightMargin); }
            }
        }
        mTotalLength += getPaddingLeft() + getPaddingRight();
        int widthSize = Math.max(mTotalLength, getSuggestedMinimumWidth());
        int widthSizeAndState = resolveSizeAndState(widthSize, ws, 0);
        widthSize = widthSizeAndState & MEASURED_SIZE_MASK;
        int remainingExcess = widthSize - mTotalLength + usedExcessSpace;
        if (skippedMeasure || (remainingExcess != 0 && totalWeight > 0.0f)) {
            float remainingWeightSum = mWeightSum > 0.0f ? mWeightSum : totalWeight;
            maxAscent[0] = maxAscent[1] = maxAscent[2] = maxAscent[3] = -1;
            maxDescent[0] = maxDescent[1] = maxDescent[2] = maxDescent[3] = -1;
            maxHeight = -1;
            mTotalLength = 0;
            for (int i = 0; i < count; ++i) {
                View child = getChildAt(i);
                if (child == null || child.getVisibility() == GONE) continue;
                LayoutParams lp = (LayoutParams) child.getLayoutParams();
                float childWeight = lp.weight;
                if (childWeight > 0) {
                    int share = (int) (childWeight * remainingExcess / remainingWeightSum);
                    remainingExcess -= share;
                    remainingWeightSum -= childWeight;
                    int childWidth;
                    if (mUseLargestChild && wm != MeasureSpec.EXACTLY) childWidth = largestChildWidth;
                    else if (lp.width == 0 && wm == MeasureSpec.EXACTLY) childWidth = share;
                    else childWidth = child.getMeasuredWidth() + share;
                    int cws = MeasureSpec.makeMeasureSpec(Math.max(0, childWidth), MeasureSpec.EXACTLY);
                    int chs = getChildMeasureSpec(hs, getPaddingTop() + getPaddingBottom() + lp.topMargin + lp.bottomMargin, lp.height);
                    child.measure(cws, chs);
                    childState = combineMeasuredStates(childState, child.getMeasuredState() & MEASURED_STATE_MASK);
                }
                if (isExactly) mTotalLength += child.getMeasuredWidth() + lp.leftMargin + lp.rightMargin;
                else { int total = mTotalLength; mTotalLength = Math.max(total, total + child.getMeasuredWidth() + lp.leftMargin + lp.rightMargin); }
                boolean matchHeightLocally = hm != MeasureSpec.EXACTLY && lp.height == LayoutParams.MATCH_PARENT;
                int margin = lp.topMargin + lp.bottomMargin, childHeight = child.getMeasuredHeight() + margin;
                maxHeight = Math.max(maxHeight, childHeight);
                alternativeMaxHeight = Math.max(alternativeMaxHeight, matchHeightLocally ? margin : childHeight);
                allFillParent = allFillParent && lp.height == LayoutParams.MATCH_PARENT;
                if (baselineAligned) {
                    int childBaseline = child.getBaseline();
                    if (childBaseline != -1) {
                        int gravity = (lp.gravity < 0 ? mGravity : lp.gravity) & Gravity.VERTICAL_GRAVITY_MASK;
                        int index = ((gravity >> Gravity.AXIS_Y_SHIFT) & ~Gravity.AXIS_SPECIFIED) >> 1;
                        maxAscent[index] = Math.max(maxAscent[index], childBaseline);
                        maxDescent[index] = Math.max(maxDescent[index], childHeight - childBaseline);
                    }
                }
            }
            mTotalLength += getPaddingLeft() + getPaddingRight();
            if (maxAscent[1] != -1 || maxAscent[0] != -1 || maxAscent[2] != -1 || maxAscent[3] != -1) {
                int ascent = Math.max(maxAscent[3], Math.max(maxAscent[0], Math.max(maxAscent[1], maxAscent[2])));
                int descent = Math.max(maxDescent[3], Math.max(maxDescent[0], Math.max(maxDescent[1], maxDescent[2])));
                maxHeight = Math.max(maxHeight, ascent + descent);
            }
        } else {
            alternativeMaxHeight = Math.max(alternativeMaxHeight, weightedMaxHeight);
            if (useLargestChild && wm != MeasureSpec.EXACTLY) {
                for (int i = 0; i < count; i++) {
                    View child = getChildAt(i);
                    if (child == null || child.getVisibility() == GONE) continue;
                    LayoutParams lp = (LayoutParams) child.getLayoutParams();
                    if (lp.weight > 0) child.measure(MeasureSpec.makeMeasureSpec(largestChildWidth, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(child.getMeasuredHeight(), MeasureSpec.EXACTLY));
                }
            }
        }
        if (!allFillParent && hm != MeasureSpec.EXACTLY) maxHeight = alternativeMaxHeight;
        maxHeight += getPaddingTop() + getPaddingBottom();
        maxHeight = Math.max(maxHeight, getSuggestedMinimumHeight());
        setMeasuredDimension(widthSizeAndState | (childState & MEASURED_STATE_MASK), resolveSizeAndState(maxHeight, hs, childState << MEASURED_HEIGHT_STATE_SHIFT));
        if (matchHeight) forceUniformHeight(count, ws);
    }
    private void forceUniformHeight(int count, int ws) {
        int uniform = MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
        for (int i = 0; i < count; ++i) {
            View child = getChildAt(i);
            if (child == null || child.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            if (lp.height == LayoutParams.MATCH_PARENT) {
                int old = lp.width;
                lp.width = child.getMeasuredWidth();
                measureChildWithMargins(child, ws, 0, uniform, 0);
                lp.width = old;
            }
        }
    }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) { if (mOrientation == VERTICAL) layoutVertical(l, t, r, b); else layoutHorizontal(l, t, r, b); }
    void layoutVertical(int left, int top, int right, int bottom) {
        int paddingLeft = getPaddingLeft(), width = right - left, childRight = width - getPaddingRight(), childSpace = width - paddingLeft - getPaddingRight();
        int count = getChildCount();
        int majorGravity = mGravity & Gravity.VERTICAL_GRAVITY_MASK, minorGravity = mGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK;
        int childTop;
        switch (majorGravity) {
        case Gravity.BOTTOM: childTop = getPaddingTop() + bottom - top - mTotalLength; break;
        case Gravity.CENTER_VERTICAL: childTop = getPaddingTop() + (bottom - top - mTotalLength) / 2; break;
        default: childTop = getPaddingTop();
        }
        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            if (child == null || child.getVisibility() == GONE) continue;
            int cw = child.getMeasuredWidth(), ch = child.getMeasuredHeight();
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int gravity = lp.gravity < 0 ? minorGravity : lp.gravity;
            int abs = Gravity.getAbsoluteGravity(gravity, getLayoutDirection());
            int childLeft;
            switch (abs & Gravity.HORIZONTAL_GRAVITY_MASK) {
            case Gravity.CENTER_HORIZONTAL: childLeft = paddingLeft + ((childSpace - cw) / 2) + lp.leftMargin - lp.rightMargin; break;
            case Gravity.RIGHT: childLeft = childRight - cw - lp.rightMargin; break;
            default: childLeft = paddingLeft + lp.leftMargin;
            }
            if (hasDividerBeforeChildAt(i)) childTop += mDividerHeight;
            childTop += lp.topMargin;
            child.layout(childLeft, childTop, childLeft + cw, childTop + ch);
            childTop += ch + lp.bottomMargin;
        }
    }
    void layoutHorizontal(int left, int top, int right, int bottom) {
        int paddingTop = getPaddingTop(), height = bottom - top, childBottom = height - getPaddingBottom(), childSpace = height - paddingTop - getPaddingBottom();
        int count = getChildCount();
        int majorGravity = mGravity & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK, minorGravity = mGravity & Gravity.VERTICAL_GRAVITY_MASK;
        boolean baselineAligned = mBaselineAligned;
        int[] maxAscent = mMaxAscent, maxDescent = mMaxDescent;
        int childLeft;
        switch (Gravity.getAbsoluteGravity(majorGravity, getLayoutDirection())) {
        case Gravity.RIGHT: childLeft = getPaddingLeft() + right - left - mTotalLength; break;
        case Gravity.CENTER_HORIZONTAL: childLeft = getPaddingLeft() + (right - left - mTotalLength) / 2; break;
        default: childLeft = getPaddingLeft();
        }
        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            if (child == null || child.getVisibility() == GONE) continue;
            int cw = child.getMeasuredWidth(), ch = child.getMeasuredHeight(), childBaseline = -1;
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            if (baselineAligned && lp.height != LayoutParams.MATCH_PARENT) childBaseline = child.getBaseline();
            int gravity = lp.gravity < 0 ? minorGravity : lp.gravity;
            int childTop;
            switch (gravity & Gravity.VERTICAL_GRAVITY_MASK) {
            case Gravity.TOP: childTop = paddingTop + lp.topMargin; if (childBaseline != -1 && maxAscent != null) childTop += maxAscent[1] - childBaseline; break;
            case Gravity.CENTER_VERTICAL: childTop = paddingTop + ((childSpace - ch) / 2) + lp.topMargin - lp.bottomMargin; break;
            case Gravity.BOTTOM: childTop = childBottom - ch - lp.bottomMargin; if (childBaseline != -1 && maxDescent != null) { int descent = ch - childBaseline; childTop -= maxDescent[2] - descent; } break;
            default: childTop = paddingTop;
            }
            if (hasDividerBeforeChildAt(i)) childLeft += mDividerWidth;
            childLeft += lp.leftMargin;
            child.layout(childLeft, childTop, childLeft + cw, childTop + ch);
            childLeft += cw + lp.rightMargin;
        }
    }
    @Override protected void onDraw(Canvas c) {
        if (mDivider == null) return;
        int count = getChildCount();
        for (int i = 0; i < count; i++) {
            View child = getChildAt(i);
            if (child != null && child.getVisibility() != GONE && hasDividerBeforeChildAt(i)) {
                LayoutParams lp = (LayoutParams) child.getLayoutParams();
                if (mOrientation == VERTICAL) { int top = child.getTop() - lp.topMargin - mDividerHeight; mDivider.setBounds(getPaddingLeft() + mDividerPadding, top, getWidth() - getPaddingRight() - mDividerPadding, top + mDividerHeight); }
                else { int l = child.getLeft() - lp.leftMargin - mDividerWidth; mDivider.setBounds(l, getPaddingTop() + mDividerPadding, l + mDividerWidth, getHeight() - getPaddingBottom() - mDividerPadding); }
                mDivider.draw(c);
            }
        }
        if (hasDividerBeforeChildAt(count)) {
            View last = null; for (int i = count - 1; i >= 0; i--) if (getChildAt(i).getVisibility() != GONE) { last = getChildAt(i); break; }
            if (mOrientation == VERTICAL) { int bottom = last == null ? getHeight() - getPaddingBottom() - mDividerHeight : last.getBottom() + ((LayoutParams) last.getLayoutParams()).bottomMargin; mDivider.setBounds(getPaddingLeft() + mDividerPadding, bottom, getWidth() - getPaddingRight() - mDividerPadding, bottom + mDividerHeight); }
            else { int r = last == null ? getWidth() - getPaddingRight() - mDividerWidth : last.getRight() + ((LayoutParams) last.getLayoutParams()).rightMargin; mDivider.setBounds(r, getPaddingTop() + mDividerPadding, r + mDividerWidth, getHeight() - getPaddingBottom() - mDividerPadding); }
            mDivider.draw(c);
        }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    // ---- end of generated members
}
