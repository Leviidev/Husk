package android.accounts;

public interface AccountManagerFuture<V> {
    boolean cancel(boolean mayInterruptIfRunning);
    boolean isCancelled();
    boolean isDone();
    V getResult() throws OperationCanceledException, java.io.IOException, AuthenticatorException;
    V getResult(long timeout, java.util.concurrent.TimeUnit unit) throws OperationCanceledException, java.io.IOException, AuthenticatorException;
}
