package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.SparseBooleanArray;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;

/** A row of cells; inside a TableLayout its columns take the table's widths. */
public class TableRow extends LinearLayout {
    private int[] mConstrainedColumnWidths;
    private SparseBooleanArray mCollapsed = new SparseBooleanArray();
    public TableRow(Context c) { super(c); setOrientation(HORIZONTAL); }
    public TableRow(Context c, AttributeSet a) { super(c, a); setOrientation(HORIZONTAL); }
    void setColumnCollapsedHusk(SparseBooleanArray c) { mCollapsed = c; }
    private int columnOf(int childIndex) { int col = 0; for (int i = 0; i < childIndex; i++) { View c = getChildAt(i); LayoutParams lp = (LayoutParams) c.getLayoutParams(); if (lp.column >= 0) col = lp.column; col += Math.max(1, lp.span); } LayoutParams lp = (LayoutParams) getChildAt(childIndex).getLayoutParams(); return lp.column >= 0 ? lp.column : col; }
    int[] getColumnsWidths(int ws, int hs) {
        int count = getChildCount(), cols = 0;
        for (int i = 0; i < count; i++) { LayoutParams lp = (LayoutParams) getChildAt(i).getLayoutParams(); cols = Math.max(cols, columnOf(i) + Math.max(1, lp.span)); }
        int[] w = new int[cols];
        for (int i = 0; i < count; i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) c.getLayoutParams();
            if (lp.span > 1) continue;
            int spec = lp.width == ViewGroup.LayoutParams.WRAP_CONTENT || lp.width == ViewGroup.LayoutParams.MATCH_PARENT ? MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED) : MeasureSpec.makeMeasureSpec(lp.width, MeasureSpec.EXACTLY);
            c.measure(spec, spec);
            w[columnOf(i)] = Math.max(w[columnOf(i)], c.getMeasuredWidth() + lp.leftMargin + lp.rightMargin);
        }
        return w;
    }
    void setColumnsWidthConstraints(int[] w) { mConstrainedColumnWidths = w; }
    @Override protected void onMeasure(int ws, int hs) {
        if (mConstrainedColumnWidths == null) { super.onMeasure(ws, hs); return; }
        int count = getChildCount(), width = getPaddingLeft() + getPaddingRight(), height = 0;
        for (int w : mConstrainedColumnWidths) width += w;
        for (int i = 0; i < count; i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) c.getLayoutParams();
            int col = columnOf(i), cw = 0;
            for (int k = col; k < Math.min(mConstrainedColumnWidths.length, col + Math.max(1, lp.span)); k++) cw += mConstrainedColumnWidths[k];
            cw = Math.max(0, cw - lp.leftMargin - lp.rightMargin);
            int chs = getChildMeasureSpec(hs, getPaddingTop() + getPaddingBottom() + lp.topMargin + lp.bottomMargin, lp.height == ViewGroup.LayoutParams.MATCH_PARENT ? ViewGroup.LayoutParams.WRAP_CONTENT : lp.height);
            c.measure(MeasureSpec.makeMeasureSpec(cw, lp.width == ViewGroup.LayoutParams.WRAP_CONTENT && lp.gravity != -1 && (lp.gravity & Gravity.HORIZONTAL_GRAVITY_MASK) != 0 ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY), chs);
            height = Math.max(height, c.getMeasuredHeight() + lp.topMargin + lp.bottomMargin);
        }
        height += getPaddingTop() + getPaddingBottom();
        setMeasuredDimension(resolveSizeAndState(width, ws, 0), resolveSizeAndState(Math.max(height, getSuggestedMinimumHeight()), hs, 0));
        // cells that match the row's height
        for (int i = 0; i < count; i++) { View c = getChildAt(i); LayoutParams lp = (LayoutParams) c.getLayoutParams(); if (c.getVisibility() != GONE && lp.height == ViewGroup.LayoutParams.MATCH_PARENT) c.measure(MeasureSpec.makeMeasureSpec(c.getMeasuredWidth(), MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(getMeasuredHeight() - getPaddingTop() - getPaddingBottom() - lp.topMargin - lp.bottomMargin, MeasureSpec.EXACTLY)); }
    }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        if (mConstrainedColumnWidths == null) { super.onLayout(changed, l, t, r, b); return; }
        int[] colLeft = new int[mConstrainedColumnWidths.length + 1];
        colLeft[0] = getPaddingLeft();
        for (int k = 0; k < mConstrainedColumnWidths.length; k++) colLeft[k + 1] = colLeft[k] + mConstrainedColumnWidths[k];
        int h = b - t;
        for (int i = 0; i < getChildCount(); i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) c.getLayoutParams();
            int col = Math.min(columnOf(i), mConstrainedColumnWidths.length - 1), end = Math.min(col + Math.max(1, lp.span), mConstrainedColumnWidths.length);
            int cellL = colLeft[col] + lp.leftMargin, cellR = colLeft[end] - lp.rightMargin, cw = c.getMeasuredWidth(), ch = c.getMeasuredHeight();
            int g = lp.gravity == -1 ? Gravity.TOP | Gravity.START : lp.gravity;
            int x = cellL; int hg = Gravity.getAbsoluteGravity(g, getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK;
            if (hg == Gravity.CENTER_HORIZONTAL) x = cellL + (cellR - cellL - cw) / 2; else if (hg == Gravity.RIGHT) x = cellR - cw;
            int vg = g & Gravity.VERTICAL_GRAVITY_MASK, y = getPaddingTop() + lp.topMargin;
            if (vg == Gravity.CENTER_VERTICAL) y = getPaddingTop() + (h - getPaddingTop() - getPaddingBottom() - ch) / 2; else if (vg == Gravity.BOTTOM) y = h - getPaddingBottom() - lp.bottomMargin - ch;
            c.layout(x, y, x + cw, y + ch);
        }
    }
    public View getVirtualChildAt(int i) { return getChildAt(i); }
    public int getVirtualChildCount() { return getChildCount(); }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new TableRow.LayoutParams(getContext(), a); }
    @Override protected LinearLayout.LayoutParams generateDefaultLayoutParams() { return new LayoutParams(); }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof TableRow.LayoutParams; }
    @Override protected LinearLayout.LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) { return new LayoutParams(p); }
    public static class LayoutParams extends LinearLayout.LayoutParams {
        public int column = -1, span = 1;
        public LayoutParams(Context c, AttributeSet a) {
            super(c, a);
            TypedArray t = c.obtainStyledAttributes(a, husk.S.TableRow_Cell);
            column = t.getInt(husk.S.TableRow_Cell_layout_column, -1);
            span = Math.max(1, t.getInt(husk.S.TableRow_Cell_layout_span, 1));
            t.recycle();
            if (width == 0 && weight == 0) width = WRAP_CONTENT;
        }
        public LayoutParams(int w, int h) { super(w, h); }
        public LayoutParams(int w, int h, float weight) { super(w, h, weight); }
        public LayoutParams() { super(MATCH_PARENT, WRAP_CONTENT); }
        public LayoutParams(int column) { this(); this.column = column; }
        public LayoutParams(ViewGroup.LayoutParams p) { super(p); }
        public LayoutParams(MarginLayoutParams p) { super(p); }
        // ---- generated by tools/compat/fillmembers.py (LayoutParams): the platform's members this class does not write (signatures only)
        protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
        // ---- end of generated members (LayoutParams)
    }
    @Override public CharSequence getAccessibilityClassName() { return TableRow.class.getName(); }
}
