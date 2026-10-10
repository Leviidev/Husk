package android.content;

import android.content.pm.ProviderInfo;
import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;

public abstract class ContentProvider implements ComponentCallbacks2 {
    public interface PipeDataWriter<T> { void writeDataToPipe(ParcelFileDescriptor out, Uri u, String mime, Bundle o, T args); }
    private Context mContext;
    private String mAuthority;
    private boolean mExported;
    private String mReadPermission, mWritePermission;
    public ContentProvider() {}
    public ContentProvider(Context c, String readPerm, String writePerm, Object[] pathPerms) { mContext = c; mReadPermission = readPerm; mWritePermission = writePerm; }
    public final Context getContext() { return mContext; }
    public final Context requireContext() { if (mContext == null) throw new IllegalStateException("Cannot find context from the provider."); return mContext; }
    public void attachInfo(Context c, ProviderInfo info) {
        if (mContext == null) {
            mContext = c;
            if (info != null) { mAuthority = info.authority; mExported = info.exported; mReadPermission = info.readPermission; mWritePermission = info.writePermission; }
            onCreate();
        }
    }
    public abstract boolean onCreate();
    public abstract Cursor query(Uri u, String[] proj, String sel, String[] args, String sort);
    public Cursor query(Uri u, String[] proj, String sel, String[] args, String sort, android.os.CancellationSignal c) { return query(u, proj, sel, args, sort); }
    public Cursor query(Uri u, String[] proj, Bundle args, android.os.CancellationSignal c) {
        if (args == null) args = Bundle.EMPTY;
        return query(u, proj, args.getString(ContentResolver.QUERY_ARG_SQL_SELECTION), args.getStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS), args.getString(ContentResolver.QUERY_ARG_SQL_SORT_ORDER), c);
    }
    public abstract String getType(Uri u);
    public abstract Uri insert(Uri u, ContentValues v);
    public Uri insert(Uri u, ContentValues v, Bundle extras) { return insert(u, v); }
    public abstract int delete(Uri u, String sel, String[] args);
    public int delete(Uri u, Bundle extras) { return delete(u, null, null); }
    public abstract int update(Uri u, ContentValues v, String sel, String[] args);
    public int update(Uri u, ContentValues v, Bundle extras) { return update(u, v, null, null); }
    public int bulkInsert(Uri u, ContentValues[] v) { int n = 0; for (ContentValues c : v) if (insert(u, c) != null) n++; return n; }
    public Bundle call(String method, String arg, Bundle extras) { return null; }
    public Bundle call(String authority, String method, String arg, Bundle extras) { return call(method, arg, extras); }
    public ParcelFileDescriptor openFile(Uri u, String mode) throws java.io.FileNotFoundException { throw new java.io.FileNotFoundException("No files supported by provider at " + u); }
    public ParcelFileDescriptor openFile(Uri u, String mode, android.os.CancellationSignal c) throws java.io.FileNotFoundException { return openFile(u, mode); }
    public AssetFileDescriptor openAssetFile(Uri u, String mode) throws java.io.FileNotFoundException { ParcelFileDescriptor fd = openFile(u, mode); return fd != null ? new AssetFileDescriptor(fd, 0, -1) : null; }
    public AssetFileDescriptor openAssetFile(Uri u, String mode, android.os.CancellationSignal c) throws java.io.FileNotFoundException { return openAssetFile(u, mode); }
    public AssetFileDescriptor openTypedAssetFile(Uri u, String mime, Bundle o) throws java.io.FileNotFoundException { return openAssetFile(u, "r"); }
    protected final ParcelFileDescriptor openFileHelper(Uri u, String mode) throws java.io.FileNotFoundException { throw new java.io.FileNotFoundException(u.toString()); }
    public <T> ParcelFileDescriptor openPipeHelper(Uri u, String mime, Bundle o, T args, PipeDataWriter<T> w) throws java.io.FileNotFoundException { throw new java.io.FileNotFoundException(u.toString()); }
    public String[] getStreamTypes(Uri u, String filter) { return null; }
    public Uri canonicalize(Uri u) { return null; }
    public Uri uncanonicalize(Uri u) { return u; }
    public boolean refresh(Uri u, Bundle extras, android.os.CancellationSignal c) { return false; }
    public void shutdown() {}
    public void onConfigurationChanged(android.content.res.Configuration c) {}
    public void onLowMemory() {}
    public void onTrimMemory(int level) {}
    public final String getCallingPackage() { return mContext != null ? mContext.getPackageName() : null; }
    public final String getCallingPackageUnchecked() { return getCallingPackage(); }
    public final String getReadPermission() { return mReadPermission; }
    public final String getWritePermission() { return mWritePermission; }
    protected final void setReadPermission(String p) { mReadPermission = p; }
    protected final void setWritePermission(String p) { mWritePermission = p; }
    protected final void setAuthorities(String a) { mAuthority = a; }
    public final String getAuthority() { return mAuthority; }
    public final boolean isTemporary() { return false; }
    public ContentProviderResult[] applyBatch(java.util.ArrayList<ContentProviderOperation> ops) throws OperationApplicationException { return new ContentProviderResult[0]; }
    public void dump(java.io.FileDescriptor fd, java.io.PrintWriter w, String[] args) {}
    public final void restoreCallingIdentity(CallingIdentity token) {}
    public final CallingIdentity clearCallingIdentity() { return null; }
    // ---- platform API stubs (tools/compat/genstubs.py)
    public static final class CallingIdentity { CallingIdentity() {}
        // ---- generated by tools/compat/fillmembers.py (CallingIdentity): the platform's members this class does not write (signatures only)
        public long binderToken;
        public android.content.AttributionSource callingAttributionSource;
        public CallingIdentity(android.content.ContentProvider p0, long p1, android.content.AttributionSource p2) { this(); }
        // ---- end of generated members (CallingIdentity)
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public ContentProvider(android.content.Context p0, java.lang.String p1, java.lang.String p2, android.content.pm.PathPermission[] p3) { this(); }
    public static android.net.Uri createContentUriForUser(android.net.Uri p0, android.os.UserHandle p1) { return null; }
    public static java.lang.String getAuthorityWithoutUserId(java.lang.String p0) { return null; }
    public static android.net.Uri getUriWithoutUserId(android.net.Uri p0) { return null; }
    public static android.os.UserHandle getUserHandleFromUri(android.net.Uri p0) { return null; }
    public static int getUserIdFromAuthority(java.lang.String p0) { return 0; }
    public static int getUserIdFromAuthority(java.lang.String p0, int p1) { return 0; }
    public static int getUserIdFromUri(android.net.Uri p0) { return 0; }
    public static int getUserIdFromUri(android.net.Uri p0, int p1) { return 0; }
    public static boolean isAuthorityRedirectedForCloneProfile(java.lang.String p0) { return false; }
    public static android.net.Uri maybeAddUserId(android.net.Uri p0, int p1) { return null; }
    public static boolean uriHasUserId(android.net.Uri p0) { return false; }
    public android.content.ContentProviderResult[] applyBatch(java.lang.String p0, java.util.ArrayList p1) { return null; }
    public int checkUriPermission(android.net.Uri p0, int p1, int p2) { return 0; }
    protected int enforceReadPermissionInner(android.net.Uri p0, android.content.AttributionSource p1) { return 0; }
    protected int enforceWritePermissionInner(android.net.Uri p0, android.content.AttributionSource p1) { return 0; }
    public android.app.AppOpsManager getAppOpsManager() { return null; }
    public android.content.AttributionSource getCallingAttributionSource() { return null; }
    public java.lang.String getCallingAttributionTag() { return null; }
    public int getCallingDeviceId() { return 0; }
    public java.lang.String getCallingFeatureId() { return null; }
    public android.content.pm.PathPermission[] getPathPermissions() { return (android.content.pm.PathPermission[]) huskFill.get("PathPermissions"); }
    protected boolean matchesOurAuthorities(java.lang.String p0) { return false; }
    public void onCallingPackageChanged() {}
    public android.net.Uri rejectInsert(android.net.Uri p0, android.content.ContentValues p1) { return null; }
    public void setAppOps(int p0, int p1) {}
    protected void setPathPermissions(android.content.pm.PathPermission[] p0) { huskFill.put("PathPermissions", p0); }
    public void setTransportLoggingEnabled(boolean p0) {}
    public android.net.Uri validateIncomingUri(android.net.Uri p0) { return null; }
    // ---- end of generated members
}
