package android.view;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import java.util.ArrayList;

/** Frames at 60 Hz on the main looper: input is already in, then animations, then the window's traversal (ViewRoot). */
public final class Choreographer {
    public interface FrameCallback { void doFrame(long frameTimeNanos); }
    public static final int CALLBACK_INPUT = 0, CALLBACK_ANIMATION = 1, CALLBACK_INSETS_ANIMATION = 2, CALLBACK_TRAVERSAL = 3, CALLBACK_COMMIT = 4;
    private static final long FRAME_NS = 16_666_667L;
    private static final ThreadLocal<Choreographer> sThread = new ThreadLocal<>();
    private static Choreographer sMain;
    private final Handler mHandler;
    @SuppressWarnings("unchecked")
    private final ArrayList<Object[]>[] mQueues = new ArrayList[5];   /* {callback (Runnable or FrameCallback), token, dueNanos} */
    private boolean mScheduled;
    private long mLastFrame;
    private final Runnable mDoFrame = this::doFrame;

    private Choreographer(Looper l) { mHandler = new Handler(l); for (int i = 0; i < 5; i++) mQueues[i] = new ArrayList<>(); }
    public static Choreographer getInstance() {
        Choreographer c = sThread.get();
        if (c == null) {
            Looper l = Looper.myLooper() != null ? Looper.myLooper() : Looper.getMainLooper();
            if (l == Looper.getMainLooper() && sMain != null) c = sMain;
            else { c = new Choreographer(l); if (l == Looper.getMainLooper()) sMain = c; }
            sThread.set(c);
        }
        return c;
    }
    public static Choreographer getMainThreadInstance() { if (sMain == null) sMain = new Choreographer(Looper.getMainLooper()); return sMain; }
    public static long getFrameDelay() { return 16; }
    public static void setFrameDelay(long d) {}
    public long getFrameTime() { return mLastFrame / 1_000_000L; }
    public long getFrameTimeNanos() { return mLastFrame; }
    public long getFrameIntervalNanos() { return FRAME_NS; }

    private synchronized void add(int type, Object cb, Object token, long delayMs) {
        mQueues[type].add(new Object[] { cb, token, System.nanoTime() + delayMs * 1_000_000L });
        schedule();
    }
    private void schedule() {
        if (mScheduled) return;
        mScheduled = true;
        long now = System.nanoTime(), next = Math.max(now, mLastFrame + FRAME_NS);
        mHandler.postDelayed(mDoFrame, Math.max(0, (next - now) / 1_000_000L));
    }
    public void postCallback(int type, Runnable r, Object token) { add(type, r, token, 0); }
    public void postCallbackDelayed(int type, Runnable r, Object token, long delayMs) { add(type, r, token, delayMs); }
    public void removeCallbacks(int type, Runnable r, Object token) { remove(type, r, token); }
    public void postFrameCallback(FrameCallback cb) { add(CALLBACK_ANIMATION, cb, null, 0); }
    public void postFrameCallbackDelayed(FrameCallback cb, long delayMs) { add(CALLBACK_ANIMATION, cb, null, delayMs); }
    public void removeFrameCallback(FrameCallback cb) { remove(CALLBACK_ANIMATION, cb, null); }
    private synchronized void remove(int type, Object cb, Object token) {
        ArrayList<Object[]> q = mQueues[type];
        for (int i = q.size() - 1; i >= 0; i--) { Object[] e = q.get(i); if ((cb == null || e[0] == cb) && (token == null || e[1] == token)) q.remove(i); }
    }

    private void doFrame() {
        long now = System.nanoTime();
        synchronized (this) { mScheduled = false; }
        mLastFrame = now;
        boolean later = false;
        for (int type = 0; type < 5; type++) {
            ArrayList<Object[]> run = new ArrayList<>();
            synchronized (this) {
                ArrayList<Object[]> q = mQueues[type];
                for (int i = 0; i < q.size(); ) { Object[] e = q.get(i); if ((Long) e[2] <= now) { run.add(e); q.remove(i); } else { i++; later = true; } }
            }
            for (Object[] e : run) {
                if (e[0] instanceof FrameCallback) ((FrameCallback) e[0]).doFrame(now);
                else ((Runnable) e[0]).run();
            }
        }
        synchronized (this) {
            boolean any = later;
            for (ArrayList<Object[]> q : mQueues) any |= !q.isEmpty();
            if (any) schedule();
        }
    }
}
