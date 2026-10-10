package android.widget;
public class EditText extends TextView {
    public EditText(android.content.Context c) { super(c); }
    public EditText(android.content.Context c, android.util.AttributeSet a) { super(c); }
    public android.text.Editable getText() { return new android.text.SpannableStringBuilder(super.getText()); }
    public void setSelection(int i) {} public void selectAll() {}
}
