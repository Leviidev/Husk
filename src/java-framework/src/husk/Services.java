package husk;

import android.app.Service;
import android.content.*;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import java.util.HashMap;

/** The app's own services (started and bound in this process, as on Android) and the system services Husk answers for. */
public final class Services {
    private static final HashMap<String, Service> sRunning = new HashMap<>();
    private static final HashMap<ServiceConnection, String> sBound = new HashMap<>();
    private Services() {}
    private static Manifest.Component find(Intent i) {
        if (i.getComponent() != null) { for (Manifest.Component c : Manifest.services) if (c.name.equals(i.getComponent().getClassName())) return c; }
        if (i.getAction() != null) for (Manifest.Component c : Manifest.services) for (Manifest.Filter f : c.filters) if (f.actions.contains(i.getAction())) return c;
        return null;
    }
    private static Service create(Manifest.Component c) throws Exception {
        Service s = sRunning.get(c.name);
        if (s == null) {
            s = (Service) Class.forName(c.name).newInstance();
            s.huskAttach(AppRunner.application() != null ? AppRunner.application() : ContextImpl.app());
            sRunning.put(c.name, s);
            s.onCreate();
        }
        return s;
    }
    public static ComponentName start(Context from, Intent i) {
        Manifest.Component c = find(i);
        if (c == null) return null;
        new Handler(Looper.getMainLooper()).post(() -> {
            try { Service s = create(c); s.onStartCommand(i, 0, 1); } catch (Throwable t) { android.util.Log.e("Husk", "service " + c.name + " failed", t); }
        });
        return new ComponentName(Native.packageName(), c.name);
    }
    public static boolean stop(Intent i) {
        Manifest.Component c = find(i);
        if (c == null) return false;
        Service s = sRunning.remove(c.name);
        if (s != null) s.onDestroy();
        return s != null;
    }
    public static void stopSelf(Service s) { String k = null; for (java.util.Map.Entry<String, Service> e : sRunning.entrySet()) if (e.getValue() == s) k = e.getKey(); if (k != null) { sRunning.remove(k); s.onDestroy(); } }
    public static boolean bind(Context from, Intent i, ServiceConnection conn) {
        Manifest.Component c = find(i);
        if (c == null) return false;
        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                Service s = create(c);
                IBinder b = s.onBind(i);
                sBound.put(conn, c.name);
                ComponentName cn = new ComponentName(Native.packageName(), c.name);
                if (b != null) conn.onServiceConnected(cn, b); else conn.onNullBinding(cn);
            } catch (Throwable t) { android.util.Log.e("Husk", "binding " + c.name + " failed", t); }
        });
        return true;
    }
    public static void unbind(ServiceConnection conn) { sBound.remove(conn); }

    /** A system service by name, or null for those Husk has nothing for. */
    public static Object system(String name, Context c) {
        switch (name) {
        case Context.AUDIO_SERVICE: return new android.media.AudioManager();
        case Context.VIBRATOR_SERVICE: return new android.os.Vibrator();
        case Context.VIBRATOR_MANAGER_SERVICE: return new android.os.VibratorManager();
        case Context.CLIPBOARD_SERVICE: return new ClipboardManager();
        case Context.INPUT_METHOD_SERVICE: return new android.view.inputmethod.InputMethodManager();
        case Context.SENSOR_SERVICE: return new android.hardware.SensorManager();
        case Context.POWER_SERVICE: return new android.os.PowerManager();
        case Context.ACTIVITY_SERVICE: return new android.app.ActivityManager();
        case Context.CONNECTIVITY_SERVICE: return new android.net.ConnectivityManager();
        case Context.NOTIFICATION_SERVICE: return new android.app.NotificationManager();
        case Context.ALARM_SERVICE: return new android.app.AlarmManager();
        case Context.ACCESSIBILITY_SERVICE: return new android.view.accessibility.AccessibilityManager();
        case Context.UI_MODE_SERVICE: return new android.app.UiModeManager();
        case Context.DISPLAY_SERVICE: return new android.hardware.display.DisplayManager();
        case Context.KEYGUARD_SERVICE: return new android.app.KeyguardManager();
        case Context.USER_SERVICE: return new android.os.UserManager();
        case Context.BATTERY_SERVICE: return new android.os.BatteryManager();
        case Context.STORAGE_SERVICE: return new android.os.storage.StorageManager();
        case Context.TELEPHONY_SERVICE: return new android.telephony.TelephonyManager();
        case Context.JOB_SCHEDULER_SERVICE: return new android.app.job.JobScheduler.Impl();
        case Context.APP_OPS_SERVICE: return new android.app.AppOpsManager();
        case Context.INPUT_SERVICE: return new android.hardware.input.InputManager();
        default: return null;
        }
    }
}
