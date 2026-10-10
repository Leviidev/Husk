package android.graphics;

public class Shader {
    public enum TileMode { CLAMP, REPEAT, MIRROR, DECAL }
    long mNative;
    private Matrix mLocal;
    protected Shader() {}
    /** The native shader, its local matrix applied. */
    public long huskNative() { return mNative; }
    public boolean getLocalMatrix(Matrix m) { if (mLocal == null) { m.reset(); return false; } m.set(mLocal); return !mLocal.isIdentity(); }
    public void setLocalMatrix(Matrix m) {
        mLocal = m == null ? null : new Matrix(m);
        if (mNative != 0) husk.Gfx.shMatrix(mNative, m == null ? null : m.huskAffine());
    }
    static int tile(TileMode t) { return t == null ? 0 : t == TileMode.REPEAT ? 1 : t == TileMode.MIRROR ? 2 : 0; }
    @Override protected void finalize() throws Throwable { if (mNative != 0) husk.Gfx.shFree(mNative); mNative = 0; super.finalize(); }
}
