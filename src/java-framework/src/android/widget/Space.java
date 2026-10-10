package android.widget;
public final class Space extends android.view.View {
    public Space(android.content.Context c) { this(c, null); }
    public Space(android.content.Context c, android.util.AttributeSet a) { this(c, a, 0); }
    public Space(android.content.Context c, android.util.AttributeSet a, int s) { this(c, a, s, 0); }
    public Space(android.content.Context c, android.util.AttributeSet a, int s, int r) { super(c, a, s, r); if (getVisibility() == VISIBLE) setVisibility(INVISIBLE); }
    @Override public void draw(android.graphics.Canvas c) {}
    private static int size(int size, int spec) { int m = MeasureSpec.getMode(spec), s = MeasureSpec.getSize(spec); return m == MeasureSpec.UNSPECIFIED ? size : m == MeasureSpec.AT_MOST ? Math.min(size, s) : s; }
    @Override protected void onMeasure(int ws, int hs) { setMeasuredDimension(size(getSuggestedMinimumWidth(), ws), size(getSuggestedMinimumHeight(), hs)); }
}
