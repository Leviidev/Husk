package android.widget;

import android.content.Context;
import android.database.DataSetObserver;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import java.util.ArrayList;

/** Groups that expand to their children: a ListView over the flattened visible rows. */
public class ExpandableListView extends ListView {
    public static final int PACKED_POSITION_TYPE_GROUP = 0, PACKED_POSITION_TYPE_CHILD = 1, PACKED_POSITION_TYPE_NULL = 2, CHILD_INDICATOR_INHERIT = -1;
    public static final long PACKED_POSITION_VALUE_NULL = 0x00000000FFFFFFFFL;
    public interface OnGroupCollapseListener { void onGroupCollapse(int g); }
    public interface OnGroupExpandListener { void onGroupExpand(int g); }
    public interface OnGroupClickListener { boolean onGroupClick(ExpandableListView p, View v, int g, long id); }
    public interface OnChildClickListener { boolean onChildClick(ExpandableListView p, View v, int g, int c, long id); }
    public static class ExpandableListContextMenuInfo implements android.view.ContextMenu.ContextMenuInfo { public View targetView; public long packedPosition, id; public ExpandableListContextMenuInfo(View t, long p, long i) { targetView = t; packedPosition = p; id = i; } }
    private ExpandableListAdapter mExpAdapter;
    private final java.util.HashSet<Integer> mExpanded = new java.util.HashSet<>();
    private final ArrayList<long[]> mRows = new ArrayList<>();
    private OnGroupCollapseListener mOnCollapse; private OnGroupExpandListener mOnExpand; private OnGroupClickListener mOnGroupClick; private OnChildClickListener mOnChildClick;
    private final Flat mFlat = new Flat();
    public ExpandableListView(Context c) { this(c, null); }
    public ExpandableListView(Context c, AttributeSet a) { this(c, a, android.R.attr.expandableListViewStyle); }
    public ExpandableListView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public ExpandableListView(Context c, AttributeSet a, int s, int r) {
        super(c, a, s, r);
        super.setOnItemClickListener((p, v, pos, id) -> handleClick(v, pos));
    }
    private void rebuild() {
        mRows.clear();
        if (mExpAdapter == null) return;
        for (int g = 0; g < mExpAdapter.getGroupCount(); g++) {
            mRows.add(new long[] { g, -1 });
            if (mExpanded.contains(g)) for (int c = 0; c < mExpAdapter.getChildrenCount(g); c++) mRows.add(new long[] { g, c });
        }
    }
    private void handleClick(View v, int pos) {
        long[] row = mRows.get(pos);
        int g = (int) row[0];
        if (row[1] < 0) {
            if (mOnGroupClick != null && mOnGroupClick.onGroupClick(this, v, g, mExpAdapter.getGroupId(g))) return;
            if (isGroupExpanded(g)) collapseGroup(g); else expandGroup(g);
        } else if (mOnChildClick != null) mOnChildClick.onChildClick(this, v, g, (int) row[1], mExpAdapter.getChildId(g, (int) row[1]));
    }
    @Override public void setAdapter(ListAdapter a) { throw new RuntimeException("For ExpandableListView, use setAdapter(ExpandableListAdapter) instead of setAdapter(ListAdapter)"); }
    @Override public ListAdapter getAdapter() { return super.getAdapter(); }
    @Override public void setOnItemClickListener(OnItemClickListener l) { super.setOnItemClickListener(l); }
    public void setAdapter(ExpandableListAdapter a) {
        mExpAdapter = a;
        if (a != null) a.registerDataSetObserver(new DataSetObserver() { @Override public void onChanged() { rebuild(); mFlat.notifyDataSetChanged(); } @Override public void onInvalidated() { rebuild(); mFlat.notifyDataSetInvalidated(); } });
        rebuild();
        super.setAdapter(mFlat);
    }
    public ExpandableListAdapter getExpandableListAdapter() { return mExpAdapter; }
    public boolean expandGroup(int g) { return expandGroup(g, false); }
    public boolean expandGroup(int g, boolean animate) { boolean added = mExpanded.add(g); if (added) { mExpAdapter.onGroupExpanded(g); rebuild(); mFlat.notifyDataSetChanged(); if (mOnExpand != null) mOnExpand.onGroupExpand(g); } return added; }
    public boolean collapseGroup(int g) { boolean removed = mExpanded.remove(g); if (removed) { mExpAdapter.onGroupCollapsed(g); rebuild(); mFlat.notifyDataSetChanged(); if (mOnCollapse != null) mOnCollapse.onGroupCollapse(g); } return removed; }
    public boolean isGroupExpanded(int g) { return mExpanded.contains(g); }
    public void setOnGroupCollapseListener(OnGroupCollapseListener l) { mOnCollapse = l; }
    public void setOnGroupExpandListener(OnGroupExpandListener l) { mOnExpand = l; }
    public void setOnGroupClickListener(OnGroupClickListener l) { mOnGroupClick = l; }
    public void setOnChildClickListener(OnChildClickListener l) { mOnChildClick = l; }
    public long getExpandableListPosition(int flat) { if (flat < 0 || flat >= mRows.size()) return PACKED_POSITION_VALUE_NULL; long[] r = mRows.get(flat); return r[1] < 0 ? getPackedPositionForGroup((int) r[0]) : getPackedPositionForChild((int) r[0], (int) r[1]); }
    public int getFlatListPosition(long packed) { int g = getPackedPositionGroup(packed), c = getPackedPositionType(packed) == PACKED_POSITION_TYPE_CHILD ? getPackedPositionChild(packed) : -1; for (int i = 0; i < mRows.size(); i++) if (mRows.get(i)[0] == g && mRows.get(i)[1] == c) return i; return -1; }
    public long getSelectedPosition() { return PACKED_POSITION_VALUE_NULL; }
    public long getSelectedId() { return -1; }
    public void setSelectedGroup(int g) { int f = getFlatListPosition(getPackedPositionForGroup(g)); if (f >= 0) setSelection(f); }
    public boolean setSelectedChild(int g, int c, boolean expand) { if (expand) expandGroup(g); int f = getFlatListPosition(getPackedPositionForChild(g, c)); if (f >= 0) { setSelection(f); return true; } return false; }
    public static int getPackedPositionType(long p) { if (p == PACKED_POSITION_VALUE_NULL) return PACKED_POSITION_TYPE_NULL; return (p & 0x8000000000000000L) == 0x8000000000000000L ? PACKED_POSITION_TYPE_CHILD : PACKED_POSITION_TYPE_GROUP; }
    public static int getPackedPositionGroup(long p) { if (p == PACKED_POSITION_VALUE_NULL) return -1; return (int) ((p & 0x7FFFFFFF00000000L) >> 32); }
    public static int getPackedPositionChild(long p) { if (p == PACKED_POSITION_VALUE_NULL) return -1; if ((p & 0x8000000000000000L) != 0x8000000000000000L) return -1; return (int) (p & 0x00000000FFFFFFFFL); }
    public static long getPackedPositionForChild(int g, int c) { return 0x8000000000000000L | (((long) g & 0x7FFFFFFF) << 32) | ((long) c & 0xFFFFFFFFL); }
    public static long getPackedPositionForGroup(int g) { return ((long) g & 0x7FFFFFFF) << 32; }
    public void setChildIndicator(android.graphics.drawable.Drawable d) {} public void setGroupIndicator(android.graphics.drawable.Drawable d) {}
    public void setChildIndicatorBounds(int l, int r) {} public void setIndicatorBounds(int l, int r) {} public void setChildDivider(android.graphics.drawable.Drawable d) {}
    private final class Flat extends BaseAdapter {
        public int getCount() { return mRows.size(); }
        public Object getItem(int p) { long[] r = mRows.get(p); return r[1] < 0 ? mExpAdapter.getGroup((int) r[0]) : mExpAdapter.getChild((int) r[0], (int) r[1]); }
        public long getItemId(int p) { long[] r = mRows.get(p); return r[1] < 0 ? mExpAdapter.getCombinedGroupId(mExpAdapter.getGroupId((int) r[0])) : mExpAdapter.getCombinedChildId(mExpAdapter.getGroupId((int) r[0]), mExpAdapter.getChildId((int) r[0], (int) r[1])); }
        @Override public boolean hasStableIds() { return mExpAdapter.hasStableIds(); }
        @Override public int getViewTypeCount() { if (mExpAdapter instanceof HeterogeneousExpandableList) { HeterogeneousExpandableList h = (HeterogeneousExpandableList) mExpAdapter; return h.getGroupTypeCount() + h.getChildTypeCount(); } return 2; }
        @Override public int getItemViewType(int p) { long[] r = mRows.get(p); if (mExpAdapter instanceof HeterogeneousExpandableList) { HeterogeneousExpandableList h = (HeterogeneousExpandableList) mExpAdapter; return r[1] < 0 ? h.getGroupType((int) r[0]) : h.getGroupTypeCount() + h.getChildType((int) r[0], (int) r[1]); } return r[1] < 0 ? 0 : 1; }
        @Override public boolean isEnabled(int p) { long[] r = mRows.get(p); return r[1] < 0 || mExpAdapter.isChildSelectable((int) r[0], (int) r[1]); }
        @Override public boolean areAllItemsEnabled() { return mExpAdapter.areAllItemsEnabled(); }
        public View getView(int p, View cv, ViewGroup parent) {
            long[] r = mRows.get(p);
            int g = (int) r[0];
            if (r[1] < 0) return mExpAdapter.getGroupView(g, isGroupExpanded(g), cv, parent);
            return mExpAdapter.getChildView(g, (int) r[1], r[1] == mExpAdapter.getChildrenCount(g) - 1, cv, parent);
        }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void onInitializeAccessibilityNodeInfoForItem(android.view.View p0, int p1, android.view.accessibility.AccessibilityNodeInfo p2) {}
    public void setChildIndicatorBoundsRelative(int p0, int p1) {}
    public void setIndicatorBoundsRelative(int p0, int p1) {}
    // ---- end of generated members
}
