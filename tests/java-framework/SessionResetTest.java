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
        application.set(null, new android.app.Application());
        loader.set(null, SessionResetTest.class.getClassLoader());
        info.set(null, new android.content.pm.ApplicationInfo());
        ((Map) prefs.get(null)).put("previous-game", null);
        ((List) receivers.get(null)).add(new Object[0]);
        scheduled.setBoolean(null, true);
        AppRunner.resetSession();
        if (application.get(null) != null || loader.get(null) != null || info.get(null) != null)
            throw new AssertionError("previous app identity survived reset");
        if (!((Map) prefs.get(null)).isEmpty() || !((List) receivers.get(null)).isEmpty() || scheduled.getBoolean(null))
            throw new AssertionError("previous app callbacks/preferences/frame handler survived reset");
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
        android.os.Handler.drain();
        System.out.println("PASS: framework reset drops previous APK identity, preferences, receivers, pending exits and frame scheduling");
    }
}
