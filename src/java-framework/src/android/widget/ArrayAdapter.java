package android.widget;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import java.util.*;

public class ArrayAdapter<T> extends BaseAdapter implements Filterable, ThemedSpinnerAdapter {
    private final Object mLock = new Object();
    private final LayoutInflater mInflater;
    private final Context mContext;
    private final int mResource;
    private int mDropDownResource, mFieldId;
    private List<T> mObjects;
    private boolean mObjectsFromResources, mNotifyOnChange = true;
    private ArrayList<T> mOriginalValues;
    private ArrayFilter mFilter;
    private LayoutInflater mDropDownInflater;
    public ArrayAdapter(Context c, int resource) { this(c, resource, 0, new ArrayList<>()); }
    public ArrayAdapter(Context c, int resource, int textViewResourceId) { this(c, resource, textViewResourceId, new ArrayList<>()); }
    public ArrayAdapter(Context c, int resource, T[] objects) { this(c, resource, 0, Arrays.asList(objects)); }
    public ArrayAdapter(Context c, int resource, int textViewResourceId, T[] objects) { this(c, resource, textViewResourceId, Arrays.asList(objects)); }
    public ArrayAdapter(Context c, int resource, List<T> objects) { this(c, resource, 0, objects); }
    public ArrayAdapter(Context c, int resource, int textViewResourceId, List<T> objects) { this(c, resource, textViewResourceId, objects, false); }
    private ArrayAdapter(Context c, int resource, int textViewResourceId, List<T> objects, boolean fromRes) {
        mContext = c; mInflater = LayoutInflater.from(c); mResource = mDropDownResource = resource; mObjects = objects; mObjectsFromResources = fromRes; mFieldId = textViewResourceId;
    }
    public void add(T o) { synchronized (mLock) { if (mOriginalValues != null) mOriginalValues.add(o); else mObjects.add(o); mObjectsFromResources = false; } if (mNotifyOnChange) notifyDataSetChanged(); }
    public void addAll(Collection<? extends T> c) { synchronized (mLock) { if (mOriginalValues != null) mOriginalValues.addAll(c); else mObjects.addAll(c); mObjectsFromResources = false; } if (mNotifyOnChange) notifyDataSetChanged(); }
    @SafeVarargs public final void addAll(T... items) { synchronized (mLock) { if (mOriginalValues != null) Collections.addAll(mOriginalValues, items); else Collections.addAll(mObjects, items); mObjectsFromResources = false; } if (mNotifyOnChange) notifyDataSetChanged(); }
    public void insert(T o, int index) { synchronized (mLock) { if (mOriginalValues != null) mOriginalValues.add(index, o); else mObjects.add(index, o); mObjectsFromResources = false; } if (mNotifyOnChange) notifyDataSetChanged(); }
    public void remove(T o) { synchronized (mLock) { if (mOriginalValues != null) mOriginalValues.remove(o); else mObjects.remove(o); mObjectsFromResources = false; } if (mNotifyOnChange) notifyDataSetChanged(); }
    public void clear() { synchronized (mLock) { if (mOriginalValues != null) mOriginalValues.clear(); else mObjects.clear(); mObjectsFromResources = false; } if (mNotifyOnChange) notifyDataSetChanged(); }
    public void sort(Comparator<? super T> c) { synchronized (mLock) { if (mOriginalValues != null) Collections.sort(mOriginalValues, c); else Collections.sort(mObjects, c); } if (mNotifyOnChange) notifyDataSetChanged(); }
    @Override public void notifyDataSetChanged() { super.notifyDataSetChanged(); mNotifyOnChange = true; }
    public void setNotifyOnChange(boolean n) { mNotifyOnChange = n; }
    public Context getContext() { return mContext; }
    public int getCount() { return mObjects.size(); }
    public T getItem(int position) { return mObjects.get(position); }
    public int getPosition(T item) { return mObjects.indexOf(item); }
    public long getItemId(int position) { return position; }
    public View getView(int position, View convertView, ViewGroup parent) { return createViewFromResource(mInflater, position, convertView, parent, mResource); }
    private View createViewFromResource(LayoutInflater inflater, int position, View convertView, ViewGroup parent, int resource) {
        final View view = convertView == null ? inflater.inflate(resource, parent, false) : convertView;
        final TextView text;
        try {
            if (mFieldId == 0) text = (TextView) view;
            else { text = view.findViewById(mFieldId); if (text == null) throw new RuntimeException("Failed to find view with ID " + mContext.getResources().getResourceName(mFieldId) + " in item layout"); }
        } catch (ClassCastException e) { throw new IllegalStateException("ArrayAdapter requires the resource ID to be a TextView", e); }
        final T item = getItem(position);
        if (item instanceof CharSequence) text.setText((CharSequence) item); else text.setText(String.valueOf(item));
        return view;
    }
    public void setDropDownViewResource(int r) { mDropDownResource = r; }
    public void setDropDownViewTheme(Resources.Theme t) { mDropDownInflater = t == null ? null : LayoutInflater.from(new android.view.ContextThemeWrapper(mContext, t)); }
    public Resources.Theme getDropDownViewTheme() { return mDropDownInflater == null ? null : mDropDownInflater.getContext().getTheme(); }
    @Override public View getDropDownView(int position, View convertView, ViewGroup parent) { return createViewFromResource(mDropDownInflater == null ? mInflater : mDropDownInflater, position, convertView, parent, mDropDownResource); }
    public static ArrayAdapter<CharSequence> createFromResource(Context c, int textArrayResId, int textViewResId) { CharSequence[] s = c.getResources().getTextArray(textArrayResId); return new ArrayAdapter<>(c, textViewResId, 0, new ArrayList<>(Arrays.asList(s)), true); }
    public Filter getFilter() { if (mFilter == null) mFilter = new ArrayFilter(); return mFilter; }
    @Override public CharSequence[] getAutofillOptions() { CharSequence[] e = super.getAutofillOptions(); if (e != null) return e; if (!mObjectsFromResources || mObjects == null || mObjects.isEmpty()) return null; CharSequence[] o = new CharSequence[mObjects.size()]; mObjects.toArray(o); return o; }
    private class ArrayFilter extends Filter {
        @Override protected FilterResults performFiltering(CharSequence prefix) {
            final FilterResults results = new FilterResults();
            if (mOriginalValues == null) synchronized (mLock) { mOriginalValues = new ArrayList<>(mObjects); }
            if (prefix == null || prefix.length() == 0) { final ArrayList<T> list; synchronized (mLock) { list = new ArrayList<>(mOriginalValues); } results.values = list; results.count = list.size(); }
            else {
                final String prefixString = prefix.toString().toLowerCase();
                final ArrayList<T> values; synchronized (mLock) { values = new ArrayList<>(mOriginalValues); }
                final ArrayList<T> newValues = new ArrayList<>();
                for (T value : values) {
                    final String valueText = String.valueOf(value).toLowerCase();
                    if (valueText.startsWith(prefixString)) newValues.add(value);
                    else for (String w : valueText.split(" ")) if (w.startsWith(prefixString)) { newValues.add(value); break; }
                }
                results.values = newValues; results.count = newValues.size();
            }
            return results;
        }
        @SuppressWarnings("unchecked") @Override protected void publishResults(CharSequence constraint, FilterResults results) {
            mObjects = (List<T>) results.values;
            if (results.count > 0) notifyDataSetChanged(); else notifyDataSetInvalidated();
        }
    }
}
