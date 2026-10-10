package android.database;

import android.content.ContentValues;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteProgram;
import android.database.sqlite.SQLiteStatement;
import android.os.Parcel;
import android.text.TextUtils;
import java.io.PrintStream;
import java.util.Locale;

public class DatabaseUtils {
    public static final int STATEMENT_SELECT = 1, STATEMENT_UPDATE = 2, STATEMENT_ATTACH = 3, STATEMENT_BEGIN = 4, STATEMENT_COMMIT = 5, STATEMENT_ABORT = 6, STATEMENT_PRAGMA = 7, STATEMENT_DDL = 8, STATEMENT_UNPREPARED = 9, STATEMENT_OTHER = 99;
    public DatabaseUtils() {}
    public static final void writeExceptionToParcel(Parcel reply, Exception e) { reply.writeInt(1); reply.writeString(e.getMessage()); }
    public static final void readExceptionFromParcel(Parcel reply) { int code = reply.readInt(); if (code == 0) return; String msg = reply.readString(); throw new SQLException(msg); }
    public static void readExceptionWithFileNotFoundExceptionFromParcel(Parcel reply) throws java.io.FileNotFoundException { readExceptionFromParcel(reply); }
    public static void readExceptionWithOperationApplicationExceptionFromParcel(Parcel reply) throws android.content.OperationApplicationException { readExceptionFromParcel(reply); }
    public static void bindObjectToProgram(SQLiteProgram prog, int index, Object value) {
        if (value == null) prog.bindNull(index);
        else if (value instanceof Double || value instanceof Float) prog.bindDouble(index, ((Number) value).doubleValue());
        else if (value instanceof Number) prog.bindLong(index, ((Number) value).longValue());
        else if (value instanceof Boolean) prog.bindLong(index, (Boolean) value ? 1 : 0);
        else if (value instanceof byte[]) prog.bindBlob(index, (byte[]) value);
        else prog.bindString(index, value.toString());
    }
    public static int getTypeOfObject(Object obj) {
        if (obj == null) return Cursor.FIELD_TYPE_NULL;
        if (obj instanceof byte[]) return Cursor.FIELD_TYPE_BLOB;
        if (obj instanceof Float || obj instanceof Double) return Cursor.FIELD_TYPE_FLOAT;
        if (obj instanceof Long || obj instanceof Integer || obj instanceof Short || obj instanceof Byte) return Cursor.FIELD_TYPE_INTEGER;
        return Cursor.FIELD_TYPE_STRING;
    }
    public static void cursorFillWindow(final Cursor cursor, int position, final CursorWindow window) {
        if (position < 0 || position >= cursor.getCount()) return;
        final int oldPos = cursor.getPosition(), numColumns = cursor.getColumnCount();
        window.clear(); window.setStartPosition(position); window.setNumColumns(numColumns);
        if (cursor.moveToPosition(position)) {
            do {
                Object[] r = new Object[numColumns];
                for (int i = 0; i < numColumns; i++) {
                    switch (cursor.getType(i)) {
                    case Cursor.FIELD_TYPE_INTEGER: r[i] = cursor.getLong(i); break;
                    case Cursor.FIELD_TYPE_FLOAT: r[i] = cursor.getDouble(i); break;
                    case Cursor.FIELD_TYPE_BLOB: r[i] = cursor.getBlob(i); break;
                    case Cursor.FIELD_TYPE_NULL: r[i] = null; break;
                    default: r[i] = cursor.getString(i);
                    }
                }
                window.huskAddRow(r);
            } while (cursor.moveToNext());
        }
        cursor.moveToPosition(oldPos);
    }
    public static void appendEscapedSQLString(StringBuilder sb, String sqlString) {
        sb.append('\'');
        if (sqlString.indexOf('\'') != -1) { int length = sqlString.length(); for (int i = 0; i < length; i++) { char c = sqlString.charAt(i); if (c == '\'') sb.append('\''); sb.append(c); } }
        else sb.append(sqlString);
        sb.append('\'');
    }
    public static String sqlEscapeString(String value) { StringBuilder escaper = new StringBuilder(); appendEscapedSQLString(escaper, value); return escaper.toString(); }
    public static final void appendValueToSql(StringBuilder sql, Object value) {
        if (value == null) sql.append("NULL");
        else if (value instanceof Boolean) sql.append((Boolean) value ? '1' : '0');
        else appendEscapedSQLString(sql, value.toString());
    }
    public static String concatenateWhere(String a, String b) { if (TextUtils.isEmpty(a)) return b; if (TextUtils.isEmpty(b)) return a; return "(" + a + ") AND (" + b + ")"; }
    public static String getCollationKey(String name) { return name == null ? "" : name.toLowerCase(Locale.ROOT); }
    public static String getHexCollationKey(String name) { StringBuilder b = new StringBuilder(); for (char c : getCollationKey(name).toCharArray()) b.append(String.format("%04x", (int) c)); return b.toString(); }
    public static void dumpCursor(Cursor cursor) { dumpCursor(cursor, System.out); }
    public static void dumpCursor(Cursor cursor, PrintStream stream) {
        stream.println(">>>>> Dumping cursor " + cursor);
        if (cursor != null) { int startPos = cursor.getPosition(); cursor.moveToPosition(-1); while (cursor.moveToNext()) dumpCurrentRow(cursor, stream); cursor.moveToPosition(startPos); }
        stream.println("<<<<<");
    }
    public static void dumpCursor(Cursor cursor, StringBuilder sb) {
        sb.append(">>>>> Dumping cursor ").append(cursor).append('\n');
        if (cursor != null) { int startPos = cursor.getPosition(); cursor.moveToPosition(-1); while (cursor.moveToNext()) dumpCurrentRow(cursor, sb); cursor.moveToPosition(startPos); }
        sb.append("<<<<<\n");
    }
    public static String dumpCursorToString(Cursor cursor) { StringBuilder sb = new StringBuilder(); dumpCursor(cursor, sb); return sb.toString(); }
    public static void dumpCurrentRow(Cursor cursor) { dumpCurrentRow(cursor, System.out); }
    public static void dumpCurrentRow(Cursor cursor, PrintStream stream) { StringBuilder sb = new StringBuilder(); dumpCurrentRow(cursor, sb); stream.print(sb); }
    public static void dumpCurrentRow(Cursor cursor, StringBuilder sb) {
        String[] cols = cursor.getColumnNames();
        sb.append(cursor.getPosition()).append(" {\n");
        for (int i = 0; i < cols.length; i++) { String value; try { value = cursor.getString(i); } catch (RuntimeException e) { value = "<unprintable>"; } sb.append("   ").append(cols[i]).append('=').append(value).append('\n'); }
        sb.append("}\n");
    }
    public static String dumpCurrentRowToString(Cursor cursor) { StringBuilder sb = new StringBuilder(); dumpCurrentRow(cursor, sb); return sb.toString(); }
    public static void cursorStringToContentValues(Cursor cursor, String field, ContentValues values) { cursorStringToContentValues(cursor, field, values, field); }
    public static void cursorStringToContentValues(Cursor cursor, String field, ContentValues values, String key) { values.put(key, cursor.getString(cursor.getColumnIndexOrThrow(field))); }
    public static void cursorIntToContentValues(Cursor cursor, String field, ContentValues values) { cursorIntToContentValues(cursor, field, values, field); }
    public static void cursorIntToContentValues(Cursor cursor, String field, ContentValues values, String key) { int colIndex = cursor.getColumnIndex(field); if (!cursor.isNull(colIndex)) values.put(key, cursor.getInt(colIndex)); else values.put(key, (Integer) null); }
    public static void cursorLongToContentValues(Cursor cursor, String field, ContentValues values) { cursorLongToContentValues(cursor, field, values, field); }
    public static void cursorLongToContentValues(Cursor cursor, String field, ContentValues values, String key) { int colIndex = cursor.getColumnIndex(field); if (!cursor.isNull(colIndex)) values.put(key, Long.valueOf(cursor.getLong(colIndex))); else values.put(key, (Long) null); }
    public static void cursorDoubleToCursorValues(Cursor cursor, String field, ContentValues values) { cursorDoubleToContentValues(cursor, field, values, field); }
    public static void cursorDoubleToContentValues(Cursor cursor, String field, ContentValues values, String key) { int colIndex = cursor.getColumnIndex(field); if (!cursor.isNull(colIndex)) values.put(key, cursor.getDouble(colIndex)); else values.put(key, (Double) null); }
    public static void cursorRowToContentValues(Cursor cursor, ContentValues values) {
        String[] columns = cursor.getColumnNames();
        for (int i = 0; i < columns.length; i++) { if (cursor.getType(i) == Cursor.FIELD_TYPE_BLOB) values.put(columns[i], cursor.getBlob(i)); else values.put(columns[i], cursor.getString(i)); }
    }
    public static int findRowIdColumnIndex(String[] columnNames) { for (int i = 0; i < columnNames.length; i++) if (columnNames[i].equals("_id")) return i; return -1; }
    public static long queryNumEntries(SQLiteDatabase db, String table) { return queryNumEntries(db, table, null, null); }
    public static long queryNumEntries(SQLiteDatabase db, String table, String selection) { return queryNumEntries(db, table, selection, null); }
    public static long queryNumEntries(SQLiteDatabase db, String table, String selection, String[] selectionArgs) { String s = (!TextUtils.isEmpty(selection)) ? " where " + selection : ""; return longForQuery(db, "select count(*) from " + table + s, selectionArgs); }
    public static boolean queryIsEmpty(SQLiteDatabase db, String table) { return longForQuery(db, "select exists(select 1 from " + table + ")", null) == 0; }
    public static long longForQuery(SQLiteDatabase db, String query, String[] selectionArgs) { try (SQLiteStatement prog = db.compileStatement(query)) { return longForQuery(prog, selectionArgs); } }
    public static long longForQuery(SQLiteStatement prog, String[] selectionArgs) { prog.bindAllArgsAsStrings(selectionArgs); return prog.simpleQueryForLong(); }
    public static String stringForQuery(SQLiteDatabase db, String query, String[] selectionArgs) { try (SQLiteStatement prog = db.compileStatement(query)) { return stringForQuery(prog, selectionArgs); } }
    public static String stringForQuery(SQLiteStatement prog, String[] selectionArgs) { prog.bindAllArgsAsStrings(selectionArgs); return prog.simpleQueryForString(); }
    public static android.os.ParcelFileDescriptor blobFileDescriptorForQuery(SQLiteDatabase db, String query, String[] selectionArgs) { return null; }
    public static android.os.ParcelFileDescriptor blobFileDescriptorForQuery(SQLiteStatement prog, String[] selectionArgs) { return null; }
    public static void cursorStringToInsertHelper(Cursor c, String field, Object h, int i) {}
    public static void createDbFromSqlStatements(android.content.Context context, String dbName, int dbVersion, String sqlStatements) {
        SQLiteDatabase db = context.openOrCreateDatabase(dbName, 0, null);
        for (String statement : TextUtils.split(sqlStatements, ";\n")) { if (TextUtils.isEmpty(statement)) continue; db.execSQL(statement); }
        db.setVersion(dbVersion);
        db.close();
    }
    public static int getSqlStatementType(String sql) {
        sql = sql.trim();
        if (sql.length() < 3) return STATEMENT_OTHER;
        String prefixSql = sql.substring(0, 3).toUpperCase(Locale.ROOT);
        switch (prefixSql) {
        case "SEL": return STATEMENT_SELECT;
        case "INS": case "UPD": case "REP": case "DEL": return STATEMENT_UPDATE;
        case "ATT": return STATEMENT_ATTACH;
        case "COM": case "END": return STATEMENT_COMMIT;
        case "ROL": return sql.toUpperCase(Locale.ROOT).contains(" TO ") ? STATEMENT_OTHER : STATEMENT_ABORT;
        case "BEG": return STATEMENT_BEGIN;
        case "PRA": return STATEMENT_PRAGMA;
        case "CRE": case "DRO": case "ALT": return STATEMENT_DDL;
        case "ANA": case "DET": return STATEMENT_UNPREPARED;
        default: return STATEMENT_OTHER;
        }
    }
    public static String[] appendSelectionArgs(String[] originalValues, String[] newValues) {
        if (originalValues == null || originalValues.length == 0) return newValues;
        String[] result = new String[originalValues.length + newValues.length];
        System.arraycopy(originalValues, 0, result, 0, originalValues.length);
        System.arraycopy(newValues, 0, result, originalValues.length, newValues.length);
        return result;
    }
}
