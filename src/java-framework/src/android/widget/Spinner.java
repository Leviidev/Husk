package android.widget;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.*;

/** The selected item's view with a drop-down arrow; a tap opens the items in a list popup (or a dialog in dialog mode). */
public class Spinner extends AbsSpinner implements android.content.DialogInterface.OnClickListener {
    public static final int MODE_DIALOG = 0, MODE_DROPDOWN = 1;
    private int mMode = MODE_DROPDOWN, mDropDownWidth = ViewGroup.LayoutParams.WRAP_CONTENT, mGravity = Gravity.CENTER;
    private CharSequence mPrompt;
    private Drawable mPopupBackground;
    private ListPopupWindow mPopup;
    private android.app.AlertDialog mDialog;
    private final Context mPopupContext;
    private final Paint mArrowPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private boolean mHasBackgroundArrow;
    public Spinner(Context c) { this(c, null); }
    public Spinner(Context c, int mode) { this(c, null, android.R.attr.spinnerStyle, mode); }
    public Spinner(Context c, AttributeSet a) { this(c, a, android.R.attr.spinnerStyle); }
    public Spinner(Context c, AttributeSet a, int s) { this(c, a, s, 0, -1); }
    public Spinner(Context c, AttributeSet a, int s, int mode) { this(c, a, s, 0, mode); }
    public Spinner(Context c, AttributeSet a, int s, int r, int mode) { this(c, a, s, r, mode, null); }
    public Spinner(Context c, AttributeSet attrs, int s, int r, int mode, Resources.Theme popupTheme) {
        super(c, attrs, s, r);
        mPopupContext = popupTheme != null ? new ContextThemeWrapper(c, popupTheme) : c;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.Spinner, s, r);
        if (mode == -1) mode = a.getInt(husk.S.Spinner_spinnerMode, MODE_DROPDOWN);
        mMode = mode;
        mPrompt = a.getText(husk.S.Spinner_prompt);
        mDropDownWidth = a.getLayoutDimension(husk.S.Spinner_dropDownWidth, ViewGroup.LayoutParams.WRAP_CONTENT);
        try { mPopupBackground = a.getDrawable(husk.S.Spinner_popupBackground); } catch (RuntimeException e) {}
        a.recycle();
        mHasBackgroundArrow = getBackground() != null && !(getBackground() instanceof android.graphics.drawable.ColorDrawable);
        setClickable(true);
    }
    public void setPopupBackgroundDrawable(Drawable d) { mPopupBackground = d; }
    public void setPopupBackgroundResource(int r) { setPopupBackgroundDrawable(getContext().getDrawable(r)); }
    public Drawable getPopupBackground() { return mPopupBackground; }
    public void setDropDownVerticalOffset(int o) {} public int getDropDownVerticalOffset() { return 0; }
    public void setDropDownHorizontalOffset(int o) {} public int getDropDownHorizontalOffset() { return 0; }
    public void setDropDownWidth(int w) { mDropDownWidth = w; } public int getDropDownWidth() { return mDropDownWidth; }
    public void setGravity(int g) { if (mGravity != g) { mGravity = g; requestLayout(); } } public int getGravity() { return mGravity; }
    public Context getPopupContext() { return mPopupContext; }
    public void setPrompt(CharSequence p) { mPrompt = p; } public void setPromptId(int r) { setPrompt(getContext().getText(r)); } public CharSequence getPrompt() { return mPrompt; }
    @Override public void setOnItemClickListener(OnItemClickListener l) { throw new RuntimeException("setOnItemClickListener cannot be used with a spinner."); }
    @Override public void setEnabled(boolean e) { super.setEnabled(e); }
    private View mSelectedView;
    private void syncSelectedView() {
        if (mAdapter == null || mItemCount == 0 || mSelectedPosition < 0) { removeAllViewsInLayout(); mSelectedView = null; return; }
        View v = mAdapter.getView(mSelectedPosition, mSelectedView, this);
        if (v != mSelectedView || v.getParent() != this) {
            removeAllViewsInLayout();
            ViewGroup.LayoutParams lp = v.getLayoutParams();
            if (lp == null) lp = generateDefaultLayoutParams();
            addViewInLayout(v, 0, lp, true);
            mSelectedView = v;
        }
        v.setEnabled(isEnabled());
    }
    @Override protected ViewGroup.LayoutParams generateDefaultLayoutParams() { return new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT); }
    private int arrowSpace() { return mHasBackgroundArrow ? 0 : (int) (24 * getResources().getDisplayMetrics().density); }
    @Override protected void onMeasure(int ws, int hs) {
        syncSelectedView();
        int pl = getPaddingLeft(), pr = getPaddingRight() + arrowSpace(), pt = getPaddingTop(), pb = getPaddingBottom();
        int w = 0, h = 0;
        if (mSelectedView != null) {
            ViewGroup.LayoutParams lp = mSelectedView.getLayoutParams();
            mSelectedView.measure(getChildMeasureSpec(ws, pl + pr, lp.width), getChildMeasureSpec(hs, pt + pb, lp.height));
            w = mSelectedView.getMeasuredWidth(); h = mSelectedView.getMeasuredHeight();
        }
        if (mMode == MODE_DROPDOWN && MeasureSpec.getMode(ws) == MeasureSpec.AT_MOST && mAdapter != null) w = Math.max(w, Math.min(MeasureSpec.getSize(ws) - pl - pr, measureContentWidth()));
        w = Math.max(w + pl + pr, getSuggestedMinimumWidth()); h = Math.max(h + pt + pb, getSuggestedMinimumHeight());
        setMeasuredDimension(resolveSizeAndState(w, ws, 0), resolveSizeAndState(h, hs, 0));
    }
    int measureContentWidth() {
        if (mAdapter == null) return 0;
        int width = 0; View itemView = null; int itemType = 0;
        final int ws = MeasureSpec.makeSafeMeasureSpec(getMeasuredWidth(), MeasureSpec.UNSPECIFIED), hs = MeasureSpec.makeSafeMeasureSpec(getMeasuredHeight(), MeasureSpec.UNSPECIFIED);
        int start = Math.max(0, mSelectedPosition), end = Math.min(mAdapter.getCount(), start + 15);
        start = Math.max(0, start - (15 - (end - start)));
        for (int i = start; i < end; i++) {
            final int positionType = mAdapter.getItemViewType(i);
            if (positionType != itemType) { itemType = positionType; itemView = null; }
            itemView = mAdapter.getView(i, itemView, this);
            if (itemView.getLayoutParams() == null) itemView.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            itemView.measure(ws, hs);
            width = Math.max(width, itemView.getMeasuredWidth());
        }
        return width;
    }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        mInLayout = true;
        syncSelectedView();
        if (mSelectedView != null) {
            int pl = getPaddingLeft(), pr = getPaddingRight() + arrowSpace();
            int w = mSelectedView.getMeasuredWidth(), h = mSelectedView.getMeasuredHeight();
            int avail = r - l - pl - pr;
            int left;
            switch (Gravity.getAbsoluteGravity(mGravity, getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK) {
            case Gravity.CENTER_HORIZONTAL: left = pl + (avail - w) / 2; break;
            case Gravity.RIGHT: left = pl + avail - w; break;
            default: left = pl;
            }
            int top = getPaddingTop() + (b - t - getPaddingTop() - getPaddingBottom() - h) / 2;
            mSelectedView.layout(left, top, left + w, top + h);
        }
        mInLayout = false;
        mDataChanged = false;
        checkSelectionChanged();
    }
    @Override protected void onDraw(Canvas c) {
        super.onDraw(c);
        if (mHasBackgroundArrow) return;
        float d = getResources().getDisplayMetrics().density;
        float cx = getWidth() - getPaddingRight() - 12 * d, cy = getHeight() / 2f;
        mArrowPaint.setColor(0x8A000000);
        android.util.TypedValue v = new android.util.TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorControlNormal, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) mArrowPaint.setColor(v.data);
        Path p = new Path(); p.moveTo(cx - 5 * d, cy - 2.5f * d); p.lineTo(cx + 5 * d, cy - 2.5f * d); p.lineTo(cx, cy + 2.5f * d); p.close();
        c.drawPath(p, mArrowPaint);
    }
    @Override public boolean performClick() {
        boolean handled = super.performClick();
        if (!handled) {
            handled = true;
            if (mAdapter == null) return true;
            if (mMode == MODE_DIALOG) showDialog(); else showDropDown();
        }
        return true;
    }
    @Override public boolean onTouchEvent(MotionEvent e) { return super.onTouchEvent(e); }
    private void showDropDown() {
        mPopup = new ListPopupWindow(mPopupContext);
        if (mPopupBackground != null) mPopup.setBackgroundDrawable(mPopupBackground);
        final SpinnerAdapter sa = mAdapter;
        mPopup.setAdapter(new DropDownAdapter(sa));
        mPopup.setAnchorView(this);
        mPopup.setModal(true);
        mPopup.setWidth(mDropDownWidth == ViewGroup.LayoutParams.MATCH_PARENT ? getWidth() : ListPopupWindow.WRAP_CONTENT);
        mPopup.setVerticalOffset(-getHeight());
        mPopup.setOnItemClickListener((parent, v, position, id) -> { setSelection(position); mPopup.dismiss(); });
        mPopup.setSelection(mSelectedPosition);
        mPopup.show();
    }
    private void showDialog() {
        android.app.AlertDialog.Builder b = new android.app.AlertDialog.Builder(mPopupContext);
        if (mPrompt != null) b.setTitle(mPrompt);
        mDialog = b.setSingleChoiceItems(new DropDownAdapter(mAdapter), getSelectedItemPosition(), this).create();
        mDialog.show();
    }
    public void onClick(android.content.DialogInterface dialog, int which) { setSelection(which); if (mDialog != null) { mDialog.dismiss(); mDialog = null; } }
    @Override public CharSequence getAccessibilityClassName() { return Spinner.class.getName(); }
    private static class DropDownAdapter implements ListAdapter, SpinnerAdapter {
        private final SpinnerAdapter mAdapter;
        DropDownAdapter(SpinnerAdapter a) { mAdapter = a; }
        public int getCount() { return mAdapter == null ? 0 : mAdapter.getCount(); }
        public Object getItem(int p) { return mAdapter == null ? null : mAdapter.getItem(p); }
        public long getItemId(int p) { return mAdapter == null ? -1 : mAdapter.getItemId(p); }
        public View getView(int p, View cv, ViewGroup parent) { return getDropDownView(p, cv, parent); }
        public View getDropDownView(int p, View cv, ViewGroup parent) { return mAdapter == null ? null : mAdapter.getDropDownView(p, cv, parent); }
        public boolean hasStableIds() { return mAdapter != null && mAdapter.hasStableIds(); }
        public void registerDataSetObserver(android.database.DataSetObserver o) { if (mAdapter != null) mAdapter.registerDataSetObserver(o); }
        public void unregisterDataSetObserver(android.database.DataSetObserver o) { if (mAdapter != null) mAdapter.unregisterDataSetObserver(o); }
        public boolean areAllItemsEnabled() { return !(mAdapter instanceof ListAdapter) || ((ListAdapter) mAdapter).areAllItemsEnabled(); }
        public boolean isEnabled(int p) { return !(mAdapter instanceof ListAdapter) || ((ListAdapter) mAdapter).isEnabled(p); }
        public int getItemViewType(int p) { return 0; }
        public int getViewTypeCount() { return 1; }
        public boolean isEmpty() { return getCount() == 0; }
    }
}
