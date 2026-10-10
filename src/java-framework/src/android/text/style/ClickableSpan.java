package android.text.style;
public abstract class ClickableSpan extends CharacterStyle implements UpdateAppearance {
    public abstract void onClick(android.view.View widget);
    @Override public void updateDrawState(android.text.TextPaint ds) { ds.setColor(ds.linkColor != 0 ? ds.linkColor : 0xFF2962FF); ds.setUnderlineText(true); }
}
