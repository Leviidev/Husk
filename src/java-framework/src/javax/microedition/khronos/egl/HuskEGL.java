package javax.microedition.khronos.egl;

/**
 * EGL10/EGL11. On Husk's render thread (where GLSurfaceView's renderer runs) it answers for the context that thread made. On any
 * other thread it is real: contexts and window surfaces from husk.EGLNative, so an app that drives EGL itself (a GLSurfaceView copied
 * into the app, an engine's own GL thread) draws, and each swap shows its frame where its SurfaceView is.
 */
final class HuskEGL implements EGL11 {
    static final class Display extends EGLDisplay {}
    static final class Config extends EGLConfig {}
    static final class Surface extends EGLSurface {
        long handle; int w, h; android.view.SurfaceView view;
    }
    static final class Context extends EGLContext {
        long handle; int version;
        public javax.microedition.khronos.opengles.GL getGL() { return android.opengl.GLSurfaceView.huskGL(); }
    }
    static final Display DISPLAY = new Display();
    static final Config CONFIG = new Config();
    static final Surface SURFACE = new Surface();
    static final Context CONTEXT = new Context();
    private static final ThreadLocal<Object[]> sCurrent = new ThreadLocal<>();     /* { context, draw surface } */
    private static final ThreadLocal<Integer> sError = new ThreadLocal<>();
    private static boolean render() { return husk.EGLNative.isRenderThread(); }
    private static boolean err(int e) { sError.set(e); return false; }

    public boolean eglChooseConfig(EGLDisplay d, int[] attribs, EGLConfig[] configs, int size, int[] num) { if (configs != null && size > 0) configs[0] = CONFIG; if (num != null) num[0] = 1; return true; }
    public boolean eglGetConfigAttrib(EGLDisplay d, EGLConfig c, int attr, int[] value) {
        int v;
        switch (attr) {
        case EGL_RED_SIZE: case EGL_GREEN_SIZE: case EGL_BLUE_SIZE: case EGL_ALPHA_SIZE: v = 8; break;
        case EGL_DEPTH_SIZE: v = 24; break; case EGL_STENCIL_SIZE: v = 8; break; case EGL_BUFFER_SIZE: v = 32; break;
        case EGL_RENDERABLE_TYPE: v = 1 | 4 | 64; break; case EGL_SURFACE_TYPE: v = EGL_WINDOW_BIT | EGL_PBUFFER_BIT; break;
        case EGL_CONFIG_ID: v = 1; break; case EGL_NATIVE_VISUAL_ID: v = 1; break; case EGL_CONFIG_CAVEAT: v = EGL_NONE; break;
        case EGL_SAMPLES: case EGL_SAMPLE_BUFFERS: v = 0; break; case 0x3042 /* CONFORMANT */: v = 1 | 4 | 64; break;
        default: v = 0;
        }
        value[0] = v; return true;
    }
    public boolean eglGetConfigs(EGLDisplay d, EGLConfig[] configs, int size, int[] num) { return eglChooseConfig(d, null, configs, size, num); }
    public EGLDisplay eglGetDisplay(Object n) { return DISPLAY; }
    public boolean eglInitialize(EGLDisplay d, int[] version) { if (version != null && version.length >= 2) { version[0] = 1; version[1] = 4; } return true; }
    public boolean eglTerminate(EGLDisplay d) { return true; }
    public EGLContext eglGetCurrentContext() { if (render()) return CONTEXT; Object[] c = sCurrent.get(); return c != null && c[0] != null ? (EGLContext) c[0] : EGL_NO_CONTEXT; }
    public EGLDisplay eglGetCurrentDisplay() { return DISPLAY; }
    public EGLSurface eglGetCurrentSurface(int which) { if (render()) return SURFACE; Object[] c = sCurrent.get(); return c != null && c[1] != null ? (EGLSurface) c[1] : EGL_NO_SURFACE; }
    static volatile int sRequestedVersion = 1;
    public EGLContext eglCreateContext(EGLDisplay d, EGLConfig c, EGLContext share, int[] attribs) {
        int version = 1;
        if (attribs != null) for (int i = 0; i + 1 < attribs.length && attribs[i] != EGL_NONE; i += 2) if (attribs[i] == 0x3098) version = attribs[i + 1];
        if (render()) { sRequestedVersion = version; return CONTEXT; }
        long h = husk.EGLNative.createContext(version, share instanceof Context ? ((Context) share).handle : 0);
        if (h == 0) { err(EGL_BAD_ALLOC); return EGL_NO_CONTEXT; }
        Context ctx = new Context(); ctx.handle = h; ctx.version = version;
        return ctx;
    }
    public boolean eglDestroyContext(EGLDisplay d, EGLContext c) { if (c instanceof Context && ((Context) c).handle != 0) { husk.EGLNative.destroyContext(((Context) c).handle); ((Context) c).handle = 0; } return true; }
    private static android.view.SurfaceView viewOf(Object w) {
        if (w instanceof android.view.SurfaceView) return (android.view.SurfaceView) w;
        if (w instanceof android.view.SurfaceHolder) { android.view.Surface s = ((android.view.SurfaceHolder) w).getSurface(); return s != null ? s.huskView : null; }
        if (w instanceof android.view.Surface) return ((android.view.Surface) w).huskView;
        return null;
    }
    public EGLSurface eglCreateWindowSurface(EGLDisplay d, EGLConfig c, Object w, int[] attribs) {
        if (render()) return SURFACE;
        android.view.SurfaceView v = viewOf(w);
        int[] size = v != null ? v.huskSurfaceSize() : new int[] { husk.Native.screenWidth(), husk.Native.screenHeight() };
        long h = husk.EGLNative.createSurface(size[0], size[1]);
        if (h == 0) { err(EGL_BAD_ALLOC); return EGL_NO_SURFACE; }
        Surface s = new Surface(); s.handle = h; s.w = size[0]; s.h = size[1]; s.view = v;
        if (v != null) v.huskSetGL(true);
        return s;
    }
    public EGLSurface eglCreatePbufferSurface(EGLDisplay d, EGLConfig c, int[] attribs) {
        int w = 1, h = 1;
        if (attribs != null) for (int i = 0; i + 1 < attribs.length && attribs[i] != EGL_NONE; i += 2) { if (attribs[i] == EGL_WIDTH) w = attribs[i + 1]; if (attribs[i] == EGL_HEIGHT) h = attribs[i + 1]; }
        if (render()) return SURFACE;
        long hd = husk.EGLNative.createSurface(w, h);
        if (hd == 0) { err(EGL_BAD_ALLOC); return EGL_NO_SURFACE; }
        Surface s = new Surface(); s.handle = hd; s.w = w; s.h = h;
        return s;
    }
    public boolean eglDestroySurface(EGLDisplay d, EGLSurface s) {
        if (s instanceof Surface && ((Surface) s).handle != 0) { husk.EGLNative.destroySurface(((Surface) s).handle); ((Surface) s).handle = 0; }
        return true;
    }
    public boolean eglMakeCurrent(EGLDisplay d, EGLSurface draw, EGLSurface read, EGLContext c) {
        if (render()) return true;
        long ch = c instanceof Context ? ((Context) c).handle : 0, sh = draw instanceof Surface ? ((Surface) draw).handle : 0;
        boolean ok = husk.EGLNative.makeCurrent(sh, ch);
        if (ok) sCurrent.set(ch == 0 ? null : new Object[] { c, draw });
        else err(EGL_BAD_MATCH);
        return ok;
    }
    public boolean eglSwapBuffers(EGLDisplay d, EGLSurface s) {
        if (render() || !(s instanceof Surface) || ((Surface) s).handle == 0) return true;
        Surface su = (Surface) s;
        Object[] cur = sCurrent.get();
        long ch = cur != null && cur[0] instanceof Context ? ((Context) cur[0]).handle : 0;
        int x = 0, y = 0, w = 0, h = 0;
        if (su.view != null) {
            int[] loc = new int[2];
            try { su.view.getLocationOnScreen(loc); } catch (RuntimeException e) {}
            x = loc[0]; y = loc[1]; w = su.view.getWidth(); h = su.view.getHeight();
            if (w <= 0 || h <= 0) { w = su.w; h = su.h; }
        }
        return husk.EGLNative.swap(su.handle, ch, x, y, w, h);
    }
    public int eglGetError() { Integer e = sError.get(); sError.set(EGL_SUCCESS); return e == null ? EGL_SUCCESS : e; }
    public String eglQueryString(EGLDisplay d, int name) { return name == EGL_VENDOR ? "Husk" : name == EGL_VERSION ? "1.4 Husk" : name == EGL_EXTENSIONS ? "EGL_KHR_surfaceless_context EGL_KHR_create_context" : ""; }
    public boolean eglQuerySurface(EGLDisplay d, EGLSurface s, int attr, int[] value) {
        int w = husk.Native.screenWidth(), h = husk.Native.screenHeight();
        if (s instanceof Surface && ((Surface) s).handle != 0) { w = ((Surface) s).w; h = ((Surface) s).h; }
        value[0] = attr == EGL_WIDTH ? w : attr == EGL_HEIGHT ? h : 0; return true;
    }
    public boolean eglQueryContext(EGLDisplay d, EGLContext c, int attr, int[] value) { value[0] = attr == 0x3098 && c instanceof Context ? Math.max(1, ((Context) c).version) : 0; return true; }
    public boolean eglReleaseThread() { if (!render()) { husk.EGLNative.makeCurrent(0, 0); sCurrent.set(null); } return true; }
    public boolean eglWaitGL() { return true; }
    public boolean eglWaitNative(int engine, Object bindTarget) { return true; }
    public boolean eglCopyBuffers(EGLDisplay d, EGLSurface s, Object target) { return true; }
    public EGLSurface eglCreatePixmapSurface(EGLDisplay d, EGLConfig c, Object pixmap, int[] attribs) { return EGL_NO_SURFACE; }
}
