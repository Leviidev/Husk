package android.database;

/** Several cursors, one after another. */
public class MergeCursor extends AbstractCursor {
    private final Cursor[] mCursors;
    private Cursor mCursor;
    private final DataSetObserver mObserver = new DataSetObserver() { @Override public void onChanged() { mPos = -1; } @Override public void onInvalidated() { mPos = -1; } };
    public MergeCursor(Cursor[] cursors) {
        mCursors = cursors; mCursor = cursors.length > 0 ? cursors[0] : null;
        for (Cursor c : mCursors) if (c != null) c.registerDataSetObserver(mObserver);
    }
    @Override public int getCount() { int count = 0; for (Cursor c : mCursors) if (c != null) count += c.getCount(); return count; }
    @Override public boolean onMove(int oldPosition, int newPosition) {
        mCursor = null;
        int cursorStartPos = 0;
        for (Cursor c : mCursors) {
            if (c == null) continue;
            if (newPosition < (cursorStartPos + c.getCount())) { mCursor = c; break; }
            cursorStartPos += c.getCount();
        }
        return mCursor != null && mCursor.moveToPosition(newPosition - cursorStartPos);
    }
    @Override public String getString(int column) { return mCursor.getString(column); }
    @Override public short getShort(int column) { return mCursor.getShort(column); }
    @Override public int getInt(int column) { return mCursor.getInt(column); }
    @Override public long getLong(int column) { return mCursor.getLong(column); }
    @Override public float getFloat(int column) { return mCursor.getFloat(column); }
    @Override public double getDouble(int column) { return mCursor.getDouble(column); }
    @Override public int getType(int column) { return mCursor.getType(column); }
    @Override public boolean isNull(int column) { return mCursor.isNull(column); }
    @Override public byte[] getBlob(int column) { return mCursor.getBlob(column); }
    @Override public String[] getColumnNames() { if (mCursor != null) return mCursor.getColumnNames(); for (Cursor c : mCursors) if (c != null) return c.getColumnNames(); return new String[0]; }
    @Override public void deactivate() { for (Cursor c : mCursors) if (c != null) c.deactivate(); super.deactivate(); }
    @Override public void close() { for (Cursor c : mCursors) if (c != null) c.close(); super.close(); }
    @Override public boolean requery() { for (Cursor c : mCursors) if (c != null && !c.requery()) return false; return true; }
}
