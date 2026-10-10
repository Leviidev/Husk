package android.widget;
public class TextSwitcher extends ViewSwitcher {
    public TextSwitcher(android.content.Context c) { super(c); }
    public TextSwitcher(android.content.Context c, android.util.AttributeSet a) { super(c, a); }
    @Override public void addView(android.view.View child, int index, android.view.ViewGroup.LayoutParams p) { if (!(child instanceof TextView)) throw new IllegalArgumentException("TextSwitcher children must be instances of TextView"); super.addView(child, index, p); }
    public void setText(CharSequence t) { final TextView v = (TextView) getNextView(); v.setText(t); showNext(); }
    public void setCurrentText(CharSequence t) { ((TextView) getCurrentView()).setText(t); }
}
