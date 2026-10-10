package android.os;

public class ConditionVariable {
    private volatile boolean mCondition;
    public ConditionVariable() { mCondition = false; }
    public ConditionVariable(boolean state) { mCondition = state; }
    public void open() { synchronized (this) { boolean old = mCondition; mCondition = true; if (!old) notifyAll(); } }
    public void close() { synchronized (this) { mCondition = false; } }
    public void block() { synchronized (this) { while (!mCondition) { try { wait(); } catch (InterruptedException e) {} } } }
    public boolean block(long timeoutMs) {
        if (timeoutMs == 0) { block(); return true; }
        synchronized (this) {
            long now = SystemClock.elapsedRealtime(), end = now + timeoutMs;
            while (!mCondition && now < end) { try { wait(end - now); } catch (InterruptedException e) {} now = SystemClock.elapsedRealtime(); }
            return mCondition;
        }
    }
}
