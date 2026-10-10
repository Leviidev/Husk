package husk;

import android.app.ActionBar;
import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;

/** The platform action bar a plain Activity gets from its theme: a title bar (husk.DecorView's), its menu shown as actions when there is room. */
public final class SimpleActionBar extends ActionBar {
    private final Activity mActivity;
    private final DecorView mDecor;
    private CharSequence mTitle, mSubtitle;
    private int mOptions = DISPLAY_SHOW_TITLE | DISPLAY_SHOW_HOME;
    private View mCustom;
    private boolean mShowing = true;
    public SimpleActionBar(Activity a, DecorView d) { mActivity = a; mDecor = d; mTitle = a.getTitle(); }
    public void setCustomView(View v) { mCustom = v; } public void setCustomView(View v, LayoutParams p) { mCustom = v; } public void setCustomView(int r) { mCustom = android.view.LayoutInflater.from(mActivity).inflate(r, null); }
    public void setIcon(int r) {} public void setIcon(Drawable d) {} public void setLogo(int r) {} public void setLogo(Drawable d) {}
    public void setDisplayOptions(int o) { mOptions = o; } public void setDisplayOptions(int o, int mask) { mOptions = (mOptions & ~mask) | (o & mask); }
    public void setDisplayUseLogoEnabled(boolean e) {} public void setDisplayShowHomeEnabled(boolean e) {} public void setDisplayHomeAsUpEnabled(boolean e) {}
    public void setDisplayShowTitleEnabled(boolean e) { mDecor.setTitle(e ? mTitle : ""); } public void setDisplayShowCustomEnabled(boolean e) {}
    public void setBackgroundDrawable(Drawable d) { View v = mDecor.actionBarView(); if (v != null) v.setBackground(d); }
    public View getCustomView() { return mCustom; }
    public CharSequence getTitle() { return mTitle; } public CharSequence getSubtitle() { return mSubtitle; }
    public void setTitle(CharSequence t) { mTitle = t; mDecor.setTitle(t); } public void setTitle(int r) { setTitle(mActivity.getText(r)); }
    public void setSubtitle(CharSequence s) { mSubtitle = s; } public void setSubtitle(int r) { mSubtitle = mActivity.getText(r); }
    public int getDisplayOptions() { return mOptions; }
    public int getHeight() { View v = mDecor.actionBarView(); return v == null ? 0 : v.getHeight(); }
    public void show() { mShowing = true; mDecor.showActionBar(true); } public void hide() { mShowing = false; mDecor.showActionBar(false); }
    public boolean isShowing() { return mShowing; }
    public void addOnMenuVisibilityListener(OnMenuVisibilityListener l) {} public void removeOnMenuVisibilityListener(OnMenuVisibilityListener l) {}
    public android.content.Context getThemedContext() { return mActivity; }
}
