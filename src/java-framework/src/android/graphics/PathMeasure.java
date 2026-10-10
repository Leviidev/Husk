package android.graphics;
/** Length only, over the whole path; positions are along straight segments between its points. */
public class PathMeasure {
    public static final int POSITION_MATRIX_FLAG = 1, TANGENT_MATRIX_FLAG = 2;
    private Path mPath; private float mLength;
    public PathMeasure() {}
    public PathMeasure(Path p, boolean forceClosed) { setPath(p, forceClosed); }
    public void setPath(Path p, boolean forceClosed) { mPath = p; mLength = p == null || p.n == 0 ? 0 : husk.Gfx.pathLength(p.ops, p.n, new float[2]); }
    public float getLength() { return mLength; }
    public boolean getPosTan(float d, float[] pos, float[] tan) { if (pos != null) { pos[0] = 0; pos[1] = 0; } if (tan != null) { tan[0] = 1; tan[1] = 0; } return mPath != null; }
    public boolean getMatrix(float d, Matrix m, int flags) { m.reset(); return mPath != null; }
    public boolean getSegment(float start, float stop, Path dst, boolean moveTo) { if (mPath != null && start <= 0 && stop >= mLength) { dst.addPath(mPath); return true; } return false; }
    public boolean isClosed() { return false; }
    public boolean nextContour() { return false; }
}
