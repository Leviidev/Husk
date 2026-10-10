package javax.microedition.khronos.egl;

/** What apps ask EGL10 about the context Husk's GL thread made: an 8888 config with depth and stencil, the current context. */
final class HuskEGL implements EGL11 {
    static final class Display extends EGLDisplay {}
    static final class Config extends EGLConfig {}
    static final class Surface extends EGLSurface {}
    static final class Context extends EGLContext { public javax.microedition.khronos.opengles.GL getGL() { return null; } }
    static final Display DISPLAY = new Display();
    static final Config CONFIG = new Config();
    static final Surface SURFACE = new Surface();
    static final Context CONTEXT = new Context();
    public boolean eglChooseConfig(EGLDisplay d, int[] attribs, EGLConfig[] configs, int size, int[] num) { if (configs != null && size > 0) configs[0] = CONFIG; if (num != null) num[0] = 1; return true; }
    public boolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, int attr, int[] value) {
        int v;
        switch (attr) {
        case EGL_RED_SIZE: case EGL_GREEN_SIZE: case EGL_BLUE_SIZE: case EGL_ALPHA_SIZE: v = 8; break;
        case EGL_DEPTH_SIZE: v = 24; break; case EGL_STENCIL_SIZE: v = 8; break; case EGL_BUFFER_SIZE: v = 32; break;
        case EGL_RENDERABLE_TYPE: v = 4 | 64; break; case EGL_SURFACE_TYPE: v = EGL_WINDOW_BIT; break; default: v = 0;
        }
        value[0] = v; return true;
    }
    public boolean eglGetConfigs(EGLDisplay d, EGLConfig[] configs, int size, int[] num) { return eglChooseConfig(d, null, configs, size, num); }
    public EGLDisplay eglGetDisplay(Object n) { return DISPLAY; }
    public boolean eglInitialize(EGLDisplay d, int[] version) { if (version != null && version.length >= 2) { version[0] = 1; version[1] = 4; } return true; }
    public boolean eglTerminate(EGLDisplay d) { return true; }
    public EGLContext eglGetCurrentContext() { return CONTEXT; }
    public EGLDisplay eglGetCurrentDisplay() { return DISPLAY; }
    public EGLSurface eglGetCurrentSurface(int which) { return SURFACE; }
    public EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, int[] attribs) { return CONTEXT; }
    public boolean eglDestroyContext(EGLDisplay d, EGLContext c) { return true; }
    public EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, Object w, int[] attribs) { return SURFACE; }
    public boolean eglDestroySurface(EGLDisplay d, EGLSurface s) { return true; }
    public boolean eglMakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c) { return true; }
    public boolean eglSwapBuffers(EGLDisplay d, EGLSurface s) { return true; }
    public int eglGetError() { return EGL_SUCCESS; }
    public String eglQueryString(EGLDisplay d, int name) { return ""; }
    public boolean eglQuerySurface(EGLDisplay d, EGLSurface s, int attr, int[] value) {
        value[0] = attr == EGL_WIDTH ? husk.Native.screenWidth() : attr == EGL_HEIGHT ? husk.Native.screenHeight() : 0; return true;
    }
}
