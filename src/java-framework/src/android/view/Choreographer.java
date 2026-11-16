package android.view;

import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

/** Frames at 60 Hz on the main looper: input is already in, then animations, then the window's traversal (ViewRoot). */
public final class Choreographer {
    public interface FrameCallback { void doFrame(long frameTimeNanos); }
    public static final int CALLBACK_INPUT = 0, CALLBACK_ANIMATION = 1, CALLBACK_INSETS_ANIMATION = 2, CALLBACK_TRAVERSAL = 3, CALLBACK_COMMIT = 4;
    private static final long FRAME_NS = 16_666_667L;
    private static ThreadLocal<Choreographer> sThread = new ThreadLocal<>();
    private static Choreographer sMain;
    public static synchronized void resetSession() {
        sMain = null;
        sThread = new ThreadLocal<>();
    }
    private final Handler mHandler;
    private static final int MAX_CALLBACK_POOL = 64;
    private static final class CallbackRecord {
        Object callback, token;
        long dueNanos;
        CallbackRecord next;
    }
    private final CallbackRecord[] mHeads = new CallbackRecord[5], mTails = new CallbackRecord[5];
    private CallbackRecord mCallbackPool;
    private int mCallbackPoolSize;
    private boolean mScheduled;
    private long mLastFrameTimeNanos;
    private final Runnable mDoFrame = this::doFrame;

    private Choreographer(Looper l) { mHandler = new Handler(l); }
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
    public long getFrameTime() { return mLastFrameTimeNanos / 1_000_000L; }
    public long getFrameTimeNanos() { return mLastFrameTimeNanos; }
    public long getFrameIntervalNanos() { return FRAME_NS; }

    private synchronized void add(int type, Object cb, Object token, long delayMs) {
        if (type < 0 || type >= mHeads.length || cb == null) throw new IllegalArgumentException("Invalid frame callback");
        CallbackRecord record;
        if (mCallbackPool != null) {
            record = mCallbackPool; mCallbackPool = record.next; mCallbackPoolSize--;
        } else record = new CallbackRecord();
        record.callback = cb; record.token = token;
        record.dueNanos = System.nanoTime() + delayMs * 1_000_000L;
        record.next = null;
        if (mTails[type] == null) mHeads[type] = record;
        else mTails[type].next = record;
        mTails[type] = record;
        schedule();
    }
    private void schedule() {
        if (mScheduled) return;
        mScheduled = true;
        long now = System.nanoTime(), next = Math.max(now, mLastFrameTimeNanos + FRAME_NS);
        mHandler.postDelayed(mDoFrame, Math.max(0, (next - now) / 1_000_000L));
    }
    public void postCallback(int type, Runnable r, Object token) { add(type, r, token, 0); }
    public void postCallbackDelayed(int type, Runnable r, Object token, long delayMs) { add(type, r, token, delayMs); }
    public void removeCallbacks(int type, Runnable r, Object token) { remove(type, r, token); }
    public void postFrameCallback(FrameCallback cb) { add(CALLBACK_ANIMATION, cb, null, 0); }
    public void postFrameCallbackDelayed(FrameCallback cb, long delayMs) { add(CALLBACK_ANIMATION, cb, null, delayMs); }
    public void removeFrameCallback(FrameCallback cb) { remove(CALLBACK_ANIMATION, cb, null); }
    private synchronized void remove(int type, Object cb, Object token) {
        CallbackRecord previous = null, record = mHeads[type];
        while (record != null) {
            CallbackRecord next = record.next;
            if ((cb == null || record.callback == cb) && (token == null || record.token == token)) {
                if (previous == null) mHeads[type] = next;
                else previous.next = next;
                if (mTails[type] == record) mTails[type] = previous;
                recycle(record);
            } else previous = record;
            record = next;
        }
    }

    private synchronized void recycle(CallbackRecord record) {
        record.callback = record.token = null;
        record.dueNanos = 0;
        record.next = null;
        if (mCallbackPoolSize < MAX_CALLBACK_POOL) {
            record.next = mCallbackPool; mCallbackPool = record; mCallbackPoolSize++;
        }
    }

    /** Detach only this phase's due work; callbacks posted while it runs stay queued. */
    private synchronized CallbackRecord takeDue(int type, long now) {
        CallbackRecord run = null, tail = null, previous = null, record = mHeads[type];
        while (record != null) {
            CallbackRecord next = record.next;
            if (record.dueNanos <= now) {
                if (previous == null) mHeads[type] = next;
                else previous.next = next;
                if (mTails[type] == record) mTails[type] = previous;
                record.next = null;
                if (tail == null) run = record;
                else tail.next = record;
                tail = record;
            } else previous = record;
            record = next;
        }
        return run;
    }

    private void doFrame() {
        long now = System.nanoTime();
        synchronized (this) { mScheduled = false; mLastFrameTimeNanos = now; }
        for (int type = 0; type < 5; type++) {
            CallbackRecord run = takeDue(type, now);
            try {
                while (run != null) {
                    CallbackRecord record = run;
                    run = run.next;
                    try {
                        if (record.callback instanceof FrameCallback) ((FrameCallback) record.callback).doFrame(now);
                        else ((Runnable) record.callback).run();
                    } finally { recycle(record); }
                }
            } finally {
                // A failed callback must not retain the rest of its detached batch.
                while (run != null) { CallbackRecord next = run.next; recycle(run); run = next; }
            }
        }
        synchronized (this) {
            boolean any = false;
            for (CallbackRecord head : mHeads) any |= head != null;
            if (any) schedule();
        }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static android.view.Choreographer getSfInstance() { return null; }
    public static void releaseInstance() {}
    public static long subtractFrameDelay(long p0) { return 0L; }
    public long getExpectedPresentationTimeMillis() { return 0L; }
    public long getFrameDeadline() { return 0L; }
    public long getLastFrameTimeNanos() { return 0L; }
    public long getLatestExpectedPresentTimeNanos() { return 0L; }
    public android.os.Looper getLooper() { return null; }
    public void onWaitForBufferRelease(long p0) {}
    public void postVsyncCallback(android.view.Choreographer.VsyncCallback p0) {}
    public void removeVsyncCallback(android.view.Choreographer.VsyncCallback p0) {}
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static class FrameData {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public long getFrameTimeNanos() { return (huskProps.get("FrameTimeNanos") instanceof Long ? (Long) huskProps.get("FrameTimeNanos") : 0L); }
        public android.view.Choreographer.FrameTimeline[] getFrameTimelines() { return (android.view.Choreographer.FrameTimeline[]) huskProps.get("FrameTimelines"); }
        public android.view.Choreographer.FrameTimeline getPreferredFrameTimeline() { return (android.view.Choreographer.FrameTimeline) huskProps.get("PreferredFrameTimeline"); }
        protected FrameData() {}
    }
    public static class FrameTimeline {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public long getDeadlineNanos() { return (huskProps.get("DeadlineNanos") instanceof Long ? (Long) huskProps.get("DeadlineNanos") : 0L); }
        public long getExpectedPresentationTimeNanos() { return (huskProps.get("ExpectedPresentationTimeNanos") instanceof Long ? (Long) huskProps.get("ExpectedPresentationTimeNanos") : 0L); }
        public long getVsyncId() { return (huskProps.get("VsyncId") instanceof Long ? (Long) huskProps.get("VsyncId") : 0L); }
        protected FrameTimeline() {}
    }
    public interface VsyncCallback {
        void onVsync(android.view.Choreographer.FrameData p0);
    }
    // ---- end of generated nested classes
}
