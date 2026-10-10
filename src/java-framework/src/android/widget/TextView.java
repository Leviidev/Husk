package android.widget;
public class TextView extends android.view.View {
    private CharSequence text = "";
    public TextView(android.content.Context c) { super(c); }
    public TextView(android.content.Context c, android.util.AttributeSet a) { super(c); }
    public void setText(CharSequence t) { text = t == null ? "" : t; }
    public void setText(int res) {}
    public CharSequence getText() { return text; }
    public void setTextSize(float s) {} public void setTextSize(int unit, float s) {} public void setTextColor(int c) {}
    public void setGravity(int g) {} public void setSingleLine() {} public void setSingleLine(boolean b) {} public void setMaxLines(int n) {} public void setLines(int n) {}
    public void setInputType(int t) {} public void setImeOptions(int o) {} public void setHint(CharSequence h) {}
    public void setTypeface(Object t) {} public void addTextChangedListener(android.text.TextWatcher w) {}
    public void setOnEditorActionListener(OnEditorActionListener l) {}
    public interface OnEditorActionListener { boolean onEditorAction(TextView v, int action, android.view.KeyEvent e); }
}
