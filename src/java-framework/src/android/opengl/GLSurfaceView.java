package android.opengl;

import java.util.ArrayList;
import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.egl.EGLSurface;
import javax.microedition.khronos.opengles.GL10;

/** The renderer runs on Husk's GL thread (husk-tl-dvm-android.c), which owns the EGL context on the app's screen. */
public class GLSurfaceView extends android.view.SurfaceView implements android.view.SurfaceHolder.Callback2 {
    public static final int RENDERMODE_WHEN_DIRTY = 0, RENDERMODE_CONTINUOUSLY = 1, DEBUG_CHECK_GL_ERROR = 1, DEBUG_LOG_GL_CALLS = 2;
    public interface Renderer { void onSurfaceCreated(GL10 gl, EGLConfig config); void onSurfaceChanged(GL10 gl, int w, int h); void onDrawFrame(GL10 gl); }
    public interface EGLConfigChooser { EGLConfig chooseConfig(EGL10 egl, EGLDisplay display); }
    public interface EGLContextFactory { EGLContext createContext(EGL10 egl, EGLDisplay d, EGLConfig c); void destroyContext(EGL10 egl, EGLDisplay d, EGLContext c); }
    public interface EGLWindowSurfaceFactory { EGLSurface createWindowSurface(EGL10 egl, EGLDisplay d, EGLConfig c, Object w); void destroySurface(EGL10 egl, EGLDisplay d, EGLSurface s); }
    public interface GLWrapper { javax.microedition.khronos.opengles.GL wrap(javax.microedition.khronos.opengles.GL gl); }

    private Renderer renderer;
    private int version = 1, renderMode = RENDERMODE_CONTINUOUSLY;
    private boolean versionSet;
    private EGLContextFactory contextFactory;
    private volatile boolean dirty = true, started, paused;
    private final ArrayList<Runnable> queue = new ArrayList<>();

    public GLSurfaceView(android.content.Context c) { this(c, null); }
    public GLSurfaceView(android.content.Context c, android.util.AttributeSet a) { super(c, a); getHolder().addCallback(this); }
    public void surfaceCreated(android.view.SurfaceHolder h) { start(); }
    public void surfaceChanged(android.view.SurfaceHolder h, int format, int w, int ht) {}
    public void surfaceDestroyed(android.view.SurfaceHolder h) {}
    public void surfaceRedrawNeeded(android.view.SurfaceHolder h) { requestRender(); }
    public void surfaceRedrawNeededAsync(android.view.SurfaceHolder h, Runnable done) { requestRender(); done.run(); }
    public void setRenderer(Renderer r) { renderer = r; if (isAttachedToWindow()) start(); }
    public void setEGLContextClientVersion(int v) { version = v; versionSet = true; }
    public void setEGLConfigChooser(EGLConfigChooser c) {}
    public void setEGLConfigChooser(boolean depth) {}
    public void setEGLConfigChooser(int r, int g, int b, int a, int depth, int stencil) {}
    public void setEGLContextFactory(EGLContextFactory f) { contextFactory = f; }
    public void setEGLWindowSurfaceFactory(EGLWindowSurfaceFactory f) {}
    public void setGLWrapper(GLWrapper w) {}
    public void setDebugFlags(int f) {}
    public int getDebugFlags() { return 0; }
    public void setPreserveEGLContextOnPause(boolean b) {}
    public boolean getPreserveEGLContextOnPause() { return true; }
    public void setRenderMode(int m) { renderMode = m; dirty = true; }
    public int getRenderMode() { return renderMode; }
    public void requestRender() { dirty = true; husk.Native.requestRender(); }
    public void onPause() { paused = true; }
    public void onResume() { paused = false; dirty = true; }
    public void queueEvent(Runnable r) { synchronized (queue) { queue.add(r); } husk.Native.requestRender(); }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); start(); }
    private void start() {
        if (renderer == null || started) return;
        started = true;
        // an app with its own context factory asks for its GL ES version there (EGL_CONTEXT_CLIENT_VERSION), not here
        if (!versionSet && contextFactory != null) {
            try {
                EGL10 egl = (EGL10) EGLContext.getEGL();
                EGLDisplay d = egl.eglGetDisplay(EGL10.EGL_DEFAULT_DISPLAY);
                EGLConfig[] cfg = new EGLConfig[1];
                egl.eglChooseConfig(d, null, cfg, 1, new int[1]);
                contextFactory.createContext(egl, d, cfg[0]);
                version = Math.max(1, EGLContext.huskRequestedVersion());
            } catch (RuntimeException e) {
                android.util.Log.w("GLSurfaceView", "the app's EGLContextFactory threw: " + e);
                version = 2;
            }
        }
        husk.Native.startGL(this, renderer, version);
    }

    private static HuskGL10 sGL;
    /** @hide The GL10 object EGLContext.getGL() hands out. */
    public static synchronized javax.microedition.khronos.opengles.GL huskGL() { if (sGL == null) sGL = new HuskGL10(); return sGL; }

    /** Husk's GL thread, each frame: queued events, then whether to draw. */
    public final boolean huskBeginFrame() {
        ArrayList<Runnable> run;
        synchronized (queue) { run = new ArrayList<>(queue); queue.clear(); }
        for (Runnable r : run) r.run();
        if (paused) return false;
        if (renderMode == RENDERMODE_CONTINUOUSLY) return true;
        boolean d = dirty; dirty = false; return d;
    }
}
