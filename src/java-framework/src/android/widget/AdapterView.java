package android.widget;

import android.content.Context;
import android.database.DataSetObserver;
import android.util.AttributeSet;
import android.view.*;

public abstract class AdapterView<T extends Adapter> extends ViewGroup {
    public static final int ITEM_VIEW_TYPE_IGNORE = -1, ITEM_VIEW_TYPE_HEADER_OR_FOOTER = -2, INVALID_POSITION = -1;
    public static final long INVALID_ROW_ID = Long.MIN_VALUE;
    public interface OnItemClickListener { void onItemClick(AdapterView<?> parent, View view, int position, long id); }
    public interface OnItemLongClickListener { boolean onItemLongClick(AdapterView<?> parent, View view, int position, long id); }
    public interface OnItemSelectedListener { void onItemSelected(AdapterView<?> parent, View view, int position, long id); void onNothingSelected(AdapterView<?> parent); }
    public static class AdapterContextMenuInfo implements ContextMenu.ContextMenuInfo { public View targetView; public int position; public long id; public AdapterContextMenuInfo(View v, int p, long i) { targetView = v; position = p; id = i; } }
    int mFirstPosition;
    int mItemCount, mOldItemCount;
    int mSelectedPosition = INVALID_POSITION, mNextSelectedPosition = INVALID_POSITION, mOldSelectedPosition = INVALID_POSITION;
    long mSelectedRowId = INVALID_ROW_ID, mNextSelectedRowId = INVALID_ROW_ID;
    boolean mDataChanged, mInLayout, mBlockLayoutRequests;
    private View mEmptyView;
    OnItemClickListener mOnItemClickListener;
    OnItemLongClickListener mOnItemLongClickListener;
    OnItemSelectedListener mOnItemSelectedListener;
    public AdapterView(Context c) { this(c, null); }
    public AdapterView(Context c, AttributeSet a) { this(c, a, 0); }
    public AdapterView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public AdapterView(Context c, AttributeSet a, int s, int r) { super(c, a, s, r); }
    public void setOnItemClickListener(OnItemClickListener l) { mOnItemClickListener = l; }
    public final OnItemClickListener getOnItemClickListener() { return mOnItemClickListener; }
    public boolean performItemClick(View view, int position, long id) {
        if (mOnItemClickListener != null) { playSoundEffect(SoundEffectConstants.CLICK); mOnItemClickListener.onItemClick(this, view, position, id); return true; }
        return false;
    }
    public void setOnItemLongClickListener(OnItemLongClickListener l) { if (!isLongClickable()) setLongClickable(true); mOnItemLongClickListener = l; }
    public final OnItemLongClickListener getOnItemLongClickListener() { return mOnItemLongClickListener; }
    public void setOnItemSelectedListener(OnItemSelectedListener l) { mOnItemSelectedListener = l; }
    public final OnItemSelectedListener getOnItemSelectedListener() { return mOnItemSelectedListener; }
    public abstract T getAdapter();
    public abstract void setAdapter(T adapter);
    @Override public void addView(View child) { throw new UnsupportedOperationException("addView(View) is not supported in AdapterView"); }
    @Override public void addView(View child, int index) { throw new UnsupportedOperationException("addView(View, int) is not supported in AdapterView"); }
    @Override public void addView(View child, LayoutParams params) { throw new UnsupportedOperationException("addView(View, LayoutParams) is not supported in AdapterView"); }
    @Override public void addView(View child, int index, LayoutParams params) { throw new UnsupportedOperationException("addView(View, int, LayoutParams) is not supported in AdapterView"); }
    @Override public void removeView(View child) { throw new UnsupportedOperationException("removeView(View) is not supported in AdapterView"); }
    @Override public void removeViewAt(int index) { throw new UnsupportedOperationException("removeViewAt(int) is not supported in AdapterView"); }
    @Override public void removeAllViews() { throw new UnsupportedOperationException("removeAllViews() is not supported in AdapterView"); }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {}
    public int getSelectedItemPosition() { return mNextSelectedPosition; }
    public long getSelectedItemId() { return mNextSelectedRowId; }
    public abstract View getSelectedView();
    public Object getSelectedItem() { T adapter = getAdapter(); int s = getSelectedItemPosition(); return adapter != null && adapter.getCount() > 0 && s >= 0 ? adapter.getItem(s) : null; }
    public int getCount() { return mItemCount; }
    public int getPositionForView(View view) {
        View listItem = view;
        try { View v; while ((v = (View) listItem.getParent()) != null && !v.equals(this)) listItem = v; } catch (ClassCastException e) { return INVALID_POSITION; }
        if (listItem != null) { final int childCount = getChildCount(); for (int i = 0; i < childCount; i++) if (getChildAt(i).equals(listItem)) return mFirstPosition + i; }
        return INVALID_POSITION;
    }
    public int getFirstVisiblePosition() { return mFirstPosition; }
    public int getLastVisiblePosition() { return mFirstPosition + getChildCount() - 1; }
    public abstract void setSelection(int position);
    public void setEmptyView(View v) { mEmptyView = v; final T adapter = getAdapter(); updateEmptyStatus(adapter == null || adapter.isEmpty()); }
    public View getEmptyView() { return mEmptyView; }
    boolean isInFilterMode() { return false; }
    @Override public void setFocusable(boolean f) { super.setFocusable(f); }
    void checkFocus() { final T adapter = getAdapter(); final boolean empty = adapter == null || adapter.getCount() == 0; if (mEmptyView != null) updateEmptyStatus(empty); }
    private void updateEmptyStatus(boolean empty) {
        if (empty) { if (mEmptyView != null) { mEmptyView.setVisibility(View.VISIBLE); setVisibility(View.GONE); } else setVisibility(View.VISIBLE); if (mDataChanged) onLayout(false, mLeft, mTop, mRight, mBottom); }
        else { if (mEmptyView != null) mEmptyView.setVisibility(View.GONE); setVisibility(View.VISIBLE); }
    }
    public Object getItemAtPosition(int position) { T adapter = getAdapter(); return adapter == null || position < 0 ? null : adapter.getItem(position); }
    public long getItemIdAtPosition(int position) { T adapter = getAdapter(); return adapter == null || position < 0 ? INVALID_ROW_ID : adapter.getItemId(position); }
    @Override public void setOnClickListener(OnClickListener l) { throw new RuntimeException("Don't call setOnClickListener for an AdapterView. You probably want setOnItemClickListener instead"); }
    @Override protected void dispatchSaveInstanceState(android.util.SparseArray<android.os.Parcelable> c) { dispatchFreezeSelfOnly(c); }
    @Override protected void dispatchRestoreInstanceState(android.util.SparseArray<android.os.Parcelable> c) { dispatchThawSelfOnly(c); }
    void selectionChanged() {
        if (mOnItemSelectedListener != null) {
            int selection = getSelectedItemPosition();
            if (selection >= 0) { View v = getSelectedView(); mOnItemSelectedListener.onItemSelected(this, v, selection, getAdapter().getItemId(selection)); }
            else mOnItemSelectedListener.onNothingSelected(this);
        }
    }
    void setSelectedPositionInt(int p) { mSelectedPosition = p; mSelectedRowId = getItemIdAtPosition(p); }
    void setNextSelectedPositionInt(int p) { mNextSelectedPosition = p; mNextSelectedRowId = getItemIdAtPosition(p); }
    void checkSelectionChanged() { if (mSelectedPosition != mOldSelectedPosition) { selectionChanged(); mOldSelectedPosition = mSelectedPosition; } }
    @Override protected boolean canAnimate() { return super.canAnimate() && mItemCount > 0; }
    class AdapterDataSetObserver extends DataSetObserver {
        @Override public void onChanged() { mDataChanged = true; mOldItemCount = mItemCount; mItemCount = getAdapter().getCount(); checkFocus(); requestLayout(); }
        @Override public void onInvalidated() { mDataChanged = true; mOldItemCount = mItemCount; mItemCount = 0; mSelectedPosition = INVALID_POSITION; mSelectedRowId = INVALID_ROW_ID; mNextSelectedPosition = INVALID_POSITION; mNextSelectedRowId = INVALID_ROW_ID; checkFocus(); requestLayout(); }
        public void clearSavedState() {}
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public boolean dispatchPopulateAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) { return false; }
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    public void onInitializeAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public void onProvideAutofillStructure(android.view.ViewStructure p0, int p1) {}
    protected void onProvideStructure(android.view.ViewStructure p0, int p1, int p2) {}
    public boolean onRequestSendAccessibilityEventInternal(android.view.View p0, android.view.accessibility.AccessibilityEvent p1) { return false; }
    // ---- end of generated members
}
