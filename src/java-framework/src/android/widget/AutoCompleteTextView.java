package android.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;

/** An edit text that filters an adapter as the user types and shows the matches in a drop-down. */
public class AutoCompleteTextView extends EditText implements Filter.FilterListener {
    public interface Validator { boolean isValid(CharSequence t); CharSequence fixText(CharSequence invalid); }
    public interface OnDismissListener { void onDismiss(); }
    private ListAdapter mAdapter;
    private Filter mFilter;
    private int mThreshold = 2;
    private final ListPopupWindow mPopup;
    private AdapterView.OnItemClickListener mItemClickListener;
    private AdapterView.OnItemSelectedListener mItemSelectedListener;
    private boolean mBlockCompletion, mPopupCanBeUpdated = true;
    private Validator mValidator;
    private CharSequence mHintText;
    private int mDropDownAnchorId = View.NO_ID;
    public AutoCompleteTextView(Context c) { this(c, null); }
    public AutoCompleteTextView(Context c, AttributeSet a) { this(c, a, android.R.attr.autoCompleteTextViewStyle); }
    public AutoCompleteTextView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public AutoCompleteTextView(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        mPopup = new ListPopupWindow(c, attrs, s, r);
        mPopup.setSoftInputMode(android.view.WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        mPopup.setPromptPosition(ListPopupWindow.POSITION_PROMPT_BELOW);
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.AutoCompleteTextView, s, r);
        mThreshold = a.getInt(husk.S.AutoCompleteTextView_completionThreshold, 2);
        mPopup.setWidth(a.getLayoutDimension(husk.S.AutoCompleteTextView_dropDownWidth, ListPopupWindow.WRAP_CONTENT));
        mPopup.setHeight(a.getLayoutDimension(husk.S.AutoCompleteTextView_dropDownHeight, ListPopupWindow.WRAP_CONTENT));
        mHintText = a.getText(husk.S.AutoCompleteTextView_completionHint);
        a.recycle();
        mPopup.setOnItemClickListener((parent, v, position, id) -> performCompletion(v, position, id));
        addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
            public void onTextChanged(CharSequence s, int st, int b, int c) {}
            public void afterTextChanged(Editable s) { doAfterTextChanged(); }
        });
        setRawInputType(getInputType() | android.text.InputType.TYPE_TEXT_FLAG_AUTO_COMPLETE);
    }
    public void setCompletionHint(CharSequence h) { mHintText = h; }
    public CharSequence getCompletionHint() { return mHintText; }
    public int getDropDownWidth() { return mPopup.getWidth(); } public void setDropDownWidth(int w) { mPopup.setWidth(w); }
    public int getDropDownHeight() { return mPopup.getHeight(); } public void setDropDownHeight(int h) { mPopup.setHeight(h); }
    public int getDropDownAnchor() { return mDropDownAnchorId; } public void setDropDownAnchor(int id) { mDropDownAnchorId = id; }
    public android.graphics.drawable.Drawable getDropDownBackground() { return mPopup.getBackground(); }
    public void setDropDownBackgroundDrawable(android.graphics.drawable.Drawable d) { mPopup.setBackgroundDrawable(d); }
    public void setDropDownBackgroundResource(int id) { mPopup.setBackgroundDrawable(getContext().getDrawable(id)); }
    public void setDropDownVerticalOffset(int o) { mPopup.setVerticalOffset(o); } public int getDropDownVerticalOffset() { return mPopup.getVerticalOffset(); }
    public void setDropDownHorizontalOffset(int o) { mPopup.setHorizontalOffset(o); } public int getDropDownHorizontalOffset() { return mPopup.getHorizontalOffset(); }
    public void setDropDownAnimationStyle(int a) {} public int getDropDownAnimationStyle() { return 0; }
    public boolean isDropDownAlwaysVisible() { return false; } public void setDropDownAlwaysVisible(boolean v) {}
    public boolean isDropDownDismissedOnCompletion() { return true; } public void setDropDownDismissedOnCompletion(boolean d) {}
    public int getThreshold() { return mThreshold; } public void setThreshold(int t) { mThreshold = Math.max(1, t); }
    public void setOnItemClickListener(AdapterView.OnItemClickListener l) { mItemClickListener = l; }
    public void setOnItemSelectedListener(AdapterView.OnItemSelectedListener l) { mItemSelectedListener = l; }
    public AdapterView.OnItemClickListener getOnItemClickListener() { return mItemClickListener; }
    public AdapterView.OnItemSelectedListener getOnItemSelectedListener() { return mItemSelectedListener; }
    public void setOnDismissListener(OnDismissListener l) { mPopup.setOnDismissListener(l == null ? null : l::onDismiss); }
    public ListAdapter getAdapter() { return mAdapter; }
    public <T extends ListAdapter & Filterable> void setAdapter(T adapter) {
        mAdapter = adapter;
        mFilter = adapter != null ? ((Filterable) adapter).getFilter() : null;
        mPopup.setAdapter(mAdapter);
    }
    public boolean enoughToFilter() { return getText().length() >= mThreshold; }
    void doAfterTextChanged() {
        if (mBlockCompletion) return;
        if (enoughToFilter()) { if (mFilter != null) { mPopupCanBeUpdated = true; performFiltering(getText(), 0); } }
        else { if (!mPopup.isDropDownAlwaysVisible()) dismissDropDown(); if (mFilter != null) mFilter.filter(null); }
    }
    public boolean isPopupShowing() { return mPopup.isShowing(); }
    protected CharSequence convertSelectionToString(Object selectedItem) { return mFilter.convertResultToString(selectedItem); }
    public void clearListSelection() { mPopup.clearListSelection(); }
    public void setListSelection(int p) { mPopup.setSelection(p); }
    public int getListSelection() { return mPopup.getSelectedItemPosition(); }
    protected void performFiltering(CharSequence text, int keyCode) { mFilter.filter(text, this); }
    public void performCompletion() { performCompletion(null, -1, -1); }
    private void performCompletion(View selectedView, int position, long id) {
        if (isPopupShowing()) {
            Object selectedItem = position < 0 ? mPopup.getSelectedItem() : mAdapter.getItem(position);
            if (selectedItem == null) return;
            mBlockCompletion = true;
            replaceText(convertSelectionToString(selectedItem));
            mBlockCompletion = false;
            if (mItemClickListener != null) { final ListPopupWindow list = mPopup; if (selectedView == null || position < 0) { selectedView = list.getSelectedView(); position = list.getSelectedItemPosition(); id = list.getSelectedItemId(); } mItemClickListener.onItemClick(list.getListView(), selectedView, position, id); }
        }
        dismissDropDown();
    }
    public boolean isPerformingCompletion() { return mBlockCompletion; }
    public void setText(CharSequence text, boolean filter) { if (filter) setText(text); else { mBlockCompletion = true; setText(text); mBlockCompletion = false; } }
    protected void replaceText(CharSequence text) { clearComposingText(); setText(text); Editable spannable = getText(); android.text.Selection.setSelection(spannable, spannable.length()); }
    public void onFilterComplete(int count) { updateDropDownForFilter(count); }
    private void updateDropDownForFilter(int count) {
        if (getWindowVisibility() == View.GONE) return;
        final boolean enough = enoughToFilter();
        if ((count > 0) && enough) { if (hasFocus() && hasWindowFocus() && mPopupCanBeUpdated) showDropDown(); }
        else if (mPopup.isShowing()) { dismissDropDown(); mPopupCanBeUpdated = true; }
    }
    @Override public void onWindowFocusChanged(boolean f) { super.onWindowFocusChanged(f); if (!f) dismissDropDown(); }
    @Override protected void onFocusChanged(boolean focused, int direction, android.graphics.Rect previous) { super.onFocusChanged(focused, direction, previous); if (!focused) { performValidation(); dismissDropDown(); } }
    @Override protected void onDetachedFromWindow() { dismissDropDown(); super.onDetachedFromWindow(); }
    public void dismissDropDown() { mPopup.dismiss(); mPopupCanBeUpdated = false; }
    public void showDropDownAfterLayout() { mPopup.postShow(); }
    public void showDropDown() {
        View anchor = mDropDownAnchorId != View.NO_ID ? getRootView().findViewById(mDropDownAnchorId) : null;
        mPopup.setAnchorView(anchor != null ? anchor : this);
        if (!isPopupShowing()) { mPopup.setInputMethodMode(ListPopupWindow.INPUT_METHOD_NEEDED); }
        mPopup.show();
    }
    public void setValidator(Validator v) { mValidator = v; }
    public Validator getValidator() { return mValidator; }
    public void performValidation() { if (mValidator == null) return; CharSequence text = getText(); if (!android.text.TextUtils.isEmpty(text) && !mValidator.isValid(text)) setText(mValidator.fixText(text)); }
    protected Filter getFilter() { return mFilter; }
    @Override public boolean onKeyPreIme(int keyCode, KeyEvent e) { if (keyCode == KeyEvent.KEYCODE_BACK && isPopupShowing()) { if (e.getAction() == KeyEvent.ACTION_UP) dismissDropDown(); return true; } return super.onKeyPreIme(keyCode, e); }
    @Override public CharSequence getAccessibilityClassName() { return AutoCompleteTextView.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public void ensureImeVisible(boolean p0) {}
    public int getInputMethodMode() { return (huskFill.get("InputMethodMode") instanceof Integer ? (Integer) huskFill.get("InputMethodMode") : 0); }
    public android.widget.AdapterView.OnItemClickListener getItemClickListener() { return null; }
    public android.widget.AdapterView.OnItemSelectedListener getItemSelectedListener() { return null; }
    public boolean isInputMethodNotNeeded() { return false; }
    public void onCommitCompletion(android.view.inputmethod.CompletionInfo p0) {}
    protected void onDisplayHint(int p0) {}
    public void refreshAutoCompleteResults() {}
    public void setForceIgnoreOutsideTouch(boolean p0) {}
    public void setInputMethodMode(int p0) { huskFill.put("InputMethodMode", Integer.valueOf(p0)); }
    // ---- end of generated members
}
