package android.opengl;
public class GLException extends RuntimeException { private final int mError; public GLException(int e) { super("0x" + Integer.toHexString(e)); mError = e; } public GLException(int e, String s) { super(s); mError = e; } int getError() { return mError; } }
