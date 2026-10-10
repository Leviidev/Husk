package android.database;

public abstract class AbstractWindowedCursor extends AbstractCursor {
    protected CursorWindow mWindow;
    public AbstractWindowedCursor() {}
    @Override public byte[] getBlob(int col) { checkPosition(); return mWindow.getBlob(mPos, col); }
    @Override public String getString(int col) { checkPosition(); return mWindow.getString(mPos, col); }
    @Override public void copyStringToBuffer(int col, CharArrayBuffer buffer) { checkPosition(); mWindow.copyStringToBuffer(mPos, col, buffer); }
    @Override public short getShort(int col) { checkPosition(); return mWindow.getShort(mPos, col); }
    @Override public int getInt(int col) { checkPosition(); return mWindow.getInt(mPos, col); }
    @Override public long getLong(int col) { checkPosition(); return mWindow.getLong(mPos, col); }
    @Override public float getFloat(int col) { checkPosition(); return mWindow.getFloat(mPos, col); }
    @Override public double getDouble(int col) { checkPosition(); return mWindow.getDouble(mPos, col); }
    @Override public boolean isNull(int col) { checkPosition(); return mWindow.getType(mPos, col) == Cursor.FIELD_TYPE_NULL; }
    @Deprecated public boolean isBlob(int col) { return getType(col) == Cursor.FIELD_TYPE_BLOB; }
    @Deprecated public boolean isString(int col) { return getType(col) == Cursor.FIELD_TYPE_STRING; }
    @Deprecated public boolean isLong(int col) { return getType(col) == Cursor.FIELD_TYPE_INTEGER; }
    @Deprecated public boolean isFloat(int col) { return getType(col) == Cursor.FIELD_TYPE_FLOAT; }
    @Override public int getType(int col) { checkPosition(); return mWindow.getType(mPos, col); }
    @Override protected void checkPosition() { super.checkPosition(); if (mWindow == null) throw new StaleDataException("Attempting to access a closed CursorWindow.Most probable cause: cursor is deactivated prior to calling this method."); }
    @Override public CursorWindow getWindow() { return mWindow; }
    public void setWindow(CursorWindow window) { if (window != mWindow) { closeWindow(); mWindow = window; } }
    public boolean hasWindow() { return mWindow != null; }
    protected void closeWindow() { if (mWindow != null) { mWindow.close(); mWindow = null; } }
    protected void clearOrCreateWindow(String name) { if (mWindow == null) mWindow = new CursorWindow(name); else mWindow.clear(); }
    @Override protected void onDeactivateOrClose() { super.onDeactivateOrClose(); closeWindow(); }
}
