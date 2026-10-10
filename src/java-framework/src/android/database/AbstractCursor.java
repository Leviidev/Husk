package android.database;

public abstract class AbstractCursor implements CrossProcessCursor {
    protected int mPos = -1;
    protected boolean mClosed;
    protected android.content.ContentResolver mContentResolver;
    private final DataSetObservable mDataSetObservable = new DataSetObservable();
    private final ContentObservable mContentObservable = new ContentObservable();
    private android.os.Bundle mExtras = android.os.Bundle.EMPTY;
    private android.net.Uri mNotifyUri;
    public abstract int getCount();
    public abstract String[] getColumnNames();
    public abstract String getString(int i);
    public abstract short getShort(int i);
    public abstract int getInt(int i);
    public abstract long getLong(int i);
    public abstract float getFloat(int i);
    public abstract double getDouble(int i);
    public abstract boolean isNull(int i);
    public int getType(int i) { return isNull(i) ? FIELD_TYPE_NULL : FIELD_TYPE_STRING; }
    public byte[] getBlob(int i) { throw new UnsupportedOperationException("getBlob is not supported"); }
    public CursorWindow getWindow() { return null; }
    public int getColumnCount() { return getColumnNames().length; }
    public void deactivate() { mDataSetObservable.notifyInvalidated(); }
    public boolean requery() { mDataSetObservable.notifyChanged(); return true; }
    public boolean isClosed() { return mClosed; }
    public void close() { mClosed = true; mContentObservable.unregisterAll(); mDataSetObservable.notifyInvalidated(); }
    public boolean onMove(int oldPos, int newPos) { return true; }
    public void copyStringToBuffer(int i, CharArrayBuffer b) { String s = getString(i); if (s != null) { char[] d = b.data; if (d == null || d.length < s.length()) b.data = s.toCharArray(); else s.getChars(0, s.length(), d, 0); b.sizeCopied = s.length(); } else b.sizeCopied = 0; }
    public final int getPosition() { return mPos; }
    public final boolean moveToPosition(int p) {
        int count = getCount();
        if (p >= count) { mPos = count; return false; }
        if (p < 0) { mPos = -1; return false; }
        if (p == mPos) return true;
        boolean r = onMove(mPos, p);
        mPos = r ? p : -1;
        return r;
    }
    public void fillWindow(int p, CursorWindow w) {}
    public final boolean move(int o) { return moveToPosition(mPos + o); }
    public final boolean moveToFirst() { return moveToPosition(0); }
    public final boolean moveToLast() { return moveToPosition(getCount() - 1); }
    public final boolean moveToNext() { return moveToPosition(mPos + 1); }
    public final boolean moveToPrevious() { return moveToPosition(mPos - 1); }
    public final boolean isFirst() { return mPos == 0 && getCount() != 0; }
    public final boolean isLast() { int c = getCount(); return mPos == c - 1 && c != 0; }
    public final boolean isBeforeFirst() { return getCount() == 0 || mPos == -1; }
    public final boolean isAfterLast() { return getCount() == 0 || mPos == getCount(); }
    public int getColumnIndex(String name) {
        int dot = name.lastIndexOf('.');
        if (dot != -1) name = name.substring(dot + 1);
        String[] cols = getColumnNames();
        for (int i = 0; i < cols.length; i++) if (cols[i].equalsIgnoreCase(name)) return i;
        return -1;
    }
    public int getColumnIndexOrThrow(String name) { int i = getColumnIndex(name); if (i < 0) throw new IllegalArgumentException("column '" + name + "' does not exist. Available columns: " + java.util.Arrays.toString(getColumnNames())); return i; }
    public String getColumnName(int i) { return getColumnNames()[i]; }
    public void registerContentObserver(ContentObserver o) { mContentObservable.registerObserver(o); }
    public void unregisterContentObserver(ContentObserver o) { if (!mClosed) mContentObservable.unregisterObserver(o); }
    public void registerDataSetObserver(DataSetObserver o) { mDataSetObservable.registerObserver(o); }
    public void unregisterDataSetObserver(DataSetObserver o) { mDataSetObservable.unregisterObserver(o); }
    protected void onChange(boolean self) { mContentObservable.dispatchChange(self, null); }
    public void setNotificationUri(android.content.ContentResolver r, android.net.Uri u) { mContentResolver = r; mNotifyUri = u; }
    public android.net.Uri getNotificationUri() { return mNotifyUri; }
    public boolean getWantsAllOnMoveCalls() { return false; }
    public void setExtras(android.os.Bundle e) { mExtras = e == null ? android.os.Bundle.EMPTY : e; }
    public android.os.Bundle getExtras() { return mExtras; }
    public android.os.Bundle respond(android.os.Bundle e) { return android.os.Bundle.EMPTY; }
    protected void checkPosition() { if (-1 == mPos || getCount() == mPos) throw new CursorIndexOutOfBoundsException(mPos, getCount()); }
}
