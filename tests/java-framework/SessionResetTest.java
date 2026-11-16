package husk;

import java.lang.reflect.Field;
import java.util.List;
import java.util.Map;

/** Boot framework caches must not keep the previous APK's context or handlers. */
public final class SessionResetTest {
    private static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    public static void main(String[] args) throws Exception {
        Field application = field(AppRunner.class, "sApp");
        Field loader = field(AppRunner.class, "sLoader");
        Field info = field(android.content.pm.ApplicationInfo.class, "sSelf");
        Field prefs = field(ContextImpl.class, "sPrefs");
        Field receivers = field(Broadcasts.class, "sReceivers");
        Field scheduled = field(ViewRoot.class, "sScheduled");
        Field screenClean = field(ViewRoot.class, "sScreenClean");
        Field cachedCutout = field(ViewRoot.class, "sCachedCutout");
        Field cachedInsets = field(ViewRoot.class, "sCachedScreenInsets");
        application.set(null, new android.app.Application());
        loader.set(null, SessionResetTest.class.getClassLoader());
        info.set(null, new android.content.pm.ApplicationInfo());
        ((Map) prefs.get(null)).put("previous-game", null);
        ((List) receivers.get(null)).add(new Object[0]);
        scheduled.setBoolean(null, true);
        screenClean.setBoolean(null, true);
        cachedCutout.set(null, android.view.DisplayCutout.NO_CUTOUT);
        cachedInsets.set(null, android.view.WindowInsets.CONSUMED);
        AppRunner.resetSession();
        if (application.get(null) != null || loader.get(null) != null || info.get(null) != null)
            throw new AssertionError("previous app identity survived reset");
        if (!((Map) prefs.get(null)).isEmpty() || !((List) receivers.get(null)).isEmpty() || scheduled.getBoolean(null))
            throw new AssertionError("previous app callbacks/preferences/frame handler survived reset");
        if (screenClean.getBoolean(null) || cachedCutout.get(null) != null || cachedInsets.get(null) != null)
            throw new AssertionError("previous ViewRoot screen/cutout/insets cache survived reset");

        /* Canvas clean-screen tracking starts clean only when explicitly armed and clears on content draw. */
        android.graphics.Canvas c = new android.graphics.Canvas();
        if (c.huskIsCleanScreen()) throw new AssertionError("fresh Canvas must not assume a clean screen");
        c.huskBeginScreenFrame(true);
        if (!c.huskIsCleanScreen()) throw new AssertionError("huskBeginScreenFrame(true) should mark screen clean");
        c.drawColor(0);
        if (!c.huskIsCleanScreen()) throw new AssertionError("drawColor with mNative==0 must keep clean when untouched");
        c.huskBeginScreenFrame(true);
        android.graphics.Canvas.class.getDeclaredMethod("touched");
        java.lang.reflect.Method touched = android.graphics.Canvas.class.getDeclaredMethod("touched");
        touched.setAccessible(true);
        touched.invoke(c);
        if (c.huskIsCleanScreen()) throw new AssertionError("touched Canvas must clear huskIsCleanScreen()");
        AppRunner.resetSession();

        /* Empty-stack exit debounce: scheduleEmptyExitCheck must not exit synchronously and resetSession/cancel removes it. */
        android.os.Handler.drain();
        Field handlerField = field(AppRunner.class, "sHandler");
        Field pendingExitField = field(AppRunner.class, "sPendingEmptyExit");
        Field stackField = field(AppRunner.class, "sStack");
        java.lang.reflect.Method scheduleEmptyExit = AppRunner.class.getDeclaredMethod("scheduleEmptyExitCheck");
        scheduleEmptyExit.setAccessible(true);
        java.lang.reflect.Method cancelPendingEmpty = AppRunner.class.getDeclaredMethod("cancelPendingEmptyExit");
        cancelPendingEmpty.setAccessible(true);

        handlerField.set(null, new android.os.Handler(android.os.Looper.getMainLooper()));
        ((List<?>) stackField.get(null)).clear();
        int basePending = android.os.Handler.pendingCount();
        scheduleEmptyExit.invoke(null);
        if (pendingExitField.get(null) == null || android.os.Handler.pendingCount() != basePending + 1)
            throw new AssertionError("expected empty stack exit to be scheduled asynchronously");
        cancelPendingEmpty.invoke(null);
        if (pendingExitField.get(null) != null || android.os.Handler.pendingCount() != basePending)
            throw new AssertionError("cancelPendingEmptyExit must cancel queued exit");
        scheduleEmptyExit.invoke(null);
        AppRunner.resetSession();
        if (pendingExitField.get(null) != null || android.os.Handler.pendingCount() != basePending)
            throw new AssertionError("resetSession must cancel pending empty-stack exit");
        Field runningServices = field(Services.class, "sRunning");
        ((Map) runningServices.get(null)).put("com.game.A/.BgService", null);
        AppRunner.resetSession();
        if (!((Map) runningServices.get(null)).isEmpty())
            throw new AssertionError("resetSession must clear previous APK services");

        /* Lazy ConnectivityManager/WebView singletons must survive repeated shutdown & reset cycles without starting pools. */
        Field webIo = field(android.webkit.WebView.class, "sIo");
        Field webBridge = field(android.webkit.WebView.class, "sBridge");
        Field connNet = field(android.net.ConnectivityManager.class, "sNet");
        android.webkit.WebView.huskShutdownSession();
        AppRunner.resetSession();
        if (webIo.get(null) != null || webBridge.get(null) != null || connNet.get(null) != null)
            throw new AssertionError("lazy WebView/ConnectivityManager singletons must stay null across idle teardown");
        if (new android.net.ConnectivityManager().getActiveNetwork() == null)
            throw new AssertionError("ConnectivityManager must lazily provide active network");
        AppRunner.resetSession();
        if (connNet.get(null) != null)
            throw new AssertionError("ConnectivityManager.resetSession must clear lazy network instance");
        android.os.Handler.drain();
        System.out.println("PASS: framework reset drops previous APK identity, preferences, receivers, pending exits and frame scheduling");
    }
}
