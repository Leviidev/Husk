package android.view;
public interface SurfaceHolder {
    int SURFACE_TYPE_NORMAL = 0, SURFACE_TYPE_GPU = 2;
    interface Callback { void surfaceCreated(SurfaceHolder h); void surfaceChanged(SurfaceHolder h, int format, int w, int ht); void surfaceDestroyed(SurfaceHolder h); }
    interface Callback2 extends Callback { void surfaceRedrawNeeded(SurfaceHolder h); }
    void addCallback(Callback c); void removeCallback(Callback c); Surface getSurface(); void setFormat(int f); void setType(int t);
    void setFixedSize(int w, int h); void setSizeFromLayout(); void setKeepScreenOn(boolean b); android.graphics.Rect getSurfaceFrame(); boolean isCreating();
}
