package android.content;

import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import java.util.ArrayList;
import java.util.HashMap;

/** Content by URI: the app's own providers (by authority), files, and the app's resources. */
public class ContentResolver {
    public static final String SCHEME_CONTENT = "content", SCHEME_FILE = "file", SCHEME_ANDROID_RESOURCE = "android.resource",
        CURSOR_ITEM_BASE_TYPE = "vnd.android.cursor.item", CURSOR_DIR_BASE_TYPE = "vnd.android.cursor.dir", ANY_CURSOR_ITEM_TYPE = "vnd.android.cursor.item/*",
        QUERY_ARG_SQL_SELECTION = "android:query-arg-sql-selection", QUERY_ARG_SQL_SELECTION_ARGS = "android:query-arg-sql-selection-args",
        QUERY_ARG_SQL_SORT_ORDER = "android:query-arg-sql-sort-order", EXTRA_SIZE = "android.content.extra.SIZE";
    public static final int NOTIFY_SYNC_TO_NETWORK = 1, NOTIFY_SKIP_NOTIFY_FOR_DESCENDANTS = 2, SYNC_OBSERVER_TYPE_SETTINGS = 1;
    private static final HashMap<String, ContentProvider> sProviders = new HashMap<>();
    private static final ArrayList<Object[]> sObservers = new ArrayList<>();
    private static ContentResolver sSelf;
    private final Context mContext;
    public ContentResolver(Context c) { mContext = c; }
    public static synchronized ContentResolver self() { if (sSelf == null) sSelf = new ContentResolver(null); return sSelf; }
    public static void register(String authorities, ContentProvider p) {
        if (authorities == null) return;
        synchronized (sProviders) { for (String a : authorities.split(";")) sProviders.put(a.trim(), p); }
    }
    private static ContentProvider provider(Uri u) { if (u == null || !SCHEME_CONTENT.equals(u.getScheme())) return null; synchronized (sProviders) { return sProviders.get(u.getAuthority()); } }
    public final ContentProviderClient acquireContentProviderClient(Uri u) { ContentProvider p = provider(u); return p == null ? null : new ContentProviderClient(p); }
    public final ContentProviderClient acquireContentProviderClient(String a) { ContentProvider p; synchronized (sProviders) { p = sProviders.get(a); } return p == null ? null : new ContentProviderClient(p); }
    public final ContentProviderClient acquireUnstableContentProviderClient(Uri u) { return acquireContentProviderClient(u); }
    public final ContentProviderClient acquireUnstableContentProviderClient(String a) { return acquireContentProviderClient(a); }
    public final Cursor query(Uri u, String[] proj, String sel, String[] args, String sort) { ContentProvider p = provider(u); return p == null ? systemEmpty(u, proj) : p.query(u, proj, sel, args, sort); }
    /** The phone's own providers (media, contacts, calendar...): there is no such data here, so an empty answer, as a phone
     *  with none would give; an authority nobody has gives null, as on Android. */
    private static Cursor systemEmpty(Uri u, String[] proj) {
        String a = u == null ? null : u.getAuthority();
        if (a == null) return null;
        switch (a) {
        case "media": case "com.android.contacts": case "contacts": case "com.android.calendar": case "call_log": case "sms": case "mms":
        case "mms-sms": case "downloads": case "user_dictionary": case "com.android.externalstorage.documents": case "com.android.providers.media.documents":
        case "com.android.providers.downloads.documents": case "telephony": case "icc":
            return new android.database.MatrixCursor(proj != null ? proj : new String[] { "_id" });
        default: return null;
        }
    }
    public final Cursor query(Uri u, String[] proj, String sel, String[] args, String sort, android.os.CancellationSignal c) { return query(u, proj, sel, args, sort); }
    public final Cursor query(Uri u, String[] proj, Bundle args, android.os.CancellationSignal c) { ContentProvider p = provider(u); return p == null ? systemEmpty(u, proj) : p.query(u, proj, args, c); }
    public final String getType(Uri u) {
        ContentProvider p = provider(u);
        if (p != null) return p.getType(u);
        String path = u.getPath();
        return path == null ? null : java.net.URLConnection.guessContentTypeFromName(path);
    }
    public String[] getStreamTypes(Uri u, String filter) { return null; }
    public final Uri insert(Uri u, ContentValues v) { ContentProvider p = provider(u); return p == null ? null : p.insert(u, v); }
    public final Uri insert(Uri u, ContentValues v, Bundle extras) { return insert(u, v); }
    public final int update(Uri u, ContentValues v, String sel, String[] args) { ContentProvider p = provider(u); return p == null ? 0 : p.update(u, v, sel, args); }
    public final int delete(Uri u, String sel, String[] args) { ContentProvider p = provider(u); return p == null ? 0 : p.delete(u, sel, args); }
    public final int bulkInsert(Uri u, ContentValues[] v) { int n = 0; for (ContentValues c : v) if (insert(u, c) != null) n++; return n; }
    public final Bundle call(Uri u, String method, String arg, Bundle extras) { ContentProvider p = provider(u); return p == null ? null : p.call(method, arg, extras); }
    public final Bundle call(String authority, String method, String arg, Bundle extras) { ContentProvider p; synchronized (sProviders) { p = sProviders.get(authority); } return p == null ? null : p.call(method, arg, extras); }
    public final java.io.InputStream openInputStream(Uri u) throws java.io.FileNotFoundException {
        if (u == null) throw new java.io.FileNotFoundException("null uri");
        String s = u.getScheme();
        if (SCHEME_FILE.equals(s) || s == null) {
            String path = u.getPath();
            if (path != null && path.startsWith("/android_asset/")) { try { return husk.ContextImpl.app().getAssets().open(path.substring(15)); } catch (java.io.IOException e) { throw new java.io.FileNotFoundException(path); } }
            return new java.io.FileInputStream(path);
        }
        if (SCHEME_ANDROID_RESOURCE.equals(s)) {
            java.util.List<String> seg = u.getPathSegments();
            android.content.res.Resources r = husk.ContextImpl.app().getResources();
            int id = seg.size() == 1 ? Integer.parseInt(seg.get(0)) : r.getIdentifier(seg.get(1), seg.get(0), u.getAuthority());
            return r.openRawResource(id);
        }
        ContentProvider p = provider(u);
        if (p != null) {
            android.content.res.AssetFileDescriptor fd = p.openAssetFile(u, "r");
            if (fd == null) throw new java.io.FileNotFoundException(u.toString());
            try { return fd.createInputStream(); } catch (java.io.IOException e) { throw new java.io.FileNotFoundException(e.toString()); }
        }
        throw new java.io.FileNotFoundException("No content provider: " + u);
    }
    public final java.io.OutputStream openOutputStream(Uri u) throws java.io.FileNotFoundException { return openOutputStream(u, "w"); }
    public final java.io.OutputStream openOutputStream(Uri u, String mode) throws java.io.FileNotFoundException {
        String s = u.getScheme();
        if (SCHEME_FILE.equals(s) || s == null) return new java.io.FileOutputStream(u.getPath(), mode.contains("a"));
        ContentProvider p = provider(u);
        if (p != null) {
            android.os.ParcelFileDescriptor pfd = p.openFile(u, mode);
            if (pfd != null) return new java.io.FileOutputStream(pfd.getFileDescriptor());
        }
        throw new java.io.FileNotFoundException("No content provider: " + u);
    }
    public final android.os.ParcelFileDescriptor openFileDescriptor(Uri u, String mode) throws java.io.FileNotFoundException {
        if (SCHEME_FILE.equals(u.getScheme())) return android.os.ParcelFileDescriptor.open(new java.io.File(u.getPath()), android.os.ParcelFileDescriptor.parseMode(mode));
        ContentProvider p = provider(u);
        if (p != null) return p.openFile(u, mode);
        throw new java.io.FileNotFoundException("No content provider: " + u);
    }
    public final android.os.ParcelFileDescriptor openFileDescriptor(Uri u, String mode, android.os.CancellationSignal c) throws java.io.FileNotFoundException { return openFileDescriptor(u, mode); }
    public final android.content.res.AssetFileDescriptor openAssetFileDescriptor(Uri u, String mode) throws java.io.FileNotFoundException {
        ContentProvider p = provider(u);
        if (p != null) return p.openAssetFile(u, mode);
        android.os.ParcelFileDescriptor pfd = openFileDescriptor(u, mode);
        return new android.content.res.AssetFileDescriptor(pfd, 0, -1);
    }
    public final android.content.res.AssetFileDescriptor openTypedAssetFileDescriptor(Uri u, String mime, Bundle o) throws java.io.FileNotFoundException { return openAssetFileDescriptor(u, "r"); }
    public final void registerContentObserver(Uri u, boolean descendants, ContentObserver o) { synchronized (sObservers) { sObservers.add(new Object[] { u, descendants, o }); } }
    public final void unregisterContentObserver(ContentObserver o) { synchronized (sObservers) { for (int i = sObservers.size() - 1; i >= 0; i--) if (sObservers.get(i)[2] == o) sObservers.remove(i); } }
    public void notifyChange(Uri u, ContentObserver observer) { notifyChange(u, observer, true); }
    public void notifyChange(Uri u, ContentObserver observer, boolean sync) {
        ArrayList<Object[]> list; synchronized (sObservers) { list = new ArrayList<>(sObservers); }
        String us = u.toString();
        for (Object[] e : list) {
            String os = e[0].toString();
            if (e[2] != observer && (us.equals(os) || ((Boolean) e[1] && us.startsWith(os)))) ((ContentObserver) e[2]).dispatchChange(false, u);
        }
    }
    public void notifyChange(Uri u, ContentObserver observer, int flags) { notifyChange(u, observer, true); }
    public void takePersistableUriPermission(Uri u, int flags) {}
    public void releasePersistableUriPermission(Uri u, int flags) {}
    public java.util.List<UriPermission> getPersistedUriPermissions() { return new ArrayList<>(); }
    public static final class UriPermission {}
    public static void requestSync(android.accounts.Account a, String authority, Bundle extras) {}
    public static void setSyncAutomatically(android.accounts.Account a, String authority, boolean sync) {}
    public static boolean getMasterSyncAutomatically() { return false; }
    public final Uri canonicalize(Uri u) { return u; }
    public final Uri uncanonicalize(Uri u) { return u; }
    public final android.graphics.Bitmap loadThumbnail(Uri u, android.util.Size s, android.os.CancellationSignal c) throws java.io.IOException {
        try (java.io.InputStream in = openInputStream(u)) { return android.graphics.BitmapFactory.decodeStream(in); }
    }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class MimeTypeInfo {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public MimeTypeInfo(android.graphics.drawable.Icon p0, java.lang.CharSequence p1, java.lang.CharSequence p2) {}
        public java.lang.CharSequence getContentDescription() { return (java.lang.CharSequence) huskProps.get("ContentDescription"); }
        public android.graphics.drawable.Icon getIcon() { return (android.graphics.drawable.Icon) huskProps.get("Icon"); }
        public java.lang.CharSequence getLabel() { return (java.lang.CharSequence) huskProps.get("Label"); }
        MimeTypeInfo() { this((android.graphics.drawable.Icon) null, (java.lang.CharSequence) null, (java.lang.CharSequence) null); }
    }
    public static class OpenResourceIdResult {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public int id;
        public android.content.res.Resources r;
        public OpenResourceIdResult(android.content.ContentResolver p0) {}
        OpenResourceIdResult() { this((android.content.ContentResolver) null); }
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static android.content.Intent ACTION_SYNC_CONN_STATUS_CHANGED;
    public static final int CONTENT_PROVIDER_PUBLISH_TIMEOUT_MILLIS = 0;
    public static final int CONTENT_PROVIDER_READY_TIMEOUT_MILLIS = 0;
    public static final java.lang.String CONTENT_SERVICE_NAME = "content";
    public static final boolean DEPRECATE_DATA_COLUMNS = true;
    public static final java.lang.String DEPRECATE_DATA_PREFIX = "/mnt/content/";
    public static final java.lang.String EXTRA_HONORED_ARGS = "android.content.extra.HONORED_ARGS";
    public static final java.lang.String EXTRA_REFRESH_SUPPORTED = "android.content.extra.REFRESH_SUPPORTED";
    public static final java.lang.String EXTRA_TOTAL_COUNT = "android.content.extra.TOTAL_COUNT";
    public static final java.lang.String MIME_TYPE_DEFAULT = "application/octet-stream";
    public static final int NOTIFY_DELETE = 16;
    public static final int NOTIFY_INSERT = 4;
    public static final int NOTIFY_NO_DELAY = 32768;
    public static final int NOTIFY_UPDATE = 8;
    public static final java.lang.String QUERY_ARG_GROUP_COLUMNS = "android:query-arg-group-columns";
    public static final java.lang.String QUERY_ARG_LIMIT = "android:query-arg-limit";
    public static final java.lang.String QUERY_ARG_OFFSET = "android:query-arg-offset";
    public static final java.lang.String QUERY_ARG_SORT_COLLATION = "android:query-arg-sort-collation";
    public static final java.lang.String QUERY_ARG_SORT_COLUMNS = "android:query-arg-sort-columns";
    public static final java.lang.String QUERY_ARG_SORT_DIRECTION = "android:query-arg-sort-direction";
    public static final java.lang.String QUERY_ARG_SORT_LOCALE = "android:query-arg-sort-locale";
    public static final java.lang.String QUERY_ARG_SQL_GROUP_BY = "android:query-arg-sql-group-by";
    public static final java.lang.String QUERY_ARG_SQL_HAVING = "android:query-arg-sql-having";
    public static final java.lang.String QUERY_ARG_SQL_LIMIT = "android:query-arg-sql-limit";
    public static final int QUERY_SORT_DIRECTION_ASCENDING = 0;
    public static final int QUERY_SORT_DIRECTION_DESCENDING = 1;
    public static final java.lang.String REMOTE_CALLBACK_ERROR = "error";
    public static final java.lang.String REMOTE_CALLBACK_RESULT = "result";
    public static final int SYNC_ERROR_AUTHENTICATION = 2;
    public static final int SYNC_ERROR_CONFLICT = 5;
    public static final int SYNC_ERROR_INTERNAL = 8;
    public static final int SYNC_ERROR_IO = 3;
    public static final int SYNC_ERROR_PARSE = 4;
    public static final int SYNC_ERROR_SYNC_ALREADY_IN_PROGRESS = 1;
    public static final int SYNC_ERROR_TOO_MANY_DELETIONS = 6;
    public static final int SYNC_ERROR_TOO_MANY_RETRIES = 7;
    public static final int SYNC_EXEMPTION_NONE = 0;
    public static final int SYNC_EXEMPTION_PROMOTE_BUCKET = 1;
    public static final int SYNC_EXEMPTION_PROMOTE_BUCKET_WITH_TEMP = 2;
    public static final java.lang.String SYNC_EXTRAS_ACCOUNT = "account";
    public static final java.lang.String SYNC_EXTRAS_DISALLOW_METERED = "allow_metered";
    public static final java.lang.String SYNC_EXTRAS_DISCARD_LOCAL_DELETIONS = "discard_deletions";
    public static final java.lang.String SYNC_EXTRAS_DO_NOT_RETRY = "do_not_retry";
    public static final java.lang.String SYNC_EXTRAS_EXPECTED_DOWNLOAD = "expected_download";
    public static final java.lang.String SYNC_EXTRAS_EXPECTED_UPLOAD = "expected_upload";
    public static final java.lang.String SYNC_EXTRAS_EXPEDITED = "expedited";
    public static final java.lang.String SYNC_EXTRAS_FORCE = "force";
    public static final java.lang.String SYNC_EXTRAS_IGNORE_BACKOFF = "ignore_backoff";
    public static final java.lang.String SYNC_EXTRAS_IGNORE_SETTINGS = "ignore_settings";
    public static final java.lang.String SYNC_EXTRAS_INITIALIZE = "initialize";
    public static final java.lang.String SYNC_EXTRAS_MANUAL = "force";
    public static final java.lang.String SYNC_EXTRAS_OVERRIDE_TOO_MANY_DELETIONS = "deletions_override";
    public static final java.lang.String SYNC_EXTRAS_PRIORITY = "sync_priority";
    public static final java.lang.String SYNC_EXTRAS_REQUIRE_CHARGING = "require_charging";
    public static final java.lang.String SYNC_EXTRAS_SCHEDULE_AS_EXPEDITED_JOB = "schedule_as_expedited_job";
    public static final java.lang.String SYNC_EXTRAS_UPLOAD = "upload";
    public static final int SYNC_OBSERVER_TYPE_ACTIVE = 4;
    public static final int SYNC_OBSERVER_TYPE_ALL = 2147483647;
    public static final int SYNC_OBSERVER_TYPE_PENDING = 2;
    public static final int SYNC_OBSERVER_TYPE_STATUS = 8;
    public static final java.lang.String SYNC_VIRTUAL_EXTRAS_EXEMPTION_FLAG = "v_exemption";
    public static void addPeriodicSync(android.accounts.Account p0, java.lang.String p1, android.os.Bundle p2, long p3) {}
    public static java.lang.Object addStatusChangeListener(int p0, android.content.SyncStatusObserver p1) { return null; }
    public static void cancelSync(android.accounts.Account p0, java.lang.String p1) {}
    public static void cancelSync(android.content.SyncRequest p0) {}
    public static void cancelSyncAsUser(android.accounts.Account p0, java.lang.String p1, int p2) {}
    public static android.os.Bundle createSqlQueryBundle(java.lang.String p0, java.lang.String[] p1) { return null; }
    public static android.os.Bundle createSqlQueryBundle(java.lang.String p0, java.lang.String[] p1, java.lang.String p2) { return null; }
    public static java.lang.String createSqlSortClause(android.os.Bundle p0) { return null; }
    public static android.net.Uri decodeFromFile(java.io.File p0) { return null; }
    public static java.io.File encodeToFile(android.net.Uri p0) { return null; }
    public static android.content.SyncInfo getCurrentSync() { return null; }
    public static java.util.List getCurrentSyncs() { return new java.util.ArrayList(); }
    public static java.util.List getCurrentSyncsAsUser(int p0) { return new java.util.ArrayList(); }
    public static int getIsSyncable(android.accounts.Account p0, java.lang.String p1) { return 0; }
    public static int getIsSyncableAsUser(android.accounts.Account p0, java.lang.String p1, int p2) { return 0; }
    public static boolean getMasterSyncAutomaticallyAsUser(int p0) { return false; }
    public static java.util.List getPeriodicSyncs(android.accounts.Account p0, java.lang.String p1) { return new java.util.ArrayList(); }
    public static java.lang.String getSyncAdapterPackageAsUser(java.lang.String p0, java.lang.String p1, int p2) { return null; }
    public static java.lang.String[] getSyncAdapterPackagesForAuthorityAsUser(java.lang.String p0, int p1) { return null; }
    public static android.content.SyncAdapterType[] getSyncAdapterTypes() { return null; }
    public static android.content.SyncAdapterType[] getSyncAdapterTypesAsUser(int p0) { return null; }
    public static boolean getSyncAutomatically(android.accounts.Account p0, java.lang.String p1) { return false; }
    public static boolean getSyncAutomaticallyAsUser(android.accounts.Account p0, java.lang.String p1, int p2) { return false; }
    public static boolean hasInvalidScheduleAsEjExtras(android.os.Bundle p0) { return false; }
    public static android.os.Bundle includeSqlSelectionArgs(android.os.Bundle p0, java.lang.String p1, java.lang.String[] p2) { return null; }
    public static boolean invalidPeriodicExtras(android.os.Bundle p0) { return false; }
    public static boolean isSyncActive(android.accounts.Account p0, java.lang.String p1) { return false; }
    public static boolean isSyncPending(android.accounts.Account p0, java.lang.String p1) { return false; }
    public static boolean isSyncPendingAsUser(android.accounts.Account p0, java.lang.String p1, int p2) { return false; }
    public static void onDbCorruption(java.lang.String p0, java.lang.String p1, java.lang.Throwable p2) {}
    public static void removePeriodicSync(android.accounts.Account p0, java.lang.String p1, android.os.Bundle p2) {}
    public static void removeStatusChangeListener(java.lang.Object p0) {}
    public static void requestSync(android.content.SyncRequest p0) {}
    public static void requestSyncAsUser(android.accounts.Account p0, java.lang.String p1, int p2, android.os.Bundle p3) {}
    public static void setIsSyncable(android.accounts.Account p0, java.lang.String p1, int p2) {}
    public static void setIsSyncableAsUser(android.accounts.Account p0, java.lang.String p1, int p2, int p3) {}
    public static void setMasterSyncAutomatically(boolean p0) {}
    public static void setMasterSyncAutomaticallyAsUser(boolean p0, int p1) {}
    public static void setSyncAutomaticallyAsUser(android.accounts.Account p0, java.lang.String p1, boolean p2, int p3) {}
    public static int syncErrorStringToInt(java.lang.String p0) { return 0; }
    public static java.lang.String syncErrorToString(int p0) { return null; }
    public static android.net.Uri translateDeprecatedDataPath(java.lang.String p0) { return null; }
    public static java.lang.String translateDeprecatedDataPath(android.net.Uri p0) { return null; }
    public static void validateSyncExtrasBundle(android.os.Bundle p0) {}
    public static android.content.ContentResolver wrap(android.content.ContentProvider p0) { return null; }
    public static android.content.ContentResolver wrap(android.content.ContentProviderClient p0) { return null; }
    public android.content.ContentProviderResult[] applyBatch(java.lang.String p0, java.util.ArrayList p1) { return null; }
    public void cancelSync(android.net.Uri p0) {}
    public android.net.Uri canonicalizeOrElse(android.net.Uri p0) { return null; }
    public int checkUriPermission(android.net.Uri p0, int p1, int p2) { return 0; }
    public int delete(android.net.Uri p0, android.os.Bundle p1) { return 0; }
    public android.content.AttributionSource getAttributionSource() { return null; }
    public java.lang.String getAttributionTag() { return null; }
    public android.os.Bundle getCache(android.net.Uri p0) { return null; }
    public java.util.List getOutgoingPersistedUriPermissions() { return new java.util.ArrayList(); }
    public java.util.List getOutgoingUriPermissions() { return new java.util.ArrayList(); }
    public java.lang.String getPackageName() { return null; }
    public android.content.ContentResolver.OpenResourceIdResult getResourceId(android.net.Uri p0) { return null; }
    public int getTargetSdkVersion() { return 0; }
    public android.graphics.drawable.Drawable getTypeDrawable(java.lang.String p0) { return null; }
    public android.content.ContentResolver.MimeTypeInfo getTypeInfo(java.lang.String p0) { return null; }
    public int getUserId() { return 0; }
    public void notifyChange(android.net.Uri p0, android.database.ContentObserver p1, int p2, int p3) {}
    public void notifyChange(android.net.Uri p0, android.database.ContentObserver p1, boolean p2, int p3) {}
    public void notifyChange(java.lang.Iterable p0, android.database.ContentObserver p1, int p2) {}
    public void notifyChange(java.util.Collection p0, android.database.ContentObserver p1, int p2) {}
    public void notifyChange(android.net.Uri[] p0, android.database.ContentObserver p1, int p2, int p3) {}
    public android.content.res.AssetFileDescriptor openAssetFile(android.net.Uri p0, java.lang.String p1, android.os.CancellationSignal p2) { return null; }
    public android.content.res.AssetFileDescriptor openAssetFileDescriptor(android.net.Uri p0, java.lang.String p1, android.os.CancellationSignal p2) { return null; }
    public android.os.ParcelFileDescriptor openFile(android.net.Uri p0, java.lang.String p1, android.os.CancellationSignal p2) { return null; }
    public android.content.res.AssetFileDescriptor openTypedAssetFile(android.net.Uri p0, java.lang.String p1, android.os.Bundle p2, android.os.CancellationSignal p3) { return null; }
    public android.content.res.AssetFileDescriptor openTypedAssetFileDescriptor(android.net.Uri p0, java.lang.String p1, android.os.Bundle p2, android.os.CancellationSignal p3) { return null; }
    public void putCache(android.net.Uri p0, android.os.Bundle p1) {}
    public boolean refresh(android.net.Uri p0, android.os.Bundle p1, android.os.CancellationSignal p2) { return false; }
    public void registerContentObserver(android.net.Uri p0, boolean p1, android.database.ContentObserver p2, int p3) {}
    public void registerContentObserverAsUser(android.net.Uri p0, boolean p1, android.database.ContentObserver p2, android.os.UserHandle p3) {}
    public int resolveUserId(android.net.Uri p0) { return 0; }
    public void startSync(android.net.Uri p0, android.os.Bundle p1) {}
    public void takePersistableUriPermission(java.lang.String p0, android.net.Uri p1, int p2) {}
    public int update(android.net.Uri p0, android.content.ContentValues p1, android.os.Bundle p2) { return 0; }
    // ---- end of generated members
}
