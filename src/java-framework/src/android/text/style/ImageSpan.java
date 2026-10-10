package android.text.style;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
public class ImageSpan extends DynamicDrawableSpan {
    private Drawable mDrawable; private String mSource;
    public ImageSpan(Bitmap b) { this(null, b, ALIGN_BOTTOM); }
    public ImageSpan(Bitmap b, int a) { this(null, b, a); }
    public ImageSpan(Context c, Bitmap b) { this(c, b, ALIGN_BOTTOM); }
    public ImageSpan(Context c, Bitmap b, int a) { super(a); mDrawable = new BitmapDrawable(c != null ? c.getResources() : null, b); mDrawable.setBounds(0, 0, mDrawable.getIntrinsicWidth(), mDrawable.getIntrinsicHeight()); }
    public ImageSpan(Drawable d) { this(d, ALIGN_BOTTOM); }
    public ImageSpan(Drawable d, int a) { super(a); mDrawable = d; }
    public ImageSpan(Drawable d, String source) { this(d, source, ALIGN_BOTTOM); }
    public ImageSpan(Drawable d, String source, int a) { super(a); mDrawable = d; mSource = source; }
    public ImageSpan(Context c, int res) { this(c, res, ALIGN_BOTTOM); }
    public ImageSpan(Context c, int res, int a) { super(a); mDrawable = c.getDrawable(res); mDrawable.setBounds(0, 0, mDrawable.getIntrinsicWidth(), mDrawable.getIntrinsicHeight()); }
    public ImageSpan(Context c, android.net.Uri u) { this(c, u, ALIGN_BOTTOM); }
    public ImageSpan(Context c, android.net.Uri u, int a) { super(a); mSource = u.toString(); }
    @Override public Drawable getDrawable() { if (mDrawable == null) { mDrawable = new android.graphics.drawable.ColorDrawable(0); mDrawable.setBounds(0, 0, 1, 1); } return mDrawable; }
    public String getSource() { return mSource; }
}
