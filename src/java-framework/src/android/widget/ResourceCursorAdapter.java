package android.widget;

import android.content.Context;
import android.database.Cursor;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

public abstract class ResourceCursorAdapter extends CursorAdapter {
    private int mLayout, mDropDownLayout;
    private final LayoutInflater mInflater;
    @Deprecated public ResourceCursorAdapter(Context c, int layout, Cursor cursor) { super(c, cursor); mLayout = mDropDownLayout = layout; mInflater = LayoutInflater.from(c); }
    public ResourceCursorAdapter(Context c, int layout, Cursor cursor, boolean autoRequery) { super(c, cursor, autoRequery); mLayout = mDropDownLayout = layout; mInflater = LayoutInflater.from(c); }
    public ResourceCursorAdapter(Context c, int layout, Cursor cursor, int flags) { super(c, cursor, flags); mLayout = mDropDownLayout = layout; mInflater = LayoutInflater.from(c); }
    @Override public View newView(Context c, Cursor cursor, ViewGroup parent) { return mInflater.inflate(mLayout, parent, false); }
    @Override public View newDropDownView(Context c, Cursor cursor, ViewGroup parent) { return mInflater.inflate(mDropDownLayout, parent, false); }
    public void setViewResource(int layout) { mLayout = layout; }
    public void setDropDownViewResource(int dropDownLayout) { mDropDownLayout = dropDownLayout; }
}
