package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

public abstract class AbsSpinner extends AdapterView<SpinnerAdapter> {
    SpinnerAdapter mAdapter;
    private AdapterDataSetObserver mObserver;
    public AbsSpinner(Context c) { this(c, null); }
    public AbsSpinner(Context c, AttributeSet a) { this(c, a, 0); }
    public AbsSpinner(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public AbsSpinner(Context c, AttributeSet a, int s, int r) { super(c, a, s, r); setFocusable(true); setWillNotDraw(false); }
    @Override public SpinnerAdapter getAdapter() { return mAdapter; }
    @Override public void setAdapter(SpinnerAdapter adapter) {
        if (mAdapter != null && mObserver != null) mAdapter.unregisterDataSetObserver(mObserver);
        mAdapter = adapter;
        mOldSelectedPosition = INVALID_POSITION; mOldItemCount = mItemCount;
        if (adapter != null) {
            mItemCount = adapter.getCount();
            mObserver = new AdapterDataSetObserver();
            adapter.registerDataSetObserver(mObserver);
            int position = mItemCount > 0 ? 0 : INVALID_POSITION;
            setSelectedPositionInt(position); setNextSelectedPositionInt(position);
            if (mItemCount == 0) checkSelectionChanged();
        } else { mItemCount = 0; setSelectedPositionInt(INVALID_POSITION); setNextSelectedPositionInt(INVALID_POSITION); checkSelectionChanged(); }
        mDataChanged = true;
        requestLayout();
    }
    @Override public void setSelection(int position) { setSelection(position, false); }
    public void setSelection(int position, boolean animate) {
        if (position < 0 || position >= mItemCount) return;
        setNextSelectedPositionInt(position);
        setSelectedPositionInt(position);
        requestLayout(); invalidate();
        post(this::checkSelectionChanged);
    }
    @Override public View getSelectedView() { return getChildCount() > 0 ? getChildAt(0) : null; }
    @Override public int getCount() { return mItemCount; }
    public int pointToPosition(int x, int y) { return mSelectedPosition; }
}
