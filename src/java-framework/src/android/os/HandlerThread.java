package android.os;

public class HandlerThread extends Thread {
    private Looper mLooper;
    public HandlerThread(String name) { super(name); }
    public HandlerThread(String name, int priority) { super(name); }
    protected void onLooperPrepared() {}
    @Override public void run() {
        Looper.prepare();
        synchronized (this) { mLooper = Looper.myLooper(); notifyAll(); }
        onLooperPrepared();
        Looper.loop();
    }
    public Looper getLooper() {
        synchronized (this) { while (mLooper == null) { try { wait(); } catch (InterruptedException e) { } } }
        return mLooper;
    }
    public boolean quit() { Looper l = getLooper(); if (l != null) l.quit(); return l != null; }
    public boolean quitSafely() { return quit(); }
}
