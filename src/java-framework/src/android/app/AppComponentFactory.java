package android.app;

import android.content.BroadcastReceiver;
import android.content.ContentProvider;
import android.content.Intent;

/** Makes an app's components from their class names (androidx's CoreComponentFactory extends it). */
public class AppComponentFactory {
    public static final AppComponentFactory DEFAULT = new AppComponentFactory();
    public AppComponentFactory() {}
    private static Object make(ClassLoader cl, String className) throws ClassNotFoundException, IllegalAccessException, InstantiationException {
        try { return Class.forName(className, false, cl).getDeclaredConstructor().newInstance(); }
        catch (NoSuchMethodException | java.lang.reflect.InvocationTargetException e) { InstantiationException ie = new InstantiationException(className); ie.initCause(e); throw ie; }
    }
    public ClassLoader instantiateClassLoader(ClassLoader cl, android.content.pm.ApplicationInfo aInfo) { return cl; }
    public Application instantiateApplication(ClassLoader cl, String className) throws InstantiationException, IllegalAccessException, ClassNotFoundException { return (Application) make(cl, className); }
    public Activity instantiateActivity(ClassLoader cl, String className, Intent intent) throws InstantiationException, IllegalAccessException, ClassNotFoundException { return (Activity) make(cl, className); }
    public BroadcastReceiver instantiateReceiver(ClassLoader cl, String className, Intent intent) throws InstantiationException, IllegalAccessException, ClassNotFoundException { return (BroadcastReceiver) make(cl, className); }
    public Service instantiateService(ClassLoader cl, String className, Intent intent) throws InstantiationException, IllegalAccessException, ClassNotFoundException { return (Service) make(cl, className); }
    public ContentProvider instantiateProvider(ClassLoader cl, String className) throws InstantiationException, IllegalAccessException, ClassNotFoundException { return (ContentProvider) make(cl, className); }
}
