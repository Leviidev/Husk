package husk;

import android.app.Activity;
import android.app.Application;
import android.content.*;
import android.content.pm.ProviderInfo;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.view.KeyEvent;
import android.view.MotionEvent;
import java.util.ArrayList;

/**
 * Runs the app as ActivityThread would: the Application (with its content providers first), then the launcher activity, then the
 * main looper; activities started later go on a stack and come back off it as they finish. Input from Husk is delivered here.
 */
public final class AppRunner {
    private static final ArrayList<Activity> sStack = new ArrayList<>();
    private static Application sApp;
    private static Handler sHandler;
    private static final int[] ids = new int[16];
    private static final float[] xs = new float[16], ys = new float[16];
    private static int down;
    private static long downTime;
    private static boolean sPausedByHost;

    public static void run(String activityClass, String applicationClass) throws Exception {
        android.webkit.MimeTypeMap.huskInstallDefault();
        KeyStoreProvider.install();
        Looper.prepareMainLooper();
        sHandler = new Handler(Looper.getMainLooper());
        Manifest.read();
        ContextImpl base = ContextImpl.app();
        String appCls = applicationClass != null ? applicationClass : Manifest.applicationClass;
        Application app;
        try { app = appCls != null ? (Application) Class.forName(appCls).newInstance() : new Application(); }
        catch (ClassNotFoundException e) { android.util.Log.w("Husk", "no application class " + appCls); app = new Application(); }
        sApp = app;
        Application.huskSet(app);
        ContextImpl.setApplication(app);
        app.huskAttach(base);
        installProviders(app);
        Looper.setInputSink(AppRunner::deliver);
        applyInsets();
        app.onCreate();
        Manifest.Component launcher = Manifest.activity(activityClass);
        Intent main = new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER);
        main.setComponent(new ComponentName(Native.packageName(), activityClass));
        main.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        launch(activityClass, main, null, -1, launcher);
        Looper.loop();
    }
    public static Application application() { return sApp; }

    private static void installProviders(Application app) {
        for (Manifest.Component p : Manifest.providers) {
            if (!p.enabled) continue;
            try {
                ContentProvider cp = (ContentProvider) Class.forName(p.name).newInstance();
                ProviderInfo info = new ProviderInfo();
                info.name = p.name; info.authority = p.authorities; info.packageName = Native.packageName(); info.exported = p.exported; info.metaData = p.metaData; info.grantUriPermissions = p.grantUriPermissions;
                info.applicationInfo = app.getApplicationInfo();
                cp.attachInfo(app, info);
                ContentResolver.register(p.authorities, cp);
            } catch (Throwable t) {
                android.util.Log.w("Husk", "content provider " + p.name + " did not start: " + t);
            }
        }
    }

    // ---- activities
    public static int stackSize() { return sStack.size(); }
    public static Activity top() { return sStack.isEmpty() ? null : sStack.get(sStack.size() - 1); }
    public static CharSequence appLabel(Context c) {
        try { if (Manifest.appLabel != 0) return c.getResources().getText(Manifest.appLabel); } catch (Exception e) {}
        return Native.packageName();
    }
    static Activity launch(String cls, Intent intent, Activity caller, int request, Manifest.Component info) throws Exception {
        Activity prev = top();
        Activity a = (Activity) Class.forName(cls).newInstance();
        ContextImpl base = ContextImpl.forActivity();
        a.huskAttach(base, sApp, intent, info, caller, request);
        if (prev != null && prev.isResumedHusk()) prev.huskPause();
        sStack.add(a);
        a.huskCreate(null);
        if (!a.isFinishing()) { a.huskStart(); a.huskResume(); }
        if (prev != null && !a.isFinishing()) {
            final Activity p = prev;
            // the one below stops once the new one covers it (a translucent or floating window leaves it visible)
            if (!a.getWindow().isFloating()) p.huskStop();
        }
        return a;
    }
    /** Context.startActivity: an activity of the app's own, or something the host can open (a web page, mail, the store). */
    public static void startActivity(Context from, Intent i, int request, Bundle opts) {
        Manifest.Component c = Manifest.resolve(i);
        if (c != null) {
            final Activity caller = from instanceof Activity ? (Activity) from : top();
            final Intent intent = new Intent(i);
            if (intent.getComponent() == null) intent.setComponent(new ComponentName(Native.packageName(), c.name));
            sHandler.post(() -> {
                try {
                    if ((intent.getFlags() & Intent.FLAG_ACTIVITY_CLEAR_TOP) != 0) {
                        for (int k = sStack.size() - 1; k >= 0; k--) if (sStack.get(k).getClass().getName().equals(c.name)) {
                            Activity existing = sStack.get(k);
                            while (top() != existing) finishNow(top(), false);
                            existing.setIntent(intent);
                            existing.huskActivityResult(-1, 0, null);
                            if (!existing.isResumedHusk()) { existing.huskRestart(); existing.huskStart(); existing.huskResume(); }
                            return;
                        }
                    }
                    if ((intent.getFlags() & 0x20000000 /* SINGLE_TOP */) != 0 || c.launchMode == 1) {
                        Activity t = top();
                        if (t != null && t.getClass().getName().equals(c.name)) { t.setIntent(intent); return; }
                    }
                    launch(c.name, intent, caller, request, c);
                } catch (Throwable t) {
                    android.util.Log.e("Husk", "starting " + c.name + " failed", t);
                    if (t instanceof RuntimeException) throw (RuntimeException) t;
                    throw new RuntimeException(t);
                }
            });
            return;
        }
        android.net.Uri u = i.getData();
        String action = i.getAction();
        if (u != null && (Intent.ACTION_VIEW.equals(action) || Intent.ACTION_SENDTO.equals(action) || action == null || "android.intent.action.DIAL".equals(action))) {
            String s = u.toString();
            if (s.startsWith("market://")) s = "https://play.google.com/store/apps/" + s.substring(9);
            Native.openUrl(s);
            return;
        }
        if (Intent.ACTION_SEND.equals(action)) {
            String text = i.getStringExtra(Intent.EXTRA_TEXT);
            if (text != null) Native.share(text);
            return;
        }
        if ("android.intent.action.CHOOSER".equals(action)) { Intent inner = i.getParcelableExtra("android.intent.extra.INTENT"); if (inner != null) startActivity(from, inner, request, opts); return; }
        throw new ActivityNotFoundException("No Activity found to handle " + i);
    }
    /** Activity.finish: the one below comes back (with the result when it asked for one). */
    public static void finish(Activity a) { sHandler.post(() -> finishNow(a, true)); }
    private static void finishNow(Activity a, boolean resumeBelow) {
        int idx = sStack.indexOf(a);
        if (idx < 0) return;
        boolean wasTop = idx == sStack.size() - 1;
        if (a.isResumedHusk()) a.huskPause();
        sStack.remove(idx);
        Activity below = top();
        if (wasTop && below != null && resumeBelow) {
            Activity caller = a.huskCaller();
            if (caller != null && a.huskRequestCode() >= 0 && caller == below) below.huskActivityResult(a.huskRequestCode(), a.huskResultCode(), a.huskResultData());
            if (!below.isResumedHusk()) {
                if (!below.huskVisible()) { below.huskRestart(); below.huskStart(); }
                below.huskResume();
            }
        }
        a.huskStop();
        a.huskDestroy();
        // the app ends with its last activity, unless one it started just before (finish(); startActivity(...)) is still to come
        if (sStack.isEmpty()) sHandler.post(() -> { if (sStack.isEmpty()) Native.exit(); });
    }
    public static void finishAll() { sHandler.post(() -> { while (!sStack.isEmpty()) finishNow(top(), false); }); }
    public static void recreate(Activity a) {
        sHandler.post(() -> {
            int idx = sStack.indexOf(a);
            if (idx < 0) return;
            Bundle state = new Bundle();
            if (a.isResumedHusk()) a.huskPause();
            a.huskStop();
            a.huskDestroy();
            sStack.remove(idx);
            try { launch(a.getClass().getName(), a.getIntent(), a.huskCaller(), a.huskRequestCode(), Manifest.activity(a.getClass().getName())); }
            catch (Exception e) { throw new RuntimeException(e); }
        });
    }

    // ---- input from Husk
    /** events: [n, then n x (phase, id, xbits, ybits)] -- phase 0 down, 1 move, 2 up, 3 cancel; 4 key (id = code, x = down); 5 back; 6 host pause (id 1) / resume (0); 7 text waiting; 8 insets changed; 9 web view events waiting */
    private static void deliver(int[] ev) {
        int n = ev[0];
        for (int k = 0; k < n; k++) {
            int phase = ev[1 + 4 * k], id = ev[2 + 4 * k];
            float x = Float.intBitsToFloat(ev[3 + 4 * k]), y = Float.intBitsToFloat(ev[4 + 4 * k]);
            try {
                if (phase == 5) { back(); continue; }
                if (phase == 6) { hostPause(id != 0); continue; }
                if (phase == 7) { InputMethods.deliverPending(); continue; }
                if (phase == 8) { applyInsets(); continue; }
                if (phase == 9) { Web.deliver(); continue; }
                if (phase == 4) {
                    long t = SystemClock.uptimeMillis();
                    KeyEvent ke = new KeyEvent(t, t, ev[3 + 4 * k] != 0 ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP, id, 0);
                    if (!InputMethods.key(ke)) ViewRootDispatch.key(ke);
                    continue;
                }
                touch(phase, id, x, y);
            } catch (RuntimeException e) {
                android.util.Log.e("Husk", "input threw", e);
                throw e;
            }
        }
    }
    /** The screen's notch / home indicator areas and the keyboard, from the host. */
    private static void applyInsets() {
        int[] in = Native.insets();
        if (in == null || in.length < 4) return;
        ViewRoot.setInsets(in[0], in[1], in[2], in[3]);
        if (in.length > 4) ViewRoot.setImeHeight(in[4]);
    }
    private static void back() {
        ViewRoot r = ViewRoot.top();
        Activity a = top();
        if (r != null && a != null && r.getView() == a.getWindow().peekDecorView()) { a.huskBack(); return; }
        long t = SystemClock.uptimeMillis();
        boolean h = ViewRoot.dispatchKey(new KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_BACK, 0));
        h |= ViewRoot.dispatchKey(new KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_BACK, 0));
        if (!h && a != null) a.huskBack();
    }
    private static void hostPause(boolean pause) {
        if (pause == sPausedByHost) return;
        sPausedByHost = pause;
        Activity a = top();
        if (a == null) return;
        if (pause) { if (a.isResumedHusk()) a.huskPause(); }
        else if (!a.isResumedHusk()) a.huskResume();
    }
    private static void touch(int phase, int id, float x, float y) {
        long now = SystemClock.uptimeMillis();
        int idx = -1;
        for (int i = 0; i < down; i++) if (ids[i] == id) idx = i;
        int action;
        if (phase == 0) {
            if (idx >= 0 || down >= 16) return;
            idx = down++; ids[idx] = id; xs[idx] = x; ys[idx] = y;
            if (down == 1) downTime = now;
            action = down == 1 ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_POINTER_DOWN | (idx << 8);
        } else if (phase == 1) {
            if (idx < 0) return;
            xs[idx] = x; ys[idx] = y;
            action = MotionEvent.ACTION_MOVE;
        } else {
            if (idx < 0) return;
            xs[idx] = x; ys[idx] = y;
            action = phase == 3 ? MotionEvent.ACTION_CANCEL : down == 1 ? MotionEvent.ACTION_UP : MotionEvent.ACTION_POINTER_UP | (idx << 8);
        }
        MotionEvent e = MotionEvent.huskObtain(downTime, now, action, down, java.util.Arrays.copyOf(ids, down), java.util.Arrays.copyOf(xs, down), java.util.Arrays.copyOf(ys, down));
        ViewRoot.dispatchTouch(e);
        if (phase >= 2) {
            for (int i = idx; i + 1 < down; i++) { ids[i] = ids[i + 1]; xs[i] = xs[i + 1]; ys[i] = ys[i + 1]; }
            down--;
            if (phase == 3) down = 0;
        }
    }
    static final class ViewRootDispatch { static void key(KeyEvent e) { if (!ViewRoot.dispatchKey(e)) { Activity a = top(); if (a != null) a.dispatchKeyEvent(e); } } }
}
