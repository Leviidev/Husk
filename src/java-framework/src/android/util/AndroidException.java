package android.util;
public class AndroidException extends Exception { public AndroidException() {} public AndroidException(String s) { super(s); } public AndroidException(String s, Throwable t) { super(s, t); } public AndroidException(Exception e) { super(e); }
}
