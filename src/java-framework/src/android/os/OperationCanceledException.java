package android.os;
public class OperationCanceledException extends RuntimeException { public OperationCanceledException() { this(null); } public OperationCanceledException(String m) { super(m != null ? m : "The operation has been canceled."); } }
