// SPDX-License-Identifier: GPL-2.0-or-later
package android.view;

import android.os.Looper;
import java.lang.reflect.*;
import java.util.ArrayList;
import java.util.List;

/** Real callback dispatch with the host wake boundary represented as already scheduled. */
public final class ChoreographerCallbackPoolTest {
    private static final Field scheduled;
    private static final Method frame;
    static {
        try {
            scheduled = Choreographer.class.getDeclaredField("mScheduled"); scheduled.setAccessible(true);
            frame = Choreographer.class.getDeclaredMethod("doFrame"); frame.setAccessible(true);
        } catch (Exception e) { throw new AssertionError(e); }
    }
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    private static Choreographer make() throws Exception {
        Constructor<Looper> l = Looper.class.getDeclaredConstructor(); l.setAccessible(true);
        Constructor<Choreographer> c = Choreographer.class.getDeclaredConstructor(Looper.class); c.setAccessible(true);
        return c.newInstance(l.newInstance());
    }
    private static void post(Choreographer c, int type, Runnable r, Object token, long delay) {
        try { scheduled.setBoolean(c, true); } catch (Exception e) { throw new AssertionError(e); }
        c.postCallbackDelayed(type, r, token, delay);
    }
    private static void runFrame(Choreographer c) throws Exception { frame.invoke(c); }
    private static Object firstRecord(Choreographer c, int type) throws Exception {
        try {
            Field f = Choreographer.class.getDeclaredField("mHeads"); f.setAccessible(true);
            return ((Object[]) f.get(c))[type];
        } catch (NoSuchFieldException oldImplementation) {
            Field f = Choreographer.class.getDeclaredField("mQueues"); f.setAccessible(true);
            return ((ArrayList<?>[]) f.get(c))[type].get(0);
        }
    }
    public static void main(String[] args) throws Exception {
        Choreographer c = make();
        final int[] hits = {0};
        Runnable tick = () -> hits[0]++;
        Object first = null;
        for (int i = 0; i < 10000; i++) {
            post(c, Choreographer.CALLBACK_ANIMATION, tick, null, 0);
            Object record = firstRecord(c, Choreographer.CALLBACK_ANIMATION);
            if (first == null) first = record;
            else check(record == first, "steady frame callbacks must reuse their cleared record, not retain a new packet each frame");
            runFrame(c);
        }
        check(hits[0] == 10000, "pool reuse preserves every callback");
        Field callback = first.getClass().getDeclaredField("callback"); callback.setAccessible(true);
        Field token = first.getClass().getDeclaredField("token"); token.setAccessible(true);
        check(callback.get(first) == null && token.get(first) == null, "pooled records release callbacks and tokens");

        List<Integer> order = new ArrayList<>();
        Object cancelled = new Object();
        post(c, 0, () -> {
            order.add(0);
            post(c, 0, () -> order.add(10), null, 0);
            post(c, 4, () -> order.add(4), null, 0);
        }, null, 0);
        post(c, 1, () -> order.add(1), null, 0);
        post(c, 2, () -> order.add(99), cancelled, 0);
        c.removeCallbacks(2, null, cancelled);
        runFrame(c);
        check(order.equals(java.util.Arrays.asList(0, 1)), "a frame runs due callbacks in phase order, cancelled work stays cancelled, and newly posted callbacks await their due frame time");
        runFrame(c);
        check(order.equals(java.util.Arrays.asList(0, 1, 10, 4)), "callbacks posted during dispatch run once on the next frame");

        post(c, 3, () -> hits[0] += 100000, cancelled, 60000);
        post(c, 3, () -> {
            hits[0]++;
            // The standalone driver supplies the future timer wake; no native clock is installed.
            try { scheduled.setBoolean(c, true); } catch (Exception e) { throw new AssertionError(e); }
        }, null, 0);
        runFrame(c);
        check(hits[0] == 10001, "a delayed record does not block a later due record in the same phase");
        check(firstRecord(c, 3) != null, "future work remains queued");
        c.removeCallbacks(3, null, cancelled);
        final long[] frameTime = {0};
        Choreographer.FrameCallback animation = t -> frameTime[0] = t;
        scheduled.setBoolean(c, true);
        c.postFrameCallback(animation);
        runFrame(c);
        check(frameTime[0] == c.getFrameTimeNanos() && frameTime[0] > 0, "FrameCallback receives this frame's timestamp");

        for (int i = 0; i < 150; i++) post(c, 2, tick, new Object(), 0);
        runFrame(c);
        Field size = Choreographer.class.getDeclaredField("mCallbackPoolSize"); size.setAccessible(true);
        check(size.getInt(c) == 64, "a large callback burst leaves a bounded pool instead of retaining every record");

        post(c, 0, () -> { throw new IllegalStateException("expected"); }, null, 0);
        post(c, 0, tick, new Object(), 0);
        try { runFrame(c); throw new AssertionError("callback failure must propagate"); }
        catch (InvocationTargetException expected) { check(expected.getCause() instanceof IllegalStateException, "original callback failure propagates"); }
        check(firstRecordOrNull(c, 0) == null, "detached callbacks are not accidentally requeued after an exception");
        System.out.println("PASS: callback records remain bounded across 10000 frames, clear retained references, preserve phase/time/cancellation/reentrant dispatch and recycle after callback failure");
    }
    private static Object firstRecordOrNull(Choreographer c, int type) throws Exception {
        try { return firstRecord(c, type); }
        catch (IndexOutOfBoundsException empty) { return null; }
    }
}
