package android.opengl;
/** The GL10 a GLSurfaceView renderer is handed: GLES 2 underneath. */
final class HuskGL10 implements javax.microedition.khronos.opengles.GL11 {
    public void glViewport(int x, int y, int w, int h) { GLES20.glViewport(x, y, w, h); }
    public String glGetString(int name) { return GLES20.glGetString(name); }
    public void glClearColor(float r, float g, float b, float a) { GLES20.glClearColor(r, g, b, a); }
    public void glClear(int mask) { GLES20.glClear(mask); }
    public void glGetIntegerv(int pname, int[] params, int offset) { GLES20.glGetIntegerv(pname, params, offset); }
    public int glGetError() { return GLES20.glGetError(); }
}
