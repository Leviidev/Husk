package android.widget;

import android.database.DataSetObservable;
import android.database.DataSetObserver;

public abstract class BaseExpandableListAdapter implements ExpandableListAdapter, HeterogeneousExpandableList {
    private final DataSetObservable mDataSetObservable = new DataSetObservable();
    public void registerDataSetObserver(DataSetObserver o) { mDataSetObservable.registerObserver(o); }
    public void unregisterDataSetObserver(DataSetObserver o) { mDataSetObservable.unregisterObserver(o); }
    public void notifyDataSetInvalidated() { mDataSetObservable.notifyInvalidated(); }
    public void notifyDataSetChanged() { mDataSetObservable.notifyChanged(); }
    public boolean areAllItemsEnabled() { return true; }
    public void onGroupCollapsed(int g) {}
    public void onGroupExpanded(int g) {}
    public long getCombinedChildId(long groupId, long childId) { return 0x8000000000000000L | ((groupId & 0x7FFFFFFF) << 32) | (childId & 0xFFFFFFFF); }
    public long getCombinedGroupId(long groupId) { return (groupId & 0x7FFFFFFF) << 32; }
    public boolean isEmpty() { return getGroupCount() == 0; }
    public int getChildType(int g, int c) { return 0; }
    public int getChildTypeCount() { return 1; }
    public int getGroupType(int g) { return 0; }
    public int getGroupTypeCount() { return 1; }
}
