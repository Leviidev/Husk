package android.graphics.drawable;
import android.graphics.*;
public class PictureDrawable extends Drawable {
    private Picture mPicture;
    public PictureDrawable(Picture p) { mPicture = p; }
    public Picture getPicture() { return mPicture; }
    public void setPicture(Picture p) { mPicture = p; }
    @Override public void draw(Canvas c) { if (mPicture != null) { Rect b = getBounds(); c.save(); c.clipRect(b); c.translate(b.left, b.top); c.drawPicture(mPicture); c.restore(); } }
    @Override public int getIntrinsicWidth() { return mPicture == null ? -1 : mPicture.getWidth(); }
    @Override public int getIntrinsicHeight() { return mPicture == null ? -1 : mPicture.getHeight(); }
}
