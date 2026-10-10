package android.webkit;

public class JsPromptResult extends JsResult {
    private String mStringResult;
    public JsPromptResult(ResultReceiver receiver) { super(receiver); }
    public void confirm(String result) { mStringResult = result; confirm(); }
    public String getStringResult() { return mStringResult; }
}
