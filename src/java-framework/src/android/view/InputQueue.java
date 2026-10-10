package android.view;
public final class InputQueue {
    public interface Callback { void onInputQueueCreated(InputQueue q); void onInputQueueDestroyed(InputQueue q); }
    public interface FinishedInputEventCallback { void onFinishedInputEvent(Object token, boolean handled); }
}
