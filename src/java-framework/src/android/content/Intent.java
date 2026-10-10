package android.content;

import android.net.Uri;
import android.os.Bundle;

public class Intent implements Cloneable {
    public static final String ACTION_VIEW = "android.intent.action.VIEW", ACTION_MAIN = "android.intent.action.MAIN",
        ACTION_SEND = "android.intent.action.SEND", ACTION_SENDTO = "android.intent.action.SENDTO", CATEGORY_LAUNCHER = "android.intent.category.LAUNCHER",
        CATEGORY_DEFAULT = "android.intent.category.DEFAULT", EXTRA_TEXT = "android.intent.extra.TEXT", EXTRA_SUBJECT = "android.intent.extra.SUBJECT",
        ACTION_GET_CONTENT = "android.intent.action.GET_CONTENT", CATEGORY_OPENABLE = "android.intent.category.OPENABLE",
        ACTION_BATTERY_CHANGED = "android.intent.action.BATTERY_CHANGED";
    public static final int FLAG_ACTIVITY_NEW_TASK = 0x10000000, FLAG_ACTIVITY_CLEAR_TOP = 0x4000000, FLAG_GRANT_READ_URI_PERMISSION = 1;
    private String action, type;
    private Uri data;
    private Bundle extras;
    private ComponentName component;
    private int flags;
    public Intent() {}
    public Intent(String action) { this.action = action; }
    public Intent(String action, Uri data) { this.action = action; this.data = data; }
    public Intent(Context c, Class<?> cls) { component = new ComponentName(c, cls); }
    public Intent(Intent o) { action = o.action; data = o.data; type = o.type; extras = o.extras == null ? null : new Bundle(o.extras); flags = o.flags; component = o.component; }
    public static Intent createChooser(Intent target, CharSequence title) { return target; }
    public String getAction() { return action; }
    public Intent setAction(String a) { action = a; return this; }
    public Uri getData() { return data; }
    public Intent setData(Uri d) { data = d; return this; }
    public String getDataString() { return data == null ? null : data.toString(); }
    public String getType() { return type; }
    public Intent setType(String t) { type = t; return this; }
    public Intent setDataAndType(Uri d, String t) { data = d; type = t; return this; }
    public Intent addFlags(int f) { flags |= f; return this; }
    public Intent setFlags(int f) { flags = f; return this; }
    public int getFlags() { return flags; }
    public Intent addCategory(String c) { return this; }
    public Intent setPackage(String p) { return this; }
    public Intent setClassName(String pkg, String cls) { component = new ComponentName(pkg, cls); return this; }
    public Intent setComponent(ComponentName c) { component = c; return this; }
    public ComponentName getComponent() { return component; }
    public Bundle getExtras() { return extras; }
    private Bundle ex() { if (extras == null) extras = new Bundle(); return extras; }
    public Intent putExtra(String k, String v) { ex().putString(k, v); return this; }
    public Intent putExtra(String k, int v) { ex().putInt(k, v); return this; }
    public Intent putExtra(String k, long v) { ex().putLong(k, v); return this; }
    public Intent putExtra(String k, boolean v) { ex().putBoolean(k, v); return this; }
    public Intent putExtra(String k, float v) { ex().putFloat(k, v); return this; }
    public Intent putExtra(String k, CharSequence v) { ex().putString(k, v == null ? null : v.toString()); return this; }
    public Intent putExtra(String k, Bundle v) { ex().putBundle(k, v); return this; }
    public Intent putExtra(String k, android.os.Parcelable v) { ex().putParcelable(k, v); return this; }
    public Intent putExtras(Bundle b) { ex().putAll(b); return this; }
    public String getStringExtra(String k) { return extras == null ? null : extras.getString(k); }
    public int getIntExtra(String k, int d) { return extras == null ? d : extras.getInt(k, d); }
    public long getLongExtra(String k, long d) { return extras == null ? d : extras.getLong(k, d); }
    public boolean getBooleanExtra(String k, boolean d) { return extras == null ? d : extras.getBoolean(k, d); }
    public float getFloatExtra(String k, float d) { return extras == null ? d : extras.getFloat(k, d); }
    public boolean hasExtra(String k) { return extras != null && extras.containsKey(k); }
    public android.content.pm.ResolveInfo resolveActivity(android.content.pm.PackageManager pm) { return null; }
}
