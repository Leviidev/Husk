package android.content.pm;
public class Signature implements android.os.Parcelable {
    private final byte[] mBytes;
    public Signature(byte[] b) { mBytes = b.clone(); }
    public Signature(String hex) { mBytes = hex.getBytes(); }
    public byte[] toByteArray() { return mBytes.clone(); }
    public char[] toChars() { return toCharsString().toCharArray(); }
    public String toCharsString() { StringBuilder b = new StringBuilder(); for (byte x : mBytes) b.append(String.format("%02x", x & 255)); return b.toString(); }
    @Override public boolean equals(Object o) { return o instanceof Signature && java.util.Arrays.equals(mBytes, ((Signature) o).mBytes); }
    @Override public int hashCode() { return java.util.Arrays.hashCode(mBytes); }
    public int describeContents() { return 0; }
}
