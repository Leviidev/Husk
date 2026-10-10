package android.database;
public class CursorWrapper implements Cursor {
    protected final Cursor mCursor;
    public CursorWrapper(Cursor c) { mCursor = c; }
    public Cursor getWrappedCursor() { return mCursor; }
    public void close() { mCursor.close(); } public boolean isClosed() { return mCursor.isClosed(); } public int getCount() { return mCursor.getCount(); }
    public void deactivate() { mCursor.deactivate(); } public boolean moveToFirst() { return mCursor.moveToFirst(); } public int getColumnCount() { return mCursor.getColumnCount(); }
    public int getColumnIndex(String n) { return mCursor.getColumnIndex(n); } public int getColumnIndexOrThrow(String n) { return mCursor.getColumnIndexOrThrow(n); }
    public String getColumnName(int i) { return mCursor.getColumnName(i); } public String[] getColumnNames() { return mCursor.getColumnNames(); }
    public double getDouble(int i) { return mCursor.getDouble(i); } public void setExtras(android.os.Bundle e) { mCursor.setExtras(e); } public android.os.Bundle getExtras() { return mCursor.getExtras(); }
    public float getFloat(int i) { return mCursor.getFloat(i); } public int getInt(int i) { return mCursor.getInt(i); } public long getLong(int i) { return mCursor.getLong(i); }
    public short getShort(int i) { return mCursor.getShort(i); } public String getString(int i) { return mCursor.getString(i); } public void copyStringToBuffer(int i, CharArrayBuffer b) { mCursor.copyStringToBuffer(i, b); }
    public byte[] getBlob(int i) { return mCursor.getBlob(i); } public boolean getWantsAllOnMoveCalls() { return mCursor.getWantsAllOnMoveCalls(); }
    public boolean isAfterLast() { return mCursor.isAfterLast(); } public boolean isBeforeFirst() { return mCursor.isBeforeFirst(); } public boolean isFirst() { return mCursor.isFirst(); }
    public boolean isLast() { return mCursor.isLast(); } public int getType(int i) { return mCursor.getType(i); } public boolean isNull(int i) { return mCursor.isNull(i); }
    public boolean moveToLast() { return mCursor.moveToLast(); } public boolean move(int o) { return mCursor.move(o); } public boolean moveToPosition(int p) { return mCursor.moveToPosition(p); }
    public boolean moveToNext() { return mCursor.moveToNext(); } public int getPosition() { return mCursor.getPosition(); } public boolean moveToPrevious() { return mCursor.moveToPrevious(); }
    public void registerContentObserver(ContentObserver o) { mCursor.registerContentObserver(o); } public void registerDataSetObserver(DataSetObserver o) { mCursor.registerDataSetObserver(o); }
    public boolean requery() { return mCursor.requery(); } public android.os.Bundle respond(android.os.Bundle e) { return mCursor.respond(e); }
    public void setNotificationUri(android.content.ContentResolver r, android.net.Uri u) { mCursor.setNotificationUri(r, u); } public android.net.Uri getNotificationUri() { return mCursor.getNotificationUri(); }
    public void unregisterContentObserver(ContentObserver o) { mCursor.unregisterContentObserver(o); } public void unregisterDataSetObserver(DataSetObserver o) { mCursor.unregisterDataSetObserver(o); }
}
