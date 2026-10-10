package android.widget;

import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.view.View;

public class SimpleCursorAdapter extends ResourceCursorAdapter {
    public interface ViewBinder { boolean setViewValue(View view, Cursor cursor, int columnIndex); }
    public interface CursorToStringConverter { CharSequence convertToString(Cursor cursor); }
    protected int[] mFrom;
    protected int[] mTo;
    private String[] mOriginalFrom;
    private ViewBinder mViewBinder;
    private CursorToStringConverter mCursorToStringConverter;
    private int mStringConversionColumn = -1;
    @Deprecated public SimpleCursorAdapter(Context c, int layout, Cursor cursor, String[] from, int[] to) { super(c, layout, cursor); mTo = to; mOriginalFrom = from; findColumns(cursor, from); }
    public SimpleCursorAdapter(Context c, int layout, Cursor cursor, String[] from, int[] to, int flags) { super(c, layout, cursor, flags); mTo = to; mOriginalFrom = from; findColumns(cursor, from); }
    @Override public void bindView(View view, Context c, Cursor cursor) {
        final int count = mTo.length;
        for (int i = 0; i < count; i++) {
            final View v = view.findViewById(mTo[i]);
            if (v == null) continue;
            boolean bound = mViewBinder != null && mViewBinder.setViewValue(v, cursor, mFrom[i]);
            if (bound) continue;
            String text = cursor.getString(mFrom[i]);
            if (text == null) text = "";
            if (v instanceof TextView) setViewText((TextView) v, text);
            else if (v instanceof ImageView) setViewImage((ImageView) v, text);
            else throw new IllegalStateException(v.getClass().getName() + " is not a  view that can be bounds by this SimpleCursorAdapter");
        }
    }
    public ViewBinder getViewBinder() { return mViewBinder; }
    public void setViewBinder(ViewBinder b) { mViewBinder = b; }
    public void setViewImage(ImageView v, String value) { try { v.setImageResource(Integer.parseInt(value)); } catch (NumberFormatException e) { v.setImageURI(Uri.parse(value)); } }
    public void setViewText(TextView v, String text) { v.setText(text); }
    public int getStringConversionColumn() { return mStringConversionColumn; }
    public void setStringConversionColumn(int c) { mStringConversionColumn = c; }
    public CursorToStringConverter getCursorToStringConverter() { return mCursorToStringConverter; }
    public void setCursorToStringConverter(CursorToStringConverter c) { mCursorToStringConverter = c; }
    @Override public CharSequence convertToString(Cursor cursor) { if (mCursorToStringConverter != null) return mCursorToStringConverter.convertToString(cursor); else if (mStringConversionColumn > -1) return cursor.getString(mStringConversionColumn); return super.convertToString(cursor); }
    private void findColumns(Cursor c, String[] from) { if (c != null) { int count = from.length; if (mFrom == null || mFrom.length != count) mFrom = new int[count]; for (int i = 0; i < count; i++) mFrom[i] = c.getColumnIndexOrThrow(from[i]); } else mFrom = null; }
    @Override public Cursor swapCursor(Cursor c) { findColumns(c, mOriginalFrom); return super.swapCursor(c); }
    public void changeCursorAndColumns(Cursor c, String[] from, int[] to) { mOriginalFrom = from; mTo = to; findColumns(c, mOriginalFrom); super.changeCursor(c); }
}
