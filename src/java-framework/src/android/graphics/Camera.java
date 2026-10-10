package android.graphics;
/** A 3D camera, flattened: rotations about X and Y show as scales, Z as a rotation. */
public class Camera {
    private float rx, ry, rz, tx, ty, tz;
    private final java.util.ArrayDeque<float[]> stack = new java.util.ArrayDeque<>();
    public void save() { stack.push(new float[] { rx, ry, rz, tx, ty, tz }); }
    public void restore() { float[] s = stack.pop(); rx = s[0]; ry = s[1]; rz = s[2]; tx = s[3]; ty = s[4]; tz = s[5]; }
    public void translate(float x, float y, float z) { tx += x; ty += y; tz += z; }
    public void rotateX(float d) { rx += d; }
    public void rotateY(float d) { ry += d; }
    public void rotateZ(float d) { rz += d; }
    public void rotate(float x, float y, float z) { rx += x; ry += y; rz += z; }
    public void setLocation(float x, float y, float z) {}
    public float getLocationX() { return 0; } public float getLocationY() { return 0; } public float getLocationZ() { return -8; }
    public void getMatrix(Matrix m) {
        m.reset();
        m.preScale((float) Math.cos(Math.toRadians(ry)), (float) Math.cos(Math.toRadians(rx)));
        m.preRotate(-rz);
        m.postTranslate(tx, -ty);
    }
    public void applyToCanvas(Canvas c) { Matrix m = new Matrix(); getMatrix(m); c.concat(m); }
    public float dotWithNormal(float x, float y, float z) { return z; }
}
