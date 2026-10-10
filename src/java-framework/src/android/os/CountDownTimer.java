package android.os;

/** Ticks every countDownInterval until millisInFuture has passed, on the thread that started it. */
public abstract class CountDownTimer {
    private final long mMillisInFuture, mCountdownInterval;
    private long mStopTimeInFuture;
    private boolean mCancelled = false;
    private static final int MSG = 1;
    public CountDownTimer(long millisInFuture, long countDownInterval) { mMillisInFuture = millisInFuture; mCountdownInterval = countDownInterval; }
    public synchronized final void cancel() { mCancelled = true; mHandler.removeMessages(MSG); }
    public synchronized final CountDownTimer start() {
        mCancelled = false;
        if (mMillisInFuture <= 0) { onFinish(); return this; }
        mStopTimeInFuture = SystemClock.elapsedRealtime() + mMillisInFuture;
        mHandler.sendMessage(mHandler.obtainMessage(MSG));
        return this;
    }
    public abstract void onTick(long millisUntilFinished);
    public abstract void onFinish();
    private final Handler mHandler = new Handler() {
        @Override public void handleMessage(Message msg) {
            synchronized (CountDownTimer.this) {
                if (mCancelled) return;
                final long millisLeft = mStopTimeInFuture - SystemClock.elapsedRealtime();
                if (millisLeft <= 0) { onFinish(); return; }
                long lastTickStart = SystemClock.elapsedRealtime();
                onTick(millisLeft);
                long lastTickDuration = SystemClock.elapsedRealtime() - lastTickStart;
                long delay;
                if (millisLeft < mCountdownInterval) { delay = millisLeft - lastTickDuration; if (delay < 0) delay = 0; }
                else { delay = mCountdownInterval - lastTickDuration; while (delay < 0) delay += mCountdownInterval; }
                sendMessageDelayed(obtainMessage(MSG), delay);
            }
        }
    };
}
