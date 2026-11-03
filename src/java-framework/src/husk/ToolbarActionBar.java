// SPDX-License-Identifier: GPL-2.0-or-later
package husk;

import android.app.ActionBar;
import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.Toolbar;
import java.util.ArrayList;

/** Activity.setActionBar(toolbar)'s action bar, as AOSP's ToolbarActionBar: the title, subtitle, logo and up button are the toolbar's. */
public final class ToolbarActionBar extends ActionBar {
    private final Activity mActivity;
    private final Toolbar mToolbar;
    private int mOptions = DISPLAY_SHOW_TITLE | DISPLAY_SHOW_HOME;
    private View mCustom;
    private Drawable mUp;
    private final ArrayList<OnMenuVisibilityListener> mMenuListeners = new ArrayList<>();

    public ToolbarActionBar(Activity a, Toolbar t) {
        mActivity = a; mToolbar = t;
        if (t.getTitle() == null || t.getTitle().length() == 0) t.setTitle(a.getTitle());
    }
    public Toolbar toolbar() { return mToolbar; }

    private void apply() {
        boolean up = (mOptions & DISPLAY_HOME_AS_UP) != 0;
        if (up) {
            if (mToolbar.getNavigationIcon() == null) {
                Drawable d = mUp;
                if (d == null) try { d = mActivity.getDrawable(android.R.drawable.ic_menu_revert); } catch (RuntimeException e) {}
                mToolbar.setNavigationIcon(d);
            }
            mToolbar.setNavigationOnClickListener(v -> mActivity.onNavigateUp());
        } else mToolbar.setNavigationIcon((Drawable) null);
        if ((mOptions & DISPLAY_SHOW_CUSTOM) != 0 && mCustom != null && mCustom.getParent() == null) mToolbar.addView(mCustom);
        else if ((mOptions & DISPLAY_SHOW_CUSTOM) == 0 && mCustom != null && mCustom.getParent() == mToolbar) mToolbar.removeView(mCustom);
    }

    @Override public void setCustomView(View v) { if (mCustom != null && mCustom.getParent() == mToolbar) mToolbar.removeView(mCustom); mCustom = v; apply(); }
    @Override public void setCustomView(View v, LayoutParams p) { if (v != null) v.setLayoutParams(p); setCustomView(v); }
    @Override public void setCustomView(int r) { setCustomView(android.view.LayoutInflater.from(mToolbar.getContext()).inflate(r, mToolbar, false)); }
    @Override public void setIcon(int r) { setIcon(r != 0 ? mActivity.getDrawable(r) : null); }
    @Override public void setIcon(Drawable d) { mToolbar.setLogo(d); }
    @Override public void setLogo(int r) { setIcon(r); }
    @Override public void setLogo(Drawable d) { setIcon(d); }
    @Override public void setDisplayOptions(int o) { mOptions = o; apply(); }
    @Override public void setDisplayOptions(int o, int mask) { mOptions = (mOptions & ~mask) | (o & mask); apply(); }
    private void opt(int bit, boolean on) { setDisplayOptions(on ? bit : 0, bit); }
    @Override public void setDisplayUseLogoEnabled(boolean e) { opt(DISPLAY_USE_LOGO, e); }
    @Override public void setDisplayShowHomeEnabled(boolean e) { opt(DISPLAY_SHOW_HOME, e); }
    @Override public void setDisplayHomeAsUpEnabled(boolean e) { opt(DISPLAY_HOME_AS_UP, e); }
    @Override public void setDisplayShowTitleEnabled(boolean e) { opt(DISPLAY_SHOW_TITLE, e); if (!e) mToolbar.setTitle(null); }
    @Override public void setDisplayShowCustomEnabled(boolean e) { opt(DISPLAY_SHOW_CUSTOM, e); }
    @Override public void setBackgroundDrawable(Drawable d) { mToolbar.setBackground(d); }
    @Override public View getCustomView() { return mCustom; }
    @Override public CharSequence getTitle() { return mToolbar.getTitle(); }
    @Override public CharSequence getSubtitle() { return mToolbar.getSubtitle(); }
    @Override public void setTitle(CharSequence t) { mToolbar.setTitle(t); }
    @Override public void setTitle(int r) { mToolbar.setTitle(r != 0 ? mActivity.getText(r) : null); }
    @Override public void setSubtitle(CharSequence s) { mToolbar.setSubtitle(s); }
    @Override public void setSubtitle(int r) { mToolbar.setSubtitle(r != 0 ? mActivity.getText(r) : null); }
    @Override public int getDisplayOptions() { return mOptions; }
    @Override public int getHeight() { return mToolbar.getHeight(); }
    @Override public void show() { mToolbar.setVisibility(View.VISIBLE); }
    @Override public void hide() { mToolbar.setVisibility(View.GONE); }
    @Override public boolean isShowing() { return mToolbar.getVisibility() == View.VISIBLE; }
    @Override public void addOnMenuVisibilityListener(OnMenuVisibilityListener l) { mMenuListeners.add(l); }
    @Override public void removeOnMenuVisibilityListener(OnMenuVisibilityListener l) { mMenuListeners.remove(l); }
    @Override public void setHomeAsUpIndicator(Drawable d) { mUp = d; if ((mOptions & DISPLAY_HOME_AS_UP) != 0) mToolbar.setNavigationIcon(d); }
    @Override public void setHomeAsUpIndicator(int r) { setHomeAsUpIndicator(r != 0 ? mActivity.getDrawable(r) : null); }
}
