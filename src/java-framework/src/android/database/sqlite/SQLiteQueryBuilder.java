package android.database.sqlite;

import android.database.Cursor;
import android.os.CancellationSignal;
import android.text.TextUtils;
import java.util.Map;
import java.util.Set;
import java.util.regex.Pattern;

public class SQLiteQueryBuilder {
    private static final Pattern sLimitPattern = Pattern.compile("\\s*\\d+\\s*(,\\s*\\d+\\s*)?");
    private Map<String, String> mProjectionMap;
    private java.util.Collection<Pattern> mProjectionGreylist;
    private String mTables = "";
    private StringBuilder mWhereClause;
    private boolean mDistinct, mStrict, mStrictColumns, mStrictGrammar;
    private SQLiteDatabase.CursorFactory mFactory;
    public SQLiteQueryBuilder() {}
    public void setDistinct(boolean d) { mDistinct = d; }
    public boolean isDistinct() { return mDistinct; }
    public String getTables() { return mTables; }
    public void setTables(String t) { mTables = t; }
    public void appendWhere(CharSequence inWhere) { if (mWhereClause == null) mWhereClause = new StringBuilder(inWhere.length() + 16); mWhereClause.append(inWhere); }
    public void appendWhereEscapeString(String inWhere) { if (mWhereClause == null) mWhereClause = new StringBuilder(inWhere.length() + 16); android.database.DatabaseUtils.appendEscapedSQLString(mWhereClause, inWhere); }
    public void appendWhereStandalone(CharSequence inWhere) { if (mWhereClause == null) mWhereClause = new StringBuilder(inWhere.length() + 16); if (mWhereClause.length() > 0) mWhereClause.append(" AND "); mWhereClause.append('(').append(inWhere).append(')'); }
    public void setProjectionMap(Map<String, String> m) { mProjectionMap = m; }
    public Map<String, String> getProjectionMap() { return mProjectionMap; }
    public void setProjectionGreylist(java.util.Collection<Pattern> g) { mProjectionGreylist = g; }
    public void setCursorFactory(SQLiteDatabase.CursorFactory f) { mFactory = f; }
    public SQLiteDatabase.CursorFactory getCursorFactory() { return mFactory; }
    public void setStrict(boolean s) { mStrict = s; }
    public boolean isStrict() { return mStrict; }
    public void setStrictColumns(boolean s) { mStrictColumns = s; }
    public boolean isStrictColumns() { return mStrictColumns; }
    public void setStrictGrammar(boolean s) { mStrictGrammar = s; }
    public boolean isStrictGrammar() { return mStrictGrammar; }
    public static String buildQueryString(boolean distinct, String tables, String[] columns, String where, String groupBy, String having, String orderBy, String limit) {
        if (TextUtils.isEmpty(groupBy) && !TextUtils.isEmpty(having)) throw new IllegalArgumentException("HAVING clauses are only permitted when using a groupBy clause");
        if (!TextUtils.isEmpty(limit) && !sLimitPattern.matcher(limit).matches()) throw new IllegalArgumentException("invalid LIMIT clauses:" + limit);
        StringBuilder query = new StringBuilder(120);
        query.append("SELECT ");
        if (distinct) query.append("DISTINCT ");
        if (columns != null && columns.length != 0) appendColumns(query, columns); else query.append("* ");
        query.append("FROM ").append(tables);
        appendClause(query, " WHERE ", where);
        appendClause(query, " GROUP BY ", groupBy);
        appendClause(query, " HAVING ", having);
        appendClause(query, " ORDER BY ", orderBy);
        appendClause(query, " LIMIT ", limit);
        return query.toString();
    }
    private static void appendClause(StringBuilder s, String name, String clause) { if (!TextUtils.isEmpty(clause)) { s.append(name); s.append(clause); } }
    public static void appendColumns(StringBuilder s, String[] columns) {
        int n = columns.length;
        for (int i = 0; i < n; i++) { String column = columns[i]; if (column != null) { if (i > 0) s.append(", "); s.append(column); } }
        s.append(' ');
    }
    public Cursor query(SQLiteDatabase db, String[] projectionIn, String selection, String[] selectionArgs, String groupBy, String having, String sortOrder) { return query(db, projectionIn, selection, selectionArgs, groupBy, having, sortOrder, null, null); }
    public Cursor query(SQLiteDatabase db, String[] projectionIn, String selection, String[] selectionArgs, String groupBy, String having, String sortOrder, String limit) { return query(db, projectionIn, selection, selectionArgs, groupBy, having, sortOrder, limit, null); }
    public Cursor query(SQLiteDatabase db, String[] projectionIn, String selection, String[] selectionArgs, String groupBy, String having, String sortOrder, String limit, CancellationSignal cs) {
        if (mTables == null) return null;
        String sql = buildQuery(projectionIn, selection, groupBy, having, sortOrder, limit);
        return db.rawQueryWithFactory(mFactory, sql, selectionArgs, SQLiteDatabase.findEditTable(mTables), cs);
    }
    public long insert(SQLiteDatabase db, android.content.ContentValues values) { return db.insert(mTables, null, values); }
    public int update(SQLiteDatabase db, android.content.ContentValues values, String selection, String[] selectionArgs) { return db.update(mTables, values, computeWhere(selection), selectionArgs); }
    public int delete(SQLiteDatabase db, String selection, String[] selectionArgs) { return db.delete(mTables, computeWhere(selection), selectionArgs); }
    private String computeWhere(String selection) {
        boolean hasInternal = mWhereClause != null && mWhereClause.length() > 0, hasExternal = !TextUtils.isEmpty(selection);
        if (hasInternal || hasExternal) {
            StringBuilder where = new StringBuilder();
            if (hasInternal) where.append('(').append(mWhereClause).append(')');
            if (hasInternal && hasExternal) where.append(" AND ");
            if (hasExternal) where.append('(').append(selection).append(')');
            return where.toString();
        }
        return null;
    }
    public String buildQuery(String[] projectionIn, String selection, String groupBy, String having, String sortOrder, String limit) {
        String[] projection = computeProjection(projectionIn);
        return buildQueryString(mDistinct, mTables, projection, computeWhere(selection), groupBy, having, sortOrder, limit);
    }
    @Deprecated public String buildQuery(String[] projectionIn, String selection, String[] selectionArgs, String groupBy, String having, String sortOrder, String limit) { return buildQuery(projectionIn, selection, groupBy, having, sortOrder, limit); }
    public String buildUnionSubQuery(String typeDiscriminatorColumn, String[] unionColumns, Set<String> columnsPresentInTable, int computedColumnsOffset, String typeDiscriminatorValue, String selection, String groupBy, String having) {
        int unionColumnsCount = unionColumns.length;
        String[] projectionIn = new String[unionColumnsCount];
        for (int i = 0; i < unionColumnsCount; i++) {
            String unionColumn = unionColumns[i];
            if (unionColumn.equals(typeDiscriminatorColumn)) projectionIn[i] = "'" + typeDiscriminatorValue + "' AS " + typeDiscriminatorColumn;
            else if (i <= computedColumnsOffset || columnsPresentInTable.contains(unionColumn)) projectionIn[i] = unionColumn;
            else projectionIn[i] = "NULL AS " + unionColumn;
        }
        return buildQuery(projectionIn, selection, groupBy, having, null, null);
    }
    @Deprecated public String buildUnionSubQuery(String typeDiscriminatorColumn, String[] unionColumns, Set<String> columnsPresentInTable, int computedColumnsOffset, String typeDiscriminatorValue, String selection, String[] selectionArgs, String groupBy, String having) { return buildUnionSubQuery(typeDiscriminatorColumn, unionColumns, columnsPresentInTable, computedColumnsOffset, typeDiscriminatorValue, selection, groupBy, having); }
    public String buildUnionQuery(String[] subQueries, String sortOrder, String limit) {
        StringBuilder query = new StringBuilder(128);
        int subQueryCount = subQueries.length;
        String unionOperator = mDistinct ? " UNION " : " UNION ALL ";
        for (int i = 0; i < subQueryCount; i++) { if (i > 0) query.append(unionOperator); query.append(subQueries[i]); }
        appendClause(query, " ORDER BY ", sortOrder);
        appendClause(query, " LIMIT ", limit);
        return query.toString();
    }
    private String[] computeProjection(String[] projectionIn) {
        if (projectionIn != null && projectionIn.length > 0) {
            if (mProjectionMap == null) return projectionIn;
            String[] projection = new String[projectionIn.length];
            for (int i = 0; i < projectionIn.length; i++) {
                String userColumn = projectionIn[i], column = mProjectionMap.get(userColumn);
                if (column != null) projection[i] = column;
                else if (!mStrictColumns && (userColumn.contains(" AS ") || userColumn.contains(" as "))) projection[i] = userColumn;
                else if (!mStrictColumns) projection[i] = userColumn;
                else throw new IllegalArgumentException("Invalid column " + projectionIn[i]);
            }
            return projection;
        } else if (mProjectionMap != null) {
            Set<Map.Entry<String, String>> entrySet = mProjectionMap.entrySet();
            String[] projection = new String[entrySet.size()];
            int i = 0;
            for (Map.Entry<String, String> entry : entrySet) { if (entry.getKey().equals("_count")) continue; projection[i++] = entry.getValue(); }
            return java.util.Arrays.copyOf(projection, i);
        }
        return null;
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public java.lang.String buildDelete(java.lang.String p0) { return null; }
    public java.lang.String buildInsert(android.content.ContentValues p0) { return null; }
    public java.lang.String buildUpdate(android.content.ContentValues p0, java.lang.String p1) { return null; }
    public java.util.Collection getProjectionGreylist() { return new java.util.ArrayList(); }
    public boolean isProjectionAggregationAllowed() { return (huskFill.get("ProjectionAggregationAllowed") instanceof Boolean ? (Boolean) huskFill.get("ProjectionAggregationAllowed") : false); }
    public void setProjectionAggregationAllowed(boolean p0) { huskFill.put("ProjectionAggregationAllowed", Boolean.valueOf(p0)); }
    // ---- end of generated members
}
