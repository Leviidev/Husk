package android.graphics;
/** A GL texture fed by a producer (camera, video). Nothing produces frames into one on Husk yet. */
public class SurfaceTexture {
    public interface OnFrameAvailableListener { void onFrameAvailable(SurfaceTexture t); }
    public static class OutOfResourcesException extends Exception { public OutOfResourcesException() {} public OutOfResourcesException(String s) { super(s); } }
    private final int mTex;
    public SurfaceTexture(int texName) { this(texName, false); }
    public SurfaceTexture(int texName, boolean singleBufferMode) { mTex = texName; }
    public SurfaceTexture(boolean singleBufferMode) { mTex = 0; }
    public void setOnFrameAvailableListener(OnFrameAvailableListener l) {}
    public void setOnFrameAvailableListener(OnFrameAvailableListener l, android.os.Handler h) {}
    public void setDefaultBufferSize(int w, int h) {}
    public void updateTexImage() {}
    public void releaseTexImage() {}
    public void detachFromGLContext() {}
    public void attachToGLContext(int tex) {}
    public void getTransformMatrix(float[] m) { android.opengl.Matrix.setIdentityM(m, 0); }
    public long getTimestamp() { return 0; }
    public void release() {}
    public boolean isReleased() { return false; }
}
