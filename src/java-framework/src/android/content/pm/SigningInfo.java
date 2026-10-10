package android.content.pm;
public final class SigningInfo implements android.os.Parcelable {
    public boolean hasMultipleSigners() { return false; } public boolean hasPastSigningCertificates() { return false; }
    public Signature[] getSigningCertificateHistory() { return new Signature[] { new Signature("") }; } public Signature[] getApkContentsSigners() { return getSigningCertificateHistory(); }
    public int describeContents() { return 0; }
}
