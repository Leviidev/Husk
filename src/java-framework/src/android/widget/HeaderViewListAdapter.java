package android.widget;

import android.database.DataSetObserver;
import android.view.View;
import android.view.ViewGroup;
import java.util.ArrayList;

public class HeaderViewListAdapter implements WrapperListAdapter, Filterable {
    private final ListAdapter mAdapter;
    final ArrayList<ListView.FixedViewInfo> mHeaderViewInfos, mFooterViewInfos;
    private final boolean mIsFilterable;
    public HeaderViewListAdapter(ArrayList<ListView.FixedViewInfo> h, ArrayList<ListView.FixedViewInfo> f, ListAdapter a) { mAdapter = a; mIsFilterable = a instanceof Filterable; mHeaderViewInfos = h != null ? h : new ArrayList<>(); mFooterViewInfos = f != null ? f : new ArrayList<>(); }
    public int getHeadersCount() { return mHeaderViewInfos.size(); }
    public int getFootersCount() { return mFooterViewInfos.size(); }
    public boolean isEmpty() { return mAdapter == null || mAdapter.isEmpty(); }
    public boolean removeHeader(View v) { for (int i = 0; i < mHeaderViewInfos.size(); i++) if (mHeaderViewInfos.get(i).view == v) { mHeaderViewInfos.remove(i); return true; } return false; }
    public boolean removeFooter(View v) { for (int i = 0; i < mFooterViewInfos.size(); i++) if (mFooterViewInfos.get(i).view == v) { mFooterViewInfos.remove(i); return true; } return false; }
    public int getCount() { return getFootersCount() + getHeadersCount() + (mAdapter != null ? mAdapter.getCount() : 0); }
    public boolean areAllItemsEnabled() { return mAdapter == null || mAdapter.areAllItemsEnabled(); }
    public boolean isEnabled(int position) {
        int numHeaders = getHeadersCount();
        if (position < numHeaders) return mHeaderViewInfos.get(position).isSelectable;
        final int adjPosition = position - numHeaders;
        int adapterCount = mAdapter != null ? mAdapter.getCount() : 0;
        if (adjPosition < adapterCount) return mAdapter.isEnabled(adjPosition);
        return mFooterViewInfos.get(adjPosition - adapterCount).isSelectable;
    }
    public Object getItem(int position) {
        int numHeaders = getHeadersCount();
        if (position < numHeaders) return mHeaderViewInfos.get(position).data;
        final int adjPosition = position - numHeaders;
        int adapterCount = mAdapter != null ? mAdapter.getCount() : 0;
        if (adjPosition < adapterCount) return mAdapter.getItem(adjPosition);
        return mFooterViewInfos.get(adjPosition - adapterCount).data;
    }
    public long getItemId(int position) { int numHeaders = getHeadersCount(); if (mAdapter != null && position >= numHeaders) { int adj = position - numHeaders; if (adj < mAdapter.getCount()) return mAdapter.getItemId(adj); } return -1; }
    public boolean hasStableIds() { return mAdapter != null && mAdapter.hasStableIds(); }
    public View getView(int position, View convertView, ViewGroup parent) {
        int numHeaders = getHeadersCount();
        if (position < numHeaders) return mHeaderViewInfos.get(position).view;
        final int adjPosition = position - numHeaders;
        int adapterCount = mAdapter != null ? mAdapter.getCount() : 0;
        if (adjPosition < adapterCount) return mAdapter.getView(adjPosition, convertView, parent);
        return mFooterViewInfos.get(adjPosition - adapterCount).view;
    }
    public int getItemViewType(int position) { int numHeaders = getHeadersCount(); if (mAdapter != null && position >= numHeaders) { int adj = position - numHeaders; if (adj < mAdapter.getCount()) return mAdapter.getItemViewType(adj); } return AdapterView.ITEM_VIEW_TYPE_HEADER_OR_FOOTER; }
    public int getViewTypeCount() { return mAdapter != null ? mAdapter.getViewTypeCount() : 1; }
    public void registerDataSetObserver(DataSetObserver o) { if (mAdapter != null) mAdapter.registerDataSetObserver(o); }
    public void unregisterDataSetObserver(DataSetObserver o) { if (mAdapter != null) mAdapter.unregisterDataSetObserver(o); }
    public Filter getFilter() { return mIsFilterable ? ((Filterable) mAdapter).getFilter() : null; }
    public ListAdapter getWrappedAdapter() { return mAdapter; }
}
