package android.content;
import android.database.Cursor;
import android.net.Uri;
public class ContentProviderClient implements AutoCloseable {
    private final ContentProvider p;
    ContentProviderClient(ContentProvider p) { this.p = p; }
    public Cursor query(Uri u, String[] proj, String sel, String[] args, String sort) { return p.query(u, proj, sel, args, sort); }
    public Uri insert(Uri u, ContentValues v) { return p.insert(u, v); }
    public int update(Uri u, ContentValues v, String sel, String[] args) { return p.update(u, v, sel, args); }
    public int delete(Uri u, String sel, String[] args) { return p.delete(u, sel, args); }
    public String getType(Uri u) { return p.getType(u); }
    public android.os.Bundle call(String method, String arg, android.os.Bundle extras) { return p.call(method, arg, extras); }
    public ContentProvider getLocalContentProvider() { return p; }
    public boolean release() { return true; }
    public void close() {}
}
