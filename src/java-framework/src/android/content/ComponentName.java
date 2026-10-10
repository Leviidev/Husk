package android.content;
public final class ComponentName implements android.os.Parcelable, Cloneable, Comparable<ComponentName> {
    private final String pkg, cls;
    public ComponentName(String pkg, String cls) { if (pkg == null) throw new NullPointerException("package name is null"); if (cls == null) throw new NullPointerException("class name is null"); this.pkg = pkg; this.cls = cls; }
    public ComponentName(Context c, String cls) { this(c.getPackageName(), cls); }
    public ComponentName(Context c, Class<?> k) { this(c.getPackageName(), k.getName()); }
    public static ComponentName createRelative(String pkg, String cls) { return new ComponentName(pkg, cls.startsWith(".") ? pkg + cls : cls); }
    public static ComponentName createRelative(Context c, String cls) { return createRelative(c.getPackageName(), cls); }
    public String getPackageName() { return pkg; }
    public String getClassName() { return cls; }
    public String getShortClassName() { return cls.startsWith(pkg + ".") ? cls.substring(pkg.length()) : cls; }
    public String flattenToString() { return pkg + "/" + cls; }
    public String flattenToShortString() { return pkg + "/" + getShortClassName(); }
    public String toShortString() { return "{" + flattenToShortString() + "}"; }
    public static ComponentName unflattenFromString(String s) { int i = s.indexOf('/'); if (i < 0 || i + 1 >= s.length()) return null; String p = s.substring(0, i), c = s.substring(i + 1); if (c.startsWith(".")) c = p + c; return new ComponentName(p, c); }
    public static String flattenToShortString(ComponentName c) { return c == null ? null : c.flattenToShortString(); }
    @Override public ComponentName clone() { return new ComponentName(pkg, cls); }
    @Override public boolean equals(Object o) { return o instanceof ComponentName && pkg.equals(((ComponentName) o).pkg) && cls.equals(((ComponentName) o).cls); }
    @Override public int hashCode() { return pkg.hashCode() + cls.hashCode(); }
    public int compareTo(ComponentName o) { int r = pkg.compareTo(o.pkg); return r != 0 ? r : cls.compareTo(o.cls); }
    @Override public String toString() { return "ComponentInfo{" + pkg + "/" + cls + "}"; }
    public int describeContents() { return 0; }
}
