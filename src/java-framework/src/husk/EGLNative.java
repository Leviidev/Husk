package husk;

/** EGL for the app's own threads (husk-tl-dvm-android.c): ANGLE contexts sharing the render thread's objects, window surfaces that
 *  are pbuffers the size of their SurfaceView, and a swap that puts the frame on screen where the view is. */
public final class EGLNative {
    public static native long createContext(int version, long share);
    public static native void destroyContext(long ctx);
    public static native long createSurface(int w, int h);
    public static native void destroySurface(long surf);
    public static native boolean makeCurrent(long surf, long ctx);
    public static native boolean swap(long surf, long ctx, int x, int y, int w, int h);
    public static native boolean isRenderThread();
    public static native void hide();
}
