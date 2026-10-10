package android.content;
public class ClipboardManager {
    private CharSequence text;
    public CharSequence getText() { return text; }
    public void setText(CharSequence t) { text = t; }
    public boolean hasText() { return text != null && text.length() > 0; }
    public ClipData getPrimaryClip() { return text == null ? null : ClipData.newPlainText("", text); }
    public void setPrimaryClip(ClipData c) { text = c != null && c.getItemCount() > 0 ? c.getItemAt(0).getText() : null; }
    public boolean hasPrimaryClip() { return text != null; }
}
