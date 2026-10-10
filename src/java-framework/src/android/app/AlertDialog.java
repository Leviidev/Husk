package android.app;

import android.content.Context;
import android.content.DialogInterface;
import android.database.Cursor;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.*;
import android.widget.*;

/** A Material alert: title, message or list (plain, single- or multi-choice) or the app's view, and up to three text buttons. */
public class AlertDialog extends Dialog implements DialogInterface {
    public static final int THEME_TRADITIONAL = 1, THEME_HOLO_DARK = 2, THEME_HOLO_LIGHT = 3, THEME_DEVICE_DEFAULT_DARK = 4, THEME_DEVICE_DEFAULT_LIGHT = 5;
    private CharSequence mTitle, mMessage;
    private Drawable mIcon;
    private View mView, mCustomTitle;
    private int mViewLayout;
    private final CharSequence[] mButtonText = new CharSequence[3];
    private final DialogInterface.OnClickListener[] mButtonListener = new DialogInterface.OnClickListener[3];
    private final Button[] mButtons = new Button[3];
    private ListAdapter mAdapter;
    private ListView mListView;
    private int mCheckedItem = -1, mChoiceMode;
    private boolean[] mCheckedItems;
    private DialogInterface.OnClickListener mItemListener;
    private DialogInterface.OnMultiChoiceClickListener mMultiListener;
    private TextView mMessageView, mTitleView;
    private boolean mBuilt;
    protected AlertDialog(Context c) { this(c, 0); }
    protected AlertDialog(Context c, boolean cancelable, OnCancelListener l) { this(c, 0); setCancelable(cancelable); setOnCancelListener(l); }
    protected AlertDialog(Context c, int themeResId) { super(c, resolveTheme(c, themeResId)); getWindow().requestFeature(Window.FEATURE_NO_TITLE); }
    static int resolveTheme(Context c, int themeResId) {
        if (themeResId >= 0x01000000) return themeResId;
        TypedValue v = new TypedValue();
        c.getTheme().resolveAttribute(android.R.attr.alertDialogTheme, v, true);
        return v.resourceId;
    }
    private float dp(float v) { return v * getContext().getResources().getDisplayMetrics().density; }
    private int themeColor(int attr, int def) {
        TypedValue v = new TypedValue();
        if (!getContext().getTheme().resolveAttribute(attr, v, true)) return def;
        if (v.type >= TypedValue.TYPE_FIRST_COLOR_INT && v.type <= TypedValue.TYPE_LAST_COLOR_INT) return v.data;
        if (v.resourceId != 0) { try { android.content.res.ColorStateList l = getContext().getColorStateList(v.resourceId); return l.getDefaultColor(); } catch (Exception e) {} }
        return def;
    }
    @Override protected void onCreate(Bundle s) { super.onCreate(s); build(); }
    private void build() {
        if (mBuilt) return;
        mBuilt = true;
        Context c = getContext();
        int textPrimary = themeColor(android.R.attr.textColorPrimary, 0xDE000000), textSecondary = themeColor(android.R.attr.textColorSecondary, 0x8A000000), accent = themeColor(android.R.attr.colorAccent, 0xFF6750A4);
        LinearLayout root = new LinearLayout(c);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(0, (int) dp(24), 0, (int) dp(8));
        // title
        if (mCustomTitle != null) root.addView(mCustomTitle);
        else if (mTitle != null && mTitle.length() > 0) {
            LinearLayout tl = new LinearLayout(c); tl.setOrientation(LinearLayout.HORIZONTAL); tl.setGravity(Gravity.CENTER_VERTICAL);
            tl.setPadding((int) dp(24), 0, (int) dp(24), (int) dp(16));
            if (mIcon != null) { ImageView iv = new ImageView(c); iv.setImageDrawable(mIcon); iv.setScaleType(ImageView.ScaleType.FIT_CENTER); LinearLayout.LayoutParams ip = new LinearLayout.LayoutParams((int) dp(32), (int) dp(32)); ip.rightMargin = (int) dp(8); tl.addView(iv, ip); }
            mTitleView = new TextView(c);
            mTitleView.setText(mTitle); mTitleView.setTextColor(textPrimary); mTitleView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20);
            mTitleView.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
            tl.addView(mTitleView, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
            root.addView(tl);
        }
        // body: message, list or the app's view
        if (mMessage != null) {
            ScrollView sv = new ScrollView(c);
            mMessageView = new TextView(c);
            mMessageView.setText(mMessage); mMessageView.setTextColor(textSecondary); mMessageView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            mMessageView.setLineSpacing(0, 1.15f);
            mMessageView.setPadding((int) dp(24), 0, (int) dp(24), (int) dp(16));
            sv.addView(mMessageView);
            root.addView(sv, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));
            ((LinearLayout.LayoutParams) sv.getLayoutParams()).height = ViewGroup.LayoutParams.WRAP_CONTENT;
            ((LinearLayout.LayoutParams) sv.getLayoutParams()).weight = 0;
        }
        if (mAdapter != null) {
            mListView = new ListView(c);
            mListView.setDivider(null);
            mListView.setAdapter(mAdapter);
            if (mChoiceMode != AbsListView.CHOICE_MODE_NONE) mListView.setChoiceMode(mChoiceMode);
            if (mChoiceMode == AbsListView.CHOICE_MODE_SINGLE && mCheckedItem >= 0) { mListView.setItemChecked(mCheckedItem, true); mListView.setSelection(mCheckedItem); }
            if (mChoiceMode == AbsListView.CHOICE_MODE_MULTIPLE && mCheckedItems != null) for (int i = 0; i < mCheckedItems.length; i++) mListView.setItemChecked(i, mCheckedItems[i]);
            mListView.setOnItemClickListener((p, v, pos, id) -> {
                if (mChoiceMode == AbsListView.CHOICE_MODE_MULTIPLE) { boolean on = mListView.isItemChecked(pos); if (mCheckedItems != null && pos < mCheckedItems.length) mCheckedItems[pos] = on; if (mMultiListener != null) mMultiListener.onClick(this, pos, on); return; }
                if (mItemListener != null) mItemListener.onClick(this, pos);
                if (mChoiceMode != AbsListView.CHOICE_MODE_SINGLE) dismiss();
            });
            root.addView(mListView, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, 0f));
        }
        if (mView == null && mViewLayout != 0) mView = LayoutInflater.from(c).inflate(mViewLayout, root, false);
        if (mView != null) {
            FrameLayout custom = new FrameLayout(c);
            custom.setId(android.R.id.custom);
            if (mView.getParent() instanceof ViewGroup) ((ViewGroup) mView.getParent()).removeView(mView);
            custom.addView(mView, new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            root.addView(custom, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        }
        // buttons, end-aligned: neutral at the start, then negative, positive
        boolean any = false;
        for (CharSequence t : mButtonText) if (t != null) any = true;
        if (any) {
            LinearLayout bar = new LinearLayout(c);
            bar.setOrientation(LinearLayout.HORIZONTAL);
            bar.setGravity(Gravity.END | Gravity.CENTER_VERTICAL);
            bar.setPadding((int) dp(16), (int) dp(8), (int) dp(16), 0);
            int[] order = { 2, -1, 1, 0 };
            for (int k : order) {
                if (k == -1) { bar.addView(new Space(c), new LinearLayout.LayoutParams(0, 1, 1f)); continue; }
                if (mButtonText[k] == null) continue;
                Button b = new Button(c, null, 0);
                b.setText(mButtonText[k]);
                b.setAllCaps(false);
                b.setTextColor(accent);
                b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
                b.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
                b.setBackground(null);
                b.setMinHeight((int) dp(40)); b.setMinimumHeight((int) dp(40));
                b.setPadding((int) dp(12), 0, (int) dp(12), 0);
                b.setGravity(Gravity.CENTER);
                TypedValue tv = new TypedValue();
                try { if (c.getTheme().resolveAttribute(android.R.attr.selectableItemBackground, tv, true) && tv.resourceId != 0) b.setBackground(c.getDrawable(tv.resourceId)); } catch (RuntimeException e) {}
                final int which = k == 0 ? BUTTON_POSITIVE : k == 1 ? BUTTON_NEGATIVE : BUTTON_NEUTRAL;
                final int idx = k;
                b.setOnClickListener(v -> { if (mButtonListener[idx] != null) mButtonListener[idx].onClick(this, which); dismiss(); });
                b.setId(k == 0 ? android.R.id.button1 : k == 1 ? android.R.id.button2 : android.R.id.button3);
                mButtons[k] = b;
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                lp.leftMargin = (int) dp(8);
                bar.addView(b, lp);
            }
            root.addView(bar, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        } else root.setPadding(0, (int) dp(24), 0, (int) dp(mAdapter != null ? 8 : 24));
        setContentView(root, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        int sw = husk.Native.screenWidth();
        getWindow().setLayout(Math.min((int) (sw * 0.9f), (int) dp(560)), ViewGroup.LayoutParams.WRAP_CONTENT);
    }
    public Button getButton(int which) { build(); int k = which == BUTTON_POSITIVE ? 0 : which == BUTTON_NEGATIVE ? 1 : which == BUTTON_NEUTRAL ? 2 : -1; return k < 0 ? null : mButtons[k]; }
    public ListView getListView() { build(); return mListView; }
    @Override public void setTitle(CharSequence t) { mTitle = t; if (mTitleView != null) mTitleView.setText(t); }
    public void setCustomTitle(View v) { mCustomTitle = v; }
    public void setMessage(CharSequence m) { mMessage = m; if (mMessageView != null) mMessageView.setText(m); }
    public void setView(View v) { mView = v; }
    public void setView(View v, int l, int t, int r, int b) { mView = v; if (v != null) v.setPadding(l, t, r, b); }
    public void setButton(int which, CharSequence text, android.os.Message msg) { setButton(which, text, (DialogInterface.OnClickListener) null); }
    public void setButton(int which, CharSequence text, DialogInterface.OnClickListener l) { int k = which == BUTTON_POSITIVE ? 0 : which == BUTTON_NEGATIVE ? 1 : 2; mButtonText[k] = text; mButtonListener[k] = l; }
    @Deprecated public void setButton(CharSequence t, DialogInterface.OnClickListener l) { setButton(BUTTON_POSITIVE, t, l); }
    @Deprecated public void setButton2(CharSequence t, DialogInterface.OnClickListener l) { setButton(BUTTON_NEGATIVE, t, l); }
    @Deprecated public void setButton3(CharSequence t, DialogInterface.OnClickListener l) { setButton(BUTTON_NEUTRAL, t, l); }
    public void setIcon(int r) { mIcon = r == 0 ? null : getContext().getDrawable(r); }
    public void setIcon(Drawable d) { mIcon = d; }
    public void setIconAttribute(int a) { TypedValue v = new TypedValue(); getContext().getTheme().resolveAttribute(a, v, true); setIcon(v.resourceId); }
    public void setInverseBackgroundForced(boolean f) {}
    @Override public boolean onKeyDown(int k, KeyEvent e) { return super.onKeyDown(k, e); }

    public static class Builder {
        private final Context mContext;
        private final int mTheme;
        CharSequence title, message; View customTitle, view; int viewLayout; Drawable icon;
        final CharSequence[] buttonText = new CharSequence[3];
        final DialogInterface.OnClickListener[] buttonListener = new DialogInterface.OnClickListener[3];
        boolean cancelable = true;
        OnCancelListener onCancel; OnDismissListener onDismiss; OnKeyListener onKey;
        CharSequence[] items; ListAdapter adapter; DialogInterface.OnClickListener itemListener; DialogInterface.OnMultiChoiceClickListener multiListener;
        boolean[] checkedItems; int checkedItem = -1, choiceMode;
        AdapterView.OnItemSelectedListener selectedListener;
        public Builder(Context c) { this(c, resolveTheme(c, 0)); }
        public Builder(Context c, int themeResId) { mTheme = resolveTheme(c, themeResId); mContext = new ContextThemeWrapper(c, mTheme); }
        public Context getContext() { return mContext; }
        public Builder setTitle(int r) { title = mContext.getText(r); return this; }
        public Builder setTitle(CharSequence t) { title = t; return this; }
        public Builder setCustomTitle(View v) { customTitle = v; return this; }
        public Builder setMessage(int r) { message = mContext.getText(r); return this; }
        public Builder setMessage(CharSequence m) { message = m; return this; }
        public Builder setIcon(int r) { icon = r == 0 ? null : mContext.getDrawable(r); return this; }
        public Builder setIcon(Drawable d) { icon = d; return this; }
        public Builder setIconAttribute(int a) { TypedValue v = new TypedValue(); mContext.getTheme().resolveAttribute(a, v, true); return setIcon(v.resourceId); }
        public Builder setPositiveButton(int r, DialogInterface.OnClickListener l) { return setPositiveButton(mContext.getText(r), l); }
        public Builder setPositiveButton(CharSequence t, DialogInterface.OnClickListener l) { buttonText[0] = t; buttonListener[0] = l; return this; }
        public Builder setPositiveButtonIcon(Drawable d) { return this; }
        public Builder setNegativeButton(int r, DialogInterface.OnClickListener l) { return setNegativeButton(mContext.getText(r), l); }
        public Builder setNegativeButton(CharSequence t, DialogInterface.OnClickListener l) { buttonText[1] = t; buttonListener[1] = l; return this; }
        public Builder setNegativeButtonIcon(Drawable d) { return this; }
        public Builder setNeutralButton(int r, DialogInterface.OnClickListener l) { return setNeutralButton(mContext.getText(r), l); }
        public Builder setNeutralButton(CharSequence t, DialogInterface.OnClickListener l) { buttonText[2] = t; buttonListener[2] = l; return this; }
        public Builder setNeutralButtonIcon(Drawable d) { return this; }
        public Builder setCancelable(boolean c) { cancelable = c; return this; }
        public Builder setOnCancelListener(OnCancelListener l) { onCancel = l; return this; }
        public Builder setOnDismissListener(OnDismissListener l) { onDismiss = l; return this; }
        public Builder setOnKeyListener(OnKeyListener l) { onKey = l; return this; }
        public Builder setItems(int r, DialogInterface.OnClickListener l) { return setItems(mContext.getResources().getTextArray(r), l); }
        public Builder setItems(CharSequence[] i, DialogInterface.OnClickListener l) { items = i; itemListener = l; choiceMode = 0; return this; }
        public Builder setAdapter(ListAdapter a, DialogInterface.OnClickListener l) { adapter = a; itemListener = l; return this; }
        public Builder setCursor(Cursor c, DialogInterface.OnClickListener l, String labelColumn) { return this; }
        public Builder setMultiChoiceItems(int r, boolean[] checked, DialogInterface.OnMultiChoiceClickListener l) { return setMultiChoiceItems(mContext.getResources().getTextArray(r), checked, l); }
        public Builder setMultiChoiceItems(CharSequence[] i, boolean[] checked, DialogInterface.OnMultiChoiceClickListener l) { items = i; checkedItems = checked; multiListener = l; choiceMode = AbsListView.CHOICE_MODE_MULTIPLE; return this; }
        public Builder setMultiChoiceItems(Cursor c, String isChecked, String label, DialogInterface.OnMultiChoiceClickListener l) { return this; }
        public Builder setSingleChoiceItems(int r, int checked, DialogInterface.OnClickListener l) { return setSingleChoiceItems(mContext.getResources().getTextArray(r), checked, l); }
        public Builder setSingleChoiceItems(Cursor c, int checked, String label, DialogInterface.OnClickListener l) { return this; }
        public Builder setSingleChoiceItems(CharSequence[] i, int checked, DialogInterface.OnClickListener l) { items = i; checkedItem = checked; itemListener = l; choiceMode = AbsListView.CHOICE_MODE_SINGLE; return this; }
        public Builder setSingleChoiceItems(ListAdapter a, int checked, DialogInterface.OnClickListener l) { adapter = a; checkedItem = checked; itemListener = l; choiceMode = AbsListView.CHOICE_MODE_SINGLE; return this; }
        public Builder setOnItemSelectedListener(AdapterView.OnItemSelectedListener l) { selectedListener = l; return this; }
        public Builder setView(int layoutResId) { viewLayout = layoutResId; view = null; return this; }
        public Builder setView(View v) { view = v; viewLayout = 0; return this; }
        @Deprecated public Builder setView(View v, int l, int t, int r, int b) { if (v != null) v.setPadding(l, t, r, b); return setView(v); }
        @Deprecated public Builder setInverseBackgroundForced(boolean f) { return this; }
        public AlertDialog create() {
            final AlertDialog d = new AlertDialog(mContext, mTheme);
            d.mTitle = title; d.mCustomTitle = customTitle; d.mMessage = message; d.mIcon = icon; d.mView = view; d.mViewLayout = viewLayout;
            for (int i = 0; i < 3; i++) { d.mButtonText[i] = buttonText[i]; d.mButtonListener[i] = buttonListener[i]; }
            if (items != null || adapter != null) {
                d.mChoiceMode = choiceMode; d.mCheckedItem = checkedItem; d.mCheckedItems = checkedItems; d.mItemListener = itemListener; d.mMultiListener = multiListener;
                d.mAdapter = adapter != null ? adapter : new ItemsAdapter(mContext, items, choiceMode);
            }
            d.setCancelable(cancelable);
            if (cancelable) d.setCanceledOnTouchOutside(true);
            d.setOnCancelListener(onCancel);
            d.setOnDismissListener(onDismiss);
            if (onKey != null) d.setOnKeyListener(onKey);
            return d;
        }
        public AlertDialog show() { AlertDialog d = create(); d.show(); return d; }
        // ---- generated by tools/compat/fillmembers.py (Builder): the platform's members this class does not write (signatures only)
        public android.app.AlertDialog.Builder setRecycleOnMeasureEnabled(boolean p0) { return this; }
        // ---- end of generated members (Builder)
    }
    /** Rows for items: plain text, or a check box / radio button at the end. */
    private static final class ItemsAdapter extends BaseAdapter {
        private final Context c; private final CharSequence[] items; private final int mode;
        ItemsAdapter(Context c, CharSequence[] items, int mode) { this.c = c; this.items = items; this.mode = mode; }
        public int getCount() { return items.length; }
        public Object getItem(int i) { return items[i]; }
        public long getItemId(int i) { return i; }
        public View getView(int i, View cv, ViewGroup parent) {
            float d = c.getResources().getDisplayMetrics().density;
            if (mode == 0) {
                TextView t = cv instanceof TextView ? (TextView) cv : new TextView(c);
                t.setText(items[i]);
                t.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
                t.setMinHeight((int) (48 * d));
                t.setGravity(Gravity.CENTER_VERTICAL);
                t.setPadding((int) (24 * d), 0, (int) (24 * d), 0);
                TypedValue v = new TypedValue();
                if (c.getTheme().resolveAttribute(android.R.attr.textColorPrimary, v, true) && v.resourceId != 0) { try { t.setTextColor(c.getColorStateList(v.resourceId)); } catch (Exception e) {} } else if (v.type >= TypedValue.TYPE_FIRST_COLOR_INT && v.type <= TypedValue.TYPE_LAST_COLOR_INT) t.setTextColor(v.data);
                return t;
            }
            CheckedRow row = cv instanceof CheckedRow ? (CheckedRow) cv : new CheckedRow(c, mode);
            row.text.setText(items[i]);
            return row;
        }
    }
    private static final class CheckedRow extends LinearLayout implements Checkable {
        final TextView text; final CompoundButton box;
        CheckedRow(Context c, int mode) {
            super(c);
            float d = c.getResources().getDisplayMetrics().density;
            setOrientation(HORIZONTAL); setGravity(Gravity.CENTER_VERTICAL);
            setMinimumHeight((int) (48 * d));
            setPadding((int) (16 * d), 0, (int) (24 * d), 0);
            box = mode == AbsListView.CHOICE_MODE_MULTIPLE ? new CheckBox(c) : new RadioButton(c);
            box.setClickable(false); box.setFocusable(false);
            addView(box);
            text = new TextView(c);
            text.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            text.setPadding((int) (16 * d), 0, 0, 0);
            addView(text, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        }
        public void setChecked(boolean c) { box.setChecked(c); }
        public boolean isChecked() { return box.isChecked(); }
        public void toggle() { box.toggle(); }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final int LAYOUT_HINT_NONE = 0;
    public static final int LAYOUT_HINT_SIDE = 1;
    public void setButton(java.lang.CharSequence p0, android.os.Message p1) {}
    public void setButton2(java.lang.CharSequence p0, android.os.Message p1) {}
    public void setButton3(java.lang.CharSequence p0, android.os.Message p1) {}
    public void setMessageHyphenationFrequency(int p0) {}
    public void setMessageMovementMethod(android.text.method.MovementMethod p0) {}
    // ---- end of generated members
}
