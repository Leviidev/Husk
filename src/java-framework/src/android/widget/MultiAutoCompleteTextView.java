package android.widget;

import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.text.Spanned;
import android.util.AttributeSet;

public class MultiAutoCompleteTextView extends AutoCompleteTextView {
    public interface Tokenizer { int findTokenStart(CharSequence t, int cursor); int findTokenEnd(CharSequence t, int cursor); CharSequence terminateToken(CharSequence t); }
    public static class CommaTokenizer implements Tokenizer {
        public int findTokenStart(CharSequence text, int cursor) { int i = cursor; while (i > 0 && text.charAt(i - 1) != ',') i--; while (i < cursor && text.charAt(i) == ' ') i++; return i; }
        public int findTokenEnd(CharSequence text, int cursor) { int i = cursor, len = text.length(); while (i < len) { if (text.charAt(i) == ',') return i; else i++; } return len; }
        public CharSequence terminateToken(CharSequence text) { int i = text.length(); while (i > 0 && text.charAt(i - 1) == ' ') i--; if (i > 0 && text.charAt(i - 1) == ',') return text; return text + ", "; }
    }
    private Tokenizer mTokenizer;
    public MultiAutoCompleteTextView(Context c) { this(c, null); }
    public MultiAutoCompleteTextView(Context c, AttributeSet a) { this(c, a, android.R.attr.autoCompleteTextViewStyle); }
    public MultiAutoCompleteTextView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public MultiAutoCompleteTextView(Context c, AttributeSet a, int s, int r) { super(c, a, s, r); }
    public void setTokenizer(Tokenizer t) { mTokenizer = t; }
    @Override protected void performFiltering(CharSequence text, int keyCode) {
        if (enoughToFilter()) { int end = getSelectionEnd(), start = mTokenizer.findTokenStart(text, end); performFiltering(text, start, end, keyCode); }
        else { dismissDropDown(); Filter f = getFilter(); if (f != null) f.filter(null); }
    }
    @Override public boolean enoughToFilter() { Editable text = getText(); int end = getSelectionEnd(); if (end < 0 || mTokenizer == null) return false; int start = mTokenizer.findTokenStart(text, end); return end - start >= getThreshold(); }
    protected void performFiltering(CharSequence text, int start, int end, int keyCode) { getFilter().filter(text.subSequence(start, end), this); }
    @Override protected void replaceText(CharSequence text) {
        clearComposingText();
        int end = getSelectionEnd(), start = mTokenizer.findTokenStart(getText(), end);
        Editable editable = getText();
        String original = android.text.TextUtils.substring(editable, start, end);
        editable.replace(start, end, mTokenizer.terminateToken(text));
    }
}
