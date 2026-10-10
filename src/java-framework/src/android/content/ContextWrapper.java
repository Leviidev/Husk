package android.content;

public class ContextWrapper extends Context {
    private Context mBase;
    public ContextWrapper(Context base) { mBase = base; }
    protected void attachBaseContext(Context base) { mBase = base; }
    public Context getBaseContext() { return mBase != null ? mBase : this; }
}
