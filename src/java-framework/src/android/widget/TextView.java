package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.text.*;
import android.text.method.*;
import android.text.style.ClickableSpan;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.*;
import android.view.inputmethod.*;
import java.util.ArrayList;
import java.util.Locale;

/** Text on screen, and when editable, text being typed: laid out with a StaticLayout, drawn with its spans, a cursor when focused. */
public class TextView extends View implements ViewTreeObserver.OnPreDrawListener {
    public enum BufferType { NORMAL, SPANNABLE, EDITABLE }
    public interface OnEditorActionListener { boolean onEditorAction(TextView v, int actionId, KeyEvent event); }
    public static final int AUTO_SIZE_TEXT_TYPE_NONE = 0, AUTO_SIZE_TEXT_TYPE_UNIFORM = 1, FOCUSED_SEARCH_RESULT_INDEX_NONE = -1;
    public static class SavedState extends BaseSavedState {
        CharSequence text; int selStart, selEnd;
        SavedState(android.os.Parcelable superState) { super(superState); }
    }

    private CharSequence mText = "", mTransformed = "", mHint;
    private BufferType mBufferType = BufferType.NORMAL;
    final TextPaint mTextPaint = new TextPaint(Paint.ANTI_ALIAS_FLAG);
    private ColorStateList mTextColor = ColorStateList.valueOf(0xFF000000), mHintTextColor, mLinkTextColor;
    private int mCurTextColor = 0xFF000000, mCurHintTextColor = 0x80000000;
    private int mGravity = Gravity.TOP | Gravity.START;
    private Layout mLayout, mHintLayout;
    private int mMaxLines = Integer.MAX_VALUE, mMinLines = 0, mMaxMode, mMinMode, mMaxWidth = Integer.MAX_VALUE, mMinWidth = 0, mMaxWidthMode = 2, mMinWidthMode = 2, mMaximum = Integer.MAX_VALUE, mMinimum;
    private static final int LINES = 1, EMS = LINES, PIXELS = 2;
    private boolean mSingleLine, mIncludePad = true, mAllCaps, mHorizontallyScrolling, mCursorVisible = true, mSelectAllOnFocus, mTextIsSelectable, mFreezesText, mLinksClickable = true;
    private TextUtils.TruncateAt mEllipsize;
    private float mSpacingMult = 1, mSpacingAdd;
    private int mLineHeight = -1;
    private Drawable[] mDrawables = new Drawable[4];   /* left top right bottom */
    private int mDrawablePadding;
    private ColorStateList mDrawableTint;
    private PorterDuff.Mode mDrawableTintMode;
    private ArrayList<TextWatcher> mListeners;
    private OnEditorActionListener mEditorAction;
    private int mInputType = EditorInfo.TYPE_NULL, mImeOptions = EditorInfo.IME_NULL, mImeActionId;
    private CharSequence mImeActionLabel;
    private String mPrivateImeOptions;
    private android.os.Bundle mInputExtras;
    private KeyListener mKeyListener;
    private MovementMethod mMovement;
    private TransformationMethod mTransformation;
    private InputFilter[] mFilters = new InputFilter[0];
    private CharSequence mError;
    private int mAutoLinkMask;
    private boolean mLayoutDirty = true;
    private long mBlinkStart;
    private final Paint mHighlightPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private int mHighlightColor = 0x6633B5E5;
    private ColorStateList mCursorColor;
    private Drawable mCursorDrawable;
    private int mShadowColor;
    private float mShadowRadius, mShadowDx, mShadowDy;
    private int mBreakStrategy, mHyphenation, mJustification;
    private Locale mLocale;
    private int mAutoSizeType;
    private float mLetterSpacing;
    private ChangeWatcher mChangeWatcher;

    public TextView(Context c) { this(c, null); }
    public TextView(Context c, AttributeSet a) { this(c, a, android.R.attr.textViewStyle); }
    public TextView(Context c, AttributeSet a, int defStyleAttr) { this(c, a, defStyleAttr, 0); }
    public TextView(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(c, attrs, defStyleAttr, defStyleRes);
        mTextPaint.density = getResources().getDisplayMetrics().density;
        mTextPaint.setTextSize(TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 14, getResources().getDisplayMetrics()));
        // text appearance from the theme, then the style's own, then the attributes in the XML
        TypedArray ta = c.obtainStyledAttributes(attrs, husk.S.TextView, defStyleAttr, defStyleRes);
        int appearance = ta.getResourceId(husk.S.TextView_textAppearance, -1);
        if (appearance == -1) { TypedValue v = new TypedValue(); if (c.getTheme().resolveAttribute(android.R.attr.textAppearance, v, true) && v.resourceId != 0) appearance = v.resourceId; }
        if (appearance != -1 && appearance != 0) setTextAppearance(appearance);
        CharSequence text = "", hint = null;
        int inputType = EditorInfo.TYPE_NULL, ellipsize = -1, lines = -1, maxLines = -1, minLines = -1, ems = -1, maxEms = -1, minEms = -1, maxLength = -1;
        boolean singleLine = false, password = false, editable = false, phone = false, hasNumeric = false;
        int numeric = 0;
        String digits = null, family = null;
        int typefaceIndex = -1, styleIndex = -1, weight = -1;
        Drawable dl = null, dt = null, dr = null, db = null, ds = null, de = null;
        for (int i = 0, n = ta.getIndexCount(); i < n; i++) {
            int at = ta.getIndex(i);
            switch (at) {
            case husk.S.TextView_text: text = ta.getText(at); break;
            case husk.S.TextView_hint: hint = ta.getText(at); break;
            case husk.S.TextView_textSize: setRawTextSize(ta.getDimensionPixelSize(at, (int) mTextPaint.getTextSize())); break;
            case husk.S.TextView_textColor: mTextColor = ta.getColorStateList(at); break;
            case husk.S.TextView_textColorHint: mHintTextColor = ta.getColorStateList(at); break;
            case husk.S.TextView_textColorLink: mLinkTextColor = ta.getColorStateList(at); break;
            case husk.S.TextView_textStyle: styleIndex = ta.getInt(at, -1); break;
            case husk.S.TextView_typeface: typefaceIndex = ta.getInt(at, -1); break;
            case husk.S.TextView_fontFamily: { Typeface tf = null; try { tf = ta.getFont(at); } catch (Exception e) {} if (tf != null) mTextPaint.setTypeface(tf); else family = ta.getString(at); break; }
            case husk.S.TextView_textFontWeight: weight = ta.getInt(at, -1); break;
            case husk.S.TextView_gravity: mGravity = ta.getInt(at, mGravity); break;
            case husk.S.TextView_singleLine: singleLine = ta.getBoolean(at, false); break;
            case husk.S.TextView_maxLines: maxLines = ta.getInt(at, -1); break;
            case husk.S.TextView_minLines: minLines = ta.getInt(at, -1); break;
            case husk.S.TextView_lines: lines = ta.getInt(at, -1); break;
            case husk.S.TextView_ellipsize: ellipsize = ta.getInt(at, -1); break;
            case husk.S.TextView_inputType: inputType = ta.getInt(at, EditorInfo.TYPE_NULL); break;
            case husk.S.TextView_imeOptions: mImeOptions = ta.getInt(at, mImeOptions); break;
            case husk.S.TextView_imeActionLabel: mImeActionLabel = ta.getText(at); break;
            case husk.S.TextView_imeActionId: mImeActionId = ta.getInt(at, 0); break;
            case husk.S.TextView_privateImeOptions: mPrivateImeOptions = ta.getString(at); break;
            case husk.S.TextView_drawableLeft: dl = ta.getDrawable(at); break;
            case husk.S.TextView_drawableTop: dt = ta.getDrawable(at); break;
            case husk.S.TextView_drawableRight: dr = ta.getDrawable(at); break;
            case husk.S.TextView_drawableBottom: db = ta.getDrawable(at); break;
            case husk.S.TextView_drawableStart: ds = ta.getDrawable(at); break;
            case husk.S.TextView_drawableEnd: de = ta.getDrawable(at); break;
            case husk.S.TextView_drawablePadding: mDrawablePadding = ta.getDimensionPixelSize(at, 0); break;
            case husk.S.TextView_drawableTint: mDrawableTint = ta.getColorStateList(at); break;
            case husk.S.TextView_lineSpacingExtra: mSpacingAdd = ta.getDimensionPixelSize(at, 0); break;
            case husk.S.TextView_lineSpacingMultiplier: mSpacingMult = ta.getFloat(at, 1); break;
            case husk.S.TextView_lineHeight: mLineHeight = ta.getDimensionPixelSize(at, -1); break;
            case husk.S.TextView_includeFontPadding: mIncludePad = ta.getBoolean(at, true); break;
            case husk.S.TextView_textAllCaps: mAllCaps = ta.getBoolean(at, false); break;
            case husk.S.TextView_letterSpacing: setLetterSpacing(ta.getFloat(at, 0)); break;
            case husk.S.TextView_shadowColor: mShadowColor = ta.getInt(at, 0); break;
            case husk.S.TextView_shadowDx: mShadowDx = ta.getFloat(at, 0); break;
            case husk.S.TextView_shadowDy: mShadowDy = ta.getFloat(at, 0); break;
            case husk.S.TextView_shadowRadius: mShadowRadius = ta.getFloat(at, 0); break;
            case husk.S.TextView_maxWidth: mMaxWidth = ta.getDimensionPixelSize(at, Integer.MAX_VALUE); mMaxWidthMode = PIXELS; break;
            case husk.S.TextView_minWidth: mMinWidth = ta.getDimensionPixelSize(at, 0); mMinWidthMode = PIXELS; break;
            case husk.S.TextView_maxHeight: mMaximum = ta.getDimensionPixelSize(at, Integer.MAX_VALUE); mMaxMode = PIXELS; break;
            case husk.S.TextView_minHeight: mMinimum = ta.getDimensionPixelSize(at, 0); mMinMode = PIXELS; break;
            case husk.S.TextView_ems: ems = ta.getInt(at, -1); break;
            case husk.S.TextView_maxEms: maxEms = ta.getInt(at, -1); break;
            case husk.S.TextView_minEms: minEms = ta.getInt(at, -1); break;
            case husk.S.TextView_editable: editable = ta.getBoolean(at, false); break;
            case husk.S.TextView_password: password = ta.getBoolean(at, false); break;
            case husk.S.TextView_digits: digits = ta.getString(at); break;
            case husk.S.TextView_numeric: numeric = ta.getInt(at, 0); hasNumeric = true; break;
            case husk.S.TextView_phoneNumber: phone = ta.getBoolean(at, false); break;
            case husk.S.TextView_autoLink: mAutoLinkMask = ta.getInt(at, 0); break;
            case husk.S.TextView_linksClickable: mLinksClickable = ta.getBoolean(at, true); break;
            case husk.S.TextView_textIsSelectable: mTextIsSelectable = ta.getBoolean(at, false); break;
            case husk.S.TextView_cursorVisible: mCursorVisible = ta.getBoolean(at, true); break;
            case husk.S.TextView_selectAllOnFocus: mSelectAllOnFocus = ta.getBoolean(at, false); break;
            case husk.S.TextView_scrollHorizontally: mHorizontallyScrolling = ta.getBoolean(at, false); break;
            case husk.S.TextView_maxLength: maxLength = ta.getInt(at, -1); break;
            case husk.S.TextView_breakStrategy: mBreakStrategy = ta.getInt(at, 0); break;
            case husk.S.TextView_autoSizeTextType: mAutoSizeType = ta.getInt(at, 0); break;
            case husk.S.TextView_textScaleX: mTextPaint.setTextScaleX(ta.getFloat(at, 1)); break;
            case husk.S.TextView_freezesText: mFreezesText = ta.getBoolean(at, false); break;
            case husk.S.TextView_textCursorDrawable: try { mCursorDrawable = ta.getDrawable(at); } catch (Exception e) {} break;
            }
        }
        ta.recycle();
        if (family != null || typefaceIndex >= 0 || styleIndex >= 0 || weight >= 0) setTypefaceFromAttrs(family, typefaceIndex, styleIndex >= 0 ? styleIndex : (mTextPaint.getTypeface() != null ? mTextPaint.getTypeface().getStyle() : 0), weight);
        if (ds != null) dl = ds;
        if (de != null) dr = de;
        if (dl != null || dt != null || dr != null || db != null) setCompoundDrawablesWithIntrinsicBounds(dl, dt, dr, db);
        if (password && inputType == EditorInfo.TYPE_NULL) inputType = EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_PASSWORD;
        if (phone && inputType == EditorInfo.TYPE_NULL) inputType = EditorInfo.TYPE_CLASS_PHONE;
        if (hasNumeric && inputType == EditorInfo.TYPE_NULL) inputType = EditorInfo.TYPE_CLASS_NUMBER | ((numeric & 2) != 0 ? EditorInfo.TYPE_NUMBER_FLAG_SIGNED : 0) | ((numeric & 4) != 0 ? EditorInfo.TYPE_NUMBER_FLAG_DECIMAL : 0);
        if (editable && inputType == EditorInfo.TYPE_NULL) inputType = EditorInfo.TYPE_CLASS_TEXT;
        BufferType bt = BufferType.NORMAL;
        if (inputType != EditorInfo.TYPE_NULL || getDefaultEditable()) {
            mInputType = inputType != EditorInfo.TYPE_NULL ? inputType : EditorInfo.TYPE_CLASS_TEXT;
            if (digits != null) mKeyListener = DigitsKeyListener.getInstance(digits);
            else if ((mInputType & EditorInfo.TYPE_MASK_CLASS) == EditorInfo.TYPE_CLASS_NUMBER) mKeyListener = DigitsKeyListener.getInstance((mInputType & EditorInfo.TYPE_NUMBER_FLAG_SIGNED) != 0, (mInputType & EditorInfo.TYPE_NUMBER_FLAG_DECIMAL) != 0);
            else if ((mInputType & EditorInfo.TYPE_MASK_CLASS) == EditorInfo.TYPE_CLASS_PHONE) mKeyListener = DialerKeyListener.getInstance();
            else mKeyListener = TextKeyListener.getInstance();
            bt = BufferType.EDITABLE;
            if (getDefaultMovementMethod() != null) mMovement = getDefaultMovementMethod();
            if ((mInputType & EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE) == 0 && (mInputType & EditorInfo.TYPE_MASK_CLASS) == EditorInfo.TYPE_CLASS_TEXT) singleLine = singleLine || false;
            if (isPasswordInputType(mInputType)) mTransformation = PasswordTransformationMethod.getInstance();
            if ((mInputType & EditorInfo.TYPE_MASK_CLASS) != EditorInfo.TYPE_CLASS_TEXT || (mInputType & EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE) == 0) { if (lines < 0 && maxLines < 0 && (mInputType & EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE) == 0) singleLine = true; }
        } else if (mTextIsSelectable) { bt = BufferType.SPANNABLE; mMovement = ArrowKeyMovementMethod.getInstance(); }
        if (mAutoLinkMask != 0) bt = bt == BufferType.EDITABLE ? bt : BufferType.SPANNABLE;
        if (singleLine) applySingleLine(true, mTransformation == null);
        if (lines >= 0) setLines(lines);
        else { if (maxLines >= 0) setMaxLines(maxLines); if (minLines >= 0) setMinLines(minLines); }
        if (ems >= 0) setEms(ems); else { if (maxEms >= 0) setMaxEms(maxEms); if (minEms >= 0) setMinEms(minEms); }
        if (ellipsize >= 1 && ellipsize <= 4) mEllipsize = new TextUtils.TruncateAt[] { null, TextUtils.TruncateAt.START, TextUtils.TruncateAt.MIDDLE, TextUtils.TruncateAt.END, TextUtils.TruncateAt.MARQUEE }[ellipsize];
        if (maxLength >= 0) mFilters = new InputFilter[] { new InputFilter.LengthFilter(maxLength) };
        if (mShadowRadius > 0) mTextPaint.setShadowLayer(mShadowRadius, mShadowDx, mShadowDy, mShadowColor);
        if (mTextColor == null) mTextColor = ColorStateList.valueOf(0xFF000000);
        setText(text, bt);
        if (hint != null) setHint(hint);
        updateTextColors();
        if (bt == BufferType.EDITABLE || mTextIsSelectable) { setFocusable(true); setFocusableInTouchMode(true); setClickable(true); setLongClickable(true); }
    }
    private void setTypefaceFromAttrs(String family, int typefaceIndex, int style, int weight) {
        Typeface tf = null;
        if (family != null) tf = Typeface.create(family, style);
        else if (typefaceIndex == 1) tf = Typeface.SANS_SERIF; else if (typefaceIndex == 2) tf = Typeface.SERIF; else if (typefaceIndex == 3) tf = Typeface.MONOSPACE;
        if (weight >= 0) tf = Typeface.create(tf != null ? tf : mTextPaint.getTypeface(), weight, (style & Typeface.ITALIC) != 0);
        setTypeface(tf, style);
    }

    // ---- appearance
    public void setTextAppearance(int resId) { setTextAppearance(getContext(), resId); }
    @Deprecated public void setTextAppearance(Context c, int resId) {
        TypedArray a = c.obtainStyledAttributes(resId, husk.S.TextAppearance);
        ColorStateList col = a.getColorStateList(husk.S.TextAppearance_textColor);
        if (col != null) mTextColor = col;
        ColorStateList hint = a.getColorStateList(husk.S.TextAppearance_textColorHint);
        if (hint != null) mHintTextColor = hint;
        ColorStateList link = a.getColorStateList(husk.S.TextAppearance_textColorLink);
        if (link != null) mLinkTextColor = link;
        int size = a.getDimensionPixelSize(husk.S.TextAppearance_textSize, 0);
        if (size != 0) setRawTextSize(size);
        String family = a.getString(husk.S.TextAppearance_fontFamily);
        Typeface font = null;
        try { if (a.peekValue(husk.S.TextAppearance_fontFamily) != null && a.peekValue(husk.S.TextAppearance_fontFamily).resourceId != 0) font = a.getFont(husk.S.TextAppearance_fontFamily); } catch (Exception e) {}
        int tfi = a.getInt(husk.S.TextAppearance_typeface, -1), st = a.getInt(husk.S.TextAppearance_textStyle, -1), w = a.getInt(husk.S.TextAppearance_textFontWeight, -1);
        if (font != null) mTextPaint.setTypeface(font);
        else if (family != null || tfi >= 0 || st >= 0 || w >= 0) setTypefaceFromAttrs(font == null ? family : null, tfi, st >= 0 ? st : 0, w);
        if (a.hasValue(husk.S.TextAppearance_textAllCaps)) setAllCaps(a.getBoolean(husk.S.TextAppearance_textAllCaps, false));
        if (a.hasValue(husk.S.TextAppearance_letterSpacing)) setLetterSpacing(a.getFloat(husk.S.TextAppearance_letterSpacing, 0));
        if (a.hasValue(husk.S.TextAppearance_shadowColor)) { mShadowColor = a.getInt(husk.S.TextAppearance_shadowColor, 0); mShadowRadius = a.getFloat(husk.S.TextAppearance_shadowRadius, 0); mShadowDx = a.getFloat(husk.S.TextAppearance_shadowDx, 0); mShadowDy = a.getFloat(husk.S.TextAppearance_shadowDy, 0); if (mShadowRadius > 0) mTextPaint.setShadowLayer(mShadowRadius, mShadowDx, mShadowDy, mShadowColor); }
        int hl = a.getColor(husk.S.TextAppearance_textColorHighlight, 0);
        if (hl != 0) mHighlightColor = hl;
        a.recycle();
        updateTextColors();
        relayout();
    }
    public void setTextSize(float size) { setTextSize(TypedValue.COMPLEX_UNIT_SP, size); }
    public void setTextSize(int unit, float size) { setRawTextSize(TypedValue.applyDimension(unit, size, getResources().getDisplayMetrics())); }
    private void setRawTextSize(float size) { if (size != mTextPaint.getTextSize()) { mTextPaint.setTextSize(size); relayout(); } }
    public float getTextSize() { return mTextPaint.getTextSize(); }
    public int getTextSizeUnit() { return TypedValue.COMPLEX_UNIT_PX; }
    public float getTextScaleX() { return mTextPaint.getTextScaleX(); }
    public void setTextScaleX(float s) { mTextPaint.setTextScaleX(s); relayout(); }
    public void setTypeface(Typeface tf) { if (mTextPaint.getTypeface() != tf) { mTextPaint.setTypeface(tf); relayout(); } }
    public void setTypeface(Typeface tf, int style) {
        if (style > 0) {
            tf = tf == null ? Typeface.defaultFromStyle(style) : Typeface.create(tf, style);
            setTypeface(tf);
            int need = style & ~(tf != null ? tf.getStyle() : 0);
            mTextPaint.setFakeBoldText((need & Typeface.BOLD) != 0);
            mTextPaint.setTextSkewX((need & Typeface.ITALIC) != 0 ? -0.25f : 0);
        } else { mTextPaint.setFakeBoldText(false); mTextPaint.setTextSkewX(0); setTypeface(tf); }
    }
    public Typeface getTypeface() { return mTextPaint.getTypeface(); }
    public void setLetterSpacing(float s) { mLetterSpacing = s; mTextPaint.setLetterSpacing(s); relayout(); }
    public float getLetterSpacing() { return mLetterSpacing; }
    public void setFontFeatureSettings(String s) { mTextPaint.setFontFeatureSettings(s); }
    public String getFontFeatureSettings() { return mTextPaint.getFontFeatureSettings(); }
    public boolean setFontVariationSettings(String s) { return mTextPaint.setFontVariationSettings(s); }
    public void setElegantTextHeight(boolean b) {}
    public boolean isElegantTextHeight() { return false; }
    public void setFallbackLineSpacing(boolean b) {}
    public boolean isFallbackLineSpacing() { return false; }
    public void setTextColor(int c) { mTextColor = ColorStateList.valueOf(c); updateTextColors(); }
    public void setTextColor(ColorStateList c) { if (c == null) throw new NullPointerException(); mTextColor = c; updateTextColors(); }
    public final ColorStateList getTextColors() { return mTextColor; }
    public final int getCurrentTextColor() { return mCurTextColor; }
    public final void setHintTextColor(int c) { mHintTextColor = ColorStateList.valueOf(c); updateTextColors(); }
    public final void setHintTextColor(ColorStateList c) { mHintTextColor = c; updateTextColors(); }
    public final ColorStateList getHintTextColors() { return mHintTextColor; }
    public final int getCurrentHintTextColor() { return mCurHintTextColor; }
    public final void setLinkTextColor(int c) { mLinkTextColor = ColorStateList.valueOf(c); updateTextColors(); }
    public final void setLinkTextColor(ColorStateList c) { mLinkTextColor = c; updateTextColors(); }
    public final ColorStateList getLinkTextColors() { return mLinkTextColor; }
    public void setHighlightColor(int c) { mHighlightColor = c; invalidate(); }
    public int getHighlightColor() { return mHighlightColor; }
    public void setTextCursorDrawable(Drawable d) { mCursorDrawable = d; invalidate(); }
    public void setTextCursorDrawable(int r) { setTextCursorDrawable(r != 0 ? getContext().getDrawable(r) : null); }
    public Drawable getTextCursorDrawable() { return mCursorDrawable; }
    public void setTextSelectHandle(Drawable d) {} public void setTextSelectHandle(int r) {} public void setTextSelectHandleLeft(Drawable d) {} public void setTextSelectHandleRight(Drawable d) {}
    public void setShadowLayer(float radius, float dx, float dy, int color) { mShadowRadius = radius; mShadowDx = dx; mShadowDy = dy; mShadowColor = color; mTextPaint.setShadowLayer(radius, dx, dy, color); invalidate(); }
    public float getShadowRadius() { return mShadowRadius; } public float getShadowDx() { return mShadowDx; } public float getShadowDy() { return mShadowDy; } public int getShadowColor() { return mShadowColor; }
    public TextPaint getPaint() { return mTextPaint; }
    public void setPaintFlags(int f) { if (mTextPaint.getFlags() != f) { mTextPaint.setFlags(f); relayout(); } }
    public int getPaintFlags() { return mTextPaint.getFlags(); }
    public void setAllCaps(boolean caps) { mAllCaps = caps; if (caps && mTransformation == null) { mTransformation = new AllCapsTransformation(); } else if (!caps && mTransformation instanceof AllCapsTransformation) mTransformation = null; setText(mText, mBufferType); }
    public boolean isAllCaps() { return mAllCaps; }
    private static final class AllCapsTransformation implements TransformationMethod {
        public CharSequence getTransformation(CharSequence s, View v) { if (s == null) return null; String up = s.toString().toUpperCase(Locale.getDefault()); if (s instanceof Spanned && up.length() == s.length()) { SpannableString ss = new SpannableString(up); TextUtils.copySpansFrom((Spanned) s, 0, s.length(), null, ss, 0); return ss; } return up; }
        public void onFocusChanged(View v, CharSequence s, boolean f, int d, Rect p) {}
    }
    @Override protected void drawableStateChanged() { super.drawableStateChanged(); updateTextColors(); for (Drawable d : mDrawables) if (d != null && d.isStateful()) d.setState(getDrawableState()); }
    private void updateTextColors() {
        boolean inval = false;
        int[] st = getDrawableState();
        int c = mTextColor.getColorForState(st, mTextColor.getDefaultColor());
        if (c != mCurTextColor) { mCurTextColor = c; inval = true; }
        if (mHintTextColor != null) { int h = mHintTextColor.getColorForState(st, mHintTextColor.getDefaultColor()); if (h != mCurHintTextColor) { mCurHintTextColor = h; inval = true; } }
        else { int h = (mCurTextColor & 0xFFFFFF) | (((mCurTextColor >>> 24) * 0x61 / 0xFF) << 24); if (h != mCurHintTextColor) { mCurHintTextColor = h; inval = true; } }
        if (mLinkTextColor != null) { int l = mLinkTextColor.getColorForState(st, 0); if (l != mTextPaint.linkColor) { mTextPaint.linkColor = l; inval = true; } }
        else if (mTextPaint.linkColor == 0) mTextPaint.linkColor = 0xFF1A73E8;
        if (inval) invalidate();
    }

    // ---- text
    public CharSequence getText() { return mText; }
    public int length() { return mText.length(); }
    public Editable getEditableText() { return mText instanceof Editable ? (Editable) mText : null; }
    public CharSequence getTransformed() { return mTransformed; }
    protected boolean getDefaultEditable() { return false; }
    protected MovementMethod getDefaultMovementMethod() { return null; }
    public final void setText(CharSequence text) { setText(text, mBufferType); }
    public final void setText(int resid) { setText(getContext().getResources().getText(resid)); }
    public final void setText(int resid, BufferType type) { setText(getContext().getResources().getText(resid), type); }
    public final void setText(char[] text, int start, int len) { setText(new String(text, start, len)); }
    public final void setTextKeepState(CharSequence text) { setTextKeepState(text, mBufferType); }
    public final void setTextKeepState(CharSequence text, BufferType type) {
        int s = getSelectionStart(), e = getSelectionEnd(), len = text == null ? 0 : text.length();
        setText(text, type);
        if ((s >= 0 || e >= 0) && mText instanceof Spannable) Selection.setSelection((Spannable) mText, Math.max(0, Math.min(s, len)), Math.max(0, Math.min(e, len)));
    }
    public void setText(CharSequence text, BufferType type) {
        if (text == null) text = "";
        if (!(text instanceof Spanned) && mAutoLinkMask != 0 && type == BufferType.NORMAL) type = BufferType.SPANNABLE;
        int oldLen = mText.length();
        sendBeforeTextChanged(mText, 0, oldLen, text.length());
        if (type == BufferType.EDITABLE || mKeyListener != null) {
            Editable t = Editable.Factory.getInstance().newEditable(text);
            if (mFilters.length > 0) { InputFilter[] f = mFilters; t.setFilters(new InputFilter[0]); CharSequence filtered = text; for (InputFilter fi : f) { CharSequence r = fi.filter(filtered, 0, filtered.length(), new SpannedString(""), 0, 0); if (r != null) filtered = r; } if (filtered != text) t = Editable.Factory.getInstance().newEditable(filtered); t.setFilters(f); }
            text = t;
            type = BufferType.EDITABLE;
        } else if (type == BufferType.SPANNABLE || mMovement != null) {
            text = Spannable.Factory.getInstance().newSpannable(text);
            type = BufferType.SPANNABLE;
        } else if (!(text instanceof Spanned)) {
            text = text.toString();
        }
        if (mAutoLinkMask != 0 && text instanceof Spannable && android.text.util.Linkify.addLinks((Spannable) text, mAutoLinkMask)) { if (mLinksClickable && mMovement == null) mMovement = LinkMovementMethod.getInstance(); }
        mBufferType = type;
        mText = text;
        if (text instanceof Spannable) {
            Spannable sp = (Spannable) text;
            if (mChangeWatcher == null) mChangeWatcher = new ChangeWatcher();
            sp.setSpan(mChangeWatcher, 0, text.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE | (100 << Spanned.SPAN_PRIORITY_SHIFT));
            if (mListeners != null) for (TextWatcher w : mListeners) sp.setSpan(w, 0, text.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            if (mMovement != null) { mMovement.initialize(this, sp); if (mText instanceof Editable && Selection.getSelectionStart(sp) < 0) Selection.setSelection(sp, text.length()); }
        }
        mTransformed = mTransformation == null ? text : mTransformation.getTransformation(text, this);
        if (mTransformed == null) mTransformed = text;
        relayout();
        sendOnTextChanged(text, 0, oldLen, text.length());
        onTextChanged(text, 0, oldLen, text.length());
        sendAfterTextChanged(text instanceof Editable ? (Editable) text : null);
        if (husk.InputMethods.focused() == this) husk.InputMethods.restart(this);
    }
    /** Edits to an Editable buffer, as they happen: relayout and the watchers' callbacks. */
    private final class ChangeWatcher implements TextWatcher, SpanWatcher {
        public void beforeTextChanged(CharSequence s, int st, int c, int a) {}
        public void onTextChanged(CharSequence s, int st, int b, int c) {
            if (s != mText) return;
            mTransformed = mTransformation == null ? mText : mTransformation.getTransformation(mText, TextView.this);
            relayout();
            TextView.this.onTextChanged(s, st, b, c);
            mBlinkStart = android.os.SystemClock.uptimeMillis();
        }
        public void afterTextChanged(Editable s) {}
        public void onSpanAdded(Spannable t, Object w, int s, int e) { if (w == Selection.SELECTION_END || w == Selection.SELECTION_START) { mBlinkStart = android.os.SystemClock.uptimeMillis(); invalidate(); } else if (w instanceof android.text.style.UpdateAppearance) relayout(); }
        public void onSpanRemoved(Spannable t, Object w, int s, int e) { if (w instanceof android.text.style.UpdateAppearance) relayout(); }
        public void onSpanChanged(Spannable t, Object w, int os, int oe, int ns, int ne) {
            if (w == Selection.SELECTION_END || w == Selection.SELECTION_START) { mBlinkStart = android.os.SystemClock.uptimeMillis(); invalidate(); onSelectionChanged(getSelectionStart(), getSelectionEnd()); }
            else if (w instanceof android.text.style.UpdateAppearance) relayout();
        }
    }
    protected void onTextChanged(CharSequence text, int start, int before, int after) {}
    protected void onSelectionChanged(int start, int end) {}
    public void addTextChangedListener(TextWatcher w) {
        if (mListeners == null) mListeners = new ArrayList<>();
        mListeners.add(w);
        if (mText instanceof Spannable) ((Spannable) mText).setSpan(w, 0, mText.length(), Spanned.SPAN_INCLUSIVE_INCLUSIVE);
    }
    public void removeTextChangedListener(TextWatcher w) { if (mListeners != null) mListeners.remove(w); if (mText instanceof Spannable) ((Spannable) mText).removeSpan(w); }
    private void sendBeforeTextChanged(CharSequence t, int s, int b, int a) { if (mListeners != null && !(mText instanceof Spannable)) for (TextWatcher w : new ArrayList<>(mListeners)) w.beforeTextChanged(t, s, b, a); else if (mListeners != null) for (TextWatcher w : new ArrayList<>(mListeners)) w.beforeTextChanged(t, s, b, a); }
    private void sendOnTextChanged(CharSequence t, int s, int b, int a) { if (mListeners != null) for (TextWatcher w : new ArrayList<>(mListeners)) w.onTextChanged(t, s, b, a); }
    private void sendAfterTextChanged(Editable e) { if (mListeners != null) for (TextWatcher w : new ArrayList<>(mListeners)) w.afterTextChanged(e != null ? e : new SpannableStringBuilder(mText)); }
    public void append(CharSequence text) { append(text, 0, text.length()); }
    public void append(CharSequence text, int start, int end) {
        if (!(mText instanceof Editable)) setText(mText, BufferType.EDITABLE);
        ((Editable) mText).append(text, start, end);
    }
    public CharSequence getHint() { return mHint; }
    public final void setHint(CharSequence hint) { mHint = hint; mHintLayout = null; if (mText.length() == 0) relayout(); invalidate(); }
    public final void setHint(int resid) { setHint(getContext().getResources().getText(resid)); }
    public void setError(CharSequence e) { mError = e; invalidate(); }
    public void setError(CharSequence e, Drawable icon) { setError(e); }
    public CharSequence getError() { return mError; }
    public final void setTransformationMethod(TransformationMethod m) { if (m == mTransformation) return; mTransformation = m; setText(mText, mBufferType); }
    public final TransformationMethod getTransformationMethod() { return mTransformation; }
    public final void setMovementMethod(MovementMethod m) {
        if (mMovement != m) {
            mMovement = m;
            if (m != null && !(mText instanceof Spannable)) setText(mText, BufferType.SPANNABLE);
            if (m != null && mText instanceof Spannable) m.initialize(this, (Spannable) mText);
            setFocusable(true); setClickable(true); setLongClickable(true);
        }
    }
    public final MovementMethod getMovementMethod() { return mMovement; }
    public final void setLinksClickable(boolean w) { mLinksClickable = w; }
    public final boolean getLinksClickable() { return mLinksClickable; }
    public final void setAutoLinkMask(int m) { mAutoLinkMask = m; }
    public final int getAutoLinkMask() { return mAutoLinkMask; }
    public android.text.style.URLSpan[] getUrls() { return mText instanceof Spanned ? ((Spanned) mText).getSpans(0, mText.length(), android.text.style.URLSpan.class) : new android.text.style.URLSpan[0]; }
    public void setKeyListener(KeyListener k) { mKeyListener = k; if (k != null && !(mText instanceof Editable)) setText(mText, BufferType.EDITABLE); if (k != null) { mInputType = k.getInputType(); setFocusable(true); setFocusableInTouchMode(true); } else mInputType = EditorInfo.TYPE_NULL; }
    public final KeyListener getKeyListener() { return mKeyListener; }
    public void setFilters(InputFilter[] f) { mFilters = f == null ? new InputFilter[0] : f; if (mText instanceof Editable) ((Editable) mText).setFilters(mFilters); }
    public InputFilter[] getFilters() { return mFilters; }
    public void setTextIsSelectable(boolean s) { mTextIsSelectable = s; setFocusableInTouchMode(s); setFocusable(s); setClickable(s); setLongClickable(s); if (s) { setMovementMethod(ArrowKeyMovementMethod.getInstance()); setText(mText, BufferType.SPANNABLE); } }
    public boolean isTextSelectable() { return mTextIsSelectable; }
    public boolean isTextEditable() { return mText instanceof Editable && onCheckIsTextEditor() && isEnabled(); }
    public void setFreezesText(boolean f) { mFreezesText = f; }
    public boolean getFreezesText() { return mFreezesText; }
    public void setSpannableFactory(Spannable.Factory f) {}
    public void setEditableFactory(Editable.Factory f) {}

    // ---- selection
    public int getSelectionStart() { return Selection.getSelectionStart(mText); }
    public int getSelectionEnd() { return Selection.getSelectionEnd(mText); }
    public boolean hasSelection() { int s = getSelectionStart(), e = getSelectionEnd(); return s >= 0 && s != e; }
    public void setSelection(int start, int stop) { if (mText instanceof Spannable) Selection.setSelection((Spannable) mText, start, stop); else throw new IllegalStateException("not spannable"); }
    public void setSelection(int index) { setSelection(index, index); }
    public void selectAll() { if (mText instanceof Spannable) Selection.selectAll((Spannable) mText); }
    public void extendSelection(int index) { if (mText instanceof Spannable) Selection.extendSelection((Spannable) mText, index); }
    public void setSelectAllOnFocus(boolean s) { mSelectAllOnFocus = s; }
    public void setCursorVisible(boolean v) { mCursorVisible = v; invalidate(); }
    public boolean isCursorVisible() { return mCursorVisible; }
    public int getOffsetForPosition(float x, float y) {
        if (mLayout == null) return -1;
        int line = mLayout.getLineForVertical((int) (y - getTotalPaddingTop() + getScrollY()));
        return mLayout.getOffsetForHorizontal(line, x - getTotalPaddingLeft() + getScrollX());
    }

    // ---- lines and sizes
    public void setSingleLine() { setSingleLine(true); }
    public void setSingleLine(boolean s) { if (s) mInputType &= ~EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE; else if ((mInputType & EditorInfo.TYPE_MASK_CLASS) == EditorInfo.TYPE_CLASS_TEXT) mInputType |= EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE; applySingleLine(s, true); }
    private void applySingleLine(boolean single, boolean applyTransformation) {
        mSingleLine = single;
        if (single) { setLines(1); setHorizontallyScrolling(true); if (applyTransformation) setTransformationMethod(SingleLineTransformationMethod.getInstance()); }
        else { setMaxLines(Integer.MAX_VALUE); setMinLines(0); setHorizontallyScrolling(false); if (applyTransformation && mTransformation instanceof SingleLineTransformationMethod) setTransformationMethod(null); }
    }
    public boolean isSingleLine() { return mSingleLine; }
    public void setHorizontallyScrolling(boolean w) { mHorizontallyScrolling = w; relayout(); }
    public boolean isHorizontallyScrollable() { return mHorizontallyScrolling; }
    public void setLines(int n) { mMaxLines = mMinLines = n; mMaxMode = mMinMode = LINES; relayout(); }
    public void setMaxLines(int n) { mMaxLines = n; mMaxMode = LINES; relayout(); }
    public int getMaxLines() { return mMaxMode == LINES ? mMaxLines : -1; }
    public void setMinLines(int n) { mMinLines = n; mMinMode = LINES; relayout(); }
    public int getMinLines() { return mMinMode == LINES ? mMinLines : -1; }
    public void setMaxHeight(int px) { mMaximum = px; mMaxMode = PIXELS; relayout(); }
    public int getMaxHeight() { return mMaxMode == PIXELS ? mMaximum : -1; }
    public void setMinHeight(int px) { mMinimum = px; mMinMode = PIXELS; relayout(); }
    public int getMinHeight() { return mMinMode == PIXELS ? mMinimum : -1; }
    public void setHeight(int px) { mMaximum = mMinimum = px; mMaxMode = mMinMode = PIXELS; relayout(); }
    public void setMaxWidth(int px) { mMaxWidth = px; mMaxWidthMode = PIXELS; relayout(); }
    public int getMaxWidth() { return mMaxWidthMode == PIXELS ? mMaxWidth : -1; }
    public void setMinWidth(int px) { mMinWidth = px; mMinWidthMode = PIXELS; relayout(); }
    public int getMinWidth() { return mMinWidthMode == PIXELS ? mMinWidth : -1; }
    public void setWidth(int px) { mMaxWidth = mMinWidth = px; mMaxWidthMode = mMinWidthMode = PIXELS; relayout(); }
    public void setEms(int ems) { mMaxWidth = mMinWidth = ems; mMaxWidthMode = mMinWidthMode = EMS; relayout(); }
    public void setMaxEms(int ems) { mMaxWidth = ems; mMaxWidthMode = EMS; relayout(); }
    public void setMinEms(int ems) { mMinWidth = ems; mMinWidthMode = EMS; relayout(); }
    public int getMaxEms() { return mMaxWidthMode == EMS ? mMaxWidth : -1; }
    public int getMinEms() { return mMinWidthMode == EMS ? mMinWidth : -1; }
    public void setLineSpacing(float add, float mult) { mSpacingAdd = add; mSpacingMult = mult; relayout(); }
    public float getLineSpacingExtra() { return mSpacingAdd; }
    public float getLineSpacingMultiplier() { return mSpacingMult; }
    public void setLineHeight(int px) { mLineHeight = px; relayout(); }
    public void setLineHeight(int unit, float h) { setLineHeight((int) TypedValue.applyDimension(unit, h, getResources().getDisplayMetrics())); }
    public int getLineHeight() { return Math.round(mTextPaint.getFontMetricsInt(null) * mSpacingMult + mSpacingAdd); }
    public void setFirstBaselineToTopHeight(int h) {}
    public void setLastBaselineToBottomHeight(int h) {}
    public int getFirstBaselineToTopHeight() { return 0; }
    public int getLastBaselineToBottomHeight() { return 0; }
    public void setIncludeFontPadding(boolean p) { mIncludePad = p; relayout(); }
    public boolean getIncludeFontPadding() { return mIncludePad; }
    public void setEllipsize(TextUtils.TruncateAt w) { mEllipsize = w; relayout(); }
    public TextUtils.TruncateAt getEllipsize() { return mEllipsize; }
    public void setMarqueeRepeatLimit(int n) {}
    public void setBreakStrategy(int s) { mBreakStrategy = s; }
    public int getBreakStrategy() { return mBreakStrategy; }
    public void setHyphenationFrequency(int f) { mHyphenation = f; }
    public int getHyphenationFrequency() { return mHyphenation; }
    public void setJustificationMode(int m) { mJustification = m; }
    public int getJustificationMode() { return mJustification; }
    public void setLineBreakStyle(int s) {}
    public void setLineBreakWordStyle(int s) {}
    public void setTextLocale(Locale l) { mLocale = l; mTextPaint.setTextLocale(l); }
    public Locale getTextLocale() { return mLocale != null ? mLocale : Locale.getDefault(); }
    public void setTextLocales(android.os.LocaleList l) {}
    public void setAutoSizeTextTypeWithDefaults(int t) { mAutoSizeType = t; }
    public void setAutoSizeTextTypeUniformWithConfiguration(int min, int max, int step, int unit) { mAutoSizeType = AUTO_SIZE_TEXT_TYPE_UNIFORM; }
    public void setAutoSizeTextTypeUniformWithPresetSizes(int[] sizes, int unit) { mAutoSizeType = AUTO_SIZE_TEXT_TYPE_UNIFORM; }
    public int getAutoSizeTextType() { return mAutoSizeType; }
    public void setGravity(int g) {
        if ((g & Gravity.RELATIVE_HORIZONTAL_GRAVITY_MASK) == 0) g |= Gravity.START;
        if ((g & Gravity.VERTICAL_GRAVITY_MASK) == 0) g |= Gravity.TOP;
        if (g != mGravity) { mGravity = g; relayout(); }
    }
    public int getGravity() { return mGravity; }
    public final Layout getLayout() { return mLayout; }
    public final Layout getHintLayout() { return mHintLayout; }
    public int getLineCount() { return mLayout != null ? mLayout.getLineCount() : 0; }
    public int getLineBounds(int line, Rect bounds) { if (mLayout == null) { if (bounds != null) bounds.set(0, 0, 0, 0); return 0; } int b = mLayout.getLineBounds(line, bounds); int dx = getTotalPaddingLeft(), dy = getTotalPaddingTop() + verticalOffset(); if (bounds != null) bounds.offset(dx, dy); return b + dy; }
    @Override public int getBaseline() { if (mLayout == null) return super.getBaseline(); return getExtendedPaddingTop() + verticalOffset() + mLayout.getLineBaseline(0); }

    // ---- compound drawables
    public void setCompoundDrawables(Drawable l, Drawable t, Drawable r, Drawable b) {
        Drawable[] d = { l, t, r, b };
        for (Drawable o : mDrawables) if (o != null) o.setCallback(null);
        for (Drawable x : d) if (x != null) { x.setCallback(this); if (x.isStateful()) x.setState(getDrawableState()); if (mDrawableTint != null) x.setTintList(mDrawableTint); }
        mDrawables = d;
        relayout();
    }
    public void setCompoundDrawablesWithIntrinsicBounds(Drawable l, Drawable t, Drawable r, Drawable b) {
        for (Drawable d : new Drawable[] { l, t, r, b }) if (d != null) d.setBounds(0, 0, d.getIntrinsicWidth(), d.getIntrinsicHeight());
        setCompoundDrawables(l, t, r, b);
    }
    public void setCompoundDrawablesWithIntrinsicBounds(int l, int t, int r, int b) {
        Context c = getContext();
        setCompoundDrawablesWithIntrinsicBounds(l != 0 ? c.getDrawable(l) : null, t != 0 ? c.getDrawable(t) : null, r != 0 ? c.getDrawable(r) : null, b != 0 ? c.getDrawable(b) : null);
    }
    public void setCompoundDrawablesRelative(Drawable s, Drawable t, Drawable e, Drawable b) { setCompoundDrawables(s, t, e, b); }
    public void setCompoundDrawablesRelativeWithIntrinsicBounds(Drawable s, Drawable t, Drawable e, Drawable b) { setCompoundDrawablesWithIntrinsicBounds(s, t, e, b); }
    public void setCompoundDrawablesRelativeWithIntrinsicBounds(int s, int t, int e, int b) { setCompoundDrawablesWithIntrinsicBounds(s, t, e, b); }
    public Drawable[] getCompoundDrawables() { return mDrawables.clone(); }
    public Drawable[] getCompoundDrawablesRelative() { return mDrawables.clone(); }
    public void setCompoundDrawablePadding(int p) { mDrawablePadding = p; relayout(); }
    public int getCompoundDrawablePadding() { return mDrawablePadding; }
    public void setCompoundDrawableTintList(ColorStateList t) { mDrawableTint = t; for (Drawable d : mDrawables) if (d != null) d.setTintList(t); invalidate(); }
    public ColorStateList getCompoundDrawableTintList() { return mDrawableTint; }
    public void setCompoundDrawableTintMode(PorterDuff.Mode m) { mDrawableTintMode = m; for (Drawable d : mDrawables) if (d != null) d.setTintMode(m); }
    public PorterDuff.Mode getCompoundDrawableTintMode() { return mDrawableTintMode; }
    @Override protected boolean verifyDrawable(Drawable who) { for (Drawable d : mDrawables) if (d == who) return true; return super.verifyDrawable(who); }
    private int dw(int i) { Drawable d = mDrawables[i]; return d == null ? 0 : d.getBounds().width(); }
    private int dh(int i) { Drawable d = mDrawables[i]; return d == null ? 0 : d.getBounds().height(); }
    public int getCompoundPaddingLeft() { return getPaddingLeft() + (mDrawables[0] != null ? dw(0) + mDrawablePadding : 0); }
    public int getCompoundPaddingRight() { return getPaddingRight() + (mDrawables[2] != null ? dw(2) + mDrawablePadding : 0); }
    public int getCompoundPaddingTop() { return getPaddingTop() + (mDrawables[1] != null ? dh(1) + mDrawablePadding : 0); }
    public int getCompoundPaddingBottom() { return getPaddingBottom() + (mDrawables[3] != null ? dh(3) + mDrawablePadding : 0); }
    public int getCompoundPaddingStart() { return getCompoundPaddingLeft(); }
    public int getCompoundPaddingEnd() { return getCompoundPaddingRight(); }
    public int getExtendedPaddingTop() { return getCompoundPaddingTop(); }
    public int getExtendedPaddingBottom() { return getCompoundPaddingBottom(); }
    public int getTotalPaddingLeft() { return getCompoundPaddingLeft(); }
    public int getTotalPaddingRight() { return getCompoundPaddingRight(); }
    public int getTotalPaddingStart() { return getCompoundPaddingLeft(); }
    public int getTotalPaddingEnd() { return getCompoundPaddingRight(); }
    public int getTotalPaddingTop() { return getExtendedPaddingTop() + verticalOffset(); }
    public int getTotalPaddingBottom() { return getExtendedPaddingBottom() + bottomVerticalOffset(); }

    // ---- layout
    private void relayout() { mLayoutDirty = true; mHintLayout = null; requestLayout(); invalidate(); }
    private int desiredWidth(CharSequence t) { return t == null ? 0 : (int) Math.ceil(Layout.getDesiredWidth(t, mTextPaint)); }
    private Layout makeLayout(CharSequence text, int width, boolean hint) {
        if (width < 0) width = 0;
        int max = mMaxMode == LINES ? mMaxLines : Integer.MAX_VALUE;
        TextUtils.TruncateAt ell = mEllipsize == TextUtils.TruncateAt.MARQUEE ? TextUtils.TruncateAt.END : mEllipsize;
        Layout.Alignment align = alignment();
        float add = mSpacingAdd, mult = mSpacingMult;
        if (mLineHeight > 0) { int fh = mTextPaint.getFontMetricsInt(null); add = mLineHeight - fh; mult = 1; }
        boolean single = mSingleLine || max == 1;
        int w = mHorizontallyScrolling && single && ell == null ? Math.max(width, desiredWidth(text)) : width;
        StaticLayout.Builder b = StaticLayout.Builder.obtain(text, 0, text.length(), mTextPaint, Math.max(0, w)).setAlignment(align).setLineSpacing(add, mult).setIncludePad(mIncludePad);
        if (ell != null && (single || max < Integer.MAX_VALUE)) b.setEllipsize(ell).setEllipsizedWidth(width).setMaxLines(max);
        else if (max < Integer.MAX_VALUE) b.setMaxLines(max);
        return b.build();
    }
    private Layout.Alignment alignment() {
        int ta = getTextAlignment();
        if (ta == TEXT_ALIGNMENT_CENTER) return Layout.Alignment.ALIGN_CENTER;
        if (ta == TEXT_ALIGNMENT_TEXT_END || ta == TEXT_ALIGNMENT_VIEW_END) return Layout.Alignment.ALIGN_OPPOSITE;
        if (ta == TEXT_ALIGNMENT_TEXT_START || ta == TEXT_ALIGNMENT_VIEW_START) return Layout.Alignment.ALIGN_NORMAL;
        int h = Gravity.getAbsoluteGravity(mGravity, 0) & Gravity.HORIZONTAL_GRAVITY_MASK;
        return h == Gravity.CENTER_HORIZONTAL ? Layout.Alignment.ALIGN_CENTER : h == Gravity.RIGHT ? Layout.Alignment.ALIGN_OPPOSITE : Layout.Alignment.ALIGN_NORMAL;
    }
    @Override protected void onMeasure(int ws, int hs) {
        int wm = MeasureSpec.getMode(ws), hm = MeasureSpec.getMode(hs), wsz = MeasureSpec.getSize(ws), hsz = MeasureSpec.getSize(hs);
        int padH = getCompoundPaddingLeft() + getCompoundPaddingRight(), padV = getCompoundPaddingTop() + getCompoundPaddingBottom();
        int width;
        if (wm == MeasureSpec.EXACTLY) width = wsz;
        else {
            int des = Math.max(desiredWidth(mTransformed), mHint != null && mText.length() == 0 ? desiredWidth(mHint) : 0);
            width = des + padH;
            if (mMaxWidthMode == EMS) width = Math.min(width, mMaxWidth * getLineHeight() + padH); else width = Math.min(width, mMaxWidth);
            if (mMinWidthMode == EMS) width = Math.max(width, mMinWidth * getLineHeight() + padH); else width = Math.max(width, mMinWidth);
            width = Math.max(width, getSuggestedMinimumWidth());
            if (mDrawables[1] != null) width = Math.max(width, dw(1) + getPaddingLeft() + getPaddingRight());
            if (mDrawables[3] != null) width = Math.max(width, dw(3) + getPaddingLeft() + getPaddingRight());
            if (wm == MeasureSpec.AT_MOST) width = Math.min(wsz, width);
        }
        int avail = width - padH;
        if (mLayoutDirty || mLayout == null || mLayout.getWidth() != avail && !(mHorizontallyScrolling && mLayout.getWidth() >= avail)) {
            mLayout = makeLayout(mTransformed, avail, false);
            mLayoutDirty = false;
            mHintLayout = null;
        }
        if (mHint != null && mText.length() == 0 && (mHintLayout == null || mHintLayout.getWidth() != avail)) mHintLayout = makeLayout(mHint, avail, true);
        int height;
        if (hm == MeasureSpec.EXACTLY) height = hsz;
        else {
            Layout l = mText.length() == 0 && mHintLayout != null ? mHintLayout : mLayout;
            int lines = l.getLineCount();
            int h = l.getLineTop(lines);
            if (mMinMode == LINES && lines < mMinLines) h += (mMinLines - lines) * getLineHeight();
            if (mMaxMode == LINES && lines > mMaxLines) h = l.getLineTop(mMaxLines);
            height = h + padV;
            if (mMaxMode == PIXELS) height = Math.min(height, mMaximum);
            if (mMinMode == PIXELS) height = Math.max(height, mMinimum);
            height = Math.max(height, getSuggestedMinimumHeight());
            if (mDrawables[0] != null) height = Math.max(height, dh(0) + getPaddingTop() + getPaddingBottom());
            if (mDrawables[2] != null) height = Math.max(height, dh(2) + getPaddingTop() + getPaddingBottom());
            if (hm == MeasureSpec.AT_MOST) height = Math.min(hsz, height);
        }
        setMeasuredDimension(width, height);
    }
    @Override protected void onSizeChanged(int w, int h, int ow, int oh) { super.onSizeChanged(w, h, ow, oh); }
    private int verticalOffset() {
        Layout l = mText.length() == 0 && mHintLayout != null ? mHintLayout : mLayout;
        if (l == null) return 0;
        int g = mGravity & Gravity.VERTICAL_GRAVITY_MASK;
        if (g == Gravity.TOP) return 0;
        int box = getMeasuredHeight() - getExtendedPaddingTop() - getExtendedPaddingBottom(), th = l.getHeight();
        if (th >= box) return 0;
        return g == Gravity.BOTTOM ? box - th : (box - th) >> 1;
    }
    private int bottomVerticalOffset() {
        Layout l = mText.length() == 0 && mHintLayout != null ? mHintLayout : mLayout;
        if (l == null) return 0;
        int g = mGravity & Gravity.VERTICAL_GRAVITY_MASK;
        if (g == Gravity.BOTTOM) return 0;
        int box = getMeasuredHeight() - getExtendedPaddingTop() - getExtendedPaddingBottom(), th = l.getHeight();
        if (th >= box) return 0;
        return g == Gravity.TOP ? box - th : (box - th) >> 1;
    }

    // ---- drawing
    @Override protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        int cl = getCompoundPaddingLeft(), cr = getCompoundPaddingRight(), ct = getExtendedPaddingTop(), cb = getExtendedPaddingBottom();
        int vspace = h - cb - ct, hspace = w - cr - cl;
        // compound drawables, centered on their side
        int sx = getScrollX(), sy = getScrollY();
        if (mDrawables[0] != null) { c.save(); c.translate(sx + getPaddingLeft(), sy + ct + (vspace - dh(0)) / 2); mDrawables[0].draw(c); c.restore(); }
        if (mDrawables[2] != null) { c.save(); c.translate(sx + w - getPaddingRight() - dw(2), sy + ct + (vspace - dh(2)) / 2); mDrawables[2].draw(c); c.restore(); }
        if (mDrawables[1] != null) { c.save(); c.translate(sx + cl + (hspace - dw(1)) / 2, sy + getPaddingTop()); mDrawables[1].draw(c); c.restore(); }
        if (mDrawables[3] != null) { c.save(); c.translate(sx + cl + (hspace - dw(3)) / 2, sy + h - getPaddingBottom() - dh(3)); mDrawables[3].draw(c); c.restore(); }
        if (mLayout == null || mLayoutDirty) { if (hspace > 0) { mLayout = makeLayout(mTransformed, hspace, false); mLayoutDirty = false; } else return; }
        Layout layout = mLayout;
        boolean hint = mText.length() == 0 && mHint != null;
        if (hint) { if (mHintLayout == null) mHintLayout = makeLayout(mHint, hspace, true); layout = mHintLayout; }
        mTextPaint.setColor(hint ? mCurHintTextColor : mCurTextColor);
        mTextPaint.drawableState = getDrawableState();
        c.save();
        int voffset = verticalOffset();
        c.clipRect(cl + sx, ct + sy, w - cr + sx, h - cb + sy);
        c.translate(cl, ct + voffset);
        // the selection, then the text, then the cursor
        int ss = getSelectionStart(), se = getSelectionEnd();
        boolean focusedEditor = isFocused() && (mText instanceof Editable || mTextIsSelectable);
        if (focusedEditor && ss >= 0 && ss != se) {
            Path sel = new Path();
            layout.getSelectionPath(ss, se, sel);
            mHighlightPaint.setColor(mHighlightColor);
            c.drawPath(sel, mHighlightPaint);
        }
        layout.draw(c);
        if (focusedEditor && mCursorVisible && ss >= 0 && ss == se && mText instanceof Editable) {
            long t = android.os.SystemClock.uptimeMillis() - mBlinkStart;
            if ((t % 1000) < 500) {
                int line = mLayout.getLineForOffset(ss);
                float x = mLayout.getPrimaryHorizontal(ss);
                int top = mLayout.getLineTop(line), bottom = mLayout.getLineBottom(line);
                mHighlightPaint.setColor(mCursorColor != null ? mCursorColor.getDefaultColor() : accentColor());
                float cw = Math.max(2, getResources().getDisplayMetrics().density * 2);
                c.drawRect(x - cw / 2, top, x + cw / 2, bottom, mHighlightPaint);
            }
            postInvalidateDelayed(500 - (t % 500));
        }
        c.restore();
        if (mError != null) {
            mHighlightPaint.setColor(0xFFD32F2F);
            c.drawRect(sx, sy + h - Math.max(2, getResources().getDisplayMetrics().density * 2), sx + w, sy + h, mHighlightPaint);
        }
    }
    private int accentColor() {
        TypedValue v = new TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.colorAccent, v, true) && v.type >= TypedValue.TYPE_FIRST_INT) return v.data;
        return 0xFF1A73E8;
    }
    public boolean onPreDraw() { return true; }

    // ---- editing
    @Override public boolean onCheckIsTextEditor() { return mInputType != EditorInfo.TYPE_NULL && mText instanceof Editable; }
    @Override public InputConnection onCreateInputConnection(EditorInfo out) {
        if (!onCheckIsTextEditor() || !isEnabled()) return null;
        out.inputType = getInputType();
        out.imeOptions = mImeOptions;
        out.privateImeOptions = mPrivateImeOptions;
        out.actionLabel = mImeActionLabel;
        out.actionId = mImeActionId;
        out.extras = mInputExtras;
        if ((out.imeOptions & EditorInfo.IME_MASK_ACTION) == EditorInfo.IME_ACTION_UNSPECIFIED) {
            View next = focusSearch(FOCUS_DOWN);
            out.imeOptions |= next != null && next != this && next.onCheckIsTextEditor() ? EditorInfo.IME_ACTION_NEXT : EditorInfo.IME_ACTION_DONE;
        }
        if (isMultiline()) out.imeOptions |= EditorInfo.IME_FLAG_NO_ENTER_ACTION;
        out.hintText = mHint;
        out.initialSelStart = getSelectionStart(); out.initialSelEnd = getSelectionEnd();
        out.initialCapsMode = getInputType() & (EditorInfo.TYPE_TEXT_FLAG_CAP_CHARACTERS | EditorInfo.TYPE_TEXT_FLAG_CAP_WORDS | EditorInfo.TYPE_TEXT_FLAG_CAP_SENTENCES);
        return new EditableInputConnection(this);
    }
    private boolean isMultiline() { return (mInputType & (EditorInfo.TYPE_MASK_CLASS | EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE)) == (EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE); }
    static final class EditableInputConnection extends BaseInputConnection {
        private final TextView mView;
        EditableInputConnection(TextView v) { super(v, true); mView = v; }
        @Override public Editable getEditable() { return mView.getEditableText(); }
        @Override public boolean performEditorAction(int action) { mView.onEditorAction(action); return true; }
        @Override public boolean commitText(CharSequence t, int pos) { if (mView.getEditableText() == null) return false; return super.commitText(t, pos); }
    }
    public void onEditorAction(int action) {
        if (mEditorAction != null && mEditorAction.onEditorAction(this, action, null)) return;
        if (action == EditorInfo.IME_ACTION_NEXT) { View v = focusSearch(FOCUS_FORWARD); if (v != null && v.requestFocus(FOCUS_FORWARD)) return; }
        if (action == EditorInfo.IME_ACTION_PREVIOUS) { View v = focusSearch(FOCUS_BACKWARD); if (v != null && v.requestFocus(FOCUS_BACKWARD)) return; }
        if (action == EditorInfo.IME_ACTION_DONE || action == EditorInfo.IME_ACTION_GO || action == EditorInfo.IME_ACTION_SEND || action == EditorInfo.IME_ACTION_SEARCH || action == EditorInfo.IME_ACTION_NEXT) {
            husk.InputMethods.hide();
            if (action != EditorInfo.IME_ACTION_DONE) {
                long t = android.os.SystemClock.uptimeMillis();
                // the enter key the action stands for, as Android sends it to a single-line field
                dispatchKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_ENTER, 0));
                dispatchKeyEvent(new KeyEvent(t, t, KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER, 0));
            }
        }
    }
    public void setOnEditorActionListener(OnEditorActionListener l) { mEditorAction = l; }
    public void setInputType(int type) {
        mInputType = type;
        boolean multi = (type & (EditorInfo.TYPE_MASK_CLASS | EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE)) == (EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_FLAG_MULTI_LINE);
        if (isPasswordInputType(type)) setTransformationMethod(PasswordTransformationMethod.getInstance());
        else if (mTransformation instanceof PasswordTransformationMethod) setTransformationMethod(null);
        if (type != EditorInfo.TYPE_NULL) {
            if ((type & EditorInfo.TYPE_MASK_CLASS) == EditorInfo.TYPE_CLASS_NUMBER) mKeyListener = DigitsKeyListener.getInstance((type & EditorInfo.TYPE_NUMBER_FLAG_SIGNED) != 0, (type & EditorInfo.TYPE_NUMBER_FLAG_DECIMAL) != 0);
            else if ((type & EditorInfo.TYPE_MASK_CLASS) == EditorInfo.TYPE_CLASS_PHONE) mKeyListener = DialerKeyListener.getInstance();
            else if (mKeyListener == null) mKeyListener = TextKeyListener.getInstance();
            if (!(mText instanceof Editable)) setText(mText, BufferType.EDITABLE);
            if (!multi && !mSingleLine && (type & EditorInfo.TYPE_MASK_CLASS) != EditorInfo.TYPE_CLASS_TEXT) applySingleLine(true, false);
        }
        husk.InputMethods.restart(this);
    }
    public void setRawInputType(int type) { mInputType = type; }
    public int getInputType() { return mInputType; }
    private static boolean isPasswordInputType(int t) {
        int v = t & (EditorInfo.TYPE_MASK_CLASS | EditorInfo.TYPE_MASK_VARIATION);
        return v == (EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_PASSWORD) || v == (EditorInfo.TYPE_CLASS_TEXT | EditorInfo.TYPE_TEXT_VARIATION_WEB_PASSWORD) || v == (EditorInfo.TYPE_CLASS_NUMBER | EditorInfo.TYPE_NUMBER_VARIATION_PASSWORD);
    }
    public void setImeOptions(int o) { mImeOptions = o; }
    public int getImeOptions() { return mImeOptions; }
    public void setImeActionLabel(CharSequence l, int id) { mImeActionLabel = l; mImeActionId = id; }
    public CharSequence getImeActionLabel() { return mImeActionLabel; }
    public int getImeActionId() { return mImeActionId; }
    public void setPrivateImeOptions(String t) { mPrivateImeOptions = t; }
    public String getPrivateImeOptions() { return mPrivateImeOptions; }
    public Bundle getInputExtras(boolean create) { if (mInputExtras == null && create) mInputExtras = new Bundle(); return mInputExtras; }
    public void setImeHintLocales(android.os.LocaleList l) {}
    public void setShowSoftInputOnFocus(boolean s) {}
    public boolean getShowSoftInputOnFocus() { return true; }
    public void setCustomSelectionActionModeCallback(ActionMode.Callback cb) {}
    public void setCustomInsertionActionModeCallback(ActionMode.Callback cb) {}
    public boolean isInputMethodTarget() { return husk.InputMethods.focused() == this; }
    public boolean didTouchFocusSelect() { return false; }
    public void beginBatchEdit() {}
    public void endBatchEdit() {}
    public void onBeginBatchEdit() {}
    public void onEndBatchEdit() {}
    public boolean onPrivateIMECommand(String a, Bundle d) { return false; }
    public boolean onTextContextMenuItem(int id) {
        android.content.ClipboardManager cb = (android.content.ClipboardManager) getContext().getSystemService(Context.CLIPBOARD_SERVICE);
        int s = Math.max(0, Math.min(getSelectionStart(), getSelectionEnd())), e = Math.max(0, Math.max(getSelectionStart(), getSelectionEnd()));
        if (id == android.R.id.selectAll) { selectAll(); return true; }
        if (id == android.R.id.copy || id == android.R.id.cut) { if (e > s) cb.setPrimaryClip(android.content.ClipData.newPlainText(null, mText.subSequence(s, e))); if (id == android.R.id.cut && mText instanceof Editable) ((Editable) mText).delete(s, e); return true; }
        if (id == android.R.id.paste && mText instanceof Editable) { CharSequence t = cb.getText(); if (t != null) ((Editable) mText).replace(s, e, t); return true; }
        return false;
    }
    @Override public boolean onKeyDown(int code, KeyEvent e) {
        if (code == KeyEvent.KEYCODE_ENTER && onCheckIsTextEditor() && !isMultiline()) {
            if (mEditorAction != null && mEditorAction.onEditorAction(this, EditorInfo.IME_NULL, e)) return true;
            return super.onKeyDown(code, e);
        }
        if (mKeyListener != null && mText instanceof Editable && mKeyListener.onKeyDown(this, (Editable) mText, code, e)) return true;
        if (mMovement != null && mText instanceof Spannable && mMovement.onKeyDown(this, (Spannable) mText, code, e)) return true;
        if (code == KeyEvent.KEYCODE_ENTER && isMultiline() && mText instanceof Editable) { int s = Math.max(0, getSelectionStart()), en = Math.max(0, getSelectionEnd()); ((Editable) mText).replace(Math.min(s, en), Math.max(s, en), "\n"); return true; }
        return super.onKeyDown(code, e);
    }
    @Override public boolean onKeyUp(int code, KeyEvent e) {
        if (mKeyListener != null && mText instanceof Editable && mKeyListener.onKeyUp(this, (Editable) mText, code, e)) return true;
        if (mMovement != null && mText instanceof Spannable && mMovement.onKeyUp(this, (Spannable) mText, code, e)) return true;
        return super.onKeyUp(code, e);
    }
    @Override protected void onFocusChanged(boolean focused, int dir, Rect prev) {
        super.onFocusChanged(focused, dir, prev);
        if (focused) {
            mBlinkStart = android.os.SystemClock.uptimeMillis();
            if (mText instanceof Spannable) {
                if (mSelectAllOnFocus) selectAll();
                else if (getSelectionStart() < 0) Selection.setSelection((Spannable) mText, mText.length());
                if (mMovement != null) mMovement.onTakeFocus(this, (Spannable) mText, dir);
            }
        }
        if (mTransformation != null) mTransformation.onFocusChanged(this, mText, focused, dir, prev);
        invalidate();
    }
    @Override public boolean onTouchEvent(MotionEvent e) {
        int a = e.getActionMasked();
        boolean superResult = super.onTouchEvent(e);
        if ((mMovement != null || onCheckIsTextEditor()) && isEnabled() && mText instanceof Spannable && mLayout != null) {
            boolean handled = false;
            if (mMovement != null) handled |= mMovement.onTouchEvent(this, (Spannable) mText, e);
            if (a == MotionEvent.ACTION_UP && isFocused() && onCheckIsTextEditor()) {
                if (!(mMovement instanceof ArrowKeyMovementMethod)) { int off = getOffsetForPosition(e.getX(), e.getY()); if (off >= 0) Selection.setSelection((Spannable) mText, off); }
                husk.InputMethods.show(this);
                handled = true;
            }
            if (handled) return true;
        }
        return superResult;
    }
    @Override public boolean performClick() {
        boolean r = super.performClick();
        if (onCheckIsTextEditor()) { if (!isFocused()) requestFocus(); husk.InputMethods.show(this); }
        return r;
    }
    @Override public boolean performLongClick() {
        if (mText instanceof Spannable && (onCheckIsTextEditor() || mTextIsSelectable) && mText.length() > 0) {
            if (!super.performLongClick()) { selectAll(); performHapticFeedback(HapticFeedbackConstants.LONG_PRESS); }
            return true;
        }
        return super.performLongClick();
    }

    // ---- state
    @Override protected android.os.Parcelable onSaveInstanceState() {
        android.os.Parcelable s = super.onSaveInstanceState();
        if (!mFreezesText && !(mText instanceof Editable)) return s;
        SavedState ss = new SavedState(s);
        ss.text = mText.toString(); ss.selStart = getSelectionStart(); ss.selEnd = getSelectionEnd();
        return ss;
    }
    @Override protected void onRestoreInstanceState(android.os.Parcelable s) {
        if (!(s instanceof SavedState)) { super.onRestoreInstanceState(s); return; }
        SavedState ss = (SavedState) s;
        super.onRestoreInstanceState(ss.getSuperState());
        if (ss.text != null) setText(ss.text);
        if (ss.selStart >= 0 && mText instanceof Spannable) Selection.setSelection((Spannable) mText, Math.min(ss.selStart, mText.length()), Math.min(ss.selEnd, mText.length()));
    }
    @Override public CharSequence getAccessibilityClassName() { return TextView.class.getName(); }
    public static int getTextColor(Context c, TypedArray a, int def) { return def; }
    public void setRawTextSizeHusk(float s) { setRawTextSize(s); }
    public int getMarqueeRepeatLimit() { return 0; }
    public boolean bringPointIntoView(int offset) { return false; }
    public boolean moveCursorToVisibleOffset() { return false; }
    public void cancelLongPress() {}
    public void clearComposingText() {}
    public boolean isSuggestionsEnabled() { return false; }
    public void setTextClassifier(Object c) {}
    public void setTextMetricsParams(Object p) {}
    public Object getTextMetricsParams() { return null; }
    public void setPrecomputedText(Object p) { setText(String.valueOf(p)); }
    public float getLineSpacingExtraHusk() { return mSpacingAdd; }
    public void setEnabled(boolean e) { super.setEnabled(e); if (!e && husk.InputMethods.focused() == this) husk.InputMethods.hide(); }
    public boolean extractText(ExtractedTextRequest r, ExtractedText out) { out.text = mText; out.selectionStart = getSelectionStart(); out.selectionEnd = getSelectionEnd(); return true; }
    public void setExtractedText(ExtractedText t) { setText(t.text); }
    public void setTextAppearanceHusk(int r) { setTextAppearance(r); }
}
