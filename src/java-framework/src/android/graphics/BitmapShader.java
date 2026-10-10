package android.graphics;
public class BitmapShader extends Shader {
    public BitmapShader(Bitmap b, TileMode x, TileMode y) { mNative = husk.Gfx.shBitmap(b.huskNative(), tile(x)); }
    public void setFilterMode(int m) {}
}
