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
        System.out.println("PASS: framework reset drops previous APK identity, preferences, receivers and frame scheduling");
    }
}
