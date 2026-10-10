package android.webkit;

/** The answer to a page's alert / confirm (and, as JsPromptResult, prompt): given once, by confirm or cancel. */
public class JsResult {
    public interface ResultReceiver { void onJsResultComplete(JsResult result); }
    private final ResultReceiver mReceiver;
    private boolean mResult, mDone;
    public JsResult(ResultReceiver receiver) { mReceiver = receiver; }
    public final void cancel() { mResult = false; wakeUp(); }
    public final void confirm() { mResult = true; wakeUp(); }
    public final boolean getResult() { return mResult; }
    final boolean huskDone() { return mDone; }
    private void wakeUp() { if (mDone) return; mDone = true; if (mReceiver != null) mReceiver.onJsResultComplete(this); }
}
