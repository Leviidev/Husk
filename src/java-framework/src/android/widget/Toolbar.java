package android.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.*;
import java.util.ArrayList;

/**
 * An app bar: navigation button, logo, title and subtitle, the app's own children, then the menu's action items and an overflow
 * button whose popup lists the rest.
 */
public class Toolbar extends ViewGroup {
    public interface OnMenuItemClickListener { boolean onMenuItemClick(MenuItem item); }
    public static class LayoutParams extends android.app.ActionBar.LayoutParams {
        public LayoutParams(Context c, AttributeSet a) { super(c, a); }
        public LayoutParams(int w, int h) { super(w, h); gravity = Gravity.CENTER_VERTICAL | Gravity.START; }
        public LayoutParams(int w, int h, int g) { super(w, h); gravity = g; }
        public LayoutParams(int g) { this(WRAP_CONTENT, MATCH_PARENT, g); }
        public LayoutParams(LayoutParams s) { super(s); }
        public LayoutParams(android.app.ActionBar.LayoutParams s) { super(s); }
        public LayoutParams(ViewGroup.MarginLayoutParams s) { super(s); }
        public LayoutParams(ViewGroup.LayoutParams s) { super(s); }
    }
    private ImageButton mNavButton;
    private ImageView mLogoView;
    private TextView mTitleTextView, mSubtitleTextView;
    private CharSequence mTitleText, mSubtitleText;
    private int mTitleTextColor = 0, mSubtitleTextColor = 0, mTitleAppearance, mSubtitleAppearance, mContentInsetStart, mContentInsetEnd, mTitleMargin;
    private boolean mTitleColorSet, mSubtitleColorSet;
    private final LinearLayout mMenuView;
    private ImageButton mOverflow;
    private husk.MenuImpl mMenu;
    private OnMenuItemClickListener mOnMenuItemClickListener;
    private Drawable mOverflowIcon;
    private int mPopupTheme;
    private final ArrayList<View> mTemp = new ArrayList<>();
    private final float mD;
    public Toolbar(Context c) { this(c, null); }
    public Toolbar(Context c, AttributeSet a) { this(c, a, android.R.attr.toolbarStyle); }
    public Toolbar(Context c, AttributeSet a, int s) { this(c, a, s, 0); }
    public Toolbar(Context c, AttributeSet attrs, int s, int r) {
        super(c, attrs, s, r);
        mD = c.getResources().getDisplayMetrics().density;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.Toolbar, s, r);
        mTitleAppearance = a.getResourceId(husk.S.Toolbar_titleTextAppearance, 0);
        mSubtitleAppearance = a.getResourceId(husk.S.Toolbar_subtitleTextAppearance, 0);
        mContentInsetStart = a.getDimensionPixelOffset(husk.S.Toolbar_contentInsetStart, (int) (16 * mD));
        mContentInsetEnd = a.getDimensionPixelOffset(husk.S.Toolbar_contentInsetEnd, 0);
        mTitleMargin = a.getDimensionPixelOffset(husk.S.Toolbar_titleMargin, 0);
        mPopupTheme = a.getResourceId(husk.S.Toolbar_popupTheme, 0);
        CharSequence title = a.getText(husk.S.Toolbar_title);
        CharSequence subtitle = a.getText(husk.S.Toolbar_subtitle);
        if (a.hasValue(husk.S.Toolbar_titleTextColor)) { mTitleTextColor = a.getColor(husk.S.Toolbar_titleTextColor, 0xFFFFFFFF); mTitleColorSet = true; }
        if (a.hasValue(husk.S.Toolbar_subtitleTextColor)) { mSubtitleTextColor = a.getColor(husk.S.Toolbar_subtitleTextColor, 0xFFFFFFFF); mSubtitleColorSet = true; }
        Drawable nav = null, logo = null;
        try { nav = a.getDrawable(husk.S.Toolbar_navigationIcon); } catch (RuntimeException e) {}
        try { logo = a.getDrawable(husk.S.Toolbar_logo); } catch (RuntimeException e) {}
        CharSequence navDesc = a.getText(husk.S.Toolbar_navigationContentDescription);
        a.recycle();
        mMenuView = new LinearLayout(c);
        mMenuView.setOrientation(LinearLayout.HORIZONTAL);
        mMenuView.setGravity(Gravity.CENTER_VERTICAL);
        addSystemView(mMenuView);
        if (title != null && title.length() > 0) setTitle(title);
        if (subtitle != null && subtitle.length() > 0) setSubtitle(subtitle);
        if (nav != null) setNavigationIcon(nav);
        if (navDesc != null) setNavigationContentDescription(navDesc);
        if (logo != null) setLogo(logo);
        if (getMinimumHeight() == 0) setMinimumHeight(actionBarSize());
    }
    private int actionBarSize() { TypedValue v = new TypedValue(); return getContext().getTheme().resolveAttribute(android.R.attr.actionBarSize, v, true) ? TypedValue.complexToDimensionPixelSize(v.data, getResources().getDisplayMetrics()) : (int) (56 * mD); }
    private void addSystemView(View v) { LayoutParams lp = new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT); lp.gravity = Gravity.CENTER_VERTICAL; v.setTag(husk.IR.id.action_bar, "system"); addView(v, lp); }
    private boolean isSystem(View v) { return "system".equals(v.getTag(husk.IR.id.action_bar)); }
    private int foreground() {
        TypedValue v = new TypedValue();
        if (getContext().getTheme().resolveAttribute(android.R.attr.textColorPrimary, v, true)) {
            if (v.type >= TypedValue.TYPE_FIRST_COLOR_INT && v.type <= TypedValue.TYPE_LAST_COLOR_INT) return v.data;
            if (v.resourceId != 0) { try { return getContext().getColorStateList(v.resourceId).getDefaultColor(); } catch (Exception e) {} }
        }
        Drawable bg = getBackground();
        if (bg instanceof android.graphics.drawable.ColorDrawable) return android.graphics.Color.luminance(((android.graphics.drawable.ColorDrawable) bg).getColor()) > 0.5f ? 0xDE000000 : 0xFFFFFFFF;
        return 0xDE000000;
    }
    public void setPopupTheme(int r) { mPopupTheme = r; } public int getPopupTheme() { return mPopupTheme; }
    public void setTitleMargin(int s, int t, int e, int b) { mTitleMargin = s; requestLayout(); }
    public int getTitleMarginStart() { return mTitleMargin; } public void setTitleMarginStart(int m) { mTitleMargin = m; requestLayout(); }
    public int getTitleMarginTop() { return 0; } public void setTitleMarginTop(int m) {} public int getTitleMarginEnd() { return mTitleMargin; } public void setTitleMarginEnd(int m) {} public int getTitleMarginBottom() { return 0; } public void setTitleMarginBottom(int m) {}
    public void setLogo(int r) { setLogo(getContext().getDrawable(r)); }
    public void setLogo(Drawable d) {
        if (d != null) { if (mLogoView == null) { mLogoView = new ImageView(getContext()); mLogoView.setScaleType(ImageView.ScaleType.CENTER_INSIDE); addSystemView(mLogoView); } mLogoView.setImageDrawable(d); mLogoView.setVisibility(VISIBLE); }
        else if (mLogoView != null) mLogoView.setVisibility(GONE);
    }
    public Drawable getLogo() { return mLogoView != null ? mLogoView.getDrawable() : null; }
    public void setLogoDescription(int r) {} public void setLogoDescription(CharSequence d) {} public CharSequence getLogoDescription() { return null; }
    public CharSequence getTitle() { return mTitleText; }
    public void setTitle(int r) { setTitle(getContext().getText(r)); }
    public void setTitle(CharSequence t) {
        if (t != null && t.length() > 0) {
            if (mTitleTextView == null) {
                mTitleTextView = new TextView(getContext());
                mTitleTextView.setSingleLine(); mTitleTextView.setEllipsize(android.text.TextUtils.TruncateAt.END);
                if (mTitleAppearance != 0) mTitleTextView.setTextAppearance(mTitleAppearance);
                else { mTitleTextView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 20); mTitleTextView.setTypeface(android.graphics.Typeface.create("sans-serif-medium", android.graphics.Typeface.NORMAL)); }
                mTitleTextView.setTextColor(mTitleColorSet ? mTitleTextColor : foreground());
                addSystemView(mTitleTextView);
            }
            mTitleTextView.setVisibility(VISIBLE);
        } else if (mTitleTextView != null) mTitleTextView.setVisibility(GONE);
        if (mTitleTextView != null) mTitleTextView.setText(t);
        mTitleText = t;
    }
    public CharSequence getSubtitle() { return mSubtitleText; }
    public void setSubtitle(int r) { setSubtitle(getContext().getText(r)); }
    public void setSubtitle(CharSequence t) {
        if (t != null && t.length() > 0) {
            if (mSubtitleTextView == null) {
                mSubtitleTextView = new TextView(getContext());
                mSubtitleTextView.setSingleLine(); mSubtitleTextView.setEllipsize(android.text.TextUtils.TruncateAt.END);
                if (mSubtitleAppearance != 0) mSubtitleTextView.setTextAppearance(mSubtitleAppearance); else mSubtitleTextView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
                int fg = foreground();
                mSubtitleTextView.setTextColor(mSubtitleColorSet ? mSubtitleTextColor : (fg & 0x00FFFFFF) | 0xB3000000);
                addSystemView(mSubtitleTextView);
            }
            mSubtitleTextView.setVisibility(VISIBLE);
        } else if (mSubtitleTextView != null) mSubtitleTextView.setVisibility(GONE);
        if (mSubtitleTextView != null) mSubtitleTextView.setText(t);
        mSubtitleText = t;
    }
    public void setTitleTextAppearance(Context c, int r) { mTitleAppearance = r; if (mTitleTextView != null) mTitleTextView.setTextAppearance(r); }
    public void setSubtitleTextAppearance(Context c, int r) { mSubtitleAppearance = r; if (mSubtitleTextView != null) mSubtitleTextView.setTextAppearance(r); }
    public void setTitleTextColor(int c) { mTitleTextColor = c; mTitleColorSet = true; if (mTitleTextView != null) mTitleTextView.setTextColor(c); }
    public void setTitleTextColor(ColorStateList c) { setTitleTextColor(c.getDefaultColor()); }
    public void setSubtitleTextColor(int c) { mSubtitleTextColor = c; mSubtitleColorSet = true; if (mSubtitleTextView != null) mSubtitleTextView.setTextColor(c); }
    public void setSubtitleTextColor(ColorStateList c) { setSubtitleTextColor(c.getDefaultColor()); }
    public CharSequence getNavigationContentDescription() { return mNavButton != null ? mNavButton.getContentDescription() : null; }
    public void setNavigationContentDescription(int r) { setNavigationContentDescription(r != 0 ? getContext().getText(r) : null); }
    public void setNavigationContentDescription(CharSequence d) { ensureNavButton(); mNavButton.setContentDescription(d); }
    public void setNavigationIcon(int r) { setNavigationIcon(getContext().getDrawable(r)); }
    public void setNavigationIcon(Drawable d) {
        if (d != null) { ensureNavButton(); mNavButton.setVisibility(VISIBLE); }
        else if (mNavButton != null) mNavButton.setVisibility(GONE);
        if (mNavButton != null) mNavButton.setImageDrawable(d);
    }
    public Drawable getNavigationIcon() { return mNavButton != null ? mNavButton.getDrawable() : null; }
    public void setNavigationOnClickListener(OnClickListener l) { ensureNavButton(); mNavButton.setOnClickListener(l); }
    public View getNavigationViewHusk() { return mNavButton; }
    private ImageButton iconButton() {
        ImageButton b = new ImageButton(getContext(), null, 0);
        b.setScaleType(ImageView.ScaleType.CENTER);
        TypedValue v = new TypedValue();
        b.setBackground(null);
        try { if (getContext().getTheme().resolveAttribute(android.R.attr.selectableItemBackgroundBorderless, v, true) && v.resourceId != 0) b.setBackground(getContext().getDrawable(v.resourceId)); } catch (RuntimeException e) {}
        b.setMinimumWidth((int) (48 * mD)); b.setMinimumHeight((int) (48 * mD));
        b.setColorFilter(foreground(), android.graphics.PorterDuff.Mode.SRC_IN);
        return b;
    }
    private void ensureNavButton() { if (mNavButton == null) { mNavButton = iconButton(); mNavButton.clearColorFilter(); addSystemView(mNavButton); } }
    public Menu getMenu() { if (mMenu == null) mMenu = new husk.MenuImpl(getContext()); return mMenu; }
    public void setOverflowIcon(Drawable d) { mOverflowIcon = d; if (mOverflow != null) mOverflow.setImageDrawable(d); }
    public Drawable getOverflowIcon() { return mOverflowIcon; }
    public void inflateMenu(int r) { new MenuInflater(getContext()).inflate(r, getMenu()); huskMenuChanged(); }
    public void setOnMenuItemClickListener(OnMenuItemClickListener l) { mOnMenuItemClickListener = l; }
    public boolean showOverflowMenu() { if (mMenu == null) return false; showOverflow(); return true; }
    public boolean isOverflowMenuShowing() { return mOverflowPopup != null && mOverflowPopup.isShowing(); }
    public boolean hideOverflowMenu() { if (mOverflowPopup != null) mOverflowPopup.dismiss(); return true; }
    public void dismissPopupMenus() { hideOverflowMenu(); }
    public boolean hasExpandedActionView() { return false; }
    public void collapseActionView() {}
    public void setContentInsetsRelative(int s, int e) { mContentInsetStart = s; mContentInsetEnd = e; requestLayout(); }
    public int getContentInsetStart() { return mContentInsetStart; } public int getContentInsetEnd() { return mContentInsetEnd; }
    public void setContentInsetsAbsolute(int l, int r) { setContentInsetsRelative(l, r); }
    public int getContentInsetLeft() { return mContentInsetStart; } public int getContentInsetRight() { return mContentInsetEnd; }
    public int getContentInsetStartWithNavigation() { return mContentInsetStart; } public void setContentInsetStartWithNavigation(int i) {}
    public int getContentInsetEndWithActions() { return mContentInsetEnd; } public void setContentInsetEndWithActions(int i) {}
    public int getCurrentContentInsetStart() { return mContentInsetStart; } public int getCurrentContentInsetEnd() { return mContentInsetEnd; }
    public int getCurrentContentInsetLeft() { return mContentInsetStart; } public int getCurrentContentInsetRight() { return mContentInsetEnd; }
    private ListPopupWindow mOverflowPopup;
    private boolean onItem(MenuItem it) {
        if (mOnMenuItemClickListener != null && mOnMenuItemClickListener.onMenuItemClick(it)) return true;
        return false;
    }
    /** Husk: the menu's items changed; rebuild the action buttons and the overflow. */
    public void huskMenuChanged() {
        mMenuView.removeAllViews();
        mOverflow = null;
        if (mMenu == null) return;
        boolean overflow = false;
        int actions = 0, maxActions = Math.max(2, (int) (husk.Native.screenWidth() / mD / 120));
        for (int i = 0; i < mMenu.size(); i++) {
            final MenuItem it = mMenu.getItem(i);
            if (!it.isVisible()) continue;
            int show = ((husk.MenuImpl.Item) it).huskShowAs();
            boolean always = (show & MenuItem.SHOW_AS_ACTION_ALWAYS) != 0, ifRoom = (show & MenuItem.SHOW_AS_ACTION_IF_ROOM) != 0;
            if ((always || (ifRoom && actions < maxActions)) && (it.getActionView() != null || it.getIcon() != null || it.getTitle() != null)) {
                actions++;
                View v;
                if (it.getActionView() != null) { v = it.getActionView(); if (v.getParent() instanceof ViewGroup) ((ViewGroup) v.getParent()).removeView(v); }
                else if (it.getIcon() != null && (show & MenuItem.SHOW_AS_ACTION_WITH_TEXT) == 0) { ImageButton b = iconButton(); b.setImageDrawable(it.getIcon()); if (it.getIconTintList() != null) b.setImageTintList(it.getIconTintList()); else b.clearColorFilter(); b.setContentDescription(it.getTitle()); v = b; }
                else { Button b = new Button(getContext(), null, 0); b.setText(it.getTitle()); b.setBackground(null); b.setTextColor(foreground()); b.setAllCaps(true); b.setPadding((int) (12 * mD), 0, (int) (12 * mD), 0); b.setMinHeight((int) (48 * mD)); b.setMinimumHeight((int) (48 * mD)); b.setGravity(Gravity.CENTER); v = b; }
                v.setEnabled(it.isEnabled());
                if (it.getActionView() == null) v.setOnClickListener(x -> { if (!onItem(it)) ((husk.MenuImpl.Item) it).huskInvoke(); });
                mMenuView.addView(v, new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            } else overflow = true;
        }
        if (overflow) {
            mOverflow = iconButton();
            mOverflow.setImageDrawable(mOverflowIcon != null ? mOverflowIcon : new OverflowDrawable(foreground(), mD));
            mOverflow.clearColorFilter();
            mOverflow.setContentDescription("More options");
            mOverflow.setOnClickListener(v -> showOverflow());
            mMenuView.addView(mOverflow);
        }
        requestLayout();
    }
    private void showOverflow() {
        husk.MenuImpl rest = new husk.MenuImpl(getContext());
        final ArrayList<MenuItem> hidden = new ArrayList<>();
        int actions = 0, maxActions = Math.max(2, (int) (husk.Native.screenWidth() / mD / 120));
        for (int i = 0; i < mMenu.size(); i++) {
            MenuItem it = mMenu.getItem(i);
            if (!it.isVisible()) continue;
            int show = ((husk.MenuImpl.Item) it).huskShowAs();
            boolean always = (show & MenuItem.SHOW_AS_ACTION_ALWAYS) != 0, ifRoom = (show & MenuItem.SHOW_AS_ACTION_IF_ROOM) != 0;
            if (always || (ifRoom && actions < maxActions)) { actions++; continue; }
            hidden.add(it);
        }
        husk.MenuImpl view = husk.MenuImpl.viewOf(getContext(), hidden);
        Context pc = mPopupTheme != 0 ? new ContextThemeWrapper(getContext(), mPopupTheme) : getContext();
        mOverflowPopup = PopupMenu.showMenu(pc, mOverflow != null ? mOverflow : this, view, Gravity.END, this::onItem, null);
    }
    /** Three dots. */
    static final class OverflowDrawable extends Drawable {
        private final Paint p = new Paint(Paint.ANTI_ALIAS_FLAG); private final float d;
        OverflowDrawable(int color, float d) { p.setColor(color); this.d = d; }
        @Override public void draw(Canvas c) { android.graphics.Rect b = getBounds(); float cx = b.exactCenterX(), cy = b.exactCenterY(); for (int i = -1; i <= 1; i++) c.drawCircle(cx, cy + i * 6 * d, 2 * d, p); }
        @Override public int getIntrinsicWidth() { return (int) (24 * d); } @Override public int getIntrinsicHeight() { return (int) (24 * d); }
        @Override public void setAlpha(int a) { p.setAlpha(a); } @Override public void setColorFilter(android.graphics.ColorFilter f) { p.setColorFilter(f); } @Override public int getOpacity() { return android.graphics.PixelFormat.TRANSLUCENT; }
    }
    // ---- layout: [nav][logo][title/subtitle][app children...] ... [menu]
    @Override protected void onMeasure(int ws, int hs) {
        int width = 0, height = 0, state = 0;
        int avail = MeasureSpec.getSize(ws);
        int childHs = getChildMeasureSpec(hs, getPaddingTop() + getPaddingBottom(), ViewGroup.LayoutParams.WRAP_CONTENT);
        for (int i = 0; i < getChildCount(); i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE) continue;
            if (isSystem(c)) c.measure(MeasureSpec.makeMeasureSpec(avail, MeasureSpec.AT_MOST), childHs);
            else measureChildWithMargins(c, ws, 0, hs, 0);
            height = Math.max(height, c.getMeasuredHeight() + (c.getLayoutParams() instanceof MarginLayoutParams ? ((MarginLayoutParams) c.getLayoutParams()).topMargin + ((MarginLayoutParams) c.getLayoutParams()).bottomMargin : 0));
            state = combineMeasuredStates(state, c.getMeasuredState());
        }
        int titleH = (mTitleTextView != null && mTitleTextView.getVisibility() != GONE ? mTitleTextView.getMeasuredHeight() : 0) + (mSubtitleTextView != null && mSubtitleTextView.getVisibility() != GONE ? mSubtitleTextView.getMeasuredHeight() : 0);
        height = Math.max(Math.max(height, titleH) + getPaddingTop() + getPaddingBottom(), getSuggestedMinimumHeight());
        width = MeasureSpec.getMode(ws) == MeasureSpec.UNSPECIFIED ? avail : avail;
        setMeasuredDimension(resolveSizeAndState(width, ws, state), resolveSizeAndState(height, hs, state << MEASURED_HEIGHT_STATE_SHIFT));
    }
    @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {
        int w = r - l, h = b - t, pt = getPaddingTop(), pb = getPaddingBottom();
        int left = getPaddingLeft(), right = w - getPaddingRight();
        if (mNavButton != null && mNavButton.getVisibility() != GONE) { left = placeV(mNavButton, left + (int) (4 * mD), h, pt, pb) ; }
        int menuW = mMenuView.getMeasuredWidth();
        if (mMenuView.getChildCount() > 0) { int x = right - menuW - (int) (4 * mD); placeAt(mMenuView, x, h, pt, pb); right = x; }
        else mMenuView.layout(right, 0, right, 0);
        left = Math.max(left, getPaddingLeft() + mContentInsetStart);
        if (mNavButton != null && mNavButton.getVisibility() != GONE) left = Math.max(left, mNavButton.getRight() + (int) (16 * mD));
        if (mLogoView != null && mLogoView.getVisibility() != GONE) left = placeV(mLogoView, left, h, pt, pb) + (int) (8 * mD);
        left += mTitleMargin;
        boolean hasTitle = mTitleTextView != null && mTitleTextView.getVisibility() != GONE, hasSub = mSubtitleTextView != null && mSubtitleTextView.getVisibility() != GONE;
        if (hasTitle || hasSub) {
            int th = (hasTitle ? mTitleTextView.getMeasuredHeight() : 0) + (hasSub ? mSubtitleTextView.getMeasuredHeight() : 0);
            int y = pt + (h - pt - pb - th) / 2, tw = 0;
            if (hasTitle) { int mw = Math.min(mTitleTextView.getMeasuredWidth(), Math.max(0, right - left)); if (mw != mTitleTextView.getMeasuredWidth()) mTitleTextView.measure(MeasureSpec.makeMeasureSpec(mw, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(mTitleTextView.getMeasuredHeight(), MeasureSpec.EXACTLY)); mTitleTextView.layout(left, y, left + mw, y + mTitleTextView.getMeasuredHeight()); y += mTitleTextView.getMeasuredHeight(); tw = mw; }
            if (hasSub) { int mw = Math.min(mSubtitleTextView.getMeasuredWidth(), Math.max(0, right - left)); if (mw != mSubtitleTextView.getMeasuredWidth()) mSubtitleTextView.measure(MeasureSpec.makeMeasureSpec(mw, MeasureSpec.EXACTLY), MeasureSpec.makeMeasureSpec(mSubtitleTextView.getMeasuredHeight(), MeasureSpec.EXACTLY)); mSubtitleTextView.layout(left, y, left + mw, y + mSubtitleTextView.getMeasuredHeight()); tw = Math.max(tw, mw); }
            left += tw + (int) (16 * mD);
        }
        // the app's own children: by their gravity in the space that is left
        for (int i = 0; i < getChildCount(); i++) {
            View c = getChildAt(i);
            if (c.getVisibility() == GONE || isSystem(c)) continue;
            LayoutParams lp = c.getLayoutParams() instanceof LayoutParams ? (LayoutParams) c.getLayoutParams() : null;
            int cw = c.getMeasuredWidth(), ch = c.getMeasuredHeight();
            int g = lp != null ? lp.gravity : Gravity.NO_GRAVITY;
            int hg = Gravity.getAbsoluteGravity(g, getLayoutDirection()) & Gravity.HORIZONTAL_GRAVITY_MASK, vg = g & Gravity.VERTICAL_GRAVITY_MASK;
            int ml = lp != null ? lp.leftMargin : 0, mr = lp != null ? lp.rightMargin : 0, mt = lp != null ? lp.topMargin : 0, mb = lp != null ? lp.bottomMargin : 0;
            int x;
            if (hg == Gravity.CENTER_HORIZONTAL) x = Math.max(left, (w - cw) / 2);
            else if (hg == Gravity.RIGHT) { x = right - mr - cw; right = x - ml; }
            else { x = left + ml; left = x + cw + mr; }
            int y = vg == Gravity.TOP ? pt + mt : vg == Gravity.BOTTOM ? h - pb - mb - ch : pt + (h - pt - pb - ch) / 2;
            c.layout(x, y, x + Math.min(cw, Math.max(0, w - x)), y + ch);
        }
    }
    private int placeV(View v, int x, int h, int pt, int pb) { int cw = v.getMeasuredWidth(), ch = v.getMeasuredHeight(), y = pt + (h - pt - pb - ch) / 2; v.layout(x, y, x + cw, y + ch); return x + cw; }
    private void placeAt(View v, int x, int h, int pt, int pb) { placeV(v, x, h, pt, pb); }
    @Override protected boolean checkLayoutParams(ViewGroup.LayoutParams p) { return p instanceof LayoutParams; }
    @Override public LayoutParams generateLayoutParams(AttributeSet a) { return new LayoutParams(getContext(), a); }
    @Override protected LayoutParams generateLayoutParams(ViewGroup.LayoutParams p) { if (p instanceof LayoutParams) return new LayoutParams((LayoutParams) p); if (p instanceof MarginLayoutParams) return new LayoutParams((MarginLayoutParams) p); return new LayoutParams(p); }
    @Override protected LayoutParams generateDefaultLayoutParams() { return new LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT); }
    @Override public boolean onTouchEvent(MotionEvent e) { return true; }
    @Override public CharSequence getAccessibilityClassName() { return Toolbar.class.getName(); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public boolean canShowOverflowMenu() { return false; }
    public java.lang.CharSequence getCollapseContentDescription() { return (java.lang.CharSequence) huskFill.get("CollapseContentDescription"); }
    public android.graphics.drawable.Drawable getCollapseIcon() { return (android.graphics.drawable.Drawable) huskFill.get("CollapseIcon"); }
    public android.view.View getNavigationView() { return null; }
    public boolean isOverflowMenuShowPending() { return false; }
    public boolean isTitleTruncated() { return false; }
    public void setCollapseContentDescription(int p0) { huskFill.put("CollapseContentDescription", Integer.valueOf(p0)); }
    public void setCollapseContentDescription(java.lang.CharSequence p0) { huskFill.put("CollapseContentDescription", p0); }
    public void setCollapseIcon(int p0) { huskFill.put("CollapseIcon", Integer.valueOf(p0)); }
    public void setCollapseIcon(android.graphics.drawable.Drawable p0) { huskFill.put("CollapseIcon", p0); }
    public void setCollapsible(boolean p0) {}
    // ---- end of generated members
}
