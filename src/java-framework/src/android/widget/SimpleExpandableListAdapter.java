package android.widget;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import java.util.List;
import java.util.Map;

public class SimpleExpandableListAdapter extends BaseExpandableListAdapter {
    private final List<? extends Map<String, ?>> mGroupData; private final int mExpandedGroupLayout, mCollapsedGroupLayout; private final String[] mGroupFrom; private final int[] mGroupTo;
    private final List<? extends List<? extends Map<String, ?>>> mChildData; private final int mChildLayout, mLastChildLayout; private final String[] mChildFrom; private final int[] mChildTo;
    private final LayoutInflater mInflater;
    public SimpleExpandableListAdapter(Context c, List<? extends Map<String, ?>> groupData, int groupLayout, String[] groupFrom, int[] groupTo, List<? extends List<? extends Map<String, ?>>> childData, int childLayout, String[] childFrom, int[] childTo) { this(c, groupData, groupLayout, groupLayout, groupFrom, groupTo, childData, childLayout, childLayout, childFrom, childTo); }
    public SimpleExpandableListAdapter(Context c, List<? extends Map<String, ?>> groupData, int expandedGroupLayout, int collapsedGroupLayout, String[] groupFrom, int[] groupTo, List<? extends List<? extends Map<String, ?>>> childData, int childLayout, String[] childFrom, int[] childTo) { this(c, groupData, expandedGroupLayout, collapsedGroupLayout, groupFrom, groupTo, childData, childLayout, childLayout, childFrom, childTo); }
    public SimpleExpandableListAdapter(Context c, List<? extends Map<String, ?>> groupData, int expandedGroupLayout, int collapsedGroupLayout, String[] groupFrom, int[] groupTo, List<? extends List<? extends Map<String, ?>>> childData, int childLayout, int lastChildLayout, String[] childFrom, int[] childTo) {
        mGroupData = groupData; mExpandedGroupLayout = expandedGroupLayout; mCollapsedGroupLayout = collapsedGroupLayout; mGroupFrom = groupFrom; mGroupTo = groupTo;
        mChildData = childData; mChildLayout = childLayout; mLastChildLayout = lastChildLayout; mChildFrom = childFrom; mChildTo = childTo;
        mInflater = LayoutInflater.from(c);
    }
    public Object getChild(int g, int c) { return mChildData.get(g).get(c); }
    public long getChildId(int g, int c) { return c; }
    public View getChildView(int g, int c, boolean last, View cv, ViewGroup parent) { View v = cv == null ? mInflater.inflate(last ? mLastChildLayout : mChildLayout, parent, false) : cv; bind(v, mChildData.get(g).get(c), mChildFrom, mChildTo); return v; }
    private void bind(View view, Map<String, ?> data, String[] from, int[] to) { for (int i = 0; i < to.length; i++) { TextView v = view.findViewById(to[i]); if (v != null) v.setText(String.valueOf(data.get(from[i]))); } }
    public int getChildrenCount(int g) { return mChildData.get(g).size(); }
    public Object getGroup(int g) { return mGroupData.get(g); }
    public int getGroupCount() { return mGroupData.size(); }
    public long getGroupId(int g) { return g; }
    public View getGroupView(int g, boolean expanded, View cv, ViewGroup parent) { View v = cv == null ? mInflater.inflate(expanded ? mExpandedGroupLayout : mCollapsedGroupLayout, parent, false) : cv; bind(v, mGroupData.get(g), mGroupFrom, mGroupTo); return v; }
    public boolean isChildSelectable(int g, int c) { return true; }
    public boolean hasStableIds() { return true; }
}
