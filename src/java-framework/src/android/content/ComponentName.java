package android.content;
public final class ComponentName {
    private final String pkg, cls;
    public ComponentName(String pkg, String cls) { this.pkg = pkg; this.cls = cls; }
    public ComponentName(Context c, Class<?> k) { this(c.getPackageName(), k.getName()); }
    public String getPackageName() { return pkg; }
    public String getClassName() { return cls; }
}
