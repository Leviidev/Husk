package android.widget;
public class ImageSwitcher extends ViewSwitcher {
    public ImageSwitcher(android.content.Context c) { super(c); }
    public ImageSwitcher(android.content.Context c, android.util.AttributeSet a) { super(c, a); }
    public void setImageResource(int r) { ((ImageView) getNextView()).setImageResource(r); showNext(); }
    public void setImageURI(android.net.Uri u) { ((ImageView) getNextView()).setImageURI(u); showNext(); }
    public void setImageDrawable(android.graphics.drawable.Drawable d) { ((ImageView) getNextView()).setImageDrawable(d); showNext(); }
}
