package android.view;
public interface SurfaceHolder {
    @Deprecated int SURFACE_TYPE_NORMAL = 0, SURFACE_TYPE_HARDWARE = 1, SURFACE_TYPE_GPU = 2, SURFACE_TYPE_PUSH_BUFFERS = 3;
    class BadSurfaceTypeException extends RuntimeException { public BadSurfaceTypeException() {} public BadSurfaceTypeException(String s) { super(s); } }
    interface Callback { void surfaceCreated(SurfaceHolder h); void surfaceChanged(SurfaceHolder h, int format, int w, int ht); void surfaceDestroyed(SurfaceHolder h); }
    interface Callback2 extends Callback { void surfaceRedrawNeeded(SurfaceHolder h); default void surfaceRedrawNeededAsync(SurfaceHolder h, Runnable drawingFinished) { surfaceRedrawNeeded(h); drawingFinished.run(); } }
    void addCallback(Callback c);
    void removeCallback(Callback c);
    boolean isCreating();
    @Deprecated void setType(int t);
    void setFixedSize(int w, int h);
    void setSizeFromLayout();
    void setFormat(int f);
    void setKeepScreenOn(boolean b);
    android.graphics.Canvas lockCanvas();
    android.graphics.Canvas lockCanvas(android.graphics.Rect dirty);
    default android.graphics.Canvas lockHardwareCanvas() { return lockCanvas(); }
    void unlockCanvasAndPost(android.graphics.Canvas c);
    android.graphics.Rect getSurfaceFrame();
    Surface getSurface();
}
