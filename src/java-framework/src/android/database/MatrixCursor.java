package android.database;

import java.util.ArrayList;

public class MatrixCursor extends AbstractCursor {
    private final String[] mColumns;
    private final ArrayList<Object[]> mRows = new ArrayList<>();
    public MatrixCursor(String[] columns) { mColumns = columns; }
    public MatrixCursor(String[] columns, int initialCapacity) { this(columns); }
    public RowBuilder newRow() { Object[] r = new Object[mColumns.length]; mRows.add(r); return new RowBuilder(r); }
    public void addRow(Object[] values) { if (values.length != mColumns.length) throw new IllegalArgumentException("columnNames.length = " + mColumns.length + ", columnValues.length = " + values.length); mRows.add(values.clone()); }
    public void addRow(Iterable<?> values) { ArrayList<Object> l = new ArrayList<>(); for (Object o : values) l.add(o); addRow(l.toArray()); }
    public class RowBuilder {
        private final Object[] mRow; private int mIndex;
        RowBuilder(Object[] r) { mRow = r; }
        public RowBuilder add(Object v) { if (mIndex == mRow.length) throw new CursorIndexOutOfBoundsException("No more columns left."); mRow[mIndex++] = v; return this; }
        public RowBuilder add(String col, Object v) { for (int i = 0; i < mColumns.length; i++) if (col.equals(mColumns[i])) mRow[i] = v; return this; }
    }
    private Object get(int c) {
        if (c < 0 || c >= mColumns.length) throw new CursorIndexOutOfBoundsException("Requested column: " + c + ", # of columns: " + mColumns.length);
        if (mPos < 0) throw new CursorIndexOutOfBoundsException("Before first row.");
        if (mPos >= mRows.size()) throw new CursorIndexOutOfBoundsException("After last row.");
        return mRows.get(mPos)[c];
    }
    public int getCount() { return mRows.size(); }
    public String[] getColumnNames() { return mColumns; }
    public String getString(int c) { Object v = get(c); return v == null ? null : v.toString(); }
    public short getShort(int c) { Object v = get(c); return v == null ? 0 : v instanceof Number ? ((Number) v).shortValue() : Short.parseShort(v.toString()); }
    public int getInt(int c) { Object v = get(c); return v == null ? 0 : v instanceof Number ? ((Number) v).intValue() : Integer.parseInt(v.toString()); }
    public long getLong(int c) { Object v = get(c); return v == null ? 0 : v instanceof Number ? ((Number) v).longValue() : Long.parseLong(v.toString()); }
    public float getFloat(int c) { Object v = get(c); return v == null ? 0 : v instanceof Number ? ((Number) v).floatValue() : Float.parseFloat(v.toString()); }
    public double getDouble(int c) { Object v = get(c); return v == null ? 0 : v instanceof Number ? ((Number) v).doubleValue() : Double.parseDouble(v.toString()); }
    public byte[] getBlob(int c) { return (byte[]) get(c); }
    public int getType(int c) { Object v = get(c); return v == null ? FIELD_TYPE_NULL : v instanceof byte[] ? FIELD_TYPE_BLOB : v instanceof Float || v instanceof Double ? FIELD_TYPE_FLOAT : v instanceof Number ? FIELD_TYPE_INTEGER : FIELD_TYPE_STRING; }
    public boolean isNull(int c) { return get(c) == null; }
}
