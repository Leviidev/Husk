package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

@Deprecated
public class AbsoluteLayout extends ViewGroup {
    public static class LayoutParams extends ViewGroup.LayoutParams {
        public int x, y;
        public LayoutParams(int w, int h, int x, int y) { super(w, h); this.x = x; this.y = y; }
        public LayoutParams(Context c, AttributeSet a) { super(c, a); if (a != null) { for (int i = 0; i < a.getAttributeCount(); i++) { String n = a.getAttributeName(i); if ("layout_x".equals(n)) x = (int) dim(c, a.getAttributeValue(i)); else if ("layout_y".equals(n)) y = (int) dim(c, a.getAttributeValue(i)); } } }
        public LayoutParams(ViewGroup.LayoutParams s) { super(s); }
        private static float dim(Context c, String v) { if (v == null) return 0; float d = c.getResources().getDisplayMetrics().density; try { if (v.endsWith("dip")) return Float.parseFloat(v.substring(0, v.length() - 3)) * d; if (v.endsWith("dp")) return Float.parseFloat(v.substring(0, v.length() - 2)) * d; if (v.endsWith("px")) return Float.parseFloat(v.substring(0, v.length() - 2)); return Float.parseFloat(v); } catch (NumberFormatException e) { return 0; } }
    }
    public AbsoluteLayout(Context c) { super(c); }
    public AbsoluteLayout(Context c, AttributeSet a) { super(c, a); }
    public AbsoluteLayout(Context c, AttributeSet a, int s) { super(c, a, s); }
    @Override protected void onMeasure(int ws, int hs) {
        int count = getChildCount(), maxHeight = 0, maxWidth = 0;
        measureChildren(ws, hs);
        for (int i = 0; i < count; i++) { View child = getChildAt(i); if (child.getVisibility() != GONE) { LayoutParams lp = (LayoutParams) child.getLayoutParams(); maxWidth = Math.max(maxWidth, lp.x + child.getMeasuredWidth()); maxHeight = Math.max(maxHeight, lp.y + child.getMeasuredHeight()); } }
        maxWidth += getPaddingLeft() + getPaddingRight(); maxHeight += getPaddingTop() + getPaddingBottom();
        maxHeight = Math.max(maxHeight, getSuggestedMinimumHeight()); maxWidth = Math.max(maxWidth, getSuggestedMinimumWidth());
        setMeasuredDimension(resolveSizeAndState(maxWidth, ws, 0), resolveSizeAndState(maxHeight, hs, 0));
    }
    @Override protected ViewGroup.LayoutParams generateDefaultLayoutParams() { return new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT, 0, 0); }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        for (int i = 0; i < getChildCount(); i++) { View child = getChildAt(i); if (child.getVisibility() != GONE) { LayoutParams lp = (LayoutParams) child.getLayoutParams(); int cl = getPaddingLeft() + lp.x, ct = getPaddingTop() + lp.y; child.layout(cl, ct, cl + child.getMeasuredWidth(), ct + child.getMeasuredHeight()); } }
    }
    @Override public ViewGroup.LayoutParams generateLayoutParams(AttributeSet a) { return new LayoutParams(getContext(), a); }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof LayoutParams; }
    @Override protected ViewGroup.LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) { return new LayoutParams(p); }
    @Override public boolean shouldDelayChildPressedState() { return false; }
}
