package android.view;

import android.graphics.Canvas;
import android.graphics.Rect;

/** A drawing surface. Software surfaces belong to a SurfaceView (lockCanvas draws into its buffer); GL ones to the render thread. */
public class Surface implements android.os.Parcelable {
    public static final int ROTATION_0 = 0, ROTATION_90 = 1, ROTATION_180 = 2, ROTATION_270 = 3;
    public static class OutOfResourcesException extends RuntimeException { public OutOfResourcesException() {} public OutOfResourcesException(String s) { super(s); } }
    interface Producer { Canvas lock(Rect dirty); void post(Canvas c); boolean valid(); }
    Producer mProducer;
    private boolean mReleased;
    public Surface() {}
    public Surface(android.graphics.SurfaceTexture t) {}
    public Surface(SurfaceControl c) {}
    public boolean isValid() { return !mReleased && (mProducer == null || mProducer.valid()); }
    public void release() { mReleased = true; }
    public Canvas lockCanvas(Rect dirty) throws OutOfResourcesException, IllegalArgumentException { if (mProducer == null) throw new IllegalStateException("Surface has no buffer to draw into"); return mProducer.lock(dirty); }
    public Canvas lockHardwareCanvas() { return lockCanvas(null); }
    public void unlockCanvasAndPost(Canvas c) { if (mProducer != null) mProducer.post(c); }
    @Deprecated public void unlockCanvas(Canvas c) { unlockCanvasAndPost(c); }
    public void setFrameRate(float r, int c) {} public void setFrameRate(float r, int c, int s) {} public void clearFrameRate() {}
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) {}
    public void readFromParcel(android.os.Parcel p) {}
    public static final Creator<Surface> CREATOR = new Creator<Surface>() { public Surface createFromParcel(android.os.Parcel p) { return new Surface(); } public Surface[] newArray(int n) { return new Surface[n]; } };
}
