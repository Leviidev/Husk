package android.database;

import java.util.ArrayList;

/** A block of rows (in memory here, not shared memory): what a windowed cursor reads its values from. */
public class CursorWindow extends android.database.sqlite.SQLiteClosable implements android.os.Parcelable {
    private final String mName;
    private int mStartPos, mNumColumns;
    private final ArrayList<Object[]> mRows = new ArrayList<>();
    public CursorWindow(String name) { this(name, 2 << 20); }
    public CursorWindow(String name, long windowSizeBytes) { mName = name != null && !name.isEmpty() ? name : "<unnamed>"; }
    @Deprecated public CursorWindow(boolean localWindow) { this((String) null); }
    public String getName() { return mName; }
    public void clear() { mStartPos = 0; mNumColumns = 0; mRows.clear(); }
    public int getStartPosition() { return mStartPos; }
    public void setStartPosition(int pos) { mStartPos = pos; }
    public int getNumRows() { return mRows.size(); }
    public boolean setNumColumns(int n) { mNumColumns = n; return true; }
    public boolean allocRow() { mRows.add(new Object[mNumColumns]); return true; }
    public void freeLastRow() { if (!mRows.isEmpty()) mRows.remove(mRows.size() - 1); }
    private Object[] row(int pos) { int r = pos - mStartPos; if (r < 0 || r >= mRows.size()) throw new IllegalStateException("Couldn't read row " + pos + " from CursorWindow"); return mRows.get(r); }
    private Object v(int pos, int col) { Object[] r = row(pos); if (col < 0 || col >= r.length) throw new IllegalStateException("Couldn't read row " + pos + ", col " + col + " from CursorWindow"); return r[col]; }
    private boolean put(Object o, int pos, int col) { int r = pos - mStartPos; if (r < 0 || r >= mRows.size() || col < 0 || col >= mNumColumns) return false; mRows.get(r)[col] = o; return true; }
    public int getType(int pos, int col) { Object o = v(pos, col); return o == null ? Cursor.FIELD_TYPE_NULL : o instanceof Long ? Cursor.FIELD_TYPE_INTEGER : o instanceof Double ? Cursor.FIELD_TYPE_FLOAT : o instanceof byte[] ? Cursor.FIELD_TYPE_BLOB : Cursor.FIELD_TYPE_STRING; }
    @Deprecated public boolean isNull(int pos, int col) { return v(pos, col) == null; }
    @Deprecated public boolean isBlob(int pos, int col) { int t = getType(pos, col); return t == Cursor.FIELD_TYPE_BLOB || t == Cursor.FIELD_TYPE_NULL; }
    @Deprecated public boolean isLong(int pos, int col) { return getType(pos, col) == Cursor.FIELD_TYPE_INTEGER; }
    @Deprecated public boolean isFloat(int pos, int col) { return getType(pos, col) == Cursor.FIELD_TYPE_FLOAT; }
    @Deprecated public boolean isString(int pos, int col) { int t = getType(pos, col); return t == Cursor.FIELD_TYPE_STRING || t == Cursor.FIELD_TYPE_NULL; }
    public byte[] getBlob(int pos, int col) {
        Object o = v(pos, col);
        if (o == null || o instanceof byte[]) return (byte[]) o;
        if (o instanceof String) return ((String) o).getBytes(java.nio.charset.StandardCharsets.UTF_8);
        throw new SQLiteExceptionHusk("INTEGER data in getBlob ");
    }
    public String getString(int pos, int col) {
        Object o = v(pos, col);
        if (o == null || o instanceof String) return (String) o;
        if (o instanceof byte[]) throw new SQLiteExceptionHusk("Unable to convert BLOB to string");
        if (o instanceof Double) { double d = (Double) o; return d == Math.rint(d) && Math.abs(d) < 1e15 ? Double.toString(d) : Double.toString(d); }
        return o.toString();
    }
    public void copyStringToBuffer(int pos, int col, CharArrayBuffer buffer) { String s = getString(pos, col); if (s == null) { buffer.sizeCopied = 0; return; } char[] d = buffer.data; if (d == null || d.length < s.length()) buffer.data = s.toCharArray(); else s.getChars(0, s.length(), d, 0); buffer.sizeCopied = s.length(); }
    public long getLong(int pos, int col) {
        Object o = v(pos, col);
        if (o == null) return 0;
        if (o instanceof Long) return (Long) o;
        if (o instanceof Double) return (long) (double) (Double) o;
        if (o instanceof String) { try { return Long.parseLong(((String) o).trim()); } catch (NumberFormatException e) { try { return (long) Double.parseDouble(((String) o).trim()); } catch (NumberFormatException e2) { return 0; } } }
        throw new SQLiteExceptionHusk("Unable to convert BLOB to long");
    }
    public double getDouble(int pos, int col) {
        Object o = v(pos, col);
        if (o == null) return 0;
        if (o instanceof Double) return (Double) o;
        if (o instanceof Long) return (Long) o;
        if (o instanceof String) { try { return Double.parseDouble(((String) o).trim()); } catch (NumberFormatException e) { return 0; } }
        throw new SQLiteExceptionHusk("Unable to convert BLOB to double");
    }
    public short getShort(int pos, int col) { return (short) getLong(pos, col); }
    public int getInt(int pos, int col) { return (int) getLong(pos, col); }
    public float getFloat(int pos, int col) { return (float) getDouble(pos, col); }
    public boolean putBlob(byte[] v, int pos, int col) { return put(v == null ? null : v.clone(), pos, col); }
    public boolean putString(String v, int pos, int col) { return put(v, pos, col); }
    public boolean putLong(long v, int pos, int col) { return put(v, pos, col); }
    public boolean putDouble(double v, int pos, int col) { return put(v, pos, col); }
    public boolean putNull(int pos, int col) { return put(null, pos, col); }
    /** Husk: one whole row at once. */
    public void huskAddRow(Object[] r) { mRows.add(r); if (mNumColumns < r.length) mNumColumns = r.length; }
    public static CursorWindow newFromParcel(android.os.Parcel p) { return new CursorWindow(p.readString()); }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) { p.writeString(mName); }
    @Override protected void onAllReferencesReleased() { mRows.clear(); }
    public static final Creator<CursorWindow> CREATOR = new Creator<CursorWindow>() { public CursorWindow createFromParcel(android.os.Parcel p) { return new CursorWindow(p.readString()); } public CursorWindow[] newArray(int n) { return new CursorWindow[n]; } };
    @Override public String toString() { return mName + " {" + mRows.size() + " rows}"; }
    static final class SQLiteExceptionHusk extends android.database.sqlite.SQLiteException { SQLiteExceptionHusk(String m) { super(m); } }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public long mWindowPtr;
    // ---- end of generated members
}
