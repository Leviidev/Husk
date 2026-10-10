package android.util;
public class AndroidRuntimeException extends RuntimeException { public AndroidRuntimeException() {} public AndroidRuntimeException(String s) { super(s); } public AndroidRuntimeException(String s, Throwable t) { super(s, t); } public AndroidRuntimeException(Exception e) { super(e); } }
