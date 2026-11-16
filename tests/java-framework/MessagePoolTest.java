// SPDX-License-Identifier: GPL-2.0-or-later
package android.os;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.IdentityHashMap;
import sun.misc.Unsafe;

/** Actual obtain/queue/dispatch paths must reuse messages without retaining an APK's callbacks or payloads. */
public final class MessagePoolTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static void illegal(Runnable action, String message) {
        try { action.run(); throw new AssertionError(message); }
        catch (IllegalStateException expected) { }
    }

    private static Message due(MessageQueue queue, long now) throws Exception {
        Method poll = MessageQueue.class.getDeclaredMethod("pollDueLocked", long.class);
        poll.setAccessible(true);
        synchronized (queue) { return (Message) poll.invoke(queue, now); }
    }

    private static void dispatch(Message message) throws Exception {
        Method dispatch = Looper.class.getDeclaredMethod("dispatchAndRecycle", Message.class);
        dispatch.setAccessible(true);
        try { dispatch.invoke(null, message); }
        catch (InvocationTargetException failure) {
            Throwable cause = failure.getCause();
            if (cause instanceof RuntimeException) throw (RuntimeException) cause;
            if (cause instanceof Error) throw (Error) cause;
            throw failure;
        }
    }

    private static Messenger messengerWithoutNativeBinder() throws Exception {
        Field field = Unsafe.class.getDeclaredField("theUnsafe");
        field.setAccessible(true);
        return (Messenger) ((Unsafe) field.get(null)).allocateInstance(Messenger.class);
    }

    private static void clearAndReuse() throws Exception {
        Message message = Message.obtain();
        message.recycle();
        Message reused = Message.obtain();
        check(reused == message, "Message.obtain must reuse a recycled Message rather than allocating another object");
        Handler handler = new Handler((Looper) null);
        reused.what = 9; reused.arg1 = 3; reused.arg2 = 7; reused.obj = new Object();
        reused.target = handler; reused.callback = () -> { }; reused.when = 999;
        reused.setData(new Bundle()); reused.getData().putString("session", "old APK");
        reused.replyTo = messengerWithoutNativeBinder(); reused.sendingUid = 12; reused.workSourceUid = 13;
        reused.heapIndex = 6; reused.insertSeq = 50; reused.setAsynchronous(true);
        reused.recycle();
        illegal(reused::recycle, "double recycling must be rejected");
        Message next = Message.obtain();
        check(next == reused, "payload-bearing message returns to the pool");
        check(next.what == 0 && next.arg1 == 0 && next.arg2 == 0 && next.obj == null, "numeric and object payloads cleared");
        check(next.target == null && next.callback == null && next.replyTo == null && next.peekData() == null,
              "APK handlers, callbacks, Messenger and Bundle are not retained");
        check(!next.isAsynchronous() && next.when == 0 && next.getWhen() == 0 && next.next == null,
              "async, timing and queue links reset on obtain");
        check(next.sendingUid == Message.UID_NONE && next.workSourceUid == Message.UID_NONE && next.heapIndex == -1 && next.insertSeq == 0,
              "sender and queue metadata reset");
        next.recycle();
    }

    private static void queueOwnershipAndRemoval() throws Exception {
        MessageQueue queue = new MessageQueue();
        Handler first = new Handler((Looper) null), second = new Handler((Looper) null);
        Object tokenA = new Object(), tokenB = new Object();
        Message[] messages = new Message[6];
        for (int i = 0; i < messages.length; i++) {
            boolean remove = i == 0 || i == 2 || i == 5;
            messages[i] = Message.obtain(i == 3 ? second : first, remove || i == 3 ? 7 : 8, remove || i == 3 ? tokenA : tokenB);
            check(queue.enqueue(messages[i], 10 + i), "enqueue succeeds");
        }
        illegal(() -> queue.enqueue(messages[0], 10), "a queued message cannot be enqueued twice");
        illegal(messages[0]::recycle, "a queued message cannot be recycled by its sender");
        queue.remove(first, null, 7, true, tokenA);
        check(messages[0].target == null && messages[2].obj == null && messages[5].target == null,
              "removed head/middle/tail messages release payloads");
        check(due(queue, 100) == messages[1] && due(queue, 100) == messages[3] && due(queue, 100) == messages[4],
              "removal preserves the remaining queue order instead of following recycled pool links");
        check(due(queue, 100) == null, "queue is exhausted once");
        dispatch(messages[1]); dispatch(messages[3]); dispatch(messages[4]);

        Runnable callback = () -> { };
        Message kept = Message.obtain(first, callback), removed = Message.obtain(first, callback);
        kept.obj = tokenB; removed.obj = tokenA;
        queue.enqueue(kept, 20); queue.enqueue(removed, 20);
        queue.remove(first, callback, 0, false, tokenA);
        check(due(queue, 19) == null && due(queue, 20) == kept && due(queue, 20) == null, "callback/token removal and due times remain correct");
        dispatch(kept);
        check(removed.callback == null && removed.target == null && removed.obj == null, "removed callback message is cleared");

        Message pending = Message.obtain(first, callback); pending.setData(new Bundle()); pending.obj = tokenA;
        queue.enqueue(pending, 1000); queue.quit();
        check(!queue.hasAny(first) && pending.callback == null && pending.target == null && pending.peekData() == null,
              "quitting releases all queued session references");
        Message rejected = Message.obtain(first, callback); rejected.obj = tokenA;
        check(!queue.enqueue(rejected, 1) && rejected.target == null && rejected.callback == null && rejected.obj == null,
              "failed enqueue on a quit queue recycles and clears the message");
    }

    private static void dispatchFinally() throws Exception {
        MessageQueue queue = new MessageQueue();
        Handler handler = new Handler((Looper) null);
        final int[] delivered = {0};
        Message normal = Message.obtain(handler, () -> delivered[0]++);
        queue.enqueue(normal, 1); dispatch(due(queue, 1));
        check(delivered[0] == 1 && normal.callback == null && normal.target == null, "normal callback dispatch recycles in the actual Looper path");
        Message next = Message.obtain(); check(next == normal, "normal dispatch is reusable"); next.recycle();

        RuntimeException marker = new RuntimeException("callback failure");
        Message failed = Message.obtain(handler, () -> { throw marker; }); failed.obj = new Object(); failed.setData(new Bundle());
        queue.enqueue(failed, 2);
        try { dispatch(due(queue, 2)); throw new AssertionError("callback failure must propagate"); }
        catch (RuntimeException expected) { check(expected == marker, "original callback exception preserved"); }
        next = Message.obtain();
        check(next == failed && next.target == null && next.callback == null && next.obj == null && next.peekData() == null,
              "throwing callbacks still return cleared messages through finally");
        next.recycle();
    }

    private static void boundedPool() {
        Message[] batch = new Message[70];
        IdentityHashMap<Message, Boolean> original = new IdentityHashMap<>();
        for (int i = 0; i < batch.length; i++) { batch[i] = Message.obtain(); original.put(batch[i], Boolean.TRUE); }
        for (Message message : batch) message.recycle();
        int reused = 0;
        for (int i = 0; i < batch.length; i++) { batch[i] = Message.obtain(); if (original.containsKey(batch[i])) reused++; }
        check(reused == 50, "the retained Message pool is bounded at 50");
        for (Message message : batch) message.recycle();
    }

    private static void concurrentReuse() throws Exception {
        IdentityHashMap<Message, Boolean> active = new IdentityHashMap<>();
        Throwable[] failed = new Throwable[4]; Thread[] threads = new Thread[4];
        for (int worker = 0; worker < threads.length; worker++) {
            final int index = worker;
            threads[worker] = new Thread(() -> {
                try {
                    for (int i = 0; i < 10000; i++) {
                        Message message = Message.obtain();
                        synchronized (active) { check(!active.containsKey(message), "two threads borrowed the same pooled message"); active.put(message, Boolean.TRUE); }
                        message.arg1 = index; message.arg2 = i; message.setAsynchronous(true);
                        synchronized (active) { active.remove(message); }
                        message.recycle();
                    }
                } catch (Throwable error) { failed[index] = error; }
            });
            threads[worker].start();
        }
        for (Thread thread : threads) thread.join();
        for (Throwable error : failed) if (error != null) throw new AssertionError("concurrent pool reuse", error);
    }

    private static void copyContracts() {
        Handler sourceHandler = new Handler((Looper) null), destinationHandler = new Handler((Looper) null);
        Runnable sourceCallback = () -> { }, destinationCallback = () -> { };
        Message original = Message.obtain(sourceHandler, sourceCallback);
        original.what = 4; original.arg1 = 5; original.arg2 = 6; original.obj = new Object();
        original.getData().putString("value", "original"); original.setAsynchronous(true);
        MessageQueue queue = new MessageQueue(); queue.enqueue(original, 1234);

        Message copy = Message.obtain(original);
        check(!copy.isInUse() && !copy.isAsynchronous() && copy.when == 0, "obtain(copy) does not copy queued/async flags or scheduling time");
        check(copy.target == sourceHandler && copy.callback == sourceCallback && copy.obj == original.obj,
              "obtain(copy) preserves routing, callback and object payload");
        check(copy.what == 4 && copy.arg1 == 5 && copy.arg2 == 6, "obtain(copy) preserves numeric payload");
        copy.getData().putString("value", "copy");
        check("original".equals(original.getData().getString("value")), "copied Bundle is independent");
        copy.recycle();

        Message destination = Message.obtain(destinationHandler, destinationCallback);
        destination.when = 9000; destination.copyFrom(original);
        check(!destination.isInUse() && destination.isAsynchronous(), "copyFrom preserves async but removes source queue ownership");
        check(destination.target == destinationHandler && destination.callback == destinationCallback && destination.when == 9000,
              "copyFrom preserves destination routing, callback and scheduling time");
        check(destination.obj == original.obj && destination.peekData() != original.peekData(), "copyFrom shares object payload and clones Bundle");
        destination.recycle(); queue.quit();
    }

    public static void main(String[] args) throws Exception {
        clearAndReuse(); queueOwnershipAndRemoval(); dispatchFinally(); boundedPool(); copyContracts(); concurrentReuse();
        System.out.println("PASS: actual Message obtain/queue/dispatch paths reuse a bounded pool, clear session payloads, reject in-use misuse, recycle removals/quit and throwing callbacks, and remain exclusive across four threads");
    }
}
