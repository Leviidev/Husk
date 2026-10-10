package android.database.sqlite;

import android.database.AbstractWindowedCursor;
import android.database.CursorWindow;
import java.util.HashMap;

/** The rows of a query, read into a window the first time they are asked for (and again on requery). */
public class SQLiteCursor extends AbstractWindowedCursor {
    private final String mEditTable;
    private final String[] mColumns;
    private final SQLiteQuery mQuery;
    private final SQLiteCursorDriver mDriver;
    private int mCount = -1;
    private HashMap<String, Integer> mColumnNameMap;
    @Deprecated public SQLiteCursor(SQLiteDatabase db, SQLiteCursorDriver driver, String editTable, SQLiteQuery query) { this(driver, editTable, query); }
    public SQLiteCursor(SQLiteCursorDriver driver, String editTable, SQLiteQuery query) {
        if (query == null) throw new IllegalArgumentException("query object cannot be null");
        mDriver = driver; mEditTable = editTable; mQuery = query; mColumns = query.getColumnNames();
    }
    public SQLiteDatabase getDatabase() { return mQuery.getDatabase(); }
    @Override public boolean onMove(int oldPosition, int newPosition) { if (mWindow == null) fillWindow(newPosition); return true; }
    @Override public int getCount() { if (mCount == -1) fillWindow(0); return mCount; }
    private void fillWindow(int requiredPos) { clearOrCreateWindow(getDatabase().getPath()); mCount = mQuery.fillWindow(mWindow, 0, requiredPos, true); }
    @Override public int getColumnIndex(String columnName) {
        if (mColumnNameMap == null) { HashMap<String, Integer> map = new HashMap<>(mColumns.length, 1); for (int i = 0; i < mColumns.length; i++) map.put(mColumns[i], i); mColumnNameMap = map; }
        int periodIndex = columnName.lastIndexOf('.');
        if (periodIndex != -1) columnName = columnName.substring(periodIndex + 1);
        Integer i = mColumnNameMap.get(columnName);
        if (i != null) return i;
        return super.getColumnIndex(columnName);
    }
    @Override public String[] getColumnNames() { return mColumns; }
    @Override public void deactivate() { super.deactivate(); mDriver.cursorDeactivated(); }
    @Override public void close() { super.close(); synchronized (this) { mQuery.close(); mDriver.cursorClosed(); } }
    @Override public boolean requery() {
        if (isClosed()) return false;
        synchronized (this) { if (!mQuery.getDatabase().isOpen()) return false; if (mWindow != null) mWindow.clear(); mPos = -1; mCount = -1; mDriver.cursorRequeried(this); }
        try { return super.requery(); } catch (IllegalStateException e) { android.util.Log.w("SQLiteCursor", "requery() failed " + e.getMessage(), e); return false; }
    }
    @Override public void setWindow(CursorWindow window) { super.setWindow(window); mCount = -1; }
    public void setSelectionArguments(String[] selectionArgs) { mDriver.setBindArguments(selectionArgs); }
    public void setFillWindowForwardOnly(boolean b) {}
    @Override public void fillWindow(int position, CursorWindow window) { mQuery.fillWindow(window, 0, position, true); }
}
