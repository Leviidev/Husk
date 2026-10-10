package javax.microedition.khronos.egl;
public abstract class EGLContext {
    private static final EGL10 EGL = new HuskEGL();
    public static EGL getEGL() { return EGL; }
    public abstract javax.microedition.khronos.opengles.GL getGL();
    /** @hide The EGL_CONTEXT_CLIENT_VERSION the app last asked eglCreateContext for (1 when it never said). */
    public static int huskRequestedVersion() { return HuskEGL.sRequestedVersion; }
}
