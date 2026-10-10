package android.accounts;
public class Account implements android.os.Parcelable {
    public final String name, type;
    public Account(String name, String type) { this.name = name; this.type = type; }
    public int describeContents() { return 0; }
    @Override public boolean equals(Object o) { return o instanceof Account && name.equals(((Account) o).name) && type.equals(((Account) o).type); }
    @Override public int hashCode() { return name.hashCode() * 31 + type.hashCode(); }
}
