package android.widget;

import android.content.Context;
import android.text.Editable;
import android.text.Selection;
import android.text.method.ArrowKeyMovementMethod;
import android.text.method.MovementMethod;
import android.util.AttributeSet;

public class EditText extends TextView {
    public EditText(Context c) { this(c, null); }
    public EditText(Context c, AttributeSet a) { this(c, a, android.R.attr.editTextStyle); }
    public EditText(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public EditText(Context c, AttributeSet a, int defStyleAttr, int defStyleRes) { super(c, a, defStyleAttr, defStyleRes); }
    @Override public boolean getFreezesText() { return true; }
    @Override protected boolean getDefaultEditable() { return true; }
    @Override protected MovementMethod getDefaultMovementMethod() { return ArrowKeyMovementMethod.getInstance(); }
    @Override public Editable getText() { CharSequence t = super.getText(); if (t == null) return null; if (t instanceof Editable) return (Editable) t; super.setText(t, BufferType.EDITABLE); return (Editable) super.getText(); }
    @Override public void setText(CharSequence text, BufferType type) { super.setText(text, BufferType.EDITABLE); }
    public void setSelection(int start, int stop) { Selection.setSelection(getText(), start, stop); }
    public void setSelection(int index) { Selection.setSelection(getText(), index); }
    public void selectAll() { Selection.selectAll(getText()); }
    public void extendSelection(int index) { Selection.extendSelection(getText(), index); }
    @Override public void setEllipsize(android.text.TextUtils.TruncateAt e) { if (e == android.text.TextUtils.TruncateAt.MARQUEE) throw new IllegalArgumentException("EditText cannot use the ellipsize mode TextUtils.TruncateAt.MARQUEE"); super.setEllipsize(e); }
    @Override public CharSequence getAccessibilityClassName() { return EditText.class.getName(); }
}
