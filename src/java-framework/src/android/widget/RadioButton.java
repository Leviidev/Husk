package android.widget;
public class RadioButton extends CompoundButton {
    public RadioButton(android.content.Context c) { this(c, null); }
    public RadioButton(android.content.Context c, android.util.AttributeSet a) { this(c, a, android.R.attr.radioButtonStyle); }
    public RadioButton(android.content.Context c, android.util.AttributeSet a, int s) { this(c, a, s, 0); }
    public RadioButton(android.content.Context c, android.util.AttributeSet a, int s, int r) { super(c, a, s, r); }
    @Override public void toggle() { if (!isChecked()) super.toggle(); }
    @Override public CharSequence getAccessibilityClassName() { return RadioButton.class.getName(); }
}
