package android.os;

public class Handler {
    public interface Callback { boolean handleMessage(Message msg); }
    final Looper mLooper;
    final Callback mCallback;
    public Handler() { this(Looper.myLooper() != null ? Looper.myLooper() : Looper.getMainLooper(), null); }
    public Handler(Callback cb) { this(Looper.myLooper() != null ? Looper.myLooper() : Looper.getMainLooper(), cb); }
    public Handler(Looper l) { this(l, null); }
    public Handler(Looper l, Callback cb) { mLooper = l; mCallback = cb; }
    public static Handler createAsync(Looper l) { return new Handler(l); }
    public final Looper getLooper() { return mLooper; }
    public void handleMessage(Message msg) {}
    public void dispatchMessage(Message msg) {
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
}
