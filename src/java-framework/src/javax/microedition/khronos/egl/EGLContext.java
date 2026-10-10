package javax.microedition.khronos.egl;
public abstract class EGLContext {
    private static final EGL10 EGL = new HuskEGL();
    public static EGL getEGL() { return EGL; }
    public abstract javax.microedition.khronos.opengles.GL getGL();
}
