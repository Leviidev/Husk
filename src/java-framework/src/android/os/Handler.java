package android.os;

public class Handler {
    public interface Callback { boolean handleMessage(Message msg); }
    final Looper mLooper;
    final Callback mCallback;
    private final husk.Processes.State mProcess = husk.Processes.current();
    public Handler() { this(Looper.myLooper() != null ? Looper.myLooper() : Looper.getMainLooper(), null); }
    public Handler(Callback cb) { this(Looper.myLooper() != null ? Looper.myLooper() : Looper.getMainLooper(), cb); }
    public Handler(Looper l) { this(l, null); }
    public Handler(Looper l, Callback cb) { mLooper = l; mCallback = cb; }
    public static Handler createAsync(Looper l) { return new Handler(l); }
    public final Looper getLooper() { return mLooper; }
    public void handleMessage(Message msg) {}
    public void dispatchMessage(Message msg) {
        try (husk.Processes.Scope ignored = husk.Processes.enter(mProcess)) { dispatchInProcess(msg); }
    }
    private void dispatchInProcess(Message msg) {
        if (msg.callback != null) { msg.callback.run(); return; }
        if (mCallback != null && mCallback.handleMessage(msg)) return;
        handleMessage(msg);
    }
    public final boolean post(Runnable r) { return postDelayed(r, 0); }
    public final boolean postDelayed(Runnable r, long ms) { Message m = Message.obtain(this, r); return mLooper.mQueue.enqueue(m, SystemClock.uptimeMillis() + Math.max(0, ms)); }
    public final boolean postDelayed(Runnable r, Object token, long ms) { Message m = Message.obtain(this, r); m.obj = token; return mLooper.mQueue.enqueue(m, SystemClock.uptimeMillis() + Math.max(0, ms)); }
    public final boolean postAtTime(Runnable r, long uptime) { return mLooper.mQueue.enqueue(Message.obtain(this, r), uptime); }
    public final boolean postAtFrontOfQueue(Runnable r) { return mLooper.mQueue.enqueue(Message.obtain(this, r), 0); }
    public final boolean sendMessage(Message m) { return sendMessageDelayed(m, 0); }
    public final boolean sendEmptyMessage(int what) { return sendMessage(Message.obtain(this, what)); }
    public final boolean sendEmptyMessageDelayed(int what, long ms) { return sendMessageDelayed(Message.obtain(this, what), ms); }
    public final boolean sendMessageDelayed(Message m, long ms) { m.target = this; return mLooper.mQueue.enqueue(m, SystemClock.uptimeMillis() + Math.max(0, ms)); }
    public final boolean sendMessageAtTime(Message m, long uptime) { m.target = this; return mLooper.mQueue.enqueue(m, uptime); }
    public final void removeCallbacks(Runnable r) { mLooper.mQueue.remove(this, r, 0, false, null); }
    public final void removeCallbacks(Runnable r, Object token) { mLooper.mQueue.remove(this, r, 0, false, token); }
    public final void removeMessages(int what) { mLooper.mQueue.remove(this, null, what, true, null); }
    public final void removeMessages(int what, Object obj) { mLooper.mQueue.remove(this, null, what, true, obj); }
    public final void removeCallbacksAndMessages(Object token) { mLooper.mQueue.remove(this, null, 0, false, token); }
    public final boolean hasMessages(int what) { return mLooper.mQueue.has(this, what); }
    public final Message obtainMessage() { return Message.obtain(this); }
    public final Message obtainMessage(int what) { return Message.obtain(this, what); }
    public final Message obtainMessage(int what, Object obj) { return Message.obtain(this, what, obj); }
    public final Message obtainMessage(int what, int a1, int a2) { return Message.obtain(this, what, a1, a2); }
    public final Message obtainMessage(int what, int a1, int a2, Object obj) { return Message.obtain(this, what, a1, a2, obj); }
    public Handler(Callback cb, boolean async) { this(cb); }
    public Handler(Looper l, Callback cb, boolean async) { this(l, cb); }
    public Handler(Looper l, Callback cb, boolean async, boolean shared) { this(l, cb); }
    public Handler(boolean async) { this(); }
    public static Handler createAsync(Looper l, Callback cb) { return new Handler(l, cb); }
    private static Handler sMain;
    public static synchronized Handler getMain() { if (sMain == null) sMain = new Handler(Looper.getMainLooper()); return sMain; }
    public static Handler mainIfNull(Handler h) { return h != null ? h : getMain(); }
    public final boolean postAtTime(Runnable r, Object token, long uptime) { Message m = Message.obtain(this, r); m.obj = token; return mLooper.mQueue.enqueue(m, uptime); }
    public final boolean postDelayed(Runnable r, int what, long ms) { Message m = Message.obtain(this, r); m.what = what; return mLooper.mQueue.enqueue(m, SystemClock.uptimeMillis() + Math.max(0, ms)); }
    public final boolean sendEmptyMessageAtTime(int what, long uptime) { return sendMessageAtTime(Message.obtain(this, what), uptime); }
    public final boolean sendMessageAtFrontOfQueue(Message m) { m.target = this; return mLooper.mQueue.enqueue(m, 0); }
    public final boolean executeOrSendMessage(Message m) { if (Looper.myLooper() == mLooper) { dispatchMessage(m); return true; } return sendMessage(m); }
    public final boolean hasCallbacks(Runnable r) { return mLooper.mQueue.hasCallback(this, r); }
    public final boolean hasMessages(int what, Object obj) { return mLooper.mQueue.has(this, what, obj, false); }
    public final boolean hasEqualMessages(int what, Object obj) { return mLooper.mQueue.has(this, what, obj, true); }
    public final boolean hasMessagesOrCallbacks() { return mLooper.mQueue.hasAny(this); }
    public final void removeEqualMessages(int what, Object obj) { removeMessages(what, obj); }
    public final void removeCallbacksAndEqualMessages(Object token) { removeCallbacksAndMessages(token); }
    public String getMessageName(Message m) { return m.callback != null ? m.callback.getClass().getName() : "0x" + Integer.toHexString(m.what); }
    public String getTraceName(Message m) { return getClass().getName() + ": " + getMessageName(m); }
    public final void dump(android.util.Printer pw, String prefix) { pw.println(prefix + this); }
    public final void dumpMine(android.util.Printer pw, String prefix) { dump(pw, prefix); }
    /** Runs r on this handler's thread and waits for it (Android's hidden runWithScissors). */
    public final boolean runWithScissors(Runnable r, long timeout) {
        if (r == null || timeout < 0) throw new IllegalArgumentException();
        if (Looper.myLooper() == mLooper) { r.run(); return true; }
        final boolean[] done = new boolean[1];
        if (!post(() -> { try { r.run(); } finally { synchronized (done) { done[0] = true; done.notifyAll(); } } })) return false;
        long end = SystemClock.uptimeMillis() + timeout;
        synchronized (done) {
            while (!done[0]) {
                long left = timeout > 0 ? end - SystemClock.uptimeMillis() : 0;
                if (timeout > 0 && left <= 0) return false;
                try { done.wait(left); } catch (InterruptedException e) { return false; }
            }
        }
        return true;
    }
    @Override public String toString() { return "Handler (" + getClass().getName() + ") {" + Integer.toHexString(System.identityHashCode(this)) + "}"; }
}
