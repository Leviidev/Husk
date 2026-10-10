// Started from tools/compat/genstubs.py's signatures; the default preferences are real.
package android.preference;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class PreferenceManager {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final java.lang.String KEY_HAS_SET_DEFAULT_VALUES = "_has_set_default_values";
    public static final java.lang.String METADATA_KEY_PREFERENCES = "android.preference";
    public PreferenceManager(android.app.Activity p0, int p1) {}
    public static android.content.SharedPreferences getDefaultSharedPreferences(android.content.Context p0) { return p0.getSharedPreferences(getDefaultSharedPreferencesName(p0), android.content.Context.MODE_PRIVATE); }
    public static java.lang.String getDefaultSharedPreferencesName(android.content.Context p0) { return p0.getPackageName() + "_preferences"; }
    public static void setDefaultValues(android.content.Context p0, int p1, boolean p2) {}
    public static void setDefaultValues(android.content.Context p0, java.lang.String p1, int p2, int p3, boolean p4) {}
    public android.preference.PreferenceScreen createPreferenceScreen(android.content.Context p0) { return null; }
    public android.preference.Preference findPreference(java.lang.CharSequence p0) { return null; }
    public android.preference.PreferenceDataStore getPreferenceDataStore() { return (android.preference.PreferenceDataStore) huskProps.get("PreferenceDataStore"); }
    public android.content.SharedPreferences getSharedPreferences() { return (android.content.SharedPreferences) huskProps.get("SharedPreferences"); }
    public int getSharedPreferencesMode() { return (huskProps.get("SharedPreferencesMode") instanceof Integer ? (Integer) huskProps.get("SharedPreferencesMode") : 0); }
    public java.lang.String getSharedPreferencesName() { return (java.lang.String) huskProps.get("SharedPreferencesName"); }
    public android.preference.PreferenceScreen inflateFromResource(android.content.Context p0, int p1, android.preference.PreferenceScreen p2) { return null; }
    public boolean isStorageCredentialProtected() { return (huskProps.get("StorageCredentialProtected") instanceof Boolean ? (Boolean) huskProps.get("StorageCredentialProtected") : false); }
    public boolean isStorageDefault() { return (huskProps.get("StorageDefault") instanceof Boolean ? (Boolean) huskProps.get("StorageDefault") : false); }
    public boolean isStorageDeviceProtected() { return (huskProps.get("StorageDeviceProtected") instanceof Boolean ? (Boolean) huskProps.get("StorageDeviceProtected") : false); }
    public void registerOnActivityStopListener(android.preference.PreferenceManager.OnActivityStopListener p0) {}
    public void setPreferenceDataStore(android.preference.PreferenceDataStore p0) { huskProps.put("PreferenceDataStore", p0); }
    public void setSharedPreferencesMode(int p0) { huskProps.put("SharedPreferencesMode", Integer.valueOf(p0)); }
    public void setSharedPreferencesName(java.lang.String p0) { huskProps.put("SharedPreferencesName", p0); }
    public void setStorageCredentialProtected() {}
    public void setStorageDefault() {}
    public void setStorageDeviceProtected() {}
    public void unregisterOnActivityStopListener(android.preference.PreferenceManager.OnActivityStopListener p0) {}
    PreferenceManager() { this((android.app.Activity) null, (int) 0); }
    public interface OnActivityDestroyListener {
        void onActivityDestroy();
    }
    public interface OnActivityResultListener {
        boolean onActivityResult(int p0, int p1, android.content.Intent p2);
    }
    public interface OnActivityStopListener {
        void onActivityStop();
    }
    public interface OnPreferenceTreeClickListener {
        boolean onPreferenceTreeClick(android.preference.PreferenceScreen p0, android.preference.Preference p1);
    }
}
