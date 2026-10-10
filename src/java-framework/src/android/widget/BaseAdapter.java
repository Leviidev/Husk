package android.widget;

import android.database.DataSetObservable;
import android.database.DataSetObserver;
import android.view.View;
import android.view.ViewGroup;

public abstract class BaseAdapter implements ListAdapter, SpinnerAdapter {
    private final DataSetObservable mDataSetObservable = new DataSetObservable();
    private CharSequence[] mAutofillOptions;
    public boolean hasStableIds() { return false; }
    public void registerDataSetObserver(DataSetObserver o) { mDataSetObservable.registerObserver(o); }
    public void unregisterDataSetObserver(DataSetObserver o) { mDataSetObservable.unregisterObserver(o); }
    public void notifyDataSetChanged() { mDataSetObservable.notifyChanged(); }
    public void notifyDataSetInvalidated() { mDataSetObservable.notifyInvalidated(); }
    public boolean areAllItemsEnabled() { return true; }
    public boolean isEnabled(int position) { return true; }
    public View getDropDownView(int position, View convertView, ViewGroup parent) { return getView(position, convertView, parent); }
    public int getItemViewType(int position) { return 0; }
    public int getViewTypeCount() { return 1; }
    public boolean isEmpty() { return getCount() == 0; }
    public CharSequence[] getAutofillOptions() { return mAutofillOptions; }
    public void setAutofillOptions(CharSequence... o) { mAutofillOptions = o; }
}
