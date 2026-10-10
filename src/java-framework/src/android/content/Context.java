package android.content;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.content.res.ColorStateList;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import java.io.File;

/**
 * A context. Husk's base context is husk.ContextImpl (the app's resources, files and services); wrappers delegate to it as on Android.
 * The methods here that are not abstract on Android are written in terms of the ones a wrapper overrides.
 */
public abstract class Context {
    public static final int MODE_PRIVATE = 0, MODE_APPEND = 0x8000, MODE_WORLD_READABLE = 1, MODE_WORLD_WRITEABLE = 2, MODE_MULTI_PROCESS = 4,
        MODE_ENABLE_WRITE_AHEAD_LOGGING = 8, MODE_NO_LOCALIZED_COLLATORS = 16;
    public static final int BIND_AUTO_CREATE = 1, BIND_DEBUG_UNBIND = 2, BIND_NOT_FOREGROUND = 4, BIND_ABOVE_CLIENT = 8, BIND_IMPORTANT = 64,
        BIND_WAIVE_PRIORITY = 32, BIND_ADJUST_WITH_ACTIVITY = 128, BIND_INCLUDE_CAPABILITIES = 4096;
    public static final int CONTEXT_INCLUDE_CODE = 1, CONTEXT_IGNORE_SECURITY = 2, CONTEXT_RESTRICTED = 4, RECEIVER_EXPORTED = 2, RECEIVER_NOT_EXPORTED = 4,
        RECEIVER_VISIBLE_TO_INSTANT_APPS = 1;
    public static final String AUDIO_SERVICE = "audio", VIBRATOR_SERVICE = "vibrator", WINDOW_SERVICE = "window", CLIPBOARD_SERVICE = "clipboard",
        INPUT_METHOD_SERVICE = "input_method", SENSOR_SERVICE = "sensor", POWER_SERVICE = "power", ACTIVITY_SERVICE = "activity",
        CONNECTIVITY_SERVICE = "connectivity", WIFI_SERVICE = "wifi", LOCATION_SERVICE = "location", NOTIFICATION_SERVICE = "notification",
        LAYOUT_INFLATER_SERVICE = "layout_inflater", DISPLAY_SERVICE = "display", UI_MODE_SERVICE = "uimode", KEYGUARD_SERVICE = "keyguard",
        TELEPHONY_SERVICE = "phone", STORAGE_SERVICE = "storage", INPUT_SERVICE = "input", VIBRATOR_MANAGER_SERVICE = "vibrator_manager",
        ALARM_SERVICE = "alarm", ACCESSIBILITY_SERVICE = "accessibility", ACCOUNT_SERVICE = "account", DOWNLOAD_SERVICE = "download",
        JOB_SCHEDULER_SERVICE = "jobscheduler", USER_SERVICE = "user", APP_OPS_SERVICE = "appops", BATTERY_SERVICE = "batterymanager",
        CAMERA_SERVICE = "camera", SEARCH_SERVICE = "search", TEXT_SERVICES_MANAGER_SERVICE = "textservices", WALLPAPER_SERVICE = "wallpaper",
        AUTOFILL_MANAGER_SERVICE = "autofill", CAPTIONING_SERVICE = "captioning", MEDIA_SESSION_SERVICE = "media_session",
        TEXT_CLASSIFICATION_SERVICE = "textclassification", LOCALE_SERVICE = "locale", USAGE_STATS_SERVICE = "usagestats", BLUETOOTH_SERVICE = "bluetooth",
        SHORTCUT_SERVICE = "shortcut", APPWIDGET_SERVICE = "appwidget", NSD_SERVICE = "servicediscovery", DEVICE_POLICY_SERVICE = "device_policy",
        CONTENT_CAPTURE_MANAGER_SERVICE = "content_capture", MEDIA_ROUTER_SERVICE = "media_router", FINGERPRINT_SERVICE = "fingerprint",
        BIOMETRIC_SERVICE = "biometric", HARDWARE_PROPERTIES_SERVICE = "hardware_properties", NETWORK_STATS_SERVICE = "netstats",
        CLIPBOARD_SERVICE_ = "clipboard", GAME_SERVICE = "game", CROSS_PROFILE_APPS_SERVICE = "crossprofileapps", DISPLAY_HASH_SERVICE = "display_hash";

    public abstract AssetManager getAssets();
    public abstract Resources getResources();
    public abstract PackageManager getPackageManager();
    public abstract ContentResolver getContentResolver();
    public abstract Looper getMainLooper();
    public abstract Context getApplicationContext();
    public abstract void setTheme(int resid);
    public abstract Resources.Theme getTheme();
    public abstract ClassLoader getClassLoader();
    public abstract String getPackageName();
    public abstract ApplicationInfo getApplicationInfo();
    public abstract String getPackageResourcePath();
    public abstract String getPackageCodePath();
    public abstract SharedPreferences getSharedPreferences(String name, int mode);
    public abstract Object getSystemService(String name);
    public abstract File getFilesDir();
    public abstract File getCacheDir();
    public abstract File getDataDir();
    public abstract File getDir(String name, int mode);
    public abstract File getExternalFilesDir(String type);
    public abstract void startActivity(Intent intent);
    public abstract void sendBroadcast(Intent intent);
    public abstract Intent registerReceiver(BroadcastReceiver r, IntentFilter f);
    public abstract void unregisterReceiver(BroadcastReceiver r);
    public abstract ComponentName startService(Intent service);
    public abstract boolean stopService(Intent service);
    public abstract boolean bindService(Intent service, ServiceConnection conn, int flags);
    public abstract void unbindService(ServiceConnection conn);
    public abstract int checkPermission(String perm, int pid, int uid);
    public abstract Context createPackageContext(String pkg, int flags) throws PackageManager.NameNotFoundException;
    public abstract Context createConfigurationContext(Configuration c);

    // ---- in terms of the above
    public final CharSequence getText(int id) { return getResources().getText(id); }
    public final String getString(int id) { return getResources().getString(id); }
    public final String getString(int id, Object... args) { return getResources().getString(id, args); }
    public final int getColor(int id) { return getResources().getColor(id, getTheme()); }
    public final Drawable getDrawable(int id) { return getResources().getDrawable(id, getTheme()); }
    public final ColorStateList getColorStateList(int id) { return getResources().getColorStateList(id, getTheme()); }
    public final TypedArray obtainStyledAttributes(int[] attrs) { return getTheme().obtainStyledAttributes(attrs); }
    public final TypedArray obtainStyledAttributes(int resid, int[] attrs) { return getTheme().obtainStyledAttributes(resid, attrs); }
    public final TypedArray obtainStyledAttributes(AttributeSet set, int[] attrs) { return getTheme().obtainStyledAttributes(set, attrs, 0, 0); }
    public final TypedArray obtainStyledAttributes(AttributeSet set, int[] attrs, int defStyleAttr, int defStyleRes) { return getTheme().obtainStyledAttributes(set, attrs, defStyleAttr, defStyleRes); }
    @SuppressWarnings("unchecked")
    public final <T> T getSystemService(Class<T> c) { String n = getSystemServiceName(c); return n == null ? null : (T) getSystemService(n); }
    public String getSystemServiceName(Class<?> c) { return husk.ContextImpl.serviceName(c); }
    public String getOpPackageName() { return getPackageName(); }
    public String getAttributionTag() { return null; }
    public String getBasePackageName() { return getPackageName(); }
    public java.util.concurrent.Executor getMainExecutor() { final Handler h = new Handler(getMainLooper()); return h::post; }
    public File getCodeCacheDir() { return sub("code_cache"); }
    public File getNoBackupFilesDir() { return sub("no_backup"); }
    public File getObbDir() { return getExternalFilesDir(null).getParentFile(); }
    public File[] getObbDirs() { return new File[] { getObbDir() }; }
    public File getExternalCacheDir() { File f = new File(getExternalFilesDir(null).getParentFile(), "cache"); f.mkdirs(); return f; }
    public File[] getExternalCacheDirs() { return new File[] { getExternalCacheDir() }; }
    public File[] getExternalFilesDirs(String type) { return new File[] { getExternalFilesDir(type) }; }
    public File[] getExternalMediaDirs() { return new File[] { getExternalFilesDir(null) }; }
    private File sub(String n) { File f = new File(getDataDir(), n); f.mkdirs(); return f; }
    public File getDatabasePath(String name) { if (name.startsWith("/")) return new File(name); File d = sub("databases"); return new File(d, name); }
    public String[] databaseList() { String[] l = sub("databases").list(); return l == null ? new String[0] : l; }
    public boolean deleteDatabase(String name) { return android.database.sqlite.SQLiteDatabase.deleteDatabase(getDatabasePath(name)); }
    public android.database.sqlite.SQLiteDatabase openOrCreateDatabase(String name, int mode, android.database.sqlite.SQLiteDatabase.CursorFactory factory) { return openOrCreateDatabase(name, mode, factory, null); }
    public android.database.sqlite.SQLiteDatabase openOrCreateDatabase(String name, int mode, android.database.sqlite.SQLiteDatabase.CursorFactory factory, android.database.DatabaseErrorHandler errorHandler) {
        File f = getDatabasePath(name);
        if (f.getParentFile() != null) f.getParentFile().mkdirs();
        int flags = android.database.sqlite.SQLiteDatabase.CREATE_IF_NECESSARY | ((mode & MODE_ENABLE_WRITE_AHEAD_LOGGING) != 0 ? android.database.sqlite.SQLiteDatabase.ENABLE_WRITE_AHEAD_LOGGING : 0);
        return android.database.sqlite.SQLiteDatabase.openDatabase(f.getPath(), factory, flags, errorHandler);
    }
    public File getFileStreamPath(String name) { return new File(getFilesDir(), name); }
    public java.io.FileInputStream openFileInput(String name) throws java.io.FileNotFoundException { return new java.io.FileInputStream(getFileStreamPath(name)); }
    public java.io.FileOutputStream openFileOutput(String name, int mode) throws java.io.FileNotFoundException { return new java.io.FileOutputStream(getFileStreamPath(name), (mode & MODE_APPEND) != 0); }
    public boolean deleteFile(String name) { return getFileStreamPath(name).delete(); }
    public String[] fileList() { String[] l = getFilesDir().list(); return l == null ? new String[0] : l; }
    public boolean deleteSharedPreferences(String name) { return new File(sub("shared_prefs"), name + ".xml").delete(); }
    public boolean moveSharedPreferencesFrom(Context src, String name) { return true; }
    public boolean moveDatabaseFrom(Context src, String name) { return true; }
    public void startActivity(Intent intent, Bundle options) { startActivity(intent); }
    public void startActivities(Intent[] intents) { for (Intent i : intents) startActivity(i); }
    public void startActivities(Intent[] intents, Bundle options) { startActivities(intents); }
    public void sendBroadcast(Intent i, String perm) { sendBroadcast(i); }
    public void sendOrderedBroadcast(Intent i, String perm) { sendBroadcast(i); }
    public void sendStickyBroadcast(Intent i) { sendBroadcast(i); }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, int flags) { return registerReceiver(r, f); }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, String perm, Handler h) { return registerReceiver(r, f); }
    public Intent registerReceiver(BroadcastReceiver r, IntentFilter f, String perm, Handler h, int flags) { return registerReceiver(r, f); }
    public ComponentName startForegroundService(Intent service) { return startService(service); }
    public boolean bindService(Intent service, int flags, java.util.concurrent.Executor e, ServiceConnection c) { return bindService(service, c, flags); }
    public int checkCallingOrSelfPermission(String p) { return checkPermission(p, android.os.Process.myPid(), android.os.Process.myUid()); }
    public int checkSelfPermission(String p) { return checkPermission(p, android.os.Process.myPid(), android.os.Process.myUid()); }
    public int checkCallingPermission(String p) { return checkSelfPermission(p); }
    public int checkUriPermission(android.net.Uri u, int pid, int uid, int flags) { return PackageManager.PERMISSION_GRANTED; }
    public int checkCallingUriPermission(android.net.Uri u, int flags) { return PackageManager.PERMISSION_GRANTED; }
    public void enforceCallingOrSelfPermission(String p, String m) {}
    public void enforcePermission(String p, int pid, int uid, String m) {}
    public void grantUriPermission(String pkg, android.net.Uri u, int flags) {}
    public void revokeUriPermission(android.net.Uri u, int flags) {}
    public void registerComponentCallbacks(ComponentCallbacks c) { getApplicationContext().registerComponentCallbacks(c); }
    public void unregisterComponentCallbacks(ComponentCallbacks c) { getApplicationContext().unregisterComponentCallbacks(c); }
    public boolean isRestricted() { return false; }
    public boolean isDeviceProtectedStorage() { return false; }
    public boolean isUiContext() { return true; }
    public Context createDeviceProtectedStorageContext() { return this; }
    public Context createDisplayContext(android.view.Display d) { return this; }
    public Context createWindowContext(int type, Bundle options) { return this; }
    public Context createAttributionContext(String tag) { return this; }
    public Context createContextForSplit(String split) { return this; }
    public android.view.Display getDisplay() { return ((android.view.WindowManager) getSystemService(WINDOW_SERVICE)).getDefaultDisplay(); }
    public int getDisplayId() { return 0; }
    public void updateDisplay(int id) {}
    public boolean isDeviceProtectedStorageHusk() { return false; }
    public Context getBaseContextHusk() { return this; }
    public int getUserId() { return 0; }
    public void revokeSelfPermissionOnKill(String p) {}
    // ---- generated by tools/compat/genstubs.py: signatures only
    public static abstract class BindServiceFlags {
        protected BindServiceFlags() {}
        // ---- generated by tools/compat/fillmembers.py (BindServiceFlags): the platform's members this class does not write (signatures only)
        public static android.content.Context.BindServiceFlags of(long p0) { return null; }
        public long getValue() { return 0L; }
        // ---- end of generated members (BindServiceFlags)
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final java.lang.String ACTIVITY_TASK_SERVICE = "activity_task";
    public static final java.lang.String ADB_SERVICE = "adb";
    public static final java.lang.String ADVANCED_PROTECTION_SERVICE = "advanced_protection";
    public static final java.lang.String AMBIENT_CONTEXT_SERVICE = "ambient_context";
    public static final java.lang.String ANOMALY_DETECTOR_SERVICE = "anomaly_detector";
    public static final java.lang.String APP_BINDING_SERVICE = "app_binding";
    public static final java.lang.String APP_FUNCTION_SERVICE = "app_function";
    public static final java.lang.String APP_HIBERNATION_SERVICE = "app_hibernation";
    public static final java.lang.String APP_INTEGRITY_SERVICE = "app_integrity";
    public static final java.lang.String APP_PREDICTION_SERVICE = "app_prediction";
    public static final java.lang.String APP_SEARCH_SERVICE = "app_search";
    public static final java.lang.String ATTENTION_SERVICE = "attention";
    public static final java.lang.String ATTESTATION_VERIFICATION_SERVICE = "attestation_verification";
    public static final java.lang.String AUDIO_DEVICE_VOLUME_SERVICE = "audio_device_volume";
    public static final java.lang.String AUTHENTICATION_POLICY_SERVICE = "authentication_policy";
    public static final java.lang.String AUTH_SERVICE = "auth";
    public static final java.lang.String BACKGROUND_INSTALL_CONTROL_SERVICE = "background_install_control";
    public static final java.lang.String BACKUP_SERVICE = "backup";
    public static final java.lang.String BATTERY_STATS_SERVICE = "batterystats";
    public static final java.lang.String BINARY_TRANSPARENCY_SERVICE = "transparency";
    public static final int BIND_ALLOW_ACTIVITY_STARTS = 512;
    public static final int BIND_ALLOW_BACKGROUND_ACTIVITY_STARTS = 1048576;
    public static final int BIND_ALLOW_FOREGROUND_SERVICE_STARTS_FROM_BACKGROUND = 262144;
    public static final long BIND_ALLOW_FREEZE = 17179869184L;
    public static final int BIND_ALLOW_INSTANT = 4194304;
    public static final int BIND_ALLOW_OOM_MANAGEMENT = 16;
    public static final int BIND_ALLOW_WHITELIST_MANAGEMENT = 16777216;
    public static final int BIND_ALMOST_PERCEPTIBLE = 65536;
    public static final int BIND_BYPASS_POWER_NETWORK_RESTRICTIONS = 131072;
    public static final long BIND_BYPASS_USER_NETWORK_RESTRICTIONS = 4294967296L;
    public static final int BIND_EXTERNAL_SERVICE = -2147483648;
    public static final long BIND_EXTERNAL_SERVICE_LONG = 4611686018427387904L;
    public static final int BIND_FOREGROUND_SERVICE = 67108864;
    public static final int BIND_FOREGROUND_SERVICE_WHILE_AWAKE = 33554432;
    public static final int BIND_IMPORTANT_BACKGROUND = 8388608;
    public static final long BIND_MATCH_QUARANTINED_COMPONENTS = 8589934592L;
    public static final int BIND_NOT_APP_COMPONENT_USAGE = 32768;
    public static final int BIND_NOT_PERCEPTIBLE = 256;
    public static final int BIND_NOT_VISIBLE = 1073741824;
    public static final int BIND_PACKAGE_ISOLATED_PROCESS = 16384;
    public static final long BIND_REDUCTION_FLAGS = 1073742128L;
    public static final int BIND_RESTRICT_ASSOCIATIONS = 2097152;
    public static final int BIND_SCHEDULE_LIKE_TOP_APP = 524288;
    public static final int BIND_SHARED_ISOLATED_PROCESS = 8192;
    public static final int BIND_SHOWING_UI = 536870912;
    public static final long BIND_SIMULATE_ALLOW_FREEZE = 34359738368L;
    public static final int BIND_TREAT_LIKE_ACTIVITY = 134217728;
    public static final int BIND_TREAT_LIKE_VISIBLE_FOREGROUND_SERVICE = 268435456;
    public static final long BIND_UPDATEABLE_FLAGS = 500L;
    public static final int BIND_VISIBLE = 268435456;
    public static final java.lang.String BLOB_STORE_SERVICE = "blob_store";
    public static final java.lang.String BLOCKED_NUMBERS_SERVICE = "blocked_numbers";
    public static final java.lang.String BUGREPORT_SERVICE = "bugreport";
    public static final java.lang.String CARRIER_CONFIG_SERVICE = "carrier_config";
    public static final java.lang.String CHOOSER_SERVICE = "chooser";
    public static final java.lang.String CLOUDSEARCH_SERVICE = "cloudsearch";
    public static final java.lang.String COLOR_DISPLAY_SERVICE = "color_display";
    public static final java.lang.String COMPANION_DEVICE_SERVICE = "companiondevice";
    public static final java.lang.String CONNECTIVITY_DIAGNOSTICS_SERVICE = "connectivity_diagnostics";
    public static final java.lang.String CONSUMER_IR_SERVICE = "consumer_ir";
    public static final java.lang.String CONTACT_KEYS_SERVICE = "contact_keys";
    public static final java.lang.String CONTENT_SUGGESTIONS_SERVICE = "content_suggestions";
    public static final java.lang.String CONTEXTHUB_SERVICE = "contexthub";
    public static final java.lang.String CONTEXTUAL_SEARCH_SERVICE = "contextual_search";
    public static final int CONTEXT_CREDENTIAL_PROTECTED_STORAGE = 16;
    public static final int CONTEXT_DEVICE_PROTECTED_STORAGE = 8;
    public static final int CONTEXT_REGISTER_PACKAGE = 1073741824;
    public static final java.lang.String COUNTRY_DETECTOR = "country_detector";
    public static final java.lang.String CREDENTIAL_SERVICE = "credential";
    public static final java.lang.String DATA_LOADER_MANAGER_SERVICE = "dataloader_manager";
    public static final java.lang.String DEVICE_IDENTIFIERS_SERVICE = "device_identifiers";
    public static final java.lang.String DEVICE_IDLE_CONTROLLER = "deviceidle";
    public static final int DEVICE_ID_DEFAULT = 0;
    public static final int DEVICE_ID_INVALID = -1;
    public static final java.lang.String DEVICE_LOCK_SERVICE = "device_lock";
    public static final java.lang.String DEVICE_STATE_SERVICE = "device_state";
    public static final java.lang.String DOMAIN_VERIFICATION_SERVICE = "domain_verification";
    public static final java.lang.String DREAM_SERVICE = "dream";
    public static final java.lang.String DROPBOX_SERVICE = "dropbox";
    public static final java.lang.String DYNAMIC_INSTRUMENTATION_SERVICE = "dynamic_instrumentation";
    public static final java.lang.String DYNAMIC_SYSTEM_SERVICE = "dynamic_system";
    public static final java.lang.String ECM_ENHANCED_CONFIRMATION_SERVICE = "ecm_enhanced_confirmation";
    public static final java.lang.String ETHERNET_SERVICE = "ethernet";
    public static final java.lang.String EUICC_CARD_SERVICE = "euicc_card";
    public static final java.lang.String EUICC_SERVICE = "euicc";
    public static final java.lang.String FACE_SERVICE = "face";
    public static final java.lang.String FEATURE_FLAGS_SERVICE = "feature_flags";
    public static final java.lang.String FILE_INTEGRITY_SERVICE = "file_integrity";
    public static final java.lang.String FONT_SERVICE = "font";
    public static final java.lang.String GATEKEEPER_SERVICE = "android.service.gatekeeper.IGateKeeperService";
    public static final java.lang.String GRAMMATICAL_INFLECTION_SERVICE = "grammatical_inflection";
    public static final java.lang.String HDMI_CONTROL_SERVICE = "hdmi_control";
    public static final java.lang.String HEALTHCONNECT_SERVICE = "healthconnect";
    public static final java.lang.String IDMAP_SERVICE = "idmap";
    public static final java.lang.String INCIDENT_COMPANION_SERVICE = "incidentcompanion";
    public static final java.lang.String INCIDENT_SERVICE = "incident";
    public static final java.lang.String INCREMENTAL_SERVICE = "incremental";
    public static final java.lang.String INTRUSION_DETECTION_SERVICE = "intrusion_detection";
    public static final java.lang.String IPSEC_SERVICE = "ipsec";
    public static final java.lang.String IRIS_SERVICE = "iris";
    public static final java.lang.String KEYSTORE_SERVICE = "keystore";
    public static final java.lang.String LAUNCHER_APPS_SERVICE = "launcherapps";
    public static final java.lang.String LEGACY_PERMISSION_SERVICE = "legacy_permission";
    public static final java.lang.String LIGHTS_SERVICE = "lights";
    public static final java.lang.String LOWPAN_SERVICE = "lowpan";
    public static final java.lang.String MEDIA_COMMUNICATION_SERVICE = "media_communication";
    public static final java.lang.String MEDIA_METRICS_SERVICE = "media_metrics";
    public static final java.lang.String MEDIA_PROJECTION_SERVICE = "media_projection";
    public static final java.lang.String MEDIA_QUALITY_SERVICE = "media_quality";
    public static final java.lang.String MEDIA_TRANSCODING_SERVICE = "media_transcoding";
    public static final java.lang.String MIDI_SERVICE = "midi";
    public static final java.lang.String MMS_SERVICE = "mms";
    public static final java.lang.String MUSIC_RECOGNITION_SERVICE = "music_recognition";
    public static final java.lang.String NEARBY_SERVICE = "nearby";
    public static final java.lang.String NETD_SERVICE = "netd";
    public static final java.lang.String NETWORKMANAGEMENT_SERVICE = "network_management";
    public static final java.lang.String NETWORK_POLICY_SERVICE = "netpolicy";
    public static final java.lang.String NETWORK_SCORE_SERVICE = "network_score";
    public static final java.lang.String NETWORK_STACK_SERVICE = "network_stack";
    public static final java.lang.String NETWORK_WATCHLIST_SERVICE = "network_watchlist";
    public static final java.lang.String NFC_SERVICE = "nfc";
    public static final java.lang.String OEM_LOCK_SERVICE = "oem_lock";
    public static final java.lang.String ON_DEVICE_INTELLIGENCE_SERVICE = "on_device_intelligence";
    public static final java.lang.String OVERLAY_SERVICE = "overlay";
    public static final long OVERRIDABLE_COMPONENT_CALLBACKS = 193247900L;
    public static final java.lang.String PAC_PROXY_SERVICE = "pac_proxy";
    public static final java.lang.String PEOPLE_SERVICE = "people";
    public static final java.lang.String PERFORMANCE_HINT_SERVICE = "performance_hint";
    public static final java.lang.String PERMISSION_CHECKER_SERVICE = "permission_checker";
    public static final java.lang.String PERMISSION_CONTROLLER_SERVICE = "permission_controller";
    public static final java.lang.String PERMISSION_ENFORCER_SERVICE = "permission_enforcer";
    public static final int PERMISSION_REQUEST_STATE_GRANTED = 0;
    public static final int PERMISSION_REQUEST_STATE_REQUESTABLE = 1;
    public static final int PERMISSION_REQUEST_STATE_UNREQUESTABLE = 2;
    public static final java.lang.String PERMISSION_SERVICE = "permission";
    public static final java.lang.String PERSISTENT_DATA_BLOCK_SERVICE = "persistent_data_block";
    public static final java.lang.String PLATFORM_COMPAT_NATIVE_SERVICE = "platform_compat_native";
    public static final java.lang.String PLATFORM_COMPAT_SERVICE = "platform_compat";
    public static final java.lang.String POWER_EXEMPTION_SERVICE = "power_exemption";
    public static final java.lang.String POWER_STATS_SERVICE = "powerstats";
    public static final java.lang.String POWER_WHITELIST_MANAGER = "power_whitelist";
    public static final java.lang.String PRINT_SERVICE = "print";
    public static final java.lang.String PROFILING_SERVICE = "profiling";
    public static final java.lang.String PROTOLOG_CONFIGURATION_SERVICE = "protolog_configuration";
    public static final java.lang.String RADIO_SERVICE = "broadcastradio";
    public static final java.lang.String RANGING_SERVICE = "ranging";
    public static final java.lang.String REBOOT_READINESS_SERVICE = "reboot_readiness";
    public static final int RECEIVER_EXPORTED_UNAUDITED = 2;
    public static final java.lang.String RECOVERY_SERVICE = "recovery";
    public static final java.lang.String REMOTE_AUTH_SERVICE = "remote_auth";
    public static final java.lang.String REMOTE_PROVISIONING_SERVICE = "remote_provisioning";
    public static final java.lang.String RESOURCES_SERVICE = "resources";
    public static final java.lang.String RESTRICTIONS_SERVICE = "restrictions";
    public static final java.lang.String ROLE_SERVICE = "role";
    public static final java.lang.String ROLLBACK_SERVICE = "rollback";
    public static final java.lang.String ROTATION_RESOLVER_SERVICE = "resolver";
    public static final java.lang.String SAFETY_CENTER_SERVICE = "safety_center";
    public static final java.lang.String SATELLITE_SERVICE = "satellite";
    public static final java.lang.String SEARCH_UI_SERVICE = "search_ui";
    public static final java.lang.String SECURE_ELEMENT_SERVICE = "secure_element";
    public static final java.lang.String SECURITY_STATE_SERVICE = "security_state";
    public static final java.lang.String SELECTION_TOOLBAR_SERVICE = "selection_toolbar";
    public static final java.lang.String SENSITIVE_CONTENT_PROTECTION_SERVICE = "sensitive_content_protection_service";
    public static final java.lang.String SENSOR_PRIVACY_SERVICE = "sensor_privacy";
    public static final java.lang.String SERIAL_SERVICE = "serial";
    public static final java.lang.String SHARED_CONNECTIVITY_SERVICE = "shared_connectivity";
    public static final java.lang.String SIP_SERVICE = "sip";
    public static final java.lang.String SLICE_SERVICE = "slice";
    public static final java.lang.String SMARTSPACE_SERVICE = "smartspace";
    public static final java.lang.String SMS_SERVICE = "sms";
    public static final java.lang.String SOUND_TRIGGER_MIDDLEWARE_SERVICE = "soundtrigger_middleware";
    public static final java.lang.String SOUND_TRIGGER_SERVICE = "soundtrigger";
    public static final java.lang.String SPEECH_RECOGNITION_SERVICE = "speech_recognition";
    public static final java.lang.String STATS_BOOTSTRAP_ATOM_SERVICE = "statsbootstrap";
    public static final java.lang.String STATS_COMPANION_SERVICE = "statscompanion";
    public static final java.lang.String STATS_MANAGER = "stats";
    public static final java.lang.String STATS_MANAGER_SERVICE = "statsmanager";
    public static final java.lang.String STATUS_BAR_SERVICE = "statusbar";
    public static final java.lang.String STORAGE_STATS_SERVICE = "storagestats";
    public static final java.lang.String SUPERVISION_SERVICE = "supervision";
    public static final java.lang.String SYSTEM_CONFIG_SERVICE = "system_config";
    public static final java.lang.String SYSTEM_HEALTH_SERVICE = "systemhealth";
    public static final java.lang.String SYSTEM_UPDATE_SERVICE = "system_update";
    public static final java.lang.String TALISMAN_SERVICE = "talisman";
    public static final java.lang.String TASK_CONTINUITY_SERVICE = "task_continuity";
    public static final java.lang.String TELECOM_SERVICE = "telecom";
    public static final java.lang.String TELEPHONY_IMS_SERVICE = "telephony_ims";
    public static final java.lang.String TELEPHONY_PHONE_NUMBER_SERVICE = "telephony_phone_number";
    public static final java.lang.String TELEPHONY_RCS_MESSAGE_SERVICE = "ircsmessage";
    public static final java.lang.String TELEPHONY_REGISTRY_SERVICE = "telephony_registry";
    public static final java.lang.String TELEPHONY_SUBSCRIPTION_SERVICE = "telephony_subscription_service";
    public static final java.lang.String TEST_NETWORK_SERVICE = "test_network";
    public static final java.lang.String TETHERING_SERVICE = "tethering";
    public static final java.lang.String TEXT_TO_SPEECH_MANAGER_SERVICE = "texttospeech";
    public static final java.lang.String THEME_SERVICE = "theme";
    public static final java.lang.String THERMAL_SERVICE = "thermalservice";
    public static final java.lang.String THREAD_NETWORK_SERVICE = "thread_network";
    public static final java.lang.String TIME_DETECTOR_SERVICE = "time_detector";
    public static final java.lang.String TIME_MANAGER_SERVICE = "time_manager";
    public static final java.lang.String TIME_ZONE_DETECTOR_SERVICE = "time_zone_detector";
    public static final java.lang.String TRANSLATION_MANAGER_SERVICE = "translation";
    public static final java.lang.String TRUST_SERVICE = "trust";
    public static final java.lang.String TV_AD_SERVICE = "tv_ad";
    public static final java.lang.String TV_INPUT_SERVICE = "tv_input";
    public static final java.lang.String TV_INTERACTIVE_APP_SERVICE = "tv_interactive_app";
    public static final java.lang.String TV_TUNER_RESOURCE_MGR_SERVICE = "tv_tuner_resource_mgr";
    public static final java.lang.String UI_TRANSLATION_SERVICE = "ui_translation";
    public static final java.lang.String UNIVERSAL_CLIPBOARD_SERVICE = "universal_clipboard";
    public static final java.lang.String UPDATE_LOCK_SERVICE = "updatelock";
    public static final java.lang.String URI_GRANTS_SERVICE = "uri_grants";
    public static final java.lang.String USB_SERVICE = "usb";
    public static final java.lang.String USER_RECOVERY_SERVICE = "user_recovery";
    public static final java.lang.String UWB_SERVICE = "uwb";
    public static final java.lang.String VCN_MANAGEMENT_SERVICE = "vcn_management";
    public static final java.lang.String VIRTUALIZATION_SERVICE = "virtualization";
    public static final java.lang.String VIRTUAL_DEVICE_SERVICE = "virtualdevice";
    public static final java.lang.String VOICE_INTERACTION_MANAGER_SERVICE = "voiceinteraction";
    public static final java.lang.String VPN_MANAGEMENT_SERVICE = "vpn_management";
    public static final java.lang.String VR_SERVICE = "vrmanager";
    public static final java.lang.String WALLPAPER_EFFECTS_GENERATION_SERVICE = "wallpaper_effects_generation";
    public static final java.lang.String WEARABLE_SENSING_SERVICE = "wearable_sensing";
    public static final java.lang.String WEBVIEW_UPDATE_SERVICE = "webviewupdate";
    public static final java.lang.String WIFI_AWARE_SERVICE = "wifiaware";
    public static final java.lang.String WIFI_NL80211_SERVICE = "wifinl80211";
    public static final java.lang.String WIFI_P2P_SERVICE = "wifip2p";
    public static final java.lang.String WIFI_RTT_RANGING_SERVICE = "wifirtt";
    public static final java.lang.String WIFI_RTT_SERVICE = "rttmanager";
    public static final java.lang.String WIFI_SCANNING_SERVICE = "wifiscanner";
    public static final java.lang.String WIFI_USD_SERVICE = "wifi_usd";
    public static android.os.IBinder getToken(android.content.Context p0) { return null; }
    public void assertRuntimeOverlayThemable() {}
    public boolean bindIsolatedService(android.content.Intent p0, int p1, java.lang.String p2, java.util.concurrent.Executor p3, android.content.ServiceConnection p4) { return false; }
    public boolean bindIsolatedService(android.content.Intent p0, android.content.Context.BindServiceFlags p1, java.lang.String p2, java.util.concurrent.Executor p3, android.content.ServiceConnection p4) { return false; }
    public boolean bindService(android.content.Intent p0, android.content.Context.BindServiceFlags p1, java.util.concurrent.Executor p2, android.content.ServiceConnection p3) { return false; }
    public boolean bindService(android.content.Intent p0, android.content.ServiceConnection p1, android.content.Context.BindServiceFlags p2) { return false; }
    public boolean bindServiceAsUser(android.content.Intent p0, android.content.ServiceConnection p1, int p2, android.os.Handler p3, android.os.UserHandle p4) { return false; }
    public boolean bindServiceAsUser(android.content.Intent p0, android.content.ServiceConnection p1, int p2, android.os.UserHandle p3) { return false; }
    public boolean bindServiceAsUser(android.content.Intent p0, android.content.ServiceConnection p1, android.content.Context.BindServiceFlags p2, android.os.Handler p3, android.os.UserHandle p4) { return false; }
    public boolean bindServiceAsUser(android.content.Intent p0, android.content.ServiceConnection p1, android.content.Context.BindServiceFlags p2, android.os.UserHandle p3) { return false; }
    public boolean canLoadUnsafeResources() { return false; }
    public boolean canStartActivityForResult() { return false; }
    public int checkCallingOrSelfUriPermission(android.net.Uri p0, int p1) { return 0; }
    public int[] checkCallingOrSelfUriPermissions(java.util.List p0, int p1) { return null; }
    public int[] checkCallingUriPermissions(java.util.List p0, int p1) { return null; }
    public int checkContentUriPermissionFull(android.net.Uri p0, int p1, int p2, int p3) { return 0; }
    public int checkPermission(java.lang.String p0, int p1, int p2, android.os.IBinder p3) { return 0; }
    public int checkUriPermission(android.net.Uri p0, int p1, int p2, int p3, android.os.IBinder p4) { return 0; }
    public int checkUriPermission(android.net.Uri p0, java.lang.String p1, java.lang.String p2, int p3, int p4, int p5) { return 0; }
    public int[] checkUriPermissions(java.util.List p0, int p1, int p2, int p3) { return null; }
    public void clearWallpaper() {}
    public void closeSystemDialogs() {}
    public android.content.Context createApplicationContext(android.content.pm.ApplicationInfo p0, int p1) { return this; }
    public android.content.Context createContextAsUser(android.os.UserHandle p0, int p1) { return this; }
    public android.content.Context createContextForSdkInSandbox(android.content.pm.ApplicationInfo p0, int p1) { return this; }
    public android.content.Context createCredentialProtectedStorageContext() { return this; }
    public android.content.Context createDeviceContext(int p0) { return this; }
    public android.content.Context createFeatureContext(java.lang.String p0) { return this; }
    public android.content.Context createPackageContextAsUser(java.lang.String p0, int p1, android.os.UserHandle p2) { return this; }
    public android.content.Context createTokenContext(android.os.IBinder p0, android.view.Display p1) { return this; }
    public void destroy() {}
    public void enforceCallingOrSelfUriPermission(android.net.Uri p0, int p1, java.lang.String p2) {}
    public void enforceCallingPermission(java.lang.String p0, java.lang.String p1) {}
    public void enforceCallingUriPermission(android.net.Uri p0, int p1, java.lang.String p2) {}
    public void enforceUriPermission(android.net.Uri p0, int p1, int p2, int p3, java.lang.String p4) {}
    public void enforceUriPermission(android.net.Uri p0, java.lang.String p1, java.lang.String p2, int p3, int p4, int p5, java.lang.String p6) {}
    public android.os.IBinder getActivityToken() { return null; }
    public int getAssociatedDisplayId() { return 0; }
    public android.content.AttributionSource getAttributionSource() { return null; }
    public android.view.autofill.AutofillManager.AutofillClient getAutofillClient() { return (android.view.autofill.AutofillManager.AutofillClient) huskFill.get("AutofillClient"); }
    public android.view.contentcapture.ContentCaptureManager.ContentCaptureClient getContentCaptureClient() { return null; }
    public android.content.ContentCaptureOptions getContentCaptureOptions() { return (android.content.ContentCaptureOptions) huskFill.get("ContentCaptureOptions"); }
    public java.io.File getCrateDir(java.lang.String p0) { return null; }
    public int getDeviceId() { return 0; }
    public android.view.Display getDisplayNoVerify() { return null; }
    public java.lang.String getFeatureId() { return null; }
    public android.os.Handler getMainThreadHandler() { return null; }
    public int getNextAutofillId() { return 0; }
    public android.content.ContextParams getParams() { return null; }
    public int getPermissionRequestState(java.lang.String p0) { return 0; }
    public java.io.File getPreloadsFileCache() { return null; }
    public android.os.IBinder getProcessToken() { return null; }
    public java.util.List getRegisteredIntentFilters(android.content.BroadcastReceiver p0) { return new java.util.ArrayList(); }
    public android.app.IServiceConnection getServiceDispatcher(android.content.ServiceConnection p0, android.os.Handler p1, long p2) { return null; }
    public android.content.SharedPreferences getSharedPreferences(java.io.File p0, int p1) { return null; }
    public java.io.File getSharedPreferencesPath(java.lang.String p0) { return null; }
    public java.io.File getSharedPrefsFile(java.lang.String p0) { return null; }
    public android.content.Context.BindServiceFlags getUpdateableFlags() { return null; }
    public android.os.UserHandle getUser() { return null; }
    public android.graphics.drawable.Drawable getWallpaper() { return (android.graphics.drawable.Drawable) huskFill.get("Wallpaper"); }
    public int getWallpaperDesiredMinimumHeight() { return 0; }
    public int getWallpaperDesiredMinimumWidth() { return 0; }
    public android.os.IBinder getWindowContextToken() { return null; }
    public boolean isAutofillCompatibilityEnabled() { return false; }
    public boolean isConfigurationContext() { return false; }
    public boolean isCredentialProtectedStorage() { return false; }
    public android.graphics.drawable.Drawable peekWallpaper() { return null; }
    public void rebindService(android.content.ServiceConnection p0, android.content.Context.BindServiceFlags p1) {}
    public void registerDeviceIdChangeListener(java.util.concurrent.Executor p0, java.util.function.IntConsumer p1) {}
    public android.content.Intent registerReceiverAsUser(android.content.BroadcastReceiver p0, android.os.UserHandle p1, android.content.IntentFilter p2, java.lang.String p3, android.os.Handler p4) { return null; }
    public android.content.Intent registerReceiverAsUser(android.content.BroadcastReceiver p0, android.os.UserHandle p1, android.content.IntentFilter p2, java.lang.String p3, android.os.Handler p4, int p5) { return null; }
    public android.content.Intent registerReceiverForAllUsers(android.content.BroadcastReceiver p0, android.content.IntentFilter p1, java.lang.String p2, android.os.Handler p3) { return null; }
    public android.content.Intent registerReceiverForAllUsers(android.content.BroadcastReceiver p0, android.content.IntentFilter p1, java.lang.String p2, android.os.Handler p3, int p4) { return null; }
    public void reloadSharedPreferences() {}
    public void removeStickyBroadcast(android.content.Intent p0) {}
    public void removeStickyBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1) {}
    public void revokeSelfPermissionsOnKill(java.util.Collection p0) {}
    public void revokeUriPermission(java.lang.String p0, android.net.Uri p1, int p2) {}
    public void sendBroadcast(android.content.Intent p0, java.lang.String p1, int p2) {}
    public void sendBroadcast(android.content.Intent p0, java.lang.String p1, android.os.Bundle p2) {}
    public void sendBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1) {}
    public void sendBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, java.lang.String p2) {}
    public void sendBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, java.lang.String p2, int p3) {}
    public void sendBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, java.lang.String p2, android.os.Bundle p3) {}
    public void sendBroadcastAsUserMultiplePermissions(android.content.Intent p0, android.os.UserHandle p1, java.lang.String[] p2) {}
    public void sendBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1) {}
    public void sendBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, android.app.BroadcastOptions p2) {}
    public void sendBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, android.os.Bundle p2) {}
    public void sendBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, java.lang.String[] p2) {}
    public void sendBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, java.lang.String[] p2, java.lang.String[] p3) {}
    public void sendBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, java.lang.String[] p2, java.lang.String[] p3, android.app.BroadcastOptions p4) {}
    public void sendBroadcastWithMultiplePermissions(android.content.Intent p0, java.lang.String[] p1) {}
    public void sendOrderedBroadcast(android.content.Intent p0, int p1, java.lang.String p2, java.lang.String p3, android.content.BroadcastReceiver p4, android.os.Handler p5, java.lang.String p6, android.os.Bundle p7, android.os.Bundle p8) {}
    public void sendOrderedBroadcast(android.content.Intent p0, java.lang.String p1, int p2, android.content.BroadcastReceiver p3, android.os.Handler p4, int p5, java.lang.String p6, android.os.Bundle p7) {}
    public void sendOrderedBroadcast(android.content.Intent p0, java.lang.String p1, android.content.BroadcastReceiver p2, android.os.Handler p3, int p4, java.lang.String p5, android.os.Bundle p6) {}
    public void sendOrderedBroadcast(android.content.Intent p0, java.lang.String p1, android.os.Bundle p2) {}
    public void sendOrderedBroadcast(android.content.Intent p0, java.lang.String p1, android.os.Bundle p2, android.content.BroadcastReceiver p3, android.os.Handler p4, int p5, java.lang.String p6, android.os.Bundle p7) {}
    public void sendOrderedBroadcast(android.content.Intent p0, java.lang.String p1, java.lang.String p2, android.content.BroadcastReceiver p3, android.os.Handler p4, int p5, java.lang.String p6, android.os.Bundle p7) {}
    public void sendOrderedBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, java.lang.String p2, int p3, android.content.BroadcastReceiver p4, android.os.Handler p5, int p6, java.lang.String p7, android.os.Bundle p8) {}
    public void sendOrderedBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, java.lang.String p2, int p3, android.os.Bundle p4, android.content.BroadcastReceiver p5, android.os.Handler p6, int p7, java.lang.String p8, android.os.Bundle p9) {}
    public void sendOrderedBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, java.lang.String p2, android.content.BroadcastReceiver p3, android.os.Handler p4, int p5, java.lang.String p6, android.os.Bundle p7) {}
    public void sendOrderedBroadcastAsUserMultiplePermissions(android.content.Intent p0, android.os.UserHandle p1, java.lang.String[] p2, int p3, android.os.Bundle p4, android.content.BroadcastReceiver p5, android.os.Handler p6, int p7, java.lang.String p8, android.os.Bundle p9) {}
    public void sendOrderedBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, java.lang.String p2, android.content.BroadcastReceiver p3, android.os.Handler p4, int p5, java.lang.String p6, android.os.Bundle p7, android.os.Bundle p8) {}
    public void sendOrderedBroadcastMultiplePermissions(android.content.Intent p0, java.lang.String[] p1, java.lang.String[] p2, java.lang.String p3, android.content.BroadcastReceiver p4, android.os.Handler p5, int p6, java.lang.String p7, android.os.Bundle p8, android.os.Bundle p9) {}
    public void sendStickyBroadcast(android.content.Intent p0, android.os.Bundle p1) {}
    public void sendStickyBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1) {}
    public void sendStickyBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, android.os.Bundle p2) {}
    public void sendStickyOrderedBroadcast(android.content.Intent p0, android.content.BroadcastReceiver p1, android.os.Handler p2, int p3, java.lang.String p4, android.os.Bundle p5) {}
    public void sendStickyOrderedBroadcastAsUser(android.content.Intent p0, android.os.UserHandle p1, android.content.BroadcastReceiver p2, android.os.Handler p3, int p4, java.lang.String p5, android.os.Bundle p6) {}
    public void setAutofillClient(android.view.autofill.AutofillManager.AutofillClient p0) { huskFill.put("AutofillClient", p0); }
    public void setContentCaptureOptions(android.content.ContentCaptureOptions p0) { huskFill.put("ContentCaptureOptions", p0); }
    public void setWallpaper(android.graphics.Bitmap p0) { huskFill.put("Wallpaper", p0); }
    public void setWallpaper(java.io.InputStream p0) { huskFill.put("Wallpaper", p0); }
    public int startActivitiesAsUser(android.content.Intent[] p0, android.os.Bundle p1, android.os.UserHandle p2) { return 0; }
    public void startActivityAsUser(android.content.Intent p0, android.os.Bundle p1, android.os.UserHandle p2) {}
    public void startActivityAsUser(android.content.Intent p0, android.os.UserHandle p1) {}
    public void startActivityForResult(java.lang.String p0, android.content.Intent p1, int p2, android.os.Bundle p3) {}
    public android.content.ComponentName startForegroundServiceAsUser(android.content.Intent p0, android.os.UserHandle p1) { return null; }
    public boolean startInstrumentation(android.content.ComponentName p0, java.lang.String p1, android.os.Bundle p2) { return false; }
    public void startIntentSender(android.content.IntentSender p0, android.content.Intent p1, int p2, int p3, int p4) {}
    public void startIntentSender(android.content.IntentSender p0, android.content.Intent p1, int p2, int p3, int p4, android.os.Bundle p5) {}
    public android.content.ComponentName startServiceAsUser(android.content.Intent p0, android.os.UserHandle p1) { return null; }
    public boolean stopServiceAsUser(android.content.Intent p0, android.os.UserHandle p1) { return false; }
    public void unregisterDeviceIdChangeListener(java.util.function.IntConsumer p0) {}
    public void updateDeviceId(int p0) {}
    public void updateServiceBindings(java.util.List p0) {}
    public void updateServiceGroup(android.content.ServiceConnection p0, int p1, int p2) {}
    // ---- end of generated members
}
