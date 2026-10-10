package android.database.sqlite;

import android.content.Context;
import android.database.DatabaseErrorHandler;
import java.io.File;

/** Opens the app's database, creating it or bringing its schema up to the version the app asks for, as Android's does. */
public abstract class SQLiteOpenHelper implements AutoCloseable {
    private final Context mContext;
    private final String mName;
    private final int mNewVersion, mMinimumSupportedVersion;
    private SQLiteDatabase mDatabase;
    private boolean mIsInitializing;
    private SQLiteDatabase.OpenParams.Builder mOpenParamsBuilder;
    public SQLiteOpenHelper(Context context, String name, SQLiteDatabase.CursorFactory factory, int version) { this(context, name, factory, version, null); }
    public SQLiteOpenHelper(Context context, String name, SQLiteDatabase.CursorFactory factory, int version, DatabaseErrorHandler errorHandler) { this(context, name, factory, version, 0, errorHandler); }
    public SQLiteOpenHelper(Context context, String name, int version, SQLiteDatabase.OpenParams openParams) { this(context, name, version, 0, openParams.toBuilder()); }
    public SQLiteOpenHelper(Context context, String name, SQLiteDatabase.CursorFactory factory, int version, int minimumSupportedVersion, DatabaseErrorHandler errorHandler) {
        this(context, name, version, minimumSupportedVersion, new SQLiteDatabase.OpenParams.Builder());
        mOpenParamsBuilder.setCursorFactory(factory);
        mOpenParamsBuilder.setErrorHandler(errorHandler);
    }
    private SQLiteOpenHelper(Context context, String name, int version, int minimumSupportedVersion, SQLiteDatabase.OpenParams.Builder b) {
        if (version < 1) throw new IllegalArgumentException("Version must be >= 1, was " + version);
        mContext = context; mName = name; mNewVersion = version; mMinimumSupportedVersion = Math.max(0, minimumSupportedVersion);
        mOpenParamsBuilder = b;
        b.addOpenFlags(SQLiteDatabase.CREATE_IF_NECESSARY);
    }
    public String getDatabaseName() { return mName; }
    public void setWriteAheadLoggingEnabled(boolean enabled) {
        synchronized (this) {
            if (enabled) mOpenParamsBuilder.addOpenFlags(SQLiteDatabase.ENABLE_WRITE_AHEAD_LOGGING); else mOpenParamsBuilder.removeOpenFlags(SQLiteDatabase.ENABLE_WRITE_AHEAD_LOGGING);
            if (mDatabase != null && mDatabase.isOpen() && !mDatabase.isReadOnly()) { if (enabled) mDatabase.enableWriteAheadLogging(); else mDatabase.disableWriteAheadLogging(); }
        }
    }
    public void setLookasideConfig(int slotSize, int slotCount) { synchronized (this) { mOpenParamsBuilder.setLookasideConfig(slotSize, slotCount); } }
    public void setOpenParams(SQLiteDatabase.OpenParams openParams) { synchronized (this) { mOpenParamsBuilder = new SQLiteDatabase.OpenParams.Builder(openParams); mOpenParamsBuilder.addOpenFlags(SQLiteDatabase.CREATE_IF_NECESSARY); } }
    @Deprecated public void setIdleConnectionTimeout(long ms) { synchronized (this) { mOpenParamsBuilder.setIdleConnectionTimeout(ms); } }
    public SQLiteDatabase getWritableDatabase() { synchronized (this) { return getDatabaseLocked(true); } }
    public SQLiteDatabase getReadableDatabase() { synchronized (this) { return getDatabaseLocked(false); } }
    private SQLiteDatabase getDatabaseLocked(boolean writable) {
        if (mDatabase != null) {
            if (!mDatabase.isOpen()) mDatabase = null;
            else if (!writable || !mDatabase.isReadOnly()) return mDatabase;
        }
        if (mIsInitializing) throw new IllegalStateException("getDatabase called recursively");
        SQLiteDatabase db = mDatabase;
        try {
            mIsInitializing = true;
            if (db == null) {
                SQLiteDatabase.OpenParams params = mOpenParamsBuilder.build();
                if (mName == null) db = SQLiteDatabase.createInMemory(params);
                else {
                    File path = mContext.getDatabasePath(mName);
                    if (path.getParentFile() != null) path.getParentFile().mkdirs();
                    db = SQLiteDatabase.openDatabase(path, params);
                }
            }
            onConfigure(db);
            final int version = db.getVersion();
            if (version != mNewVersion) {
                if (db.isReadOnly()) throw new SQLiteException("Can't upgrade read-only database from version " + db.getVersion() + " to " + mNewVersion + ": " + mName);
                if (version > 0 && version < mMinimumSupportedVersion) {
                    File databaseFile = new File(db.getPath());
                    onBeforeDelete(db);
                    db.close();
                    if (SQLiteDatabase.deleteDatabase(databaseFile)) { mIsInitializing = false; return getDatabaseLocked(writable); }
                    throw new IllegalStateException("Unable to delete obsolete database " + mName + " with version " + version);
                }
                db.beginTransaction();
                try {
                    if (version == 0) onCreate(db);
                    else if (version > mNewVersion) onDowngrade(db, version, mNewVersion);
                    else onUpgrade(db, version, mNewVersion);
                    db.setVersion(mNewVersion);
                    db.setTransactionSuccessful();
                } finally { db.endTransaction(); }
            }
            onOpen(db);
            mDatabase = db;
            return db;
        } finally {
            mIsInitializing = false;
            if (db != null && db != mDatabase) db.close();
        }
    }
    public synchronized void close() { if (mIsInitializing) throw new IllegalStateException("Closed during initialization"); if (mDatabase != null && mDatabase.isOpen()) { mDatabase.close(); mDatabase = null; } }
    public void onConfigure(SQLiteDatabase db) {}
    public void onBeforeDelete(SQLiteDatabase db) {}
    public abstract void onCreate(SQLiteDatabase db);
    public abstract void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion);
    public void onDowngrade(SQLiteDatabase db, int oldVersion, int newVersion) { throw new SQLiteException("Can't downgrade database from version " + oldVersion + " to " + newVersion); }
    public void onOpen(SQLiteDatabase db) {}
}
