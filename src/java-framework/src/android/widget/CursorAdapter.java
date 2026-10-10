package android.widget;

import android.content.Context;
import android.database.ContentObserver;
import android.database.Cursor;
import android.database.DataSetObserver;
import android.view.View;
import android.view.ViewGroup;

public abstract class CursorAdapter extends BaseAdapter implements Filterable, ThemedSpinnerAdapter {
    public static final int FLAG_AUTO_REQUERY = 1, FLAG_REGISTER_CONTENT_OBSERVER = 2;
    protected boolean mDataValid, mAutoRequery;
    protected Cursor mCursor;
    protected Context mContext;
    protected int mRowIDColumn;
    protected ChangeObserver mChangeObserver;
    protected DataSetObserver mDataSetObserver;
    @Deprecated public CursorAdapter(Context c, Cursor cursor) { init(c, cursor, FLAG_AUTO_REQUERY); }
    public CursorAdapter(Context c, Cursor cursor, boolean autoRequery) { init(c, cursor, autoRequery ? FLAG_AUTO_REQUERY : FLAG_REGISTER_CONTENT_OBSERVER); }
    public CursorAdapter(Context c, Cursor cursor, int flags) { init(c, cursor, flags); }
    @Deprecated protected void init(Context c, Cursor cursor, boolean autoRequery) { init(c, cursor, autoRequery ? FLAG_AUTO_REQUERY : FLAG_REGISTER_CONTENT_OBSERVER); }
    void init(Context c, Cursor cursor, int flags) {
        if ((flags & FLAG_AUTO_REQUERY) == FLAG_AUTO_REQUERY) { flags |= FLAG_REGISTER_CONTENT_OBSERVER; mAutoRequery = true; } else mAutoRequery = false;
        boolean cursorPresent = cursor != null;
        mCursor = cursor; mDataValid = cursorPresent; mContext = c;
        mRowIDColumn = cursorPresent ? cursor.getColumnIndexOrThrow("_id") : -1;
        if ((flags & FLAG_REGISTER_CONTENT_OBSERVER) == FLAG_REGISTER_CONTENT_OBSERVER) { mChangeObserver = new ChangeObserver(); mDataSetObserver = new MyDataSetObserver(); } else { mChangeObserver = null; mDataSetObserver = null; }
        if (cursorPresent) { if (mChangeObserver != null) cursor.registerContentObserver(mChangeObserver); if (mDataSetObserver != null) cursor.registerDataSetObserver(mDataSetObserver); }
    }
    public void setDropDownViewTheme(android.content.res.Resources.Theme t) {}
    public android.content.res.Resources.Theme getDropDownViewTheme() { return null; }
    public Cursor getCursor() { return mCursor; }
    public int getCount() { return mDataValid && mCursor != null ? mCursor.getCount() : 0; }
    public Object getItem(int position) { if (mDataValid && mCursor != null) { mCursor.moveToPosition(position); return mCursor; } return null; }
    public long getItemId(int position) { if (mDataValid && mCursor != null && mCursor.moveToPosition(position)) return mCursor.getLong(mRowIDColumn); return 0; }
    @Override public boolean hasStableIds() { return true; }
    public View getView(int position, View convertView, ViewGroup parent) {
        if (!mDataValid) throw new IllegalStateException("this should only be called when the cursor is valid");
        if (!mCursor.moveToPosition(position)) throw new IllegalStateException("couldn't move cursor to position " + position);
        View v = convertView == null ? newView(mContext, mCursor, parent) : convertView;
        bindView(v, mContext, mCursor);
        return v;
    }
    @Override public View getDropDownView(int position, View convertView, ViewGroup parent) {
        if (mDataValid) { mCursor.moveToPosition(position); View v = convertView == null ? newDropDownView(mContext, mCursor, parent) : convertView; bindView(v, mContext, mCursor); return v; }
        return null;
    }
    public abstract View newView(Context c, Cursor cursor, ViewGroup parent);
    public View newDropDownView(Context c, Cursor cursor, ViewGroup parent) { return newView(c, cursor, parent); }
    public abstract void bindView(View v, Context c, Cursor cursor);
    public void changeCursor(Cursor cursor) { Cursor old = swapCursor(cursor); if (old != null) old.close(); }
    public Cursor swapCursor(Cursor newCursor) {
        if (newCursor == mCursor) return null;
        Cursor oldCursor = mCursor;
        if (oldCursor != null) { if (mChangeObserver != null) oldCursor.unregisterContentObserver(mChangeObserver); if (mDataSetObserver != null) oldCursor.unregisterDataSetObserver(mDataSetObserver); }
        mCursor = newCursor;
        if (newCursor != null) { if (mChangeObserver != null) newCursor.registerContentObserver(mChangeObserver); if (mDataSetObserver != null) newCursor.registerDataSetObserver(mDataSetObserver); mRowIDColumn = newCursor.getColumnIndexOrThrow("_id"); mDataValid = true; notifyDataSetChanged(); }
        else { mRowIDColumn = -1; mDataValid = false; notifyDataSetInvalidated(); }
        return oldCursor;
    }
    public CharSequence convertToString(Cursor c) { return c == null ? "" : c.toString(); }
    public Cursor runQueryOnBackgroundThread(CharSequence constraint) { return mCursor; }
    public Filter getFilter() { return new Filter() { protected FilterResults performFiltering(CharSequence c) { Cursor cur = runQueryOnBackgroundThread(c); FilterResults r = new FilterResults(); if (cur != null) { r.count = cur.getCount(); r.values = cur; } return r; } protected void publishResults(CharSequence c, FilterResults r) { Cursor old = mCursor; if (r.values != null && r.values != old) changeCursor((Cursor) r.values); } @Override public CharSequence convertResultToString(Object o) { return convertToString((Cursor) o); } }; }
    public FilterQueryProvider getFilterQueryProvider() { return null; }
    public void setFilterQueryProvider(FilterQueryProvider p) {}
    protected void onContentChanged() { if (mAutoRequery && mCursor != null && !mCursor.isClosed()) mDataValid = mCursor.requery(); }
    private class ChangeObserver extends ContentObserver { ChangeObserver() { super(new android.os.Handler(android.os.Looper.getMainLooper())); } @Override public boolean deliverSelfNotifications() { return true; } @Override public void onChange(boolean selfChange) { onContentChanged(); } }
    private class MyDataSetObserver extends DataSetObserver { @Override public void onChanged() { mDataValid = true; notifyDataSetChanged(); } @Override public void onInvalidated() { mDataValid = false; notifyDataSetInvalidated(); } }
}
