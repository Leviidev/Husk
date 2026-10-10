package android.content;
public class ContentResolver {
    public java.io.InputStream openInputStream(android.net.Uri u) throws java.io.FileNotFoundException { return new java.io.FileInputStream(u.getPath()); }
    public java.io.OutputStream openOutputStream(android.net.Uri u) throws java.io.FileNotFoundException { return new java.io.FileOutputStream(u.getPath()); }
}
