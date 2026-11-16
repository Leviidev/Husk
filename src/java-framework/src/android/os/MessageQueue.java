package android.os;

/** Messages by time; next() waits for the first that is due. Also polls Husk's input queue, so touches reach the UI thread. */
public final class MessageQueue {
    private Message head;
    private boolean quitting;
    boolean mMain;
    private static boolean sNativeWakeOk = true;
    private static void wakeNative() {
        if (!sNativeWakeOk) return;
        try { husk.Native.wakeMainLooper(); } catch (UnsatisfiedLinkError e) { sNativeWakeOk = false; }
    }
    public interface IdleHandler { boolean queueIdle(); }
    public void addIdleHandler(IdleHandler h) {}
    public void removeIdleHandler(IdleHandler h) {}

    synchronized boolean enqueue(Message m, long when) {
        if (m.target == null) throw new IllegalArgumentException("Message must have a target");
        m.markInUse();
        if (quitting) { m.recycleUnchecked(); return false; }
        m.when = when;
        if (head == null || when < head.when) { m.next = head; head = m; }
        else { Message p = head; while (p.next != null && p.next.when <= when) p = p.next; m.next = p.next; p.next = m; }
        notifyAll();
        if (mMain) wakeNative();
        return true;
    }
    synchronized void remove(Handler h, Runnable r, int what, boolean byWhat, Object token) {
        Message prev = null, p = head;
        while (p != null) {
            boolean hit = p.target == h && (byWhat ? p.what == what && (token == null || p.obj == token) : (r == null || p.callback == r) && (token == null || p.obj == token));
            if (byWhat == false && r == null && token == null) hit = p.target == h;
            if (hit) {
                Message successor = p.next;
                if (prev == null) head = successor; else prev.next = successor;
                p.next = null;
                p.recycleUnchecked();
                p = successor;
            }
            else { prev = p; p = p.next; }
        }
    }
    synchronized boolean has(Handler h, int what) { for (Message p = head; p != null; p = p.next) if (p.target == h && p.what == what) return true; return false; }
    synchronized boolean has(Handler h, int what, Object obj, boolean equal) {
        for (Message p = head; p != null; p = p.next)
            if (p.target == h && p.what == what && (obj == null || (equal ? obj.equals(p.obj) : p.obj == obj))) return true;
        return false;
    }
    synchronized boolean hasCallback(Handler h, Runnable r) { for (Message p = head; p != null; p = p.next) if (p.target == h && p.callback == r && r != null) return true; return false; }
    synchronized boolean hasAny(Handler h) { for (Message p = head; p != null; p = p.next) if (p.target == h) return true; return false; }
    private void recycleAllLocked() {
        Message p = head;
        head = null;
        while (p != null) {
            Message successor = p.next;
            p.next = null;
            p.recycleUnchecked();
            p = successor;
        }
    }
    synchronized void quit() { quitting = true; recycleAllLocked(); notifyAll(); if (mMain) wakeNative(); }
    /** Called while the queue monitor is held; also serves the JVM queue contract fixture. */
    Message pollDueLocked(long now) {
        if (head == null || head.when > now) return null;
        Message message = head;
        head = message.next;
        message.next = null;
        return message;
    }

    private boolean mNativeLooper;
    Message next(boolean main) {
        for (;;) {
            if (husk.Native.sessionEnded()) { quit(); return null; }
            if (main) Looper.deliverInput();
            mNativeLooper = husk.Native.nativeLooperPoll();
            long mainWait = -1;
            synchronized (this) {
                if (quitting) return null;
                long now = SystemClock.uptimeMillis();
                Message due = pollDueLocked(now);
                if (due != null) return due;
                if (main && sNativeWakeOk) {
                    long w = head == null ? (mNativeLooper ? 8 : 60) : Math.max(1, mNativeLooper ? Math.min(8, head.when - now) : head.when - now);
                    try {
                        husk.Native.prepareMainWait();
                        mainWait = w;
                    } catch (UnsatisfiedLinkError e) {
                        sNativeWakeOk = false;
                    }
                }
                if (mainWait < 0) {
                    long wait = head == null ? 8 : Math.min(8, head.when - now);
                    if (!main) wait = head == null ? (mNativeLooper ? 8 : 0) : Math.max(1, mNativeLooper ? Math.min(8, head.when - now) : head.when - now);
                    // A session close wakes the native main queue, but helper
                    // loopers have individual monitors. Bound their sleep so
                    // HandlerThread can observe sessionEnded and exit too.
                    if (!main) wait = wait <= 0 ? 50 : Math.min(wait, 50);
                    try { if (wait <= 0) wait(); else wait(wait); }
                    catch (InterruptedException e) { quitting = true; recycleAllLocked(); return null; }
                }
            }
            if (mainWait >= 0) {
                try { husk.Native.waitMainLooper(mainWait); } catch (UnsatisfiedLinkError e) { sNativeWakeOk = false; }
            }
        }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void addOnFileDescriptorEventListener(java.io.FileDescriptor p0, int p1, android.os.MessageQueue.OnFileDescriptorEventListener p2) {}
    public boolean isIdle() { return false; }
    public boolean isPolling() { return false; }
    public android.os.Message peekLastMessageForTest() { return null; }
    public int postSyncBarrier() { return 0; }
    public void removeOnFileDescriptorEventListener(java.io.FileDescriptor p0) {}
    public void removeSyncBarrier(int p0) {}
    public void resetForTest() {}
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public interface OnFileDescriptorEventListener {
        int EVENT_ERROR = 4;
        int EVENT_INPUT = 1;
        int EVENT_OUTPUT = 2;
        int onFileDescriptorEvents(java.io.FileDescriptor p0, int p1);
    }
    // ---- end of generated nested classes
}
