package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.*;
import java.util.ArrayList;

public class ListView extends AbsListView {
    public class FixedViewInfo { public View view; public Object data; public boolean isSelectable; }
    ArrayList<FixedViewInfo> mHeaderViewInfos = new ArrayList<>(), mFooterViewInfos = new ArrayList<>();
    Drawable mDivider;
    int mDividerHeight;
    private boolean mHeaderDividersEnabled = true, mFooterDividersEnabled = true, mItemsCanFocus;
    private final boolean[] mIsScrap = new boolean[1];
    private final Rect mTempRect = new Rect();
    public ListView(Context c) { this(c, null); }
    public ListView(Context c, AttributeSet a) { this(c, a, android.R.attr.listViewStyle); }
    public ListView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public ListView(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.ListView, s, r);
        CharSequence[] entries = a.getTextArray(husk.S.ListView_entries);
        if (entries != null) setAdapter(new ArrayAdapter<>(c, android.R.layout.simple_list_item_1, entries));
        Drawable d = null;
        try { d = a.getDrawable(husk.S.ListView_divider); } catch (RuntimeException e) {}
        if (d != null) setDivider(d);
        int dh = a.getDimensionPixelSize(husk.S.ListView_dividerHeight, 0);
        if (dh != 0) setDividerHeight(dh);
        mHeaderDividersEnabled = a.getBoolean(husk.S.ListView_headerDividersEnabled, true);
        mFooterDividersEnabled = a.getBoolean(husk.S.ListView_footerDividersEnabled, true);
        a.recycle();
    }
    public int getMaxScrollAmount() { return (int) (0.33f * (mBottom - mTop)); }
    public void addHeaderView(View v, Object data, boolean isSelectable) {
        final FixedViewInfo info = new FixedViewInfo(); info.view = v; info.data = data; info.isSelectable = isSelectable;
        mHeaderViewInfos.add(info);
        if (mAdapter != null) { if (!(mAdapter instanceof HeaderViewListAdapter)) wrapHeaderListAdapterInternal(); if (mDataSetObserver != null) mDataSetObserver.onChanged(); }
    }
    public void addHeaderView(View v) { addHeaderView(v, null, true); }
    public int getHeaderViewsCount() { return mHeaderViewInfos.size(); }
    public boolean removeHeaderView(View v) { if (mHeaderViewInfos.size() > 0) { boolean result = false; if (mAdapter != null && ((HeaderViewListAdapter) mAdapter).removeHeader(v)) { if (mDataSetObserver != null) mDataSetObserver.onChanged(); result = true; } removeFixedViewInfo(v, mHeaderViewInfos); return result; } return false; }
    private void removeFixedViewInfo(View v, ArrayList<FixedViewInfo> where) { for (int i = 0; i < where.size(); i++) if (where.get(i).view == v) { where.remove(i); break; } }
    public void addFooterView(View v, Object data, boolean isSelectable) {
        final FixedViewInfo info = new FixedViewInfo(); info.view = v; info.data = data; info.isSelectable = isSelectable;
        mFooterViewInfos.add(info);
        if (mAdapter != null) { if (!(mAdapter instanceof HeaderViewListAdapter)) wrapHeaderListAdapterInternal(); if (mDataSetObserver != null) mDataSetObserver.onChanged(); }
    }
    public void addFooterView(View v) { addFooterView(v, null, true); }
    public int getFooterViewsCount() { return mFooterViewInfos.size(); }
    public boolean removeFooterView(View v) { if (mFooterViewInfos.size() > 0) { boolean result = false; if (mAdapter != null && ((HeaderViewListAdapter) mAdapter).removeFooter(v)) { if (mDataSetObserver != null) mDataSetObserver.onChanged(); result = true; } removeFixedViewInfo(v, mFooterViewInfos); return result; } return false; }
    @Override public ListAdapter getAdapter() { return mAdapter; }
    private void wrapHeaderListAdapterInternal() { mAdapter = new HeaderViewListAdapter(mHeaderViewInfos, mFooterViewInfos, mAdapter); }
    @Override public void setAdapter(ListAdapter adapter) {
        if (mHeaderViewInfos.size() > 0 || mFooterViewInfos.size() > 0) adapter = new HeaderViewListAdapter(mHeaderViewInfos, mFooterViewInfos, adapter);
        super.setAdapter(adapter);
    }
    public Drawable getDivider() { return mDivider; }
    public void setDivider(Drawable d) { mDividerHeight = d != null ? d.getIntrinsicHeight() : 0; mDivider = d; requestLayout(); invalidate(); }
    public int getDividerHeight() { return mDividerHeight; }
    public void setDividerHeight(int h) { mDividerHeight = h; requestLayout(); invalidate(); }
    public void setHeaderDividersEnabled(boolean e) { mHeaderDividersEnabled = e; invalidate(); }
    public boolean areHeaderDividersEnabled() { return mHeaderDividersEnabled; }
    public void setFooterDividersEnabled(boolean e) { mFooterDividersEnabled = e; invalidate(); }
    public boolean areFooterDividersEnabled() { return mFooterDividersEnabled; }
    public void setItemsCanFocus(boolean f) { mItemsCanFocus = f; if (!f) setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS); }
    public boolean getItemsCanFocus() { return mItemsCanFocus; }
    public void setOverscrollHeader(Drawable d) {} public void setOverscrollFooter(Drawable d) {}
    public long[] getCheckItemIds() { return getCheckedItemIds(); }
    @Override public void setSelection(int position) { setSelectionFromTop(position, 0); }
    public void smoothScrollByOffset(int offset) { smoothScrollToPosition(Math.max(0, Math.min(mItemCount - 1, mFirstPosition + offset))); }
    @Override protected void onMeasure(int ws, int hs) {
        super.onMeasure(ws, hs);
        int widthMode = MeasureSpec.getMode(ws), heightMode = MeasureSpec.getMode(hs), widthSize = MeasureSpec.getSize(ws), heightSize = MeasureSpec.getSize(hs);
        int childWidth = 0, childHeight = 0, childState = 0;
        mItemCount = mAdapter == null ? 0 : mAdapter.getCount();
        if (mItemCount > 0 && (widthMode == MeasureSpec.UNSPECIFIED || heightMode == MeasureSpec.UNSPECIFIED)) {
            final View child = obtainView(0, mIsScrap);
            measureScrapChild(child, 0, ws, heightSize);
            childWidth = child.getMeasuredWidth(); childHeight = child.getMeasuredHeight();
            childState = combineMeasuredStates(childState, child.getMeasuredState());
            mRecycler.addScrapView(child, 0);
        }
        if (widthMode == MeasureSpec.UNSPECIFIED) widthSize = mListPadding.left + mListPadding.right + childWidth + getVerticalScrollbarWidth();
        else widthSize |= (childState & MEASURED_STATE_MASK);
        if (heightMode == MeasureSpec.UNSPECIFIED) heightSize = mListPadding.top + mListPadding.bottom + childHeight + getVerticalFadingEdgeLength() * 2;
        if (heightMode == MeasureSpec.AT_MOST) heightSize = measureHeightOfChildren(ws, 0, -1, heightSize, -1);
        setMeasuredDimension(widthSize, heightSize);
        mWidthMeasureSpec = ws;
    }
    private void measureScrapChild(View child, int position, int ws, int heightHint) {
        LayoutParams p = (LayoutParams) child.getLayoutParams();
        if (p == null) { p = (LayoutParams) generateDefaultLayoutParams(); child.setLayoutParams(p); }
        p.viewType = mAdapter.getItemViewType(position);
        p.forceAdd = true;
        final int childWidthSpec = ViewGroup.getChildMeasureSpec(ws, mListPadding.left + mListPadding.right, p.width);
        final int lpHeight = p.height;
        final int childHeightSpec = lpHeight > 0 ? MeasureSpec.makeMeasureSpec(lpHeight, MeasureSpec.EXACTLY) : MeasureSpec.makeSafeMeasureSpec(heightHint, MeasureSpec.UNSPECIFIED);
        child.measure(childWidthSpec, childHeightSpec);
    }
    final int measureHeightOfChildren(int ws, int startPosition, int endPosition, final int maxHeight, int disallowPartialChildPosition) {
        final ListAdapter adapter = mAdapter;
        if (adapter == null) return mListPadding.top + mListPadding.bottom;
        int returnedHeight = mListPadding.top + mListPadding.bottom;
        final int dividerHeight = mDividerHeight;
        int prevHeightWithoutPartialChild = 0;
        endPosition = endPosition == -1 ? adapter.getCount() - 1 : endPosition;
        for (int i = startPosition; i <= endPosition; ++i) {
            View child = obtainView(i, mIsScrap);
            measureScrapChild(child, i, ws, maxHeight);
            if (i > 0) returnedHeight += dividerHeight;
            mRecycler.addScrapView(child, -1);
            returnedHeight += child.getMeasuredHeight();
            if (returnedHeight >= maxHeight) return (disallowPartialChildPosition >= 0 && i > disallowPartialChildPosition && prevHeightWithoutPartialChild > 0 && returnedHeight != maxHeight) ? prevHeightWithoutPartialChild : maxHeight;
            if (disallowPartialChildPosition >= 0 && i >= disallowPartialChildPosition) prevHeightWithoutPartialChild = returnedHeight;
        }
        return returnedHeight;
    }
    private int getVerticalScrollbarWidth() { return 0; }
    @Override int findMotionRow(int y) {
        int childCount = getChildCount();
        if (childCount > 0) {
            if (!mStackFromBottom) { for (int i = 0; i < childCount; i++) { View v = getChildAt(i); if (y <= v.getBottom()) return mFirstPosition + i; } }
            else { for (int i = childCount - 1; i >= 0; i--) { View v = getChildAt(i); if (y >= v.getTop()) return mFirstPosition + i; } }
        }
        return INVALID_POSITION;
    }
    @Override protected void layoutChildren() {
        final boolean blockLayoutRequests = mBlockLayoutRequests;
        if (blockLayoutRequests) return;
        mBlockLayoutRequests = true;
        try {
            invalidate();
            if (mAdapter == null) { resetList(); invokeOnItemScrollListener(); return; }
            final int childrenTop = mListPadding.top, childrenBottom = mBottom - mTop - mListPadding.bottom;
            final int childCount = getChildCount();
            int topOffset = childCount > 0 ? getChildAt(0).getTop() : childrenTop;
            if (mDataChanged) handleDataChanged();
            mItemCount = mAdapter.getCount();
            if (mItemCount == 0) { resetList(); invokeOnItemScrollListener(); return; }
            if (mNeedSync) { topOffset = childrenTop + mSyncTop; mNeedSync = false; }
            // every child back to the recycler, then fill again from the first position
            final int firstPosition = mFirstPosition;
            for (int i = 0; i < childCount; i++) {
                View c = getChildAt(i);
                LayoutParams lp = (LayoutParams) c.getLayoutParams();
                if (lp != null && lp.viewType == ITEM_VIEW_TYPE_HEADER_OR_FOOTER) continue;
                mRecycler.addScrapView(c, firstPosition + i);
            }
            detachAllViewsFromParent();
            for (int i = 0; i < childCount; i++) {}
            mRecycler.removeSkippedScrap();
            if (mScrollToEnd || mFirstPosition >= mItemCount) {
                mScrollToEnd = false;
                fillUp(mItemCount - 1, childrenBottom);
                adjustViewsUpOrDown();
            } else if (mStackFromBottom && childCount == 0) {
                fillUp(mItemCount - 1, childrenBottom);
                adjustViewsUpOrDown();
            } else {
                mFirstPosition = Math.max(0, Math.min(firstPosition, mItemCount - 1));
                fillDown(mFirstPosition, Math.min(topOffset, childrenTop + 0) > childrenTop ? topOffset : topOffset);
                correctTooLow();
                adjustViewsUpOrDown();
            }
            detachLeftoverScrap();
            if (mSelectorPosition != INVALID_POSITION) { View s = getChildAt(mSelectorPosition - mFirstPosition); if (s != null) positionSelector(s); }
            mDataChanged = false;
            setNextSelectedPositionInt(mSelectedPosition);
            updateOnScreenCheckedViews();
            invokeOnItemScrollListener();
        } finally {
            mBlockLayoutRequests = false;
        }
    }
    private void detachLeftoverScrap() {
        // views the recycler still holds were detached from the parent; make sure they are out of the window
        java.util.ArrayList<View> l = new java.util.ArrayList<>();
        mRecycler.reclaimScrapViews(l);
        for (View v : l) if (v.getParent() == null && v.isAttachedToWindow()) v.huskDetach();
    }
    /** After filling from the top, a list that ends above the bottom edge moves down (when items are above), so the last item touches the bottom. */
    private void correctTooLow() {
        final int childCount = getChildCount();
        if (childCount == 0) return;
        final int lastPosition = mFirstPosition + childCount - 1;
        if (lastPosition != mItemCount - 1) return;
        final View lastChild = getChildAt(childCount - 1);
        final int lastBottom = lastChild.getBottom(), end = (mBottom - mTop) - mListPadding.bottom;
        int bottomOffset = end - lastBottom;
        View firstChild = getChildAt(0);
        final int firstTop = firstChild.getTop();
        if (bottomOffset > 0 && (mFirstPosition > 0 || firstTop < mListPadding.top)) {
            if (mFirstPosition == 0) bottomOffset = Math.min(bottomOffset, mListPadding.top - firstTop);
            offsetChildrenTopAndBottom(bottomOffset);
            if (mFirstPosition > 0) { fillUp(mFirstPosition - 1, getChildAt(0).getTop() - mDividerHeight); adjustViewsUpOrDown(); }
        }
    }
    private void adjustViewsUpOrDown() {
        final int childCount = getChildCount();
        if (childCount == 0) return;
        int delta;
        if (!mStackFromBottom) { View child = getChildAt(0); delta = child.getTop() - mListPadding.top; if (mFirstPosition != 0) delta -= mDividerHeight; if (delta < 0) delta = 0; }
        else { View child = getChildAt(childCount - 1); delta = child.getBottom() - (getHeight() - mListPadding.bottom); if (mFirstPosition + childCount < mItemCount) delta += mDividerHeight; if (delta > 0) delta = 0; }
        if (delta != 0) offsetChildrenTopAndBottom(-delta);
    }
    @Override void fillGap(boolean down) {
        final int count = getChildCount();
        if (down) { final int startOffset = count > 0 ? getChildAt(count - 1).getBottom() + mDividerHeight : mListPadding.top; fillDown(mFirstPosition + count, startOffset); }
        else { final int startOffset = count > 0 ? getChildAt(0).getTop() - mDividerHeight : getHeight() - mListPadding.bottom; fillUp(mFirstPosition - 1, startOffset); }
    }
    private void fillDown(int pos, int nextTop) {
        int end = mBottom - mTop - mListPadding.bottom;
        while (nextTop < end && pos < mItemCount) {
            View child = makeAndAddView(pos, nextTop, true);
            nextTop = child.getBottom() + mDividerHeight;
            pos++;
        }
    }
    private void fillUp(int pos, int nextBottom) {
        int end = mListPadding.top;
        while (nextBottom > end && pos >= 0) {
            View child = makeAndAddView(pos, nextBottom, false);
            nextBottom = child.getTop() - mDividerHeight;
            pos--;
        }
        mFirstPosition = pos + 1;
    }
    private View makeAndAddView(int position, int y, boolean flow) {
        final View child = obtainView(position, mIsScrap);
        setupChild(child, position, y, flow, mListPadding.left);
        return child;
    }
    private void setupChild(View child, int position, int y, boolean flowDown, int childrenLeft) {
        LayoutParams p = (LayoutParams) child.getLayoutParams();
        if (p == null) p = (LayoutParams) generateDefaultLayoutParams();
        p.viewType = mAdapter.getItemViewType(position);
        if (child.getParent() == this) detachViewFromParent(child);
        if (child.getParent() != null) ((ViewGroup) child.getParent()).removeView(child);
        addViewInLayout(child, flowDown ? -1 : 0, p, true);
        setupChildChecked(child, position);
        int childWidthSpec = ViewGroup.getChildMeasureSpec(mWidthMeasureSpec, mListPadding.left + mListPadding.right, p.width);
        int lpHeight = p.height;
        int childHeightSpec = lpHeight > 0 ? MeasureSpec.makeMeasureSpec(lpHeight, MeasureSpec.EXACTLY) : MeasureSpec.makeSafeMeasureSpec(getMeasuredHeight(), MeasureSpec.UNSPECIFIED);
        child.measure(childWidthSpec, childHeightSpec);
        final int w = child.getMeasuredWidth(), h = child.getMeasuredHeight(), childTop = flowDown ? y : y - h;
        child.layout(childrenLeft, childTop, childrenLeft + w, childTop + h);
    }
    @Override protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        final int dividerHeight = mDividerHeight;
        if (dividerHeight > 0 && mDivider != null) {
            final Rect bounds = mTempRect;
            bounds.left = mPaddingLeft; bounds.right = mRight - mLeft - mPaddingRight;
            final int count = getChildCount(), headerCount = getHeaderViewsCount(), itemCount = mItemCount, footerLimit = itemCount - mFooterViewInfos.size();
            for (int i = 0; i < count; i++) {
                final int itemIndex = mFirstPosition + i;
                final boolean isHeader = itemIndex < headerCount, isFooter = itemIndex >= footerLimit;
                if ((mHeaderDividersEnabled || !isHeader) && (mFooterDividersEnabled || !isFooter) && itemIndex < itemCount - 1) {
                    final View child = getChildAt(i);
                    bounds.top = child.getBottom(); bounds.bottom = bounds.top + dividerHeight;
                    mDivider.setBounds(bounds); mDivider.draw(canvas);
                }
            }
        }
    }
    @Override public boolean onKeyDown(int keyCode, KeyEvent e) {
        if (keyCode == KeyEvent.KEYCODE_DPAD_DOWN) { scrollListBy(getMaxScrollAmount() / 3); return true; }
        if (keyCode == KeyEvent.KEYCODE_DPAD_UP) { scrollListBy(-getMaxScrollAmount() / 3); return true; }
        return super.onKeyDown(keyCode, e);
    }
    @Override public CharSequence getAccessibilityClassName() { return ListView.class.getName(); }
}
