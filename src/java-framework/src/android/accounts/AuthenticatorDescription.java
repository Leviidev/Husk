package android.accounts;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class AuthenticatorDescription implements android.os.Parcelable {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final android.os.Parcelable.Creator<AuthenticatorDescription> CREATOR = new android.os.Parcelable.Creator<AuthenticatorDescription>() {
        public AuthenticatorDescription createFromParcel(android.os.Parcel p) { return new AuthenticatorDescription(p.readString(), p.readString(), p.readInt(), p.readInt(), p.readInt(), p.readInt(), p.readByte() != 0); }
        public AuthenticatorDescription[] newArray(int n) { return new AuthenticatorDescription[n]; }
    };
    public int accountPreferencesId;
    public boolean customTokens;
    public int iconId;
    public int labelId;
    public java.lang.String packageName;
    public int smallIconId;
    public java.lang.String type;
    public AuthenticatorDescription(String type, String pkg, int label, int icon, int small, int prefs) { this(type, pkg, label, icon, small, prefs, false); }
    public AuthenticatorDescription(String type, String pkg, int label, int icon, int small, int prefs, boolean custom) {
        if (type == null) throw new IllegalArgumentException("type cannot be null");
        this.type = type; packageName = pkg; labelId = label; iconId = icon; smallIconId = small; accountPreferencesId = prefs; customTokens = custom;
    }
    public static android.accounts.AuthenticatorDescription newKey(String type) { return new AuthenticatorDescription(type, null, 0, 0, 0, 0); }
    @Override public boolean equals(Object obj) { return obj instanceof AuthenticatorDescription && type.equals(((AuthenticatorDescription)obj).type); }
    @Override public int hashCode() { return type.hashCode(); }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int flags) { p.writeString(type); p.writeString(packageName); p.writeInt(labelId); p.writeInt(iconId); p.writeInt(smallIconId); p.writeInt(accountPreferencesId); p.writeByte((byte)(customTokens ? 1 : 0)); }
}
