package android.webkit;

public class ConsoleMessage {
    public enum MessageLevel { TIP, LOG, WARNING, ERROR, DEBUG }
    private final String mMessage, mSourceId;
    private final int mLineNumber;
    private final MessageLevel mLevel;
    public ConsoleMessage(String message, String sourceId, int lineNumber, MessageLevel level) { mMessage = message; mSourceId = sourceId; mLineNumber = lineNumber; mLevel = level; }
    public MessageLevel messageLevel() { return mLevel; }
    public String message() { return mMessage; }
    public String sourceId() { return mSourceId; }
    public int lineNumber() { return mLineNumber; }
}
