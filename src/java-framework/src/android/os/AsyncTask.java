package android.os;

import java.util.ArrayDeque;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/** Work on a background thread with its results delivered on the main thread, as Android's. */
@Deprecated
public abstract class AsyncTask<Params, Progress, Result> {
    private static final String LOG_TAG = "AsyncTask";
    private static final int CORE_POOL_SIZE = 1, MAXIMUM_POOL_SIZE = 20, KEEP_ALIVE_SECONDS = 3;
    private static final ThreadFactory sThreadFactory = new ThreadFactory() {
        private final AtomicInteger mCount = new AtomicInteger(1);
        public Thread newThread(Runnable r) { return new Thread(r, "AsyncTask #" + mCount.getAndIncrement()); }
    };
    private static ThreadPoolExecutor sBackup;
    @Deprecated public static final Executor THREAD_POOL_EXECUTOR;
    static {
        ThreadPoolExecutor e = new ThreadPoolExecutor(CORE_POOL_SIZE, MAXIMUM_POOL_SIZE, KEEP_ALIVE_SECONDS, TimeUnit.SECONDS, new SynchronousQueue<Runnable>(), sThreadFactory);
        e.setRejectedExecutionHandler((r, ex) -> {
            synchronized (AsyncTask.class) {
                if (sBackup == null) { sBackup = new ThreadPoolExecutor(5, 5, KEEP_ALIVE_SECONDS, TimeUnit.SECONDS, new LinkedBlockingQueue<Runnable>(), sThreadFactory); sBackup.allowCoreThreadTimeOut(true); }
            }
            sBackup.execute(r);
        });
        THREAD_POOL_EXECUTOR = e;
    }
    @Deprecated public static final Executor SERIAL_EXECUTOR = new SerialExecutor();
    private static final int MESSAGE_POST_RESULT = 0x1, MESSAGE_POST_PROGRESS = 0x2;
    private static volatile Executor sDefaultExecutor = SERIAL_EXECUTOR;
    private static InternalHandler sHandler;

    private final WorkerRunnable<Params, Result> mWorker;
    private final FutureTask<Result> mFuture;
    private volatile Status mStatus = Status.PENDING;
    private final AtomicBoolean mCancelled = new AtomicBoolean();
    private final AtomicBoolean mTaskInvoked = new AtomicBoolean();
    private final Handler mHandler;

    private static class SerialExecutor implements Executor {
        final ArrayDeque<Runnable> mTasks = new ArrayDeque<Runnable>();
        Runnable mActive;
        public synchronized void execute(final Runnable r) {
            mTasks.offer(() -> { try { r.run(); } finally { scheduleNext(); } });
            if (mActive == null) scheduleNext();
        }
        protected synchronized void scheduleNext() { if ((mActive = mTasks.poll()) != null) THREAD_POOL_EXECUTOR.execute(mActive); }
    }

    public enum Status { PENDING, RUNNING, FINISHED }

    private static Handler getMainHandler() {
        synchronized (AsyncTask.class) { if (sHandler == null) sHandler = new InternalHandler(Looper.getMainLooper()); return sHandler; }
    }
    public static void setDefaultExecutor(Executor exec) { sDefaultExecutor = exec; }

    public AsyncTask() { this((Looper) null); }
    public AsyncTask(Handler handler) { this(handler != null ? handler.getLooper() : null); }
    public AsyncTask(Looper callbackLooper) {
        mHandler = callbackLooper == null || callbackLooper == Looper.getMainLooper() ? getMainHandler() : new Handler(callbackLooper);
        mWorker = new WorkerRunnable<Params, Result>() {
            public Result call() throws Exception {
                mTaskInvoked.set(true);
                Result result = null;
                try {
                    Process.setThreadPriority(Process.THREAD_PRIORITY_BACKGROUND);
                    result = doInBackground(mParams);
                    Binder.flushPendingCommands();
                } catch (Throwable tr) {
                    mCancelled.set(true);
                    throw tr;
                } finally {
                    postResult(result);
                }
                return result;
            }
        };
        mFuture = new FutureTask<Result>(mWorker) {
            @Override protected void done() {
                try { postResultIfNotInvoked(get()); }
                catch (InterruptedException e) { android.util.Log.w(LOG_TAG, e); }
                catch (ExecutionException e) { throw new RuntimeException("An error occurred while executing doInBackground()", e.getCause()); }
                catch (CancellationException e) { postResultIfNotInvoked(null); }
            }
        };
    }
    private void postResultIfNotInvoked(Result result) { if (!mTaskInvoked.get()) postResult(result); }
    private Result postResult(Result result) {
        Message message = mHandler.obtainMessage(MESSAGE_POST_RESULT, new AsyncTaskResult<Result>(this, result));
        message.sendToTarget();
        return result;
    }
    public final Status getStatus() { return mStatus; }
    protected abstract Result doInBackground(Params... params);
    protected void onPreExecute() {}
    protected void onPostExecute(Result result) {}
    protected void onProgressUpdate(Progress... values) {}
    protected void onCancelled(Result result) { onCancelled(); }
    protected void onCancelled() {}
    public final boolean isCancelled() { return mCancelled.get(); }
    public final boolean cancel(boolean mayInterruptIfRunning) { mCancelled.set(true); return mFuture.cancel(mayInterruptIfRunning); }
    public final Result get() throws InterruptedException, ExecutionException { return mFuture.get(); }
    public final Result get(long timeout, TimeUnit unit) throws InterruptedException, ExecutionException, TimeoutException { return mFuture.get(timeout, unit); }
    @SafeVarargs public final AsyncTask<Params, Progress, Result> execute(Params... params) { return executeOnExecutor(sDefaultExecutor, params); }
    @SafeVarargs public final AsyncTask<Params, Progress, Result> executeOnExecutor(Executor exec, Params... params) {
        if (mStatus != Status.PENDING) {
            switch (mStatus) {
            case RUNNING: throw new IllegalStateException("Cannot execute task: the task is already running.");
            case FINISHED: throw new IllegalStateException("Cannot execute task: the task has already been executed (a task can be executed only once)");
            default: break;
            }
        }
        mStatus = Status.RUNNING;
        onPreExecute();
        mWorker.mParams = params;
        exec.execute(mFuture);
        return this;
    }
    public static void execute(Runnable runnable) { sDefaultExecutor.execute(runnable); }
    @SafeVarargs protected final void publishProgress(Progress... values) {
        if (!isCancelled()) mHandler.obtainMessage(MESSAGE_POST_PROGRESS, new AsyncTaskResult<Progress>(this, values)).sendToTarget();
    }
    private void finish(Result result) {
        if (isCancelled()) onCancelled(result); else onPostExecute(result);
        mStatus = Status.FINISHED;
    }
    private static class InternalHandler extends Handler {
        InternalHandler(Looper looper) { super(looper); }
        @SuppressWarnings({"unchecked", "rawtypes"})
        @Override public void handleMessage(Message msg) {
            AsyncTaskResult<?> result = (AsyncTaskResult<?>) msg.obj;
            switch (msg.what) {
            case MESSAGE_POST_RESULT: result.mTask.finish(result.mData[0]); break;
            case MESSAGE_POST_PROGRESS: result.mTask.onProgressUpdate(result.mData); break;
            }
        }
    }
    private static abstract class WorkerRunnable<Params, Result> implements Callable<Result> { Params[] mParams; }
    @SuppressWarnings({"rawtypes"})
    private static class AsyncTaskResult<Data> {
        final AsyncTask mTask;
        final Data[] mData;
        @SafeVarargs AsyncTaskResult(AsyncTask task, Data... data) { mTask = task; mData = data; }
    }
}
