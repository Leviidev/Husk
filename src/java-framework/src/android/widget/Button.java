package android.widget;
public class Button extends TextView {
    public Button(android.content.Context c) { this(c, null); }
    public Button(android.content.Context c, android.util.AttributeSet a) { this(c, a, android.R.attr.buttonStyle); }
    public Button(android.content.Context c, android.util.AttributeSet a, int s) { this(c, a, s, 0); }
    public Button(android.content.Context c, android.util.AttributeSet a, int s, int r) { super(c, a, s, r); }
    @Override public CharSequence getAccessibilityClassName() { return Button.class.getName(); }
    @Override public android.view.PointerIcon onResolvePointerIcon(android.view.MotionEvent e, int i) { return null; }
}
