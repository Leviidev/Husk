package android.widget;
public class CheckBox extends CompoundButton {
    public CheckBox(android.content.Context c) { this(c, null); }
    public CheckBox(android.content.Context c, android.util.AttributeSet a) { this(c, a, android.R.attr.checkboxStyle); }
    public CheckBox(android.content.Context c, android.util.AttributeSet a, int s) { this(c, a, s, 0); }
    public CheckBox(android.content.Context c, android.util.AttributeSet a, int s, int r) { super(c, a, s, r); }
    @Override public CharSequence getAccessibilityClassName() { return CheckBox.class.getName(); }
}
