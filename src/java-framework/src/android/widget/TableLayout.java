package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.util.SparseBooleanArray;
import android.view.View;
import android.view.ViewGroup;

/** Rows of cells: each column as wide as its widest cell, stretchable and shrinkable columns share what is left over. */
public class TableLayout extends LinearLayout {
    private int[] mMaxWidths;
    private SparseBooleanArray mStretchable = new SparseBooleanArray(), mShrinkable = new SparseBooleanArray(), mCollapsed = new SparseBooleanArray();
    private boolean mStretchAll, mShrinkAll;
    public TableLayout(Context c) { super(c); setOrientation(VERTICAL); }
    public TableLayout(Context c, AttributeSet attrs) {
        super(c, attrs);
        setOrientation(VERTICAL);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.TableLayout);
        String s = a.getString(husk.S.TableLayout_stretchColumns);
        if (s != null) { if (s.trim().equals("*")) mStretchAll = true; else mStretchable = parse(s); }
        s = a.getString(husk.S.TableLayout_shrinkColumns);
        if (s != null) { if (s.trim().equals("*")) mShrinkAll = true; else mShrinkable = parse(s); }
        s = a.getString(husk.S.TableLayout_collapseColumns);
        if (s != null) mCollapsed = parse(s);
        a.recycle();
    }
    private static SparseBooleanArray parse(String s) { SparseBooleanArray r = new SparseBooleanArray(); for (String p : s.split("[ ,]+")) { try { if (!p.isEmpty()) r.put(Integer.parseInt(p.trim()), true); } catch (NumberFormatException e) {} } return r; }
    public void setShrinkAllColumns(boolean s) { mShrinkAll = s; requestLayout(); } public boolean isShrinkAllColumns() { return mShrinkAll; }
    public void setStretchAllColumns(boolean s) { mStretchAll = s; requestLayout(); } public boolean isStretchAllColumns() { return mStretchAll; }
    public void setColumnCollapsed(int c, boolean v) { mCollapsed.put(c, v); requestLayout(); } public boolean isColumnCollapsed(int c) { return mCollapsed.get(c); }
    public void setColumnStretchable(int c, boolean v) { mStretchable.put(c, v); requestLayout(); } public boolean isColumnStretchable(int c) { return mStretchAll || mStretchable.get(c); }
    public void setColumnShrinkable(int c, boolean v) { mShrinkable.put(c, v); requestLayout(); } public boolean isColumnShrinkable(int c) { return mShrinkAll || mShrinkable.get(c); }
    @Override protected void onMeasure(int ws, int hs) {
        int count = getChildCount();
        int[] widths = new int[0];
        for (int i = 0; i < count; i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE || !(c instanceof TableRow)) continue;
            int[] w = ((TableRow) c).getColumnsWidths(ws, hs);
            if (w.length > widths.length) widths = java.util.Arrays.copyOf(widths, w.length);
            for (int k = 0; k < w.length; k++) widths[k] = Math.max(widths[k], w[k]);
        }
        for (int k = 0; k < widths.length; k++) if (mCollapsed.get(k)) widths[k] = 0;
        int total = 0; for (int w : widths) total += w;
        int avail = MeasureSpec.getSize(ws) - getPaddingLeft() - getPaddingRight();
        if (MeasureSpec.getMode(ws) != MeasureSpec.UNSPECIFIED && widths.length > 0) {
            int extra = avail - total;
            if (extra > 0) { int n = 0; for (int k = 0; k < widths.length; k++) if (isColumnStretchable(k) && !mCollapsed.get(k)) n++; if (n > 0) { int per = extra / n; for (int k = 0; k < widths.length; k++) if (isColumnStretchable(k) && !mCollapsed.get(k)) widths[k] += per; } }
            else if (extra < 0) { int n = 0; for (int k = 0; k < widths.length; k++) if (isColumnShrinkable(k) && !mCollapsed.get(k)) n++; if (n > 0) { int per = -extra / n; for (int k = 0; k < widths.length; k++) if (isColumnShrinkable(k) && !mCollapsed.get(k)) widths[k] = Math.max(0, widths[k] - per); } }
        }
        mMaxWidths = widths;
        for (int i = 0; i < count; i++) { View c = getChildAt(i); if (c instanceof TableRow) ((TableRow) c).setColumnsWidthConstraints(widths); }
        super.onMeasure(ws, hs);
    }
    @Override public void addView(View child, int index, ViewGroup.LayoutParams p) { super.addView(child, index, p); if (child instanceof TableRow) ((TableRow) child).setColumnCollapsedHusk(mCollapsed); }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new TableLayout.LayoutParams(getContext(), a); }
    @Override protected LinearLayout.LayoutParams generateDefaultLayoutParams() { return new LayoutParams(); }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof TableLayout.LayoutParams; }
    @Override protected LinearLayout.LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) { return new LayoutParams(p); }
    public static class LayoutParams extends LinearLayout.LayoutParams {
        public LayoutParams(Context c, AttributeSet a) { super(c, a); width = MATCH_PARENT; }
        public LayoutParams(int w, int h) { super(MATCH_PARENT, h); }
        public LayoutParams(int w, int h, float weight) { super(MATCH_PARENT, h, weight); }
        public LayoutParams() { super(MATCH_PARENT, WRAP_CONTENT); }
        public LayoutParams(ViewGroup.LayoutParams p) { super(p); width = MATCH_PARENT; }
        public LayoutParams(MarginLayoutParams p) { super(p); width = MATCH_PARENT; }
    }
    @Override public CharSequence getAccessibilityClassName() { return TableLayout.class.getName(); }
}
