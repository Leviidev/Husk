package android.os;
public final class LocaleList {
    private final java.util.Locale[] l;
    public LocaleList(java.util.Locale... l) { this.l = l; }
    public static LocaleList getDefault() { return new LocaleList(java.util.Locale.getDefault()); }
    public java.util.Locale get(int i) { return l[i]; }
    public int size() { return l.length; }
    public boolean isEmpty() { return l.length == 0; }
}
