package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.database.DataSetObserver;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.*;

/** A popup with a list of an adapter's items, dropped under an anchor (menus, spinners, auto-complete). */
public class ListPopupWindow {
    public static final int MATCH_PARENT = -1, WRAP_CONTENT = -2, POSITION_PROMPT_ABOVE = 0, POSITION_PROMPT_BELOW = 1, INPUT_METHOD_FROM_FOCUSABLE = 0, INPUT_METHOD_NEEDED = 1, INPUT_METHOD_NOT_NEEDED = 2;
    private final Context mContext;
    private ListAdapter mAdapter;
    private ListView mDropDownList;
    private int mDropDownHeight = WRAP_CONTENT, mDropDownWidth = WRAP_CONTENT, mDropDownHorizontalOffset, mDropDownVerticalOffset, mDropDownGravity = Gravity.NO_GRAVITY, mPromptPosition = POSITION_PROMPT_ABOVE;
    private boolean mModal;
    private View mDropDownAnchorView, mPromptView;
    private Drawable mDropDownListHighlight;
    private AdapterView.OnItemClickListener mItemClickListener;
    private AdapterView.OnItemSelectedListener mItemSelectedListener;
    private final PopupWindow mPopup;
    private DataSetObserver mObserver;
    private int mSelection = -1;
    public ListPopupWindow(Context c) { this(c, null, android.R.attr.listPopupWindowStyle, 0); }
    public ListPopupWindow(Context c, AttributeSet a) { this(c, a, android.R.attr.listPopupWindowStyle, 0); }
    public ListPopupWindow(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public ListPopupWindow(Context c, AttributeSet attrs, int s, int r) {
        mContext = c;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.ListPopupWindow, s, r);
        mDropDownHorizontalOffset = a.getDimensionPixelOffset(husk.S.ListPopupWindow_dropDownHorizontalOffset, 0);
        mDropDownVerticalOffset = a.getDimensionPixelOffset(husk.S.ListPopupWindow_dropDownVerticalOffset, 0);
        a.recycle();
        mPopup = new PopupWindow(c, attrs, s, r);
        mPopup.setInputMethodMode(PopupWindow.INPUT_METHOD_NEEDED);
        if (mPopup.getBackground() == null) mPopup.setBackgroundDrawable(defaultBackground(c));
        if (mPopup.getElevation() == 0) mPopup.setElevation(8 * c.getResources().getDisplayMetrics().density);
    }
    static Drawable defaultBackground(Context c) {
        android.graphics.drawable.GradientDrawable g = new android.graphics.drawable.GradientDrawable();
        android.util.TypedValue v = new android.util.TypedValue();
        int bg = 0xFFFFFFFF;
        if (c.getTheme().resolveAttribute(android.R.attr.colorBackgroundFloating, v, true) && v.type >= android.util.TypedValue.TYPE_FIRST_COLOR_INT && v.type <= android.util.TypedValue.TYPE_LAST_COLOR_INT) bg = v.data;
        else if (c.getTheme().resolveAttribute(android.R.attr.colorBackgroundFloating, v, true) && v.resourceId != 0) { try { bg = c.getColor(v.resourceId); } catch (Exception e) {} }
        g.setColor(bg);
        g.setCornerRadius(4 * c.getResources().getDisplayMetrics().density);
        return g;
    }
    public void setAdapter(ListAdapter adapter) {
        if (mObserver == null) mObserver = new DataSetObserver() { @Override public void onChanged() { if (isShowing()) show(); } @Override public void onInvalidated() { dismiss(); } };
        else if (mAdapter != null) mAdapter.unregisterDataSetObserver(mObserver);
        mAdapter = adapter;
        if (adapter != null) adapter.registerDataSetObserver(mObserver);
        if (mDropDownList != null) mDropDownList.setAdapter(mAdapter);
    }
    public void setPromptPosition(int p) { mPromptPosition = p; }
    public int getPromptPosition() { return mPromptPosition; }
    public void setModal(boolean m) { mModal = m; mPopup.setFocusable(m); }
    public boolean isModal() { return mModal; }
    public void setForceIgnoreOutsideTouch(boolean f) {}
    public void setDropDownAlwaysVisible(boolean v) {}
    public boolean isDropDownAlwaysVisible() { return false; }
    public void setSoftInputMode(int m) { mPopup.setSoftInputMode(m); }
    public int getSoftInputMode() { return mPopup.getSoftInputMode(); }
    public void setListSelector(Drawable d) { mDropDownListHighlight = d; }
    public Drawable getBackground() { return mPopup.getBackground(); }
    public void setBackgroundDrawable(Drawable d) { mPopup.setBackgroundDrawable(d); }
    public void setAnimationStyle(int a) { mPopup.setAnimationStyle(a); }
    public int getAnimationStyle() { return mPopup.getAnimationStyle(); }
    public View getAnchorView() { return mDropDownAnchorView; }
    public void setAnchorView(View a) { mDropDownAnchorView = a; }
    public int getHorizontalOffset() { return mDropDownHorizontalOffset; }
    public void setHorizontalOffset(int o) { mDropDownHorizontalOffset = o; }
    public int getVerticalOffset() { return mDropDownVerticalOffset; }
    public void setVerticalOffset(int o) { mDropDownVerticalOffset = o; }
    public void setEpicenterBounds(Rect b) {}
    public void setDropDownGravity(int g) { mDropDownGravity = g; }
    public int getWidth() { return mDropDownWidth; }
    public void setWidth(int w) { mDropDownWidth = w; }
    public void setContentWidth(int w) { Drawable p = mPopup.getBackground(); if (p != null) { Rect r = new Rect(); p.getPadding(r); mDropDownWidth = r.left + r.right + w; } else mDropDownWidth = w; }
    public int getHeight() { return mDropDownHeight; }
    public void setHeight(int h) { mDropDownHeight = h; }
    public void setOnItemClickListener(AdapterView.OnItemClickListener l) { mItemClickListener = l; }
    public void setOnItemSelectedListener(AdapterView.OnItemSelectedListener l) { mItemSelectedListener = l; }
    public void setPromptView(View p) { mPromptView = p; }
    public void postShow() { new android.os.Handler(android.os.Looper.getMainLooper()).post(this::show); }
    public void show() {
        if (mDropDownList == null) {
            mDropDownList = new ListView(mContext);
            if (mDropDownListHighlight != null) mDropDownList.setSelector(mDropDownListHighlight);
            mDropDownList.setAdapter(mAdapter);
            mDropDownList.setOnItemClickListener(mItemClickListener);
            mDropDownList.setFocusable(true); mDropDownList.setFocusableInTouchMode(true);
            mDropDownList.setDivider(null);
            int pad = (int) (8 * mContext.getResources().getDisplayMetrics().density);
            mDropDownList.setPadding(0, pad, 0, pad);
            mDropDownList.setClipToPadding(false);
            View content = mDropDownList;
            if (mPromptView != null) {
                LinearLayout ll = new LinearLayout(mContext); ll.setOrientation(LinearLayout.VERTICAL);
                LinearLayout.LayoutParams hint = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1.0f);
                if (mPromptPosition == POSITION_PROMPT_BELOW) { ll.addView(mDropDownList, hint); ll.addView(mPromptView); } else { ll.addView(mPromptView); ll.addView(mDropDownList, hint); }
                content = ll;
            }
            mPopup.setContentView(content);
        }
        int width = mDropDownWidth;
        if (width == WRAP_CONTENT && mDropDownAnchorView != null) width = Math.max(measureContentWidth(), mDropDownAnchorView.getWidth());
        else if (width == MATCH_PARENT && mDropDownAnchorView != null) width = mDropDownAnchorView.getWidth();
        int maxH = mDropDownAnchorView != null ? mPopup.getMaxAvailableHeight(mDropDownAnchorView, mDropDownVerticalOffset) : husk.Native.screenHeight() / 2;
        int height = mDropDownHeight;
        if (height == WRAP_CONTENT) height = Math.min(maxH, mDropDownList.measureHeightOfChildren(View.MeasureSpec.makeMeasureSpec(Math.max(width, 0), View.MeasureSpec.EXACTLY), 0, -1, maxH, -1) + (mPromptView != null ? 0 : 0));
        if (mPopup.isShowing()) { mPopup.update(mDropDownAnchorView, mDropDownHorizontalOffset, mDropDownVerticalOffset, width, height); return; }
        mPopup.setWidth(width); mPopup.setHeight(height);
        mPopup.setOutsideTouchable(true);
        mPopup.setFocusable(mModal);
        if (mDropDownAnchorView != null) mPopup.showAsDropDown(mDropDownAnchorView, mDropDownHorizontalOffset, mDropDownVerticalOffset, mDropDownGravity);
        if (mSelection >= 0) mDropDownList.setSelection(mSelection);
    }
    private int measureContentWidth() {
        if (mAdapter == null) return 0;
        int maxWidth = 0; View itemView = null; int itemType = 0;
        final int widthMeasureSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED), heightMeasureSpec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        FrameLayout parent = new FrameLayout(mContext);
        final int count = Math.min(mAdapter.getCount(), 15);
        for (int i = 0; i < count; i++) {
            final int positionType = mAdapter.getItemViewType(i);
            if (positionType != itemType) { itemType = positionType; itemView = null; }
            itemView = mAdapter.getView(i, itemView, parent);
            if (itemView.getLayoutParams() == null) itemView.setLayoutParams(new ViewGroup.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            itemView.measure(widthMeasureSpec, heightMeasureSpec);
            maxWidth = Math.max(maxWidth, itemView.getMeasuredWidth());
        }
        Drawable bg = mPopup.getBackground();
        if (bg != null) { Rect r = new Rect(); bg.getPadding(r); maxWidth += r.left + r.right; }
        return maxWidth;
    }
    public void dismiss() { mPopup.dismiss(); mPopup.setContentView(null); mDropDownList = null; }
    public void setOnDismissListener(PopupWindow.OnDismissListener l) { mPopup.setOnDismissListener(l); }
    public void setInputMethodMode(int m) { mPopup.setInputMethodMode(m); }
    public int getInputMethodMode() { return mPopup.getInputMethodMode(); }
    public void setSelection(int p) { mSelection = p; if (mDropDownList != null) mDropDownList.setSelection(p); }
    public void clearListSelection() {}
    public boolean isInputMethodNotNeeded() { return mPopup.getInputMethodMode() == INPUT_METHOD_NOT_NEEDED; }
    public boolean performItemClick(int position) { if (isShowing() && mItemClickListener != null) { View child = mDropDownList.getChildAt(position - mDropDownList.getFirstVisiblePosition()); mItemClickListener.onItemClick(mDropDownList, child, position, mAdapter.getItemId(position)); return true; } return false; }
    public Object getSelectedItem() { return isShowing() ? mDropDownList.getSelectedItem() : null; }
    public int getSelectedItemPosition() { return isShowing() ? mDropDownList.getSelectedItemPosition() : ListView.INVALID_POSITION; }
    public long getSelectedItemId() { return isShowing() ? mDropDownList.getSelectedItemId() : ListView.INVALID_ROW_ID; }
    public View getSelectedView() { return isShowing() ? mDropDownList.getSelectedView() : null; }
    public ListView getListView() { return mDropDownList; }
    public boolean isShowing() { return mPopup.isShowing(); }
    public boolean onKeyDown(int k, KeyEvent e) { return false; }
    public boolean onKeyUp(int k, KeyEvent e) { if (isShowing() && k == KeyEvent.KEYCODE_BACK) { dismiss(); return true; } return false; }
    public boolean onKeyPreIme(int k, KeyEvent e) { return onKeyUp(k, e); }
    public View.OnTouchListener createDragToOpenListener(View src) { return null; }
}
