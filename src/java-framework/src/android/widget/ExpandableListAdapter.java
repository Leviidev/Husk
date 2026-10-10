package android.widget;
public interface ExpandableListAdapter {
    void registerDataSetObserver(android.database.DataSetObserver o); void unregisterDataSetObserver(android.database.DataSetObserver o);
    int getGroupCount(); int getChildrenCount(int g); Object getGroup(int g); Object getChild(int g, int c); long getGroupId(int g); long getChildId(int g, int c); boolean hasStableIds();
    android.view.View getGroupView(int g, boolean expanded, android.view.View cv, android.view.ViewGroup parent);
    android.view.View getChildView(int g, int c, boolean last, android.view.View cv, android.view.ViewGroup parent);
    boolean isChildSelectable(int g, int c); boolean areAllItemsEnabled(); boolean isEmpty();
    void onGroupExpanded(int g); void onGroupCollapsed(int g);
    long getCombinedChildId(long groupId, long childId); long getCombinedGroupId(long groupId);
}
