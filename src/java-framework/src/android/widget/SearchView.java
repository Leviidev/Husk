package android.widget;

import android.content.Context;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.KeyEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

/** A search field: query text, submit from the keyboard, a close button that clears it. */
public class SearchView extends LinearLayout {
    public interface OnQueryTextListener { boolean onQueryTextSubmit(String query); boolean onQueryTextChange(String newText); }
    public interface OnCloseListener { boolean onClose(); }
    public interface OnSuggestionListener { boolean onSuggestionSelect(int position); boolean onSuggestionClick(int position); }
    private final EditText mQuery;
    private final ImageButton mClose;
    private OnQueryTextListener mOnQuery;
    private OnCloseListener mOnClose;
    private OnClickListener mOnSearchClick;
    private OnFocusChangeListener mOnQueryFocus;
    private boolean mIconified = true, mIconifiedByDefault = true, mSubmitEnabled;
    private CharSequence mQueryHint;
    private int mMaxWidth;
    public SearchView(Context c) { this(c, null); }
    public SearchView(Context c, AttributeSet a) { this(c, a, android.R.attr.searchViewStyle); }
    public SearchView(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public SearchView(Context c, AttributeSet a, int s, int r) {
        super(c, a, s, r);
        setOrientation(HORIZONTAL);
        setGravity(android.view.Gravity.CENTER_VERTICAL);
        mQuery = new EditText(c);
        mQuery.setSingleLine(true);
        mQuery.setImeOptions(EditorInfo.IME_ACTION_SEARCH);
        mQuery.setBackground(null);
        mQuery.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {}
            public void onTextChanged(CharSequence s, int a, int b, int c) { if (mOnQuery != null) mOnQuery.onQueryTextChange(s.toString()); mClose.setVisibility(s.length() > 0 || !mIconifiedByDefault ? VISIBLE : GONE); }
            public void afterTextChanged(Editable s) {}
        });
        mQuery.setOnEditorActionListener((v, actionId, e) -> { if (actionId == EditorInfo.IME_ACTION_SEARCH || actionId == EditorInfo.IME_ACTION_DONE || (e != null && e.getKeyCode() == KeyEvent.KEYCODE_ENTER)) { submit(); return true; } return false; });
        mQuery.setOnFocusChangeListener((v, f) -> { if (mOnQueryFocus != null) mOnQueryFocus.onFocusChange(this, f); });
        mClose = new ImageButton(c, null, 0);
        mClose.setBackground(null);
        mClose.setImageDrawable(new CloseDrawable(c.getResources().getDisplayMetrics().density));
        mClose.setOnClickListener(v -> { if (mQuery.getText().length() > 0) mQuery.setText(""); else if (mIconifiedByDefault) { if (mOnClose == null || !mOnClose.onClose()) setIconified(true); } });
        addView(mQuery, new LayoutParams(0, LayoutParams.WRAP_CONTENT, 1f));
        addView(mClose, new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        mClose.setVisibility(GONE);
        setIconified(true);
    }
    private void submit() { String q = mQuery.getText().toString(); if (mOnQuery == null || !mOnQuery.onQueryTextSubmit(q)) { android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE); if (imm != null) imm.hideSoftInputFromWindow(getWindowToken(), 0); } }
    public void setSearchableInfo(android.app.SearchableInfo info) {}
    public void setImeOptions(int o) { mQuery.setImeOptions(o); } public int getImeOptions() { return mQuery.getImeOptions(); }
    public void setInputType(int t) { mQuery.setInputType(t); } public int getInputType() { return mQuery.getInputType(); }
    public void setOnQueryTextListener(OnQueryTextListener l) { mOnQuery = l; }
    public void setOnCloseListener(OnCloseListener l) { mOnClose = l; }
    public void setOnQueryTextFocusChangeListener(OnFocusChangeListener l) { mOnQueryFocus = l; }
    public void setOnSuggestionListener(OnSuggestionListener l) {}
    public void setOnSearchClickListener(OnClickListener l) { mOnSearchClick = l; }
    public CharSequence getQuery() { return mQuery.getText(); }
    public void setQuery(CharSequence q, boolean submit) { mQuery.setText(q); if (q != null) mQuery.setSelection(q.length()); if (submit && q != null && q.length() > 0) submit(); }
    public void setQueryHint(CharSequence h) { mQueryHint = h; mQuery.setHint(h); } public CharSequence getQueryHint() { return mQueryHint; }
    public void setIconifiedByDefault(boolean i) { mIconifiedByDefault = i; setIconified(i); } public boolean isIconfiedByDefault() { return mIconifiedByDefault; } public boolean isIconifiedByDefault() { return mIconifiedByDefault; }
    public void setIconified(boolean i) {
        mIconified = i;
        mQuery.setVisibility(i ? GONE : VISIBLE);
        mClose.setVisibility(i ? GONE : (mQuery.getText().length() > 0 || !mIconifiedByDefault ? VISIBLE : VISIBLE));
        if (i) setOnClickListener(v -> { setIconified(false); if (mOnSearchClick != null) mOnSearchClick.onClick(this); mQuery.requestFocus(); }); else setOnClickListener(null);
        setClickable(i);
        setMinimumWidth(i ? (int) (48 * getResources().getDisplayMetrics().density) : 0);
        setMinimumHeight((int) (48 * getResources().getDisplayMetrics().density));
    }
    public boolean isIconified() { return mIconified; }
    public void setSubmitButtonEnabled(boolean e) { mSubmitEnabled = e; } public boolean isSubmitButtonEnabled() { return mSubmitEnabled; }
    public void setQueryRefinementEnabled(boolean e) {} public boolean isQueryRefinementEnabled() { return false; }
    public void setSuggestionsAdapter(CursorAdapter a) {} public CursorAdapter getSuggestionsAdapter() { return null; }
    public void setMaxWidth(int w) { mMaxWidth = w; requestLayout(); } public int getMaxWidth() { return mMaxWidth; }
    public void onActionViewExpanded() { setIconified(false); mQuery.requestFocus(); }
    public void onActionViewCollapsed() { setQuery("", false); clearFocus(); setIconified(true); }
    @Override protected void onMeasure(int ws, int hs) { if (mMaxWidth > 0 && MeasureSpec.getSize(ws) > mMaxWidth) ws = MeasureSpec.makeMeasureSpec(mMaxWidth, MeasureSpec.getMode(ws)); super.onMeasure(ws, hs); }
    @Override protected void onDraw(android.graphics.Canvas c) {
        super.onDraw(c);
        if (mIconified) { float d = getResources().getDisplayMetrics().density; android.graphics.Paint p = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG); p.setStyle(android.graphics.Paint.Style.STROKE); p.setStrokeWidth(2 * d); p.setColor(0x8A000000); float cx = getWidth() / 2f - 2 * d, cy = getHeight() / 2f - 2 * d; c.drawCircle(cx, cy, 6 * d, p); c.drawLine(cx + 4.5f * d, cy + 4.5f * d, cx + 10 * d, cy + 10 * d, p); }
    }
    private static final class CloseDrawable extends android.graphics.drawable.Drawable {
        private final android.graphics.Paint p = new android.graphics.Paint(android.graphics.Paint.ANTI_ALIAS_FLAG); private final float d;
        CloseDrawable(float d) { this.d = d; p.setStrokeWidth(2 * d); p.setColor(0x8A000000); }
        public void draw(android.graphics.Canvas c) { android.graphics.Rect b = getBounds(); float cx = b.exactCenterX(), cy = b.exactCenterY(), r = 6 * d; c.drawLine(cx - r, cy - r, cx + r, cy + r, p); c.drawLine(cx - r, cy + r, cx + r, cy - r, p); }
        public int getIntrinsicWidth() { return (int) (24 * d); } public int getIntrinsicHeight() { return (int) (24 * d); }
        public void setAlpha(int a) {} public void setColorFilter(android.graphics.ColorFilter f) {} public int getOpacity() { return android.graphics.PixelFormat.TRANSLUCENT; }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void setAppSearchData(android.os.Bundle p0) {}
    // ---- end of generated members
}
