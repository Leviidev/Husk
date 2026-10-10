package android.content;

import java.util.ArrayList;
import java.util.Iterator;

public class IntentFilter implements android.os.Parcelable {
    public static final int SYSTEM_HIGH_PRIORITY = 1000, SYSTEM_LOW_PRIORITY = -1000, MATCH_CATEGORY_MASK = 0xfff0000, MATCH_ADJUSTMENT_MASK = 0xffff,
        MATCH_CATEGORY_EMPTY = 0x100000, MATCH_CATEGORY_SCHEME = 0x200000, MATCH_CATEGORY_HOST = 0x300000, MATCH_CATEGORY_PATH = 0x500000,
        MATCH_CATEGORY_TYPE = 0x600000, NO_MATCH_TYPE = -1, NO_MATCH_DATA = -2, NO_MATCH_ACTION = -3, NO_MATCH_CATEGORY = -4;
    public static class MalformedMimeTypeException extends Exception { public MalformedMimeTypeException() {} public MalformedMimeTypeException(String s) { super(s); } }
    private final ArrayList<String> mActions = new ArrayList<>(), mCategories = new ArrayList<>(), mSchemes = new ArrayList<>(), mTypes = new ArrayList<>();
    private int mPriority;
    public IntentFilter() {}
    public IntentFilter(String action) { addAction(action); }
    public IntentFilter(String action, String type) throws MalformedMimeTypeException { addAction(action); addDataType(type); }
    public IntentFilter(IntentFilter o) { mActions.addAll(o.mActions); mCategories.addAll(o.mCategories); mSchemes.addAll(o.mSchemes); mTypes.addAll(o.mTypes); mPriority = o.mPriority; }
    public static IntentFilter create(String action, String type) { try { return new IntentFilter(action, type); } catch (MalformedMimeTypeException e) { throw new RuntimeException(e); } }
    public final void setPriority(int p) { mPriority = p; }
    public final int getPriority() { return mPriority; }
    public final void addAction(String a) { if (a != null && !mActions.contains(a)) mActions.add(a); }
    public final int countActions() { return mActions.size(); }
    public final String getAction(int i) { return mActions.get(i); }
    public final boolean hasAction(String a) { return a != null && mActions.contains(a); }
    public final boolean matchAction(String a) { return hasAction(a); }
    public final Iterator<String> actionsIterator() { return mActions.iterator(); }
    public final void addCategory(String c) { if (!mCategories.contains(c)) mCategories.add(c); }
    public final int countCategories() { return mCategories.size(); }
    public final String getCategory(int i) { return mCategories.get(i); }
    public final boolean hasCategory(String c) { return mCategories.contains(c); }
    public final Iterator<String> categoriesIterator() { return mCategories.iterator(); }
    public final void addDataScheme(String s) { if (!mSchemes.contains(s)) mSchemes.add(s); }
    public final int countDataSchemes() { return mSchemes.size(); }
    public final String getDataScheme(int i) { return mSchemes.get(i); }
    public final boolean hasDataScheme(String s) { return mSchemes.contains(s); }
    public final void addDataType(String t) throws MalformedMimeTypeException { mTypes.add(t); }
    public final void addDataAuthority(String host, String port) {}
    public final void addDataPath(String path, int type) {}
    public final void addDataSchemeSpecificPart(String ssp, int type) {}
    public final int countDataTypes() { return mTypes.size(); }
    public final boolean hasDataType(String t) { return mTypes.contains(t); }
    public final int match(String action, String type, String scheme, android.net.Uri data, java.util.Set<String> categories, String tag) { return hasAction(action) ? MATCH_CATEGORY_EMPTY : NO_MATCH_ACTION; }
    public final int match(ContentResolver r, Intent i, boolean resolve, String tag) { return match(i.getAction(), i.getType(), i.getScheme(), i.getData(), i.getCategories(), tag); }
    public int describeContents() { return 0; }
}
