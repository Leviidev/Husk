package husk;

import android.content.res.XmlBlock;
import android.os.Bundle;
import android.util.TypedValue;
import java.util.ArrayList;
import org.xmlpull.v1.XmlPullParser;

/** The app's AndroidManifest.xml, read once: the application and its components, as PackageManager would report them. */
public final class Manifest {
    public static final class Component {
        public String name, kind, authorities, process, permission, parentActivity;
        public int theme, label, icon, screenOrientation = -1, configChanges, launchMode, windowSoftInputMode, uiOptions;
        public boolean exported, enabled = true, grantUriPermissions;
        public Bundle metaData;
        public final ArrayList<Filter> filters = new ArrayList<>();
    }
    public static final class Filter {
        public final ArrayList<String> actions = new ArrayList<>(), categories = new ArrayList<>(), schemes = new ArrayList<>(), mimeTypes = new ArrayList<>(), hosts = new ArrayList<>();
    }
    public static String packageName, applicationClass, versionName, appComponentFactory;
    public static int versionCode, minSdk = 21, targetSdk = 34, appTheme, appLabel, appIcon, appFlags;
    public static boolean debuggable, largeHeap, hardwareAccelerated = true, extractNativeLibs = true;
    public static Bundle appMetaData = new Bundle();
    public static final ArrayList<Component> activities = new ArrayList<>(), services = new ArrayList<>(), receivers = new ArrayList<>(), providers = new ArrayList<>();
    public static final ArrayList<String> permissions = new ArrayList<>(), libraries = new ArrayList<>();
    private static boolean sRead;

    private static final String NS = "http://schemas.android.com/apk/res/android";
    private static String full(String n) {
        if (n == null) return null;
        if (n.startsWith(".")) return packageName + n;
        if (n.indexOf('.') < 0) return packageName + "." + n;
        return n;
    }
    public static synchronized void read() {
        if (sRead) return;
        sRead = true;
        byte[] d = Native.readApkFile(0, "AndroidManifest.xml");
        if (d == null) return;
        try {
            XmlBlock.Parser p = new XmlBlock(d, "AndroidManifest.xml").newParser();
            Component cur = null; Filter filter = null;
            boolean inApp = false;          /* components are only those under <application> (<queries> names packages, intents and providers too) */
            int t;
            TypedValue v = new TypedValue();
            while ((t = p.next()) != XmlPullParser.END_DOCUMENT) {
                if (t == XmlPullParser.END_TAG) {
                    String n = p.getName();
                    if ("intent-filter".equals(n)) filter = null;
                    else if ("application".equals(n)) inApp = false;
                    else if ("activity".equals(n) || "activity-alias".equals(n) || "service".equals(n) || "receiver".equals(n) || "provider".equals(n)) cur = null;
                    continue;
                }
                if (t != XmlPullParser.START_TAG) continue;
                String tag = p.getName();
                switch (tag) {
                case "manifest":
                    packageName = p.getAttributeValue(null, "package");
                    versionCode = p.getAttributeIntValue(NS, "versionCode", 1);
                    versionName = p.getAttributeValue(NS, "versionName");
                    break;
                case "uses-sdk":
                    minSdk = p.getAttributeIntValue(NS, "minSdkVersion", 21);
                    targetSdk = p.getAttributeIntValue(NS, "targetSdkVersion", minSdk);
                    break;
                case "uses-permission": permissions.add(p.getAttributeValue(NS, "name")); break;
                case "uses-library": libraries.add(p.getAttributeValue(NS, "name")); break;
                case "application":
                    inApp = true;
                    applicationClass = full(p.getAttributeValue(NS, "name"));
                    appTheme = p.getAttributeResourceValue(NS, "theme", 0);
                    appLabel = p.getAttributeResourceValue(NS, "label", 0);
                    appIcon = p.getAttributeResourceValue(NS, "icon", 0);
                    debuggable = p.getAttributeBooleanValue(NS, "debuggable", false);
                    largeHeap = p.getAttributeBooleanValue(NS, "largeHeap", false);
                    hardwareAccelerated = p.getAttributeBooleanValue(NS, "hardwareAccelerated", true);
                    appComponentFactory = full(p.getAttributeValue(NS, "appComponentFactory"));
                    break;
                case "activity": case "activity-alias": case "service": case "receiver": case "provider": {
                    if (!inApp) break;
                    cur = new Component();
                    cur.kind = tag;
                    cur.name = full(p.getAttributeValue(NS, "name"));
                    if ("activity-alias".equals(tag)) {
                        String target = full(p.getAttributeValue(NS, "targetActivity"));
                        for (Component a : activities) if (a.name.equals(target)) { cur.theme = a.theme; cur.screenOrientation = a.screenOrientation; cur.configChanges = a.configChanges; }
                        cur.parentActivity = target;
                    }
                    cur.theme = p.getAttributeResourceValue(NS, "theme", cur.theme);
                    cur.label = p.getAttributeResourceValue(NS, "label", 0);
                    cur.icon = p.getAttributeResourceValue(NS, "icon", 0);
                    cur.screenOrientation = p.getAttributeIntValue(NS, "screenOrientation", cur.screenOrientation);
                    cur.configChanges = p.getAttributeIntValue(NS, "configChanges", cur.configChanges);
                    cur.launchMode = p.getAttributeIntValue(NS, "launchMode", 0);
                    cur.windowSoftInputMode = p.getAttributeIntValue(NS, "windowSoftInputMode", 0);
                    cur.exported = p.getAttributeBooleanValue(NS, "exported", false);
                    cur.enabled = p.getAttributeBooleanValue(NS, "enabled", true);
                    cur.authorities = p.getAttributeValue(NS, "authorities");
                    cur.grantUriPermissions = p.getAttributeBooleanValue(NS, "grantUriPermissions", false);
                    cur.process = p.getAttributeValue(NS, "process");
                    cur.permission = p.getAttributeValue(NS, "permission");
                    if (cur.parentActivity == null) cur.parentActivity = full(p.getAttributeValue(NS, "parentActivityName"));
                    if ("service".equals(tag)) services.add(cur);
                    else if ("receiver".equals(tag)) receivers.add(cur);
                    else if ("provider".equals(tag)) providers.add(cur);
                    else activities.add(cur);
                    break;
                }
                case "intent-filter": if (cur != null) { filter = new Filter(); cur.filters.add(filter); } break;
                case "action": if (filter != null) filter.actions.add(p.getAttributeValue(NS, "name")); break;
                case "category": if (filter != null) filter.categories.add(p.getAttributeValue(NS, "name")); break;
                case "data":
                    if (filter != null) {
                        String s = attrOrRef(p, "scheme"), h = attrOrRef(p, "host"), m = attrOrRef(p, "mimeType");
                        if (s != null) filter.schemes.add(s); if (h != null) filter.hosts.add(h); if (m != null) filter.mimeTypes.add(m);
                    }
                    break;
                case "meta-data": {
                    String name = p.getAttributeValue(NS, "name");
                    Bundle target = cur != null ? (cur.metaData != null ? cur.metaData : (cur.metaData = new Bundle())) : appMetaData;
                    int vi = p.indexOf(NS, "value"), ri = p.indexOf(NS, "resource");
                    if (name == null) break;
                    if (ri >= 0) target.putInt(name, p.getAttributeResourceValue(ri, 0));
                    else if (vi >= 0) {
                        p.getAttributeTypedValue(vi, v);
                        // a reference (android:value="@integer/google_play_services_version") is resolved, as Android's parser does
                        if (v.type == TypedValue.TYPE_REFERENCE && v.data != 0) {
                            try { android.content.res.Resources r = husk.AppRunner.application() != null ? husk.AppRunner.application().getResources() : android.content.res.Resources.getSystem(); r.getValue(v.data, v, true); }
                            catch (RuntimeException e) { /* left as the id */ }
                        }
                        if (v.type == TypedValue.TYPE_STRING) target.putString(name, String.valueOf(v.string));
                        else if (v.type == TypedValue.TYPE_INT_BOOLEAN) target.putBoolean(name, v.data != 0);
                        else if (v.type == TypedValue.TYPE_FLOAT) target.putFloat(name, v.getFloat());
                        else if (v.type == TypedValue.TYPE_REFERENCE) target.putInt(name, v.data);
                        else target.putInt(name, v.data);
                    }
                    break;
                }
                }
            }
        } catch (Exception e) { android.util.Log.e("Husk", "the manifest could not be read", e); }
    }
    public static Component activity(String cls) { read(); for (Component c : activities) if (c.name.equals(cls)) return c; return null; }
    public static Component launcher() {
        read();
        for (Component c : activities) for (Filter f : c.filters) if (f.categories.contains("android.intent.category.LAUNCHER") && f.actions.contains("android.intent.action.MAIN")) return c;
        for (Component c : activities) for (Filter f : c.filters) if (f.categories.contains("android.intent.category.LAUNCHER")) return c;
        return activities.isEmpty() ? null : activities.get(0);
    }
    /** The services (or receivers) of this app an intent names or whose filters take it. */
    public static java.util.List<Component> match(java.util.List<Component> list, android.content.Intent i) {
        read();
        java.util.ArrayList<Component> out = new java.util.ArrayList<>();
        android.content.ComponentName cn = i.getComponent();
        if (cn != null) {
            if (cn.getPackageName() != null && !cn.getPackageName().equals(husk.Native.packageName())) return out;
            for (Component c : list) if (c.name.equals(cn.getClassName())) { out.add(c); return out; }
            return out;
        }
        String action = i.getAction();
        if (action == null) return out;
        if (i.getPackage() != null && !i.getPackage().equals(husk.Native.packageName())) return out;
        for (Component c : list) for (Filter f : c.filters) if (matches(f, i)) { out.add(c); break; }
        return out;
    }
    /* an attribute that may name a string resource (Shazam's <data android:scheme="@string/...">): kept as "@<id>", read when used */
    private static String attrOrRef(XmlBlock.Parser p, String name) {
        int ref = p.getAttributeResourceValue(NS, name, 0);
        if (ref != 0) return "@" + ref;
        return p.getAttributeValue(NS, name);
    }
    private static String val(String v) {
        if (v == null || !v.startsWith("@")) return v;
        try { return husk.ContextImpl.app().getResources().getString(Integer.parseInt(v.substring(1))); } catch (Throwable t) { return v; }
    }
    private static boolean hasVal(java.util.List<String> list, String x) { for (String v : list) if (val(v).equals(x)) return true; return false; }
    /** Android's intent-filter match: the action (an intent without one passes any filter that lists one), then the data --
     *  an intent with a URI needs a filter naming its scheme (and host, when the filter names hosts), one without needs a
     *  filter without schemes. */
    static boolean matches(Filter f, android.content.Intent i) {
        String action = i.getAction();
        if (action != null ? !f.actions.contains(action) : f.actions.isEmpty()) return false;
        android.net.Uri d = i.getData();
        if (d == null) return f.schemes.isEmpty() || i.getType() != null && !f.mimeTypes.isEmpty();
        if (f.schemes.isEmpty() || !hasVal(f.schemes, d.getScheme())) return false;
        if (!f.hosts.isEmpty()) {
            String h = d.getHost();
            boolean hit = false;
            for (String fv : f.hosts) { String fh = val(fv); if (fh.equals(h) || "*".equals(fh) || (fh.startsWith("*") && h != null && h.endsWith(fh.substring(1)))) hit = true; }
            if (!hit) return false;
        }
        return true;
    }
    /** The activity an explicit or implicit intent would open in this app, or null. */
    public static Component resolve(android.content.Intent i) {
        read();
        android.content.ComponentName cn = i.getComponent();
        if (cn != null) { Component c = activity(cn.getClassName()); if (c != null) return c; for (Component a : activities) if (a.name.endsWith(cn.getClassName())) return a; return null; }
        for (Component c : activities) for (Filter f : c.filters) if (matches(f, i)) return c;
        return null;
    }
}
