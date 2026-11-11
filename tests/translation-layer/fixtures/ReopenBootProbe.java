import android.os.Handler;
import android.os.Looper;
import android.view.Choreographer;

/** Deliberately on the boot path, like Husk's framework: app-class replacement cannot reset this state. */
public final class ReopenBootProbe {
    private static final Object marker = new Object();
    private static int frames;
    private static final Object waitLock = new Object(), entryLock = new Object();
    public static native void entered(int kind);
    public static native void exitThread();
    public static void abnormalExit() { synchronized (entryLock) { exitThread(); } }
    public static void javaThreadExit() throws InterruptedException {
        Thread thread = new Thread(() -> { synchronized (entryLock) { exitThread(); } });
        thread.start(); thread.join();
        if (thread.isAlive()) throw new AssertionError("exited native thread is still alive");
    }
    private static final class SlowInit {
        static int value = initialize();
        private static int initialize() {
            entered(4);
            try { Thread.sleep(60000); } catch (InterruptedException ignored) { }
            return 1;
        }
    }
    public static int slowInit() { return SlowInit.value; }
    public static int waitForInit() { entered(5); return SlowInit.value; }
    private static final class ExitInit {
        static int value = initialize();
        private static int initialize() { exitThread(); return 1; }
    }
    public static int exitInInit() { return ExitInit.value; }
    public static int monitorWait() throws InterruptedException {
        synchronized (waitLock) {
            synchronized (waitLock) {
                entered(0);
                waitLock.wait();
                return Thread.holdsLock(waitLock) ? 1 : -1;
            }
        }
    }
    public static void notifyWaiter() { synchronized (waitLock) { waitLock.notify(); } }
    public static int timedWait() throws InterruptedException {
        synchronized (waitLock) { waitLock.wait(30); return Thread.holdsLock(waitLock) ? 1 : -1; }
    }
    public static void sleepUntilStop() throws InterruptedException { entered(1); Thread.sleep(60000); }
    public static void holdUntilStop() throws InterruptedException {
        synchronized (entryLock) { entered(2); Thread.sleep(60000); }
    }
    public static void contendUntilStop() { entered(3); synchronized (entryLock) { } }
    public static Object marker() { return marker; }
    public static int run(String database) {
        if (frames != 0 || System.getProperty("husk.session.probe") != null) return -2;
        System.setProperty("husk.session.probe", "previous session");
        long db = husk.Sqlite.open(database, 0x10000000);
        husk.Sqlite.busyTimeout(db, 0);
        if (db == 0 || husk.Sqlite.exec(db, "CREATE TABLE IF NOT EXISTS probe(value INTEGER)") != 0
                || husk.Sqlite.exec(db, "BEGIN EXCLUSIVE") != 0
                || husk.Sqlite.exec(db, "INSERT INTO probe VALUES (1)") != 0) return -3;
        long statement = husk.Sqlite.prepare(db, "SELECT COUNT(*) FROM probe");
        if (statement == 0 || husk.Sqlite.step(statement) != husk.Sqlite.ROW || husk.Sqlite.columnLong(statement, 0) != 1) return -4;
        // Deliberately abandon the active statement, exclusive transaction and connection.
        // A fresh session must be able to acquire the database and see the rollback.
        Looper.prepareMainLooper();
        Handler.getMain().postAtTime(() -> Choreographer.getMainThreadInstance().postFrameCallback(time -> {
            frames++;
            Looper.myLooper().quit();
        }), 0);
        Looper.loop();
        return frames;
    }
}
