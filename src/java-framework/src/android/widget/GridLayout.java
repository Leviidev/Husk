package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;

/**
 * Cells on a grid: children placed by their row/column specs (or flowed in order), each column as wide as its widest single-span
 * cell and each row as tall as its tallest; column and row weights share the space left over, and a child sits in its cell by gravity.
 */
public class GridLayout extends ViewGroup {
    public static final int HORIZONTAL = LinearLayout.HORIZONTAL, VERTICAL = LinearLayout.VERTICAL, UNDEFINED = Integer.MIN_VALUE, ALIGN_BOUNDS = 0, ALIGN_MARGINS = 1;
    public static abstract class Alignment { final int g; Alignment(int g) { this.g = g; } }
    private static final class A extends Alignment { A(int g) { super(g); } }
    public static final Alignment UNDEFINED_ALIGNMENT = new A(0), LEADING = new A(Gravity.START), TRAILING = new A(Gravity.END), TOP = new A(Gravity.TOP), BOTTOM = new A(Gravity.BOTTOM), START = new A(Gravity.START), END = new A(Gravity.END), LEFT = new A(Gravity.LEFT), RIGHT = new A(Gravity.RIGHT), CENTER = new A(Gravity.CENTER), BASELINE = new A(Gravity.TOP), FILL = new A(Gravity.FILL);
    public static class Spec {
        final int start, size; final Alignment alignment; final float weight;
        Spec(int start, int size, Alignment a, float w) { this.start = start; this.size = size; alignment = a; weight = w; }
        public Alignment getAbsoluteAlignment(boolean horizontal) { return alignment; }
        @Override public boolean equals(Object o) { if (!(o instanceof Spec)) return false; Spec s = (Spec) o; return s.start == start && s.size == size && s.alignment == alignment && s.weight == weight; }
        @Override public int hashCode() { return start * 31 + size; }
    }
    public static Spec spec(int start, int size, Alignment a, float weight) { return new Spec(start, size, a, weight); }
    public static Spec spec(int start, Alignment a, float weight) { return spec(start, 1, a, weight); }
    public static Spec spec(int start, int size, float weight) { return spec(start, size, UNDEFINED_ALIGNMENT, weight); }
    public static Spec spec(int start, float weight) { return spec(start, 1, weight); }
    public static Spec spec(int start, int size, Alignment a) { return spec(start, size, a, 0); }
    public static Spec spec(int start, Alignment a) { return spec(start, 1, a); }
    public static Spec spec(int start, int size) { return spec(start, size, UNDEFINED_ALIGNMENT); }
    public static Spec spec(int start) { return spec(start, 1); }
    public static class LayoutParams extends MarginLayoutParams {
        public Spec rowSpec = spec(UNDEFINED), columnSpec = spec(UNDEFINED);
        int gravity = Gravity.NO_GRAVITY;
        public LayoutParams(Spec r, Spec c) { super(WRAP_CONTENT, WRAP_CONTENT); rowSpec = r; columnSpec = c; }
        public LayoutParams() { this(spec(UNDEFINED), spec(UNDEFINED)); }
        public LayoutParams(ViewGroup.LayoutParams p) { super(p); }
        public LayoutParams(MarginLayoutParams p) { super(p); }
        public LayoutParams(LayoutParams p) { super(p); rowSpec = p.rowSpec; columnSpec = p.columnSpec; gravity = p.gravity; }
        public LayoutParams(Context c, AttributeSet attrs) {
            super(c, attrs);
            TypedArray a = c.obtainStyledAttributes(attrs, husk.S.GridLayout_Layout);
            gravity = a.getInt(husk.S.GridLayout_Layout_layout_gravity, Gravity.NO_GRAVITY);
            int col = a.getInt(husk.S.GridLayout_Layout_layout_column, UNDEFINED), colSpan = a.getInt(husk.S.GridLayout_Layout_layout_columnSpan, 1);
            float colW = a.getFloat(husk.S.GridLayout_Layout_layout_columnWeight, 0);
            int row = a.getInt(husk.S.GridLayout_Layout_layout_row, UNDEFINED), rowSpan = a.getInt(husk.S.GridLayout_Layout_layout_rowSpan, 1);
            float rowW = a.getFloat(husk.S.GridLayout_Layout_layout_rowWeight, 0);
            a.recycle();
            columnSpec = spec(col, colSpan, UNDEFINED_ALIGNMENT, colW);
            rowSpec = spec(row, rowSpan, UNDEFINED_ALIGNMENT, rowW);
            if (width == 0 && height == 0) { width = WRAP_CONTENT; height = WRAP_CONTENT; }
        }
        public void setGravity(int g) { gravity = g; }
    }
    private int mOrientation = HORIZONTAL, mRowCount = UNDEFINED, mColumnCount = UNDEFINED, mAlignmentMode = ALIGN_MARGINS;
    private boolean mUseDefaultMargins;
    private int[] mCol, mRow, mColW, mRowH; private int[] mCellCol, mCellRow, mCellColSpan, mCellRowSpan;
    public GridLayout(Context c) { this(c, null); }
    public GridLayout(Context c, AttributeSet a) { this(c, a, 0); }
    public GridLayout(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public GridLayout(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.GridLayout, s, r);
        mOrientation = a.getInt(husk.S.GridLayout_orientation, HORIZONTAL);
        mRowCount = a.getInt(husk.S.GridLayout_rowCount, UNDEFINED);
        mColumnCount = a.getInt(husk.S.GridLayout_columnCount, UNDEFINED);
        mUseDefaultMargins = a.getBoolean(husk.S.GridLayout_useDefaultMargins, false);
        mAlignmentMode = a.getInt(husk.S.GridLayout_alignmentMode, ALIGN_MARGINS);
        a.recycle();
    }
    public int getOrientation() { return mOrientation; } public void setOrientation(int o) { mOrientation = o; requestLayout(); }
    public int getRowCount() { return mRowCount; } public void setRowCount(int c) { mRowCount = c; requestLayout(); }
    public int getColumnCount() { return mColumnCount; } public void setColumnCount(int c) { mColumnCount = c; requestLayout(); }
    public boolean getUseDefaultMargins() { return mUseDefaultMargins; } public void setUseDefaultMargins(boolean u) { mUseDefaultMargins = u; requestLayout(); }
    public int getAlignmentMode() { return mAlignmentMode; } public void setAlignmentMode(int m) { mAlignmentMode = m; requestLayout(); }
    public boolean isRowOrderPreserved() { return true; } public void setRowOrderPreserved(boolean p) {}
    public boolean isColumnOrderPreserved() { return true; } public void setColumnOrderPreserved(boolean p) {}
    public android.util.Printer getPrinter() { return null; } public void setPrinter(android.util.Printer p) {}
    private int defMargin() { return mUseDefaultMargins ? (int) (8 * getResources().getDisplayMetrics().density) : 0; }
    /** Each child's cell: its specs, or the next free one in flow order. */
    private void place() {
        int n = getChildCount();
        mCellCol = new int[n]; mCellRow = new int[n]; mCellColSpan = new int[n]; mCellRowSpan = new int[n];
        boolean horiz = mOrientation == HORIZONTAL;
        int major = horiz ? mColumnCount : mRowCount;
        java.util.HashSet<Long> used = new java.util.HashSet<>();
        int curR = 0, curC = 0;
        for (int i = 0; i < n; i++) {
            View v = getChildAt(i);
            LayoutParams lp = (LayoutParams) v.getLayoutParams();
            int cs = Math.max(1, lp.columnSpec.size), rs = Math.max(1, lp.rowSpec.size);
            int r = lp.rowSpec.start, c = lp.columnSpec.start;
            if (r == UNDEFINED || c == UNDEFINED) {
                if (r != UNDEFINED) curR = r;
                if (c != UNDEFINED) curC = c;
                while (true) {
                    if (horiz && major != UNDEFINED && curC + cs > major && curC > 0) { curC = 0; curR++; continue; }
                    if (!horiz && major != UNDEFINED && curR + rs > major && curR > 0) { curR = 0; curC++; continue; }
                    boolean free = true;
                    for (int a = 0; a < rs && free; a++) for (int b = 0; b < cs && free; b++) if (used.contains(((long) (curR + a) << 32) | (curC + b))) free = false;
                    if (free) break;
                    if (horiz) curC++; else curR++;
                }
                r = curR; c = curC;
            }
            for (int a = 0; a < rs; a++) for (int b = 0; b < cs; b++) used.add(((long) (r + a) << 32) | (c + b));
            mCellRow[i] = r; mCellCol[i] = c; mCellRowSpan[i] = rs; mCellColSpan[i] = cs;
            if (horiz) curC = c + cs; else curR = r + rs;
        }
    }
    @Override protected void onMeasure(int ws, int hs) {
        place();
        int n = getChildCount(), cols = 0, rows = 0;
        for (int i = 0; i < n; i++) { cols = Math.max(cols, mCellCol[i] + mCellColSpan[i]); rows = Math.max(rows, mCellRow[i] + mCellRowSpan[i]); }
        if (mColumnCount != UNDEFINED) cols = Math.max(cols, mColumnCount);
        if (mRowCount != UNDEFINED) rows = Math.max(rows, mRowCount);
        mColW = new int[cols]; mRowH = new int[rows];
        float[] colWeight = new float[cols], rowWeight = new float[rows];
        int dm = defMargin();
        for (int i = 0; i < n; i++) {
            View v = getChildAt(i);
            if (v.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) v.getLayoutParams();
            int ml = lp.leftMargin + dm, mr = lp.rightMargin + dm, mt = lp.topMargin + dm, mb = lp.bottomMargin + dm;
            int cws = lp.width >= 0 ? MeasureSpec.makeMeasureSpec(lp.width, MeasureSpec.EXACTLY) : MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
            int chs = lp.height >= 0 ? MeasureSpec.makeMeasureSpec(lp.height, MeasureSpec.EXACTLY) : MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
            v.measure(cws, chs);
            if (mCellColSpan[i] == 1) mColW[mCellCol[i]] = Math.max(mColW[mCellCol[i]], v.getMeasuredWidth() + ml + mr);
            if (mCellRowSpan[i] == 1) mRowH[mCellRow[i]] = Math.max(mRowH[mCellRow[i]], v.getMeasuredHeight() + mt + mb);
            for (int k = 0; k < mCellColSpan[i]; k++) colWeight[mCellCol[i] + k] = Math.max(colWeight[mCellCol[i] + k], lp.columnSpec.weight / mCellColSpan[i]);
            for (int k = 0; k < mCellRowSpan[i]; k++) rowWeight[mCellRow[i] + k] = Math.max(rowWeight[mCellRow[i] + k], lp.rowSpec.weight / mCellRowSpan[i]);
        }
        // spanning cells widen the columns they cover when needed
        for (int i = 0; i < n; i++) {
            View v = getChildAt(i);
            if (v.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) v.getLayoutParams();
            if (mCellColSpan[i] > 1) { int need = v.getMeasuredWidth() + lp.leftMargin + lp.rightMargin + 2 * dm, have = 0; for (int k = 0; k < mCellColSpan[i]; k++) have += mColW[mCellCol[i] + k]; if (need > have) mColW[mCellCol[i] + mCellColSpan[i] - 1] += need - have; }
            if (mCellRowSpan[i] > 1) { int need = v.getMeasuredHeight() + lp.topMargin + lp.bottomMargin + 2 * dm, have = 0; for (int k = 0; k < mCellRowSpan[i]; k++) have += mRowH[mCellRow[i] + k]; if (need > have) mRowH[mCellRow[i] + mCellRowSpan[i] - 1] += need - have; }
        }
        int w = getPaddingLeft() + getPaddingRight(), h = getPaddingTop() + getPaddingBottom();
        for (int x : mColW) w += x; for (int x : mRowH) h += x;
        if (MeasureSpec.getMode(ws) == MeasureSpec.EXACTLY) distribute(mColW, colWeight, MeasureSpec.getSize(ws) - w);
        if (MeasureSpec.getMode(hs) == MeasureSpec.EXACTLY) distribute(mRowH, rowWeight, MeasureSpec.getSize(hs) - h);
        w = getPaddingLeft() + getPaddingRight(); h = getPaddingTop() + getPaddingBottom();
        for (int x : mColW) w += x; for (int x : mRowH) h += x;
        // children that fill their cells, or weighted ones, measured to the cell
        for (int i = 0; i < n; i++) {
            View v = getChildAt(i);
            if (v.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) v.getLayoutParams();
            int cw = 0, ch = 0;
            for (int k = 0; k < mCellColSpan[i]; k++) cw += mColW[mCellCol[i] + k];
            for (int k = 0; k < mCellRowSpan[i]; k++) ch += mRowH[mCellRow[i] + k];
            cw -= lp.leftMargin + lp.rightMargin + 2 * dm; ch -= lp.topMargin + lp.bottomMargin + 2 * dm;
            boolean fillH = (lp.gravity & Gravity.HORIZONTAL_GRAVITY_MASK) == Gravity.FILL_HORIZONTAL || lp.width == 0 || lp.width == ViewGroup.LayoutParams.MATCH_PARENT;
            boolean fillV = (lp.gravity & Gravity.VERTICAL_GRAVITY_MASK) == Gravity.FILL_VERTICAL || lp.height == 0 || lp.height == ViewGroup.LayoutParams.MATCH_PARENT;
            if ((fillH && v.getMeasuredWidth() != cw) || (fillV && v.getMeasuredHeight() != ch))
                v.measure(fillH ? MeasureSpec.makeMeasureSpec(Math.max(0, cw), MeasureSpec.EXACTLY) : MeasureSpec.makeMeasureSpec(v.getMeasuredWidth(), MeasureSpec.EXACTLY), fillV ? MeasureSpec.makeMeasureSpec(Math.max(0, ch), MeasureSpec.EXACTLY) : MeasureSpec.makeMeasureSpec(v.getMeasuredHeight(), MeasureSpec.EXACTLY));
        }
        setMeasuredDimension(resolveSizeAndState(Math.max(w, getSuggestedMinimumWidth()), ws, 0), resolveSizeAndState(Math.max(h, getSuggestedMinimumHeight()), hs, 0));
    }
    private static void distribute(int[] sizes, float[] weights, int extra) {
        float total = 0; for (float w : weights) total += w;
        if (extra <= 0 || total <= 0) return;
        for (int i = 0; i < sizes.length; i++) if (weights[i] > 0) sizes[i] += (int) (extra * weights[i] / total);
    }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int n = getChildCount();
        int[] colX = new int[mColW.length + 1], rowY = new int[mRowH.length + 1];
        colX[0] = getPaddingLeft(); rowY[0] = getPaddingTop();
        for (int i = 0; i < mColW.length; i++) colX[i + 1] = colX[i] + mColW[i];
        for (int i = 0; i < mRowH.length; i++) rowY[i + 1] = rowY[i] + mRowH[i];
        int dm = defMargin();
        for (int i = 0; i < n; i++) {
            View v = getChildAt(i);
            if (v.getVisibility() == GONE) continue;
            LayoutParams lp = (LayoutParams) v.getLayoutParams();
            int cl = colX[mCellCol[i]] + lp.leftMargin + dm, cr = colX[mCellCol[i] + mCellColSpan[i]] - lp.rightMargin - dm;
            int ct = rowY[mCellRow[i]] + lp.topMargin + dm, cb = rowY[mCellRow[i] + mCellRowSpan[i]] - lp.bottomMargin - dm;
            int w = v.getMeasuredWidth(), h = v.getMeasuredHeight(), g = lp.gravity;
            int hg = Gravity.getAbsoluteGravity(g, getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK, vg = g & Gravity.VERTICAL_GRAVITY_MASK;
            int x = hg == Gravity.CENTER_HORIZONTAL ? cl + (cr - cl - w) / 2 : hg == Gravity.RIGHT ? cr - w : cl;
            int y = vg == Gravity.CENTER_VERTICAL ? ct + (cb - ct - h) / 2 : vg == Gravity.BOTTOM ? cb - h : ct;
            v.layout(x, y, x + w, y + h);
        }
    }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof LayoutParams; }
    @Override protected LayoutParams generateDefaultLayoutParams() { return new LayoutParams(); }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new LayoutParams(getContext(), a); }
    @Override protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) { if (p instanceof LayoutParams) return new LayoutParams((LayoutParams) p); if (p instanceof MarginLayoutParams) return new LayoutParams((MarginLayoutParams) p); return new LayoutParams(p); }
    @Override public CharSequence getAccessibilityClassName() { return GridLayout.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    protected void onDebugDraw(android.graphics.Canvas p0) {}
    protected void onDebugDrawMargins(android.graphics.Canvas p0, android.graphics.Paint p1) {}
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InspectionCompanion implements android.view.inspector.InspectionCompanion {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public InspectionCompanion() {}
        public void mapProperties(android.view.inspector.PropertyMapper p0) {}
        public void readProperties(android.widget.GridLayout p0, android.view.inspector.PropertyReader p1) {}
        public void readProperties(java.lang.Object p0, android.view.inspector.PropertyReader p1) {}
    }
    // ---- end of generated nested classes
}
