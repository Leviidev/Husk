package javax.microedition.khronos.egl;
public interface EGL10 extends EGL {
    Object EGL_DEFAULT_DISPLAY = null;
    EGLContext EGL_NO_CONTEXT = null;
    EGLDisplay EGL_NO_DISPLAY = null;
    EGLSurface EGL_NO_SURFACE = null;
    int EGL_SUCCESS = 0x3000, EGL_BUFFER_SIZE = 0x3020, EGL_ALPHA_SIZE = 0x3021, EGL_BLUE_SIZE = 0x3022, EGL_GREEN_SIZE = 0x3023, EGL_RED_SIZE = 0x3024,
        EGL_DEPTH_SIZE = 0x3025, EGL_STENCIL_SIZE = 0x3026, EGL_CONFIG_CAVEAT = 0x3027, EGL_NONE = 0x3038, EGL_RENDERABLE_TYPE = 0x3040,
        EGL_SAMPLE_BUFFERS = 0x3032, EGL_SAMPLES = 0x3031, EGL_SURFACE_TYPE = 0x3033, EGL_WINDOW_BIT = 4, EGL_DRAW = 0x3059, EGL_READ = 0x305A,
        EGL_WIDTH = 0x3057, EGL_HEIGHT = 0x3056, EGL_BAD_SURFACE = 0x300D, EGL_CONTEXT_LOST = 0x300E;
    boolean eglChooseConfig(EGLDisplay d, int[] attribs, EGLConfig[] configs, int size, int[] num);
    boolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, int attr, int[] value);
    boolean eglGetConfigs(EGLDisplay d, EGLConfig[] configs, int size, int[] num);
    EGLDisplay eglGetDisplay(Object native_display);
    boolean eglInitialize(EGLDisplay d, int[] version);
    boolean eglTerminate(EGLDisplay d);
    EGLContext eglGetCurrentContext();
    EGLDisplay eglGetCurrentDisplay();
    EGLSurface eglGetCurrentSurface(int which);
    EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, int[] attribs);
    boolean eglDestroyContext(EGLDisplay d, EGLContext c);
    EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, Object w, int[] attribs);
    boolean eglDestroySurface(EGLDisplay d, EGLSurface s);
    boolean eglMakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c);
    boolean eglSwapBuffers(EGLDisplay d, EGLSurface s);
    int eglGetError();
    String eglQueryString(EGLDisplay d, int name);
    boolean eglQuerySurface(EGLDisplay d, EGLSurface s, int attr, int[] value);
}
