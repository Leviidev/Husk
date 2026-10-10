package android.content;

import android.net.Uri;
import android.os.Bundle;

public class Intent implements Cloneable, android.os.Parcelable {
    public static final String ACTION_VIEW = "android.intent.action.VIEW", ACTION_MAIN = "android.intent.action.MAIN", ACTION_EDIT = "android.intent.action.EDIT",
        ACTION_PICK = "android.intent.action.PICK", ACTION_SEND = "android.intent.action.SEND", ACTION_SENDTO = "android.intent.action.SENDTO",
        ACTION_SEND_MULTIPLE = "android.intent.action.SEND_MULTIPLE", ACTION_DIAL = "android.intent.action.DIAL", ACTION_CALL = "android.intent.action.CALL",
        ACTION_GET_CONTENT = "android.intent.action.GET_CONTENT", ACTION_OPEN_DOCUMENT = "android.intent.action.OPEN_DOCUMENT", ACTION_CREATE_DOCUMENT = "android.intent.action.CREATE_DOCUMENT",
        ACTION_OPEN_DOCUMENT_TREE = "android.intent.action.OPEN_DOCUMENT_TREE", ACTION_CHOOSER = "android.intent.action.CHOOSER", ACTION_INSERT = "android.intent.action.INSERT",
        ACTION_DELETE = "android.intent.action.DELETE", ACTION_SEARCH = "android.intent.action.SEARCH", ACTION_WEB_SEARCH = "android.intent.action.WEB_SEARCH",
        ACTION_BATTERY_CHANGED = "android.intent.action.BATTERY_CHANGED", ACTION_BOOT_COMPLETED = "android.intent.action.BOOT_COMPLETED", ACTION_SCREEN_ON = "android.intent.action.SCREEN_ON",
        ACTION_SCREEN_OFF = "android.intent.action.SCREEN_OFF", ACTION_USER_PRESENT = "android.intent.action.USER_PRESENT", ACTION_TIME_TICK = "android.intent.action.TIME_TICK",
        ACTION_TIMEZONE_CHANGED = "android.intent.action.TIMEZONE_CHANGED", ACTION_LOCALE_CHANGED = "android.intent.action.LOCALE_CHANGED", ACTION_CONFIGURATION_CHANGED = "android.intent.action.CONFIGURATION_CHANGED",
        ACTION_PACKAGE_ADDED = "android.intent.action.PACKAGE_ADDED", ACTION_PACKAGE_REMOVED = "android.intent.action.PACKAGE_REMOVED", ACTION_PACKAGE_REPLACED = "android.intent.action.PACKAGE_REPLACED",
        ACTION_MY_PACKAGE_REPLACED = "android.intent.action.MY_PACKAGE_REPLACED", ACTION_HEADSET_PLUG = "android.intent.action.HEADSET_PLUG", ACTION_MEDIA_BUTTON = "android.intent.action.MEDIA_BUTTON",
        ACTION_POWER_CONNECTED = "android.intent.action.ACTION_POWER_CONNECTED", ACTION_POWER_DISCONNECTED = "android.intent.action.ACTION_POWER_DISCONNECTED",
        ACTION_BATTERY_LOW = "android.intent.action.BATTERY_LOW", ACTION_BATTERY_OKAY = "android.intent.action.BATTERY_OKAY", ACTION_CLOSE_SYSTEM_DIALOGS = "android.intent.action.CLOSE_SYSTEM_DIALOGS",
        ACTION_APPLICATION_DETAILS_SETTINGS_ = "android.settings.APPLICATION_DETAILS_SETTINGS", ACTION_INSTALL_PACKAGE = "android.intent.action.INSTALL_PACKAGE",
        ACTION_DATE_CHANGED = "android.intent.action.DATE_CHANGED", ACTION_TIME_CHANGED = "android.intent.action.TIME_SET", ACTION_AIRPLANE_MODE_CHANGED = "android.intent.action.AIRPLANE_MODE",
        ACTION_CREATE_SHORTCUT = "android.intent.action.CREATE_SHORTCUT", ACTION_ASSIST = "android.intent.action.ASSIST", ACTION_APP_ERROR = "android.intent.action.APP_ERROR",
        ACTION_SET_WALLPAPER = "android.intent.action.SET_WALLPAPER", ACTION_PROCESS_TEXT = "android.intent.action.PROCESS_TEXT", ACTION_SHOW_APP_INFO = "android.intent.action.SHOW_APP_INFO",
        ACTION_MANAGE_PACKAGE_STORAGE = "android.intent.action.MANAGE_PACKAGE_STORAGE", ACTION_ATTACH_DATA = "android.intent.action.ATTACH_DATA", ACTION_RUN = "android.intent.action.RUN";
    public static final String CATEGORY_LAUNCHER = "android.intent.category.LAUNCHER", CATEGORY_DEFAULT = "android.intent.category.DEFAULT", CATEGORY_BROWSABLE = "android.intent.category.BROWSABLE",
        CATEGORY_OPENABLE = "android.intent.category.OPENABLE", CATEGORY_HOME = "android.intent.category.HOME", CATEGORY_ALTERNATIVE = "android.intent.category.ALTERNATIVE",
        CATEGORY_SELECTED_ALTERNATIVE = "android.intent.category.SELECTED_ALTERNATIVE", CATEGORY_INFO = "android.intent.category.INFO", CATEGORY_LEANBACK_LAUNCHER = "android.intent.category.LEANBACK_LAUNCHER",
        CATEGORY_APP_BROWSER = "android.intent.category.APP_BROWSER", CATEGORY_APP_EMAIL = "android.intent.category.APP_EMAIL", CATEGORY_APP_MARKET = "android.intent.category.APP_MARKET",
        CATEGORY_TAB = "android.intent.category.TAB", CATEGORY_PREFERENCE = "android.intent.category.PREFERENCE", CATEGORY_MONKEY = "android.intent.category.MONKEY";
    public static final String EXTRA_TEXT = "android.intent.extra.TEXT", EXTRA_HTML_TEXT = "android.intent.extra.HTML_TEXT", EXTRA_SUBJECT = "android.intent.extra.SUBJECT",
        EXTRA_EMAIL = "android.intent.extra.EMAIL", EXTRA_CC = "android.intent.extra.CC", EXTRA_BCC = "android.intent.extra.BCC", EXTRA_STREAM = "android.intent.extra.STREAM",
        EXTRA_TITLE = "android.intent.extra.TITLE", EXTRA_INTENT = "android.intent.extra.INTENT", EXTRA_INITIAL_INTENTS = "android.intent.extra.INITIAL_INTENTS",
        EXTRA_CHOOSER_TARGETS = "android.intent.extra.CHOOSER_TARGETS", EXTRA_MIME_TYPES = "android.intent.extra.MIME_TYPES", EXTRA_ALLOW_MULTIPLE = "android.intent.extra.ALLOW_MULTIPLE",
        EXTRA_LOCAL_ONLY = "android.intent.extra.LOCAL_ONLY", EXTRA_PHONE_NUMBER = "android.intent.extra.PHONE_NUMBER", EXTRA_UID = "android.intent.extra.UID",
        EXTRA_PACKAGE_NAME = "android.intent.extra.PACKAGE_NAME", EXTRA_REFERRER = "android.intent.extra.REFERRER", EXTRA_KEY_EVENT = "android.intent.extra.KEY_EVENT",
        EXTRA_SHORTCUT_NAME = "android.intent.extra.shortcut.NAME", EXTRA_SHORTCUT_INTENT = "android.intent.extra.shortcut.INTENT", EXTRA_SHORTCUT_ICON = "android.intent.extra.shortcut.ICON",
        EXTRA_SHORTCUT_ICON_RESOURCE = "android.intent.extra.shortcut.ICON_RESOURCE", EXTRA_PROCESS_TEXT = "android.intent.extra.PROCESS_TEXT", EXTRA_PROCESS_TEXT_READONLY = "android.intent.extra.PROCESS_TEXT_READONLY",
        EXTRA_RETURN_RESULT = "android.intent.extra.RETURN_RESULT", EXTRA_REPLACING = "android.intent.extra.REPLACING", EXTRA_COMPONENT_NAME = "android.intent.extra.COMPONENT_NAME",
        EXTRA_ALARM_COUNT = "android.intent.extra.ALARM_COUNT", EXTRA_DOCK_STATE = "android.intent.extra.DOCK_STATE", EXTRA_ASSIST_PACKAGE = "android.intent.extra.ASSIST_PACKAGE",
        EXTRA_CHOSEN_COMPONENT = "android.intent.extra.CHOSEN_COMPONENT", EXTRA_EXCLUDE_COMPONENTS = "android.intent.extra.EXCLUDE_COMPONENTS", EXTRA_TIMEZONE = "time-zone",
        EXTRA_USER = "android.intent.extra.USER", EXTRA_QUIET_MODE = "android.intent.extra.QUIET_MODE", EXTRA_CONTENT_ANNOTATIONS = "android.intent.extra.CONTENT_ANNOTATIONS",
        EXTRA_AUTO_LAUNCH_SINGLE_CHOICE = "android.intent.extra.AUTO_LAUNCH_SINGLE_CHOICE", EXTRA_NOT_UNKNOWN_SOURCE = "android.intent.extra.NOT_UNKNOWN_SOURCE";
    public static final int FLAG_GRANT_READ_URI_PERMISSION = 1, FLAG_GRANT_WRITE_URI_PERMISSION = 2, FLAG_FROM_BACKGROUND = 4, FLAG_DEBUG_LOG_RESOLUTION = 8,
        FLAG_EXCLUDE_STOPPED_PACKAGES = 16, FLAG_INCLUDE_STOPPED_PACKAGES = 32, FLAG_GRANT_PERSISTABLE_URI_PERMISSION = 64, FLAG_GRANT_PREFIX_URI_PERMISSION = 128,
        FLAG_DIRECT_BOOT_AUTO = 256, FLAG_ACTIVITY_MATCH_EXTERNAL = 2048, FLAG_ACTIVITY_REQUIRE_NON_BROWSER = 1024, FLAG_ACTIVITY_REQUIRE_DEFAULT = 512,
        FLAG_ACTIVITY_LAUNCH_ADJACENT = 4096, FLAG_ACTIVITY_RETAIN_IN_RECENTS = 8192, FLAG_ACTIVITY_TASK_ON_HOME = 16384, FLAG_ACTIVITY_CLEAR_TASK = 32768,
        FLAG_ACTIVITY_NO_ANIMATION = 65536, FLAG_ACTIVITY_REORDER_TO_FRONT = 131072, FLAG_ACTIVITY_NO_USER_ACTION = 262144, FLAG_ACTIVITY_CLEAR_WHEN_TASK_RESET = 524288,
        FLAG_ACTIVITY_NEW_DOCUMENT = 524288, FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY = 1048576, FLAG_ACTIVITY_RESET_TASK_IF_NEEDED = 2097152, FLAG_ACTIVITY_BROUGHT_TO_FRONT = 4194304,
        FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS = 8388608, FLAG_ACTIVITY_PREVIOUS_IS_TOP = 16777216, FLAG_ACTIVITY_FORWARD_RESULT = 33554432, FLAG_ACTIVITY_CLEAR_TOP = 67108864,
        FLAG_ACTIVITY_MULTIPLE_TASK = 134217728, FLAG_ACTIVITY_NEW_TASK = 268435456, FLAG_ACTIVITY_SINGLE_TOP = 536870912, FLAG_ACTIVITY_NO_HISTORY = 1073741824,
        FLAG_RECEIVER_REGISTERED_ONLY = 1073741824, FLAG_RECEIVER_REPLACE_PENDING = 536870912, FLAG_RECEIVER_FOREGROUND = 268435456, FLAG_RECEIVER_NO_ABORT = 134217728,
        FLAG_RECEIVER_VISIBLE_TO_INSTANT_APPS = 2097152, URI_INTENT_SCHEME = 1, URI_ANDROID_APP_SCHEME = 2, URI_ALLOW_UNSAFE = 4, FILL_IN_ACTION = 1, FILL_IN_DATA = 2,
        FILL_IN_CATEGORIES = 4, FILL_IN_COMPONENT = 8, FILL_IN_PACKAGE = 16, FILL_IN_SOURCE_BOUNDS = 32, FILL_IN_SELECTOR = 64, FILL_IN_CLIP_DATA = 128;
    public static final android.os.Parcelable.Creator<Intent> CREATOR = new android.os.Parcelable.Creator<Intent>() { public Intent createFromParcel(android.os.Parcel p) { return (Intent) p.readValue(null); } public Intent[] newArray(int n) { return new Intent[n]; } };
    private String action, type, pkg, identifier;
    private Uri data;
    private Bundle extras;
    private ComponentName component;
    private int flags;
    private java.util.LinkedHashSet<String> categories;
    private ClipData clip;
    private Intent selector;
    private android.graphics.Rect sourceBounds;
    public Intent() {}
    public Intent(String action) { this.action = action; }
    public Intent(String action, Uri data) { this.action = action; this.data = data; }
    public Intent(Context c, Class<?> cls) { component = new ComponentName(c, cls); }
    public Intent(String action, Uri data, Context c, Class<?> cls) { this(action, data); component = new ComponentName(c, cls); }
    public Intent(Intent o) {
        action = o.action; data = o.data; type = o.type; pkg = o.pkg; extras = o.extras == null ? null : new Bundle(o.extras); flags = o.flags; component = o.component;
        categories = o.categories == null ? null : new java.util.LinkedHashSet<>(o.categories); clip = o.clip; identifier = o.identifier;
    }
    @Override public Object clone() { return new Intent(this); }
    public Intent cloneFilter() { Intent i = new Intent(action, data); i.type = type; i.component = component; i.pkg = pkg; i.categories = categories; return i; }
    public static Intent createChooser(Intent target, CharSequence title) { Intent i = new Intent(ACTION_CHOOSER); i.putExtra(EXTRA_INTENT, target); if (title != null) i.putExtra(EXTRA_TITLE, title); return i; }
    public static Intent createChooser(Intent target, CharSequence title, IntentSender s) { return createChooser(target, title); }
    public static Intent makeMainActivity(ComponentName c) { Intent i = new Intent(ACTION_MAIN); i.setComponent(c); i.addCategory(CATEGORY_LAUNCHER); return i; }
    public static Intent makeMainSelectorActivity(String action, String category) { Intent i = new Intent(action); i.addCategory(category); return i; }
    public static Intent makeRestartActivityTask(ComponentName c) { Intent i = makeMainActivity(c); i.addFlags(FLAG_ACTIVITY_NEW_TASK | FLAG_ACTIVITY_CLEAR_TASK); return i; }
    public static Intent parseUri(String uri, int flags) throws java.net.URISyntaxException {
        if (uri.startsWith("intent:") || uri.startsWith("#Intent;")) {
            Intent i = new Intent(ACTION_VIEW);
            int h = uri.indexOf("#Intent;");
            if (h > 7) i.setData(Uri.parse(uri.substring(0, h).replaceFirst("^intent:", "")));
            if (h >= 0) for (String part : uri.substring(h + 8).split(";")) {
                int eq = part.indexOf('='); if (eq < 0) continue; String k = part.substring(0, eq), v = Uri.decode(part.substring(eq + 1));
                if (k.equals("action")) i.setAction(v); else if (k.equals("category")) i.addCategory(v); else if (k.equals("type")) i.type = v; else if (k.equals("package")) i.pkg = v;
                else if (k.equals("component")) i.setComponent(ComponentName.unflattenFromString(v)); else if (k.equals("scheme") && i.data != null) i.data = Uri.parse(v + ":" + i.data);
                else if (k.startsWith("S.")) i.putExtra(k.substring(2), v); else if (k.startsWith("i.")) i.putExtra(k.substring(2), Integer.parseInt(v)); else if (k.startsWith("B.")) i.putExtra(k.substring(2), Boolean.parseBoolean(v));
            }
            return i;
        }
        return new Intent(ACTION_VIEW, Uri.parse(uri));
    }
    public static Intent getIntent(String uri) throws java.net.URISyntaxException { return parseUri(uri, 0); }
    public static Intent getIntentOld(String uri) throws java.net.URISyntaxException { return parseUri(uri, 0); }
    public String toUri(int flags) { return data != null ? data.toString() : "#Intent;" + (action != null ? "action=" + action + ";" : "") + "end"; }
    public String getAction() { return action; }
    public Intent setAction(String a) { action = a; return this; }
    public Uri getData() { return data; }
    public Intent setData(Uri d) { data = d; type = null; return this; }
    public Intent setDataAndNormalize(Uri d) { return setData(d == null ? null : d.normalizeScheme()); }
    public String getDataString() { return data == null ? null : data.toString(); }
    public String getScheme() { return data == null ? null : data.getScheme(); }
    public String getType() { return type; }
    public Intent setType(String t) { data = null; type = t; return this; }
    public Intent setTypeAndNormalize(String t) { return setType(t == null ? null : t.toLowerCase(java.util.Locale.ROOT)); }
    public Intent setDataAndType(Uri d, String t) { data = d; type = t; return this; }
    public Intent setDataAndTypeAndNormalize(Uri d, String t) { return setDataAndType(d, t); }
    public String resolveType(Context c) { return type != null ? type : data != null && "content".equals(data.getScheme()) ? c.getContentResolver().getType(data) : null; }
    public String resolveType(ContentResolver r) { return type != null ? type : data != null && "content".equals(data.getScheme()) ? r.getType(data) : null; }
    public String resolveTypeIfNeeded(ContentResolver r) { return component != null ? type : resolveType(r); }
    public String getIdentifier() { return identifier; }
    public Intent setIdentifier(String id) { identifier = id; return this; }
    public Intent addFlags(int f) { flags |= f; return this; }
    public Intent setFlags(int f) { flags = f; return this; }
    public void removeFlags(int f) { flags &= ~f; }
    public int getFlags() { return flags; }
    public boolean hasCategory(String c) { return categories != null && categories.contains(c); }
    public java.util.Set<String> getCategories() { return categories; }
    public Intent addCategory(String c) { if (categories == null) categories = new java.util.LinkedHashSet<>(); categories.add(c); return this; }
    public void removeCategory(String c) { if (categories != null) categories.remove(c); }
    public Intent getSelector() { return selector; }
    public void setSelector(Intent s) { selector = s; }
    public ClipData getClipData() { return clip; }
    public void setClipData(ClipData c) { clip = c; }
    public android.graphics.Rect getSourceBounds() { return sourceBounds; }
    public void setSourceBounds(android.graphics.Rect r) { sourceBounds = r; }
    public String getPackage() { return pkg; }
    public Intent setPackage(String p) { pkg = p; return this; }
    public Intent setClassName(String pkgName, String cls) { component = new ComponentName(pkgName, cls); return this; }
    public Intent setClassName(Context c, String cls) { component = new ComponentName(c, cls); return this; }
    public Intent setClass(Context c, Class<?> cls) { component = new ComponentName(c, cls); return this; }
    public Intent setComponent(ComponentName c) { component = c; return this; }
    public ComponentName getComponent() { return component; }
    public ComponentName resolveActivity(android.content.pm.PackageManager pm) { if (component != null) return component; android.content.pm.ResolveInfo r = pm.resolveActivity(this, 0); return r == null ? null : new ComponentName(r.activityInfo.packageName, r.activityInfo.name); }
    public android.content.pm.ActivityInfo resolveActivityInfo(android.content.pm.PackageManager pm, int flags) { android.content.pm.ResolveInfo r = pm.resolveActivity(this, flags); return r == null ? null : r.activityInfo; }
    public Bundle getExtras() { return extras == null ? null : new Bundle(extras); }
    private Bundle ex() { if (extras == null) extras = new Bundle(); return extras; }
    public boolean hasExtra(String k) { return extras != null && extras.containsKey(k); }
    public boolean hasFileDescriptors() { return false; }
    public void setExtrasClassLoader(ClassLoader l) {}
    public Intent putExtra(String k, boolean v) { ex().putBoolean(k, v); return this; }
    public Intent putExtra(String k, byte v) { ex().putByte(k, v); return this; }
    public Intent putExtra(String k, char v) { ex().putChar(k, v); return this; }
    public Intent putExtra(String k, short v) { ex().putShort(k, v); return this; }
    public Intent putExtra(String k, int v) { ex().putInt(k, v); return this; }
    public Intent putExtra(String k, long v) { ex().putLong(k, v); return this; }
    public Intent putExtra(String k, float v) { ex().putFloat(k, v); return this; }
    public Intent putExtra(String k, double v) { ex().putDouble(k, v); return this; }
    public Intent putExtra(String k, String v) { ex().putString(k, v); return this; }
    public Intent putExtra(String k, CharSequence v) { ex().putCharSequence(k, v); return this; }
    public Intent putExtra(String k, android.os.Parcelable v) { ex().putParcelable(k, v); return this; }
    public Intent putExtra(String k, android.os.Parcelable[] v) { ex().putParcelableArray(k, v); return this; }
    public Intent putParcelableArrayListExtra(String k, java.util.ArrayList<? extends android.os.Parcelable> v) { ex().putParcelableArrayList(k, v); return this; }
    public Intent putIntegerArrayListExtra(String k, java.util.ArrayList<Integer> v) { ex().putIntegerArrayList(k, v); return this; }
    public Intent putStringArrayListExtra(String k, java.util.ArrayList<String> v) { ex().putStringArrayList(k, v); return this; }
    public Intent putCharSequenceArrayListExtra(String k, java.util.ArrayList<CharSequence> v) { ex().putCharSequenceArrayList(k, v); return this; }
    public Intent putExtra(String k, java.io.Serializable v) { ex().putSerializable(k, v); return this; }
    public Intent putExtra(String k, boolean[] v) { ex().putBooleanArray(k, v); return this; }
    public Intent putExtra(String k, byte[] v) { ex().putByteArray(k, v); return this; }
    public Intent putExtra(String k, short[] v) { ex().putShortArray(k, v); return this; }
    public Intent putExtra(String k, char[] v) { ex().putCharArray(k, v); return this; }
    public Intent putExtra(String k, int[] v) { ex().putIntArray(k, v); return this; }
    public Intent putExtra(String k, long[] v) { ex().putLongArray(k, v); return this; }
    public Intent putExtra(String k, float[] v) { ex().putFloatArray(k, v); return this; }
    public Intent putExtra(String k, double[] v) { ex().putDoubleArray(k, v); return this; }
    public Intent putExtra(String k, String[] v) { ex().putStringArray(k, v); return this; }
    public Intent putExtra(String k, CharSequence[] v) { ex().putCharSequenceArray(k, v); return this; }
    public Intent putExtra(String k, Bundle v) { ex().putBundle(k, v); return this; }
    public Intent putExtras(Intent src) { if (src.extras != null) ex().putAll(src.extras); return this; }
    public Intent putExtras(Bundle b) { if (b != null) ex().putAll(b); return this; }
    public Intent replaceExtras(Intent src) { extras = src.extras == null ? null : new Bundle(src.extras); return this; }
    public Intent replaceExtras(Bundle b) { extras = b == null ? null : new Bundle(b); return this; }
    public void removeExtra(String k) { if (extras != null) extras.remove(k); }
    public boolean getBooleanExtra(String k, boolean d) { return extras == null ? d : extras.getBoolean(k, d); }
    public byte getByteExtra(String k, byte d) { return extras == null ? d : extras.getByte(k, d); }
    public short getShortExtra(String k, short d) { return extras == null ? d : extras.getShort(k, d); }
    public char getCharExtra(String k, char d) { return extras == null ? d : extras.getChar(k, d); }
    public int getIntExtra(String k, int d) { return extras == null ? d : extras.getInt(k, d); }
    public long getLongExtra(String k, long d) { return extras == null ? d : extras.getLong(k, d); }
    public float getFloatExtra(String k, float d) { return extras == null ? d : extras.getFloat(k, d); }
    public double getDoubleExtra(String k, double d) { return extras == null ? d : extras.getDouble(k, d); }
    public String getStringExtra(String k) { return extras == null ? null : extras.getString(k); }
    public CharSequence getCharSequenceExtra(String k) { return extras == null ? null : extras.getCharSequence(k); }
    public <T extends android.os.Parcelable> T getParcelableExtra(String k) { return extras == null ? null : extras.<T>getParcelable(k); }
    public <T> T getParcelableExtra(String k, Class<T> c) { return extras == null ? null : extras.getParcelable(k, c); }
    public android.os.Parcelable[] getParcelableArrayExtra(String k) { return extras == null ? null : extras.getParcelableArray(k); }
    public <T extends android.os.Parcelable> java.util.ArrayList<T> getParcelableArrayListExtra(String k) { return extras == null ? null : extras.<T>getParcelableArrayList(k); }
    public <T> java.util.ArrayList<T> getParcelableArrayListExtra(String k, Class<? extends T> c) { return extras == null ? null : extras.getParcelableArrayList(k, c); }
    public java.io.Serializable getSerializableExtra(String k) { return extras == null ? null : extras.getSerializable(k); }
    public <T extends java.io.Serializable> T getSerializableExtra(String k, Class<T> c) { return extras == null ? null : extras.getSerializable(k, c); }
    public java.util.ArrayList<Integer> getIntegerArrayListExtra(String k) { return extras == null ? null : extras.getIntegerArrayList(k); }
    public java.util.ArrayList<String> getStringArrayListExtra(String k) { return extras == null ? null : extras.getStringArrayList(k); }
    public java.util.ArrayList<CharSequence> getCharSequenceArrayListExtra(String k) { return extras == null ? null : extras.getCharSequenceArrayList(k); }
    public boolean[] getBooleanArrayExtra(String k) { return extras == null ? null : extras.getBooleanArray(k); }
    public byte[] getByteArrayExtra(String k) { return extras == null ? null : extras.getByteArray(k); }
    public short[] getShortArrayExtra(String k) { return extras == null ? null : extras.getShortArray(k); }
    public char[] getCharArrayExtra(String k) { return extras == null ? null : extras.getCharArray(k); }
    public int[] getIntArrayExtra(String k) { return extras == null ? null : extras.getIntArray(k); }
    public long[] getLongArrayExtra(String k) { return extras == null ? null : extras.getLongArray(k); }
    public float[] getFloatArrayExtra(String k) { return extras == null ? null : extras.getFloatArray(k); }
    public double[] getDoubleArrayExtra(String k) { return extras == null ? null : extras.getDoubleArray(k); }
    public String[] getStringArrayExtra(String k) { return extras == null ? null : extras.getStringArray(k); }
    public CharSequence[] getCharSequenceArrayExtra(String k) { return extras == null ? null : extras.getCharSequenceArray(k); }
    public Bundle getBundleExtra(String k) { return extras == null ? null : extras.getBundle(k); }
    public int fillIn(Intent o, int flags) { if (o.action != null && action == null) action = o.action; if (o.data != null && data == null) data = o.data; if (o.extras != null) ex().putAll(o.extras); return 0; }
    public boolean filterEquals(Intent o) { return o != null && java.util.Objects.equals(action, o.action) && java.util.Objects.equals(data, o.data) && java.util.Objects.equals(type, o.type) && java.util.Objects.equals(component, o.component) && java.util.Objects.equals(categories, o.categories); }
    public int filterHashCode() { return java.util.Objects.hash(action, data, type, component, categories); }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) { p.writeValue(this); }
    @Override public String toString() { return "Intent { " + (action != null ? "act=" + action + " " : "") + (categories != null ? "cat=" + categories + " " : "") + (data != null ? "dat=" + data + " " : "") + (type != null ? "typ=" + type + " " : "") + (component != null ? "cmp=" + component.flattenToShortString() + " " : "") + (extras != null ? "(has extras) " : "") + "}"; }
}
