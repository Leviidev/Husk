package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.*;

public class GridView extends AbsListView {
    public static final int NO_STRETCH = 0, STRETCH_SPACING = 1, STRETCH_COLUMN_WIDTH = 2, STRETCH_SPACING_UNIFORM = 3, AUTO_FIT = -1;
    private int mNumColumns = AUTO_FIT, mHorizontalSpacing, mRequestedHorizontalSpacing, mVerticalSpacing, mStretchMode = STRETCH_COLUMN_WIDTH, mColumnWidth, mRequestedColumnWidth, mRequestedNumColumns, mGravity = Gravity.START;
    private final boolean[] mIsScrap = new boolean[1];
    public GridView(Context c) { this(c, null); }
    public GridView(Context c, AttributeSet a) { this(c, a, android.R.attr.gridViewStyle); }
    public GridView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public GridView(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.GridView, s, r);
        setHorizontalSpacing(a.getDimensionPixelOffset(husk.S.GridView_horizontalSpacing, 0));
        setVerticalSpacing(a.getDimensionPixelOffset(husk.S.GridView_verticalSpacing, 0));
        int index = a.getInt(husk.S.GridView_stretchMode, STRETCH_COLUMN_WIDTH);
        if (index >= 0) setStretchMode(index);
        int columnWidth = a.getDimensionPixelOffset(husk.S.GridView_columnWidth, -1);
        if (columnWidth > 0) setColumnWidth(columnWidth);
        setNumColumns(a.getInt(husk.S.GridView_numColumns, 1));
        index = a.getInt(husk.S.GridView_gravity, -1);
        if (index >= 0) setGravity(index);
        a.recycle();
    }
    @Override public ListAdapter getAdapter() { return mAdapter; }
    @Override public void setSelection(int position) { setSelectionFromTop(position, 0); }
    @Override public void setSelectionFromTop(int position, int y) { super.setSelectionFromTop(mNumColumns > 0 ? position - position % mNumColumns : position, y); }
    public void setGravity(int g) { if (mGravity != g) { mGravity = g; requestLayout(); } }
    public int getGravity() { return mGravity; }
    public void setHorizontalSpacing(int s) { if (s != mRequestedHorizontalSpacing) { mRequestedHorizontalSpacing = s; requestLayout(); } }
    public int getHorizontalSpacing() { return mHorizontalSpacing; }
    public int getRequestedHorizontalSpacing() { return mRequestedHorizontalSpacing; }
    public void setVerticalSpacing(int s) { if (s != mVerticalSpacing) { mVerticalSpacing = s; requestLayout(); } }
    public int getVerticalSpacing() { return mVerticalSpacing; }
    public void setStretchMode(int m) { if (m != mStretchMode) { mStretchMode = m; requestLayout(); } }
    public int getStretchMode() { return mStretchMode; }
    public void setColumnWidth(int w) { if (w != mRequestedColumnWidth) { mRequestedColumnWidth = w; requestLayout(); } }
    public int getColumnWidth() { return mColumnWidth; }
    public int getRequestedColumnWidth() { return mRequestedColumnWidth; }
    public void setNumColumns(int n) { if (n != mRequestedNumColumns) { mRequestedNumColumns = n; requestLayout(); } }
    public int getNumColumns() { return mNumColumns; }
    private boolean determineColumns(int availableSpace) {
        final int requestedHorizontalSpacing = mRequestedHorizontalSpacing, stretchMode = mStretchMode, requestedColumnWidth = mRequestedColumnWidth;
        boolean didNotInitiallyFit = false;
        if (mRequestedNumColumns == AUTO_FIT) { if (requestedColumnWidth > 0) mNumColumns = (availableSpace + requestedHorizontalSpacing) / (requestedColumnWidth + requestedHorizontalSpacing); else mNumColumns = 2; }
        else mNumColumns = mRequestedNumColumns;
        if (mNumColumns <= 0) mNumColumns = 1;
        switch (stretchMode) {
        case NO_STRETCH: mColumnWidth = requestedColumnWidth; mHorizontalSpacing = requestedHorizontalSpacing; break;
        default:
            int spaceLeftOver = availableSpace - (mNumColumns * requestedColumnWidth) - ((mNumColumns - 1) * requestedHorizontalSpacing);
            if (spaceLeftOver < 0) didNotInitiallyFit = true;
            switch (stretchMode) {
            case STRETCH_COLUMN_WIDTH: mColumnWidth = requestedColumnWidth + spaceLeftOver / mNumColumns; mHorizontalSpacing = requestedHorizontalSpacing; break;
            case STRETCH_SPACING: mColumnWidth = requestedColumnWidth; mHorizontalSpacing = mNumColumns > 1 ? requestedHorizontalSpacing + spaceLeftOver / (mNumColumns - 1) : requestedHorizontalSpacing + spaceLeftOver; break;
            case STRETCH_SPACING_UNIFORM: mColumnWidth = requestedColumnWidth; mHorizontalSpacing = mNumColumns > 1 ? requestedHorizontalSpacing + spaceLeftOver / (mNumColumns + 1) : requestedHorizontalSpacing + spaceLeftOver; break;
            }
        }
        return didNotInitiallyFit;
    }
    @Override protected void onMeasure(int ws, int hs) {
        super.onMeasure(ws, hs);
        int widthMode = MeasureSpec.getMode(ws), heightMode = MeasureSpec.getMode(hs), widthSize = MeasureSpec.getSize(ws), heightSize = MeasureSpec.getSize(hs);
        if (widthMode == MeasureSpec.UNSPECIFIED) { widthSize = mColumnWidth > 0 ? mColumnWidth + mListPadding.left + mListPadding.right : mListPadding.left + mListPadding.right; }
        int childWidth = widthSize - mListPadding.left - mListPadding.right;
        boolean didNotInitiallyFit = determineColumns(childWidth);
        int childHeight = 0;
        mItemCount = mAdapter == null ? 0 : mAdapter.getCount();
        final int count = mItemCount;
        if (count > 0) {
            final View child = obtainView(0, mIsScrap);
            AbsListView.LayoutParams p = (AbsListView.LayoutParams) child.getLayoutParams();
            if (p == null) { p = (AbsListView.LayoutParams) generateDefaultLayoutParams(); child.setLayoutParams(p); }
            p.viewType = mAdapter.getItemViewType(0); p.forceAdd = true;
            int childHeightSpec = getChildMeasureSpec(MeasureSpec.makeSafeMeasureSpec(MeasureSpec.getSize(hs), MeasureSpec.UNSPECIFIED), 0, p.height);
            int childWidthSpec = getChildMeasureSpec(MeasureSpec.makeMeasureSpec(mColumnWidth, MeasureSpec.EXACTLY), 0, p.width);
            child.measure(childWidthSpec, childHeightSpec);
            childHeight = child.getMeasuredHeight();
            mRecycler.addScrapView(child, -1);
        }
        if (heightMode == MeasureSpec.UNSPECIFIED) heightSize = mListPadding.top + mListPadding.bottom + childHeight + getVerticalFadingEdgeLength() * 2;
        if (heightMode == MeasureSpec.AT_MOST) {
            int ourSize = mListPadding.top + mListPadding.bottom;
            final int numColumns = mNumColumns;
            for (int i = 0; i < count; i += numColumns) { ourSize += childHeight; if (i + numColumns < count) ourSize += mVerticalSpacing; if (ourSize >= heightSize) { ourSize = heightSize; break; } }
            heightSize = ourSize;
        }
        if (widthMode == MeasureSpec.AT_MOST && mRequestedNumColumns != AUTO_FIT) {
            int ourSize = (mRequestedNumColumns * mColumnWidth) + ((mRequestedNumColumns - 1) * mHorizontalSpacing) + mListPadding.left + mListPadding.right;
            if (ourSize > widthSize || didNotInitiallyFit) widthSize |= MEASURED_STATE_TOO_SMALL;
        }
        setMeasuredDimension(widthSize, heightSize);
        mWidthMeasureSpec = ws;
    }
    @Override int findMotionRow(int y) {
        final int childCount = getChildCount();
        if (childCount > 0) for (int i = 0; i < childCount; i += mNumColumns) {
            int rowBottom = 0; for (int k = i; k < Math.min(childCount, i + mNumColumns); k++) rowBottom = Math.max(rowBottom, getChildAt(k).getBottom());
            if (y <= rowBottom) return mFirstPosition + Math.min(childCount - 1, i + columnAt(mMotionXHusk));
        }
        return INVALID_POSITION;
    }
    private int mMotionXHusk;
    @Override public boolean onInterceptTouchEvent(MotionEvent ev) { if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) mMotionXHusk = (int) ev.getX(); return super.onInterceptTouchEvent(ev); }
    @Override public boolean onTouchEvent(MotionEvent ev) { if (ev.getActionMasked() == MotionEvent.ACTION_DOWN) mMotionXHusk = (int) ev.getX(); return super.onTouchEvent(ev); }
    /** The row start from findMotionRow, plus the column under the finger. */
    int columnAt(int x) { int left = mListPadding.left; for (int c = 0; c < mNumColumns; c++) { int r = left + mColumnWidth + mHorizontalSpacing / 2; if (x < r) return c; left += mColumnWidth + mHorizontalSpacing; } return mNumColumns - 1; }
    @Override protected void layoutChildren() {
        if (mBlockLayoutRequests) return;
        mBlockLayoutRequests = true;
        try {
            invalidate();
            if (mAdapter == null) { resetList(); invokeOnItemScrollListener(); return; }
            final int childrenTop = mListPadding.top;
            final int childCount = getChildCount();
            int topOffset = childCount > 0 ? getChildAt(0).getTop() : childrenTop;
            if (mDataChanged) handleDataChanged();
            mItemCount = mAdapter.getCount();
            if (mItemCount == 0) { resetList(); invokeOnItemScrollListener(); return; }
            if (mNeedSync) { topOffset = childrenTop + mSyncTop; mNeedSync = false; }
            final int firstPosition = mFirstPosition;
            for (int i = 0; i < childCount; i++) mRecycler.addScrapView(getChildAt(i), firstPosition + i);
            detachAllViewsFromParent();
            mRecycler.removeSkippedScrap();
            if (mScrollToEnd || mFirstPosition >= mItemCount) { mScrollToEnd = false; int last = mItemCount - 1; fillUp(last - last % mNumColumns, getHeight() - mListPadding.bottom); }
            else { int p = Math.max(0, Math.min(firstPosition, mItemCount - 1)); p -= p % mNumColumns; mFirstPosition = p; fillDown(p, topOffset); correctTooLow(); }
            java.util.ArrayList<View> l = new java.util.ArrayList<>(); mRecycler.reclaimScrapViews(l);
            for (View v : l) if (v.getParent() == null && v.isAttachedToWindow()) v.huskDetach();
            mDataChanged = false;
            updateOnScreenCheckedViews();
            invokeOnItemScrollListener();
        } finally { mBlockLayoutRequests = false; }
    }
    private void correctTooLow() {
        final int childCount = getChildCount();
        if (childCount == 0 || mFirstPosition + childCount != mItemCount) return;
        int bottomOffset = (getHeight() - mListPadding.bottom) - getChildAt(childCount - 1).getBottom();
        int firstTop = getChildAt(0).getTop();
        if (bottomOffset > 0 && (mFirstPosition > 0 || firstTop < mListPadding.top)) {
            if (mFirstPosition == 0) bottomOffset = Math.min(bottomOffset, mListPadding.top - firstTop);
            offsetChildrenTopAndBottom(bottomOffset);
            if (mFirstPosition > 0) fillUp(mFirstPosition - mNumColumns, getChildAt(0).getTop() - mVerticalSpacing);
        }
    }
    @Override void fillGap(boolean down) {
        final int numColumns = mNumColumns, count = getChildCount();
        if (down) { int startOffset = count > 0 ? getChildAt(count - 1).getBottom() + mVerticalSpacing : mListPadding.top; int position = mFirstPosition + count; fillDown(position, startOffset); }
        else { int startOffset = count > 0 ? getChildAt(0).getTop() - mVerticalSpacing : getHeight() - mListPadding.bottom; int position = mFirstPosition - numColumns; fillUp(position, startOffset); }
    }
    private void fillDown(int pos, int nextTop) {
        int end = mBottom - mTop - mListPadding.bottom;
        while (nextTop < end && pos < mItemCount) { int bottom = makeRow(pos, nextTop, true); nextTop = bottom + mVerticalSpacing; pos += mNumColumns; }
    }
    private void fillUp(int pos, int nextBottom) {
        int end = mListPadding.top;
        while (nextBottom > end && pos >= 0) { int top = makeRow(pos, nextBottom, false); nextBottom = top - mVerticalSpacing; mFirstPosition = pos; pos -= mNumColumns; }
    }
    /** One row starting at startPos; returns its bottom (flowing down) or top (flowing up). */
    private int makeRow(int startPos, int y, boolean flow) {
        final int columnWidth = mColumnWidth, horizontalSpacing = mHorizontalSpacing;
        int nextLeft = mListPadding.left + (mStretchMode == STRETCH_SPACING_UNIFORM ? horizontalSpacing : 0);
        final int last = Math.min(startPos + mNumColumns, mItemCount);
        View[] row = new View[last - startPos];
        int rowHeight = 0;
        for (int pos = startPos; pos < last; pos++) {
            View child = obtainView(pos, mIsScrap);
            AbsListView.LayoutParams p = (AbsListView.LayoutParams) child.getLayoutParams();
            if (p == null) p = (AbsListView.LayoutParams) generateDefaultLayoutParams();
            p.viewType = mAdapter.getItemViewType(pos);
            if (child.getParent() != null) ((ViewGroup) child.getParent()).removeView(child);
            int index = flow ? -1 : pos - startPos;
            addViewInLayout(child, index, p, true);
            setupChildChecked(child, pos);
            int childHeightSpec = getChildMeasureSpec(MeasureSpec.makeSafeMeasureSpec(0, MeasureSpec.UNSPECIFIED), 0, p.height);
            int childWidthSpec = getChildMeasureSpec(MeasureSpec.makeMeasureSpec(columnWidth, MeasureSpec.EXACTLY), 0, p.width);
            child.measure(childWidthSpec, childHeightSpec);
            rowHeight = Math.max(rowHeight, child.getMeasuredHeight());
            row[pos - startPos] = child;
        }
        int top = flow ? y : y - rowHeight;
        for (int i = 0; i < row.length; i++) {
            View child = row[i];
            int w = child.getMeasuredWidth(), h = child.getMeasuredHeight();
            int childLeft;
            switch (Gravity.getAbsoluteGravity(mGravity, getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK) {
            case Gravity.CENTER_HORIZONTAL: childLeft = nextLeft + (columnWidth - w) / 2; break;
            case Gravity.RIGHT: childLeft = nextLeft + columnWidth - w; break;
            default: childLeft = nextLeft;
            }
            child.layout(childLeft, top, childLeft + w, top + h);
            nextLeft += columnWidth + horizontalSpacing;
        }
        return flow ? top + rowHeight : top;
    }
    @Override int rowAlign() { return Math.max(1, mNumColumns); }
    @Override public CharSequence getAccessibilityClassName() { return GridView.class.getName(); }
}
