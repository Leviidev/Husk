package husk;

import android.app.Application;
import android.content.*;
import android.database.Cursor;
import android.net.Uri;
import android.os.*;
import java.util.*;
import java.util.concurrent.*;

/** Android process contexts hosted by Husk. APK classes, Applications and component state are isolated; the host's UI
 * event loop and Android system services are shared. This is VM isolation, not an OS security boundary or native fork. */
public final class Processes {
    private static final InheritableThreadLocal<State> sCurrent = new InheritableThreadLocal<>();
    private static final ThreadLocal<State> sCaller = new ThreadLocal<>();
    private static final Map<String, State> sProcesses = new LinkedHashMap<>();
    private static State sMain;
    public static final class State {
        public String name;
        public final int pid;
        public volatile Application application;
        public volatile ClassLoader loader;
        public ContextImpl context;
        public final Map<String, Object> services = new HashMap<>();
        public android.app.AppComponentFactory factory = android.app.AppComponentFactory.DEFAULT;
        private FutureTask<Void> startup;
        private final ExecutorService binder;
        private State(String n, int id) {
            name = n; pid = id;
            binder = Executors.newCachedThreadPool(r -> {
                Thread t = new Thread(() -> { try (Scope ignored = enter(this)) { r.run(); } }, "Binder:" + name);
                t.setDaemon(true); return t;
            });
        }
        public <T> T call(Callable<T> action) {
            if (current() == this) try { return action.call(); } catch (Exception e) { throw failure(e); }
            State caller = current();
            try { return binder.submit(() -> {
                State previous = sCaller.get(); sCaller.set(caller);
                try { return action.call(); } finally { sCaller.set(previous); }
            }).get(); }
            catch (InterruptedException e) { Thread.currentThread().interrupt(); throw new IllegalStateException("Interrupted calling " + name, e); }
            catch (ExecutionException e) { throw failure(e.getCause()); }
        }
        public void start() {
            if (this == main()) return;
            FutureTask<Void> task;
            synchronized (this) {
                if (startup == null) {
                    startup = new FutureTask<>(() -> {
                        loader = new ProcessLoader();
                        Thread.currentThread().setContextClassLoader(loader);
                        AppRunner.startProcess(this);
                        android.util.Log.i("Husk", "process ready: " + name + " (pid " + pid + ")");
                        return null;
                    });
                    binder.execute(startup);
                }
                task = startup;
            }
            // A provider/Application may resolve a second component in its own process during startup.
            if (current() == this && !task.isDone()) return;
            try { task.get(); }
            catch (InterruptedException e) { Thread.currentThread().interrupt(); throw new IllegalStateException(e); }
            catch (ExecutionException e) { throw failure(e.getCause()); }
        }
    }
    private static RuntimeException failure(Throwable e) {
        if (e instanceof Error) throw (Error)e;
        return e instanceof RuntimeException ? (RuntimeException)e : new RuntimeException(e);
    }
    public static synchronized State main() {
        if (sMain == null) { sMain = new State(Native.packageName(), 4242); sProcesses.put(sMain.name, sMain); }
        return sMain;
    }
    public static State current() { State s = sCurrent.get(); return s != null ? s : main(); }
    public static int callingPid() { State s = sCaller.get(); return (s != null ? s : current()).pid; }
    public static long clearCallingIdentity() { State s = sCaller.get(); sCaller.remove(); return s != null ? s.pid : 0; }
    public static void restoreCallingIdentity(long token) { sCaller.remove(); for (State s : all()) if (s.pid == token) sCaller.set(s); }
    public static synchronized void configureMain() {
        State s = main(); sProcesses.remove(s.name); s.name = Manifest.processName(null); sProcesses.put(s.name, s);
    }
    public static synchronized State named(String name) {
        main(); State s = sProcesses.get(name);
        if (s == null) { s = new State(name, 4242 + sProcesses.size()); sProcesses.put(name, s); }
        return s;
    }
    public static State component(Manifest.Component c) { return named(Manifest.processName(c)); }
    public static synchronized java.util.List<State> all() { main(); return new ArrayList<>(sProcesses.values()); }
    public static State owner(Context c) {
        Context app = c == null ? null : c.getApplicationContext();
        for (State s : all()) if (s.application == app) return s;
        return current();
    }
    public static final class Scope implements AutoCloseable {
        private final State previous;
        private final ClassLoader loader;
        private Scope(State s) {
            previous = sCurrent.get(); loader = Thread.currentThread().getContextClassLoader();
            sCurrent.set(s);
            if (s.loader != null) Thread.currentThread().setContextClassLoader(s.loader);
        }
        @Override public void close() { sCurrent.set(previous); Thread.currentThread().setContextClassLoader(loader); }
    }
    public static Scope enter(State s) { return new Scope(s); }
    public static void reset() {
        Geo.reset();
        resetProcesses();
    }
    private static synchronized void resetProcesses() {
        for (State s : sProcesses.values()) s.binder.shutdownNow();
        ContentResolver.huskReset();
        android.app.LoadedApk.huskReset();
        android.app.ActivityThread.huskReset();
        android.content.pm.ApplicationInfo.huskReset();
        android.view.LayoutInflater.huskReset();
        Services.huskReset(); Broadcasts.huskReset();
        android.accounts.AccountManager.huskReset();
        sProcesses.clear(); sMain = null; sCurrent.remove();
        sCaller.remove();
    }
    private static final class ProcessLoader extends ClassLoader {
        ProcessLoader() {
            super(AppRunner.class.getClassLoader());
            if (!Native.registerProcessLoader(this)) throw new IllegalStateException("Cannot create a process class namespace");
        }
        @Override protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
            return Class.forName(name, false, this);
        }
        @Override protected synchronized Package getPackage(String name) {
            Package p = super.getPackage(name);
            return p != null ? p : definePackage(name, null, null, null, null, null, null, null);
        }
        @Override public java.net.URL getResource(String name) { return AppRunner.mainClassLoader().getResource(name); }
        @Override public java.io.InputStream getResourceAsStream(String name) { return AppRunner.mainClassLoader().getResourceAsStream(name); }
    }
    /** Resolve providers lazily, as ActivityManager does when acquiring an authority. */
    public static ContentProvider provider(String authority) {
        Manifest.read();
        for (Manifest.Component c : Manifest.providers) {
            if (!c.enabled || c.authorities == null) continue;
            for (String a : Manifest.authorities(c).split(";")) if (a.trim().equals(authority)) {
                State s = component(c);
                if (s == main()) return null;
                s.start();
                return ContentResolver.huskRegistered(authority);
            }
        }
        return null;
    }
    public static ContentProvider transport(ContentProvider p, State process) { return new ProviderTransport(p, process); }
    private static final class ProviderTransport extends ContentProvider {
        private final ContentProvider target;
        private final State process;
        ProviderTransport(ContentProvider p, State s) { target = p; process = s; }
        @Override public boolean onCreate() { return true; }
        @Override public Bundle call(String method, String arg, Bundle extras) {
            if (current() == process) return target.call(method, arg, extras);
            ClassLoader caller = AppRunner.classLoader();
            Bundle result = process.call(() -> target.call(method, arg, (Bundle)copy(extras, process.loader)));
            return (Bundle)copy(result, caller);
        }
        @Override public Cursor query(Uri u, String[] p, String sel, String[] args, String sort) { return process.call(() -> target.query(u, p, sel, args, sort)); }
        @Override public String getType(Uri u) { return process.call(() -> target.getType(u)); }
        @Override public Uri insert(Uri u, ContentValues v) { return process.call(() -> target.insert(u, v)); }
        @Override public int update(Uri u, ContentValues v, String s, String[] a) { return process.call(() -> target.update(u, v, s, a)); }
        @Override public int delete(Uri u, String s, String[] a) { return process.call(() -> target.delete(u, s, a)); }
        private <T> T fileCall(Callable<T> action) throws java.io.FileNotFoundException {
            try { return process.call(action); }
            catch (RuntimeException error) { for(Throwable cause=error;cause!=null;cause=cause.getCause())if(cause instanceof java.io.FileNotFoundException)throw (java.io.FileNotFoundException)cause; throw error; }
        }
        @Override public android.os.ParcelFileDescriptor openFile(Uri u,String mode) throws java.io.FileNotFoundException { return fileCall(() -> target.openFile(u,mode)); }
        @Override public android.content.res.AssetFileDescriptor openAssetFile(Uri u,String mode) throws java.io.FileNotFoundException { return fileCall(() -> target.openAssetFile(u,mode)); }
    }
    /** Reconstruct app-owned parcelables in the receiving loader; no app objects cross a namespace by reference. */
    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Object copy(Object value, ClassLoader loader) {
        if (value == null || value instanceof String || value instanceof Number || value instanceof Boolean || value instanceof Character || value instanceof IBinder) return value;
        if (value instanceof Bundle) return ((Bundle)value).huskCopy(loader);
        if (value instanceof List) { ArrayList<Object> out = new ArrayList<>(); for (Object v : (List)value) out.add(copy(v, loader)); return out; }
        if (value instanceof Map) { Map<Object,Object> out = new HashMap<>(); for (Object obj : ((Map)value).entrySet()) { Map.Entry e = (Map.Entry)obj; out.put(copy(e.getKey(), loader), copy(e.getValue(), loader)); } return out; }
        Class<?> cls = value.getClass();
        if (cls.isArray()) {
            int n = java.lang.reflect.Array.getLength(value);
            try {
                Class<?> component = cls.getComponentType();
                if (!component.isPrimitive()) component = Class.forName(component.getName(), false, loader);
                Object out = java.lang.reflect.Array.newInstance(component, n);
                for (int i = 0; i < n; i++) java.lang.reflect.Array.set(out, i, copy(java.lang.reflect.Array.get(value, i), loader));
                return out;
            } catch (ClassNotFoundException e) { throw new BadParcelableException(e); }
        }
        if (value instanceof Parcelable) {
            Parcel original = Parcel.obtain(), received = null;
            try {
                ((Parcelable)value).writeToParcel(original, 0);
                received = original.huskCopy(loader);
                Class<?> receiver = Class.forName(cls.getName(), true, loader);
                java.lang.reflect.Field field = receiver.getField("CREATOR"); field.setAccessible(true);
                Parcelable.Creator creator = (Parcelable.Creator)field.get(null);
                return creator instanceof Parcelable.ClassLoaderCreator ? ((Parcelable.ClassLoaderCreator)creator).createFromParcel(received, loader) : creator.createFromParcel(received);
            } catch (ReflectiveOperationException e) { throw new BadParcelableException(e); }
            finally { original.recycle(); if (received != null) received.recycle(); }
        }
        if (value instanceof java.io.Serializable) {
            try {
                java.io.ByteArrayOutputStream bytes = new java.io.ByteArrayOutputStream();
                try (java.io.ObjectOutputStream out = new java.io.ObjectOutputStream(bytes)) { out.writeObject(value); }
                try (java.io.ObjectInputStream in = new java.io.ObjectInputStream(new java.io.ByteArrayInputStream(bytes.toByteArray())) {
                    @Override protected Class<?> resolveClass(java.io.ObjectStreamClass desc) throws java.io.IOException, ClassNotFoundException { return Class.forName(desc.getName(), false, loader); }
                }) { return in.readObject(); }
            } catch (java.io.IOException | ClassNotFoundException e) { throw new BadParcelableException(e); }
        }
        throw new BadParcelableException("Cannot transfer " + cls.getName() + " between processes");
    }
}
