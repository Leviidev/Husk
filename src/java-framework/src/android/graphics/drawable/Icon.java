package android.graphics.drawable;
import android.graphics.Bitmap;
public final class Icon {
    private Bitmap mBitmap; private int mRes; private String mPkg;
    private Icon() {}
    public static Icon createWithBitmap(Bitmap b) { Icon i = new Icon(); i.mBitmap = b; return i; }
    public static Icon createWithAdaptiveBitmap(Bitmap b) { return createWithBitmap(b); }
    public static Icon createWithResource(android.content.Context c, int res) { Icon i = new Icon(); i.mRes = res; return i; }
    public static Icon createWithResource(String pkg, int res) { Icon i = new Icon(); i.mRes = res; i.mPkg = pkg; return i; }
    public Drawable loadDrawable(android.content.Context c) { return mBitmap != null ? new BitmapDrawable(c.getResources(), mBitmap) : mRes != 0 ? c.getDrawable(mRes) : null; }
    public Icon setTint(int t) { return this; }
    public int getResId() { return mRes; }
}
