package android.view;
import android.graphics.Outline;
public abstract class ViewOutlineProvider {
    public static final ViewOutlineProvider BACKGROUND = new ViewOutlineProvider() {
        public void getOutline(View v, Outline o) { android.graphics.drawable.Drawable b = v.getBackground(); if (b != null) b.getOutline(o); else { o.setRect(0, 0, v.getWidth(), v.getHeight()); o.setAlpha(0); } }
    };
    public static final ViewOutlineProvider BOUNDS = new ViewOutlineProvider() { public void getOutline(View v, Outline o) { o.setRect(0, 0, v.getWidth(), v.getHeight()); } };
    public static final ViewOutlineProvider PADDED_BOUNDS = new ViewOutlineProvider() { public void getOutline(View v, Outline o) { o.setRect(v.getPaddingLeft(), v.getPaddingTop(), v.getWidth() - v.getPaddingRight(), v.getHeight() - v.getPaddingBottom()); } };
    public abstract void getOutline(View v, Outline o);
}
