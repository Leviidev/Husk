package android.widget;
public interface Adapter {
    int IGNORE_ITEM_VIEW_TYPE = -1, NO_SELECTION = Integer.MIN_VALUE;
    void registerDataSetObserver(android.database.DataSetObserver o);
    void unregisterDataSetObserver(android.database.DataSetObserver o);
    int getCount();
    Object getItem(int position);
    long getItemId(int position);
    boolean hasStableIds();
    android.view.View getView(int position, android.view.View convertView, android.view.ViewGroup parent);
    int getItemViewType(int position);
    int getViewTypeCount();
    boolean isEmpty();
    default CharSequence[] getAutofillOptions() { return null; }
}
