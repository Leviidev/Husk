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
    public final Cursor query(Uri u, String[] proj, String sel, String[] args, String sort) { ContentProvider p = provider(u); return p == null ? null : p.query(u, proj, sel, args, sort); }
    public final Cursor query(Uri u, String[] proj, String sel, String[] args, String sort, android.os.CancellationSignal c) { return query(u, proj, sel, args, sort); }
    public final Cursor query(Uri u, String[] proj, Bundle args, android.os.CancellationSignal c) { ContentProvider p = provider(u); return p == null ? null : p.query(u, proj, args, c); }
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
}
