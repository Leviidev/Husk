package android.app;

import android.graphics.drawable.Drawable;
import android.view.View;

public abstract class ActionBar {
    public static final int NAVIGATION_MODE_STANDARD = 0, NAVIGATION_MODE_LIST = 1, NAVIGATION_MODE_TABS = 2, DISPLAY_USE_LOGO = 1, DISPLAY_SHOW_HOME = 2,
        DISPLAY_HOME_AS_UP = 4, DISPLAY_SHOW_TITLE = 8, DISPLAY_SHOW_CUSTOM = 16;
    public interface OnMenuVisibilityListener { void onMenuVisibilityChanged(boolean v); }
    public interface OnNavigationListener { boolean onNavigationItemSelected(int pos, long id); }
    public static class LayoutParams extends android.view.ViewGroup.MarginLayoutParams {
        public int gravity;
        public LayoutParams(android.content.Context c, android.util.AttributeSet a) { super(c, a); android.content.res.TypedArray t = c.obtainStyledAttributes(a, husk.S.FrameLayout_Layout); gravity = t.getInt(husk.S.FrameLayout_Layout_layout_gravity, android.view.Gravity.NO_GRAVITY); t.recycle(); }
        public LayoutParams(android.view.ViewGroup.MarginLayoutParams o) { super(o); }
        public LayoutParams(int w, int h) { super(w, h); } public LayoutParams(int w, int h, int g) { super(w, h); gravity = g; }
        public LayoutParams(int g) { this(WRAP_CONTENT, MATCH_PARENT, g); } public LayoutParams(LayoutParams o) { super(o); gravity = o.gravity; }
        public LayoutParams(android.view.ViewGroup.LayoutParams o) { super(o); }
    }
    public abstract static class Tab {
        public abstract int getPosition(); public abstract Drawable getIcon(); public abstract CharSequence getText(); public abstract Tab setIcon(Drawable d);
        public abstract Tab setIcon(int r); public abstract Tab setText(CharSequence t); public abstract Tab setText(int r); public abstract Tab setCustomView(View v);
        public abstract Tab setCustomView(int r); public abstract View getCustomView(); public abstract Tab setTag(Object t); public abstract Object getTag();
        public abstract Tab setTabListener(TabListener l); public abstract void select(); public abstract Tab setContentDescription(int r);
        public abstract Tab setContentDescription(CharSequence c); public abstract CharSequence getContentDescription();
    }
    public interface TabListener { void onTabSelected(Tab t, FragmentTransaction ft); void onTabUnselected(Tab t, FragmentTransaction ft); void onTabReselected(Tab t, FragmentTransaction ft); }
    public abstract void setCustomView(View v);
    public abstract void setCustomView(View v, LayoutParams p);
    public abstract void setCustomView(int r);
    public abstract void setIcon(int r);
    public abstract void setIcon(Drawable d);
    public abstract void setLogo(int r);
    public abstract void setLogo(Drawable d);
    public abstract void setDisplayOptions(int o);
    public abstract void setDisplayOptions(int o, int mask);
    public abstract void setDisplayUseLogoEnabled(boolean e);
    public abstract void setDisplayShowHomeEnabled(boolean e);
    public abstract void setDisplayHomeAsUpEnabled(boolean e);
    public abstract void setDisplayShowTitleEnabled(boolean e);
    public abstract void setDisplayShowCustomEnabled(boolean e);
    public abstract void setBackgroundDrawable(Drawable d);
    public abstract View getCustomView();
    public abstract CharSequence getTitle();
    public abstract CharSequence getSubtitle();
    public abstract void setTitle(CharSequence t);
    public abstract void setTitle(int r);
    public abstract void setSubtitle(CharSequence s);
    public abstract void setSubtitle(int r);
    public abstract int getDisplayOptions();
    public abstract int getHeight();
    public abstract void show();
    public abstract void hide();
    public abstract boolean isShowing();
    public abstract void addOnMenuVisibilityListener(OnMenuVisibilityListener l);
    public abstract void removeOnMenuVisibilityListener(OnMenuVisibilityListener l);
    public void setNavigationMode(int m) {} public int getNavigationMode() { return NAVIGATION_MODE_STANDARD; }
    public void setListNavigationCallbacks(android.widget.SpinnerAdapter a, OnNavigationListener l) {}
    public void setSelectedNavigationItem(int p) {} public int getSelectedNavigationIndex() { return -1; } public int getNavigationItemCount() { return 0; }
    public Tab newTab() { return null; } public void addTab(Tab t) {} public void addTab(Tab t, boolean s) {} public void addTab(Tab t, int p) {} public void addTab(Tab t, int p, boolean s) {}
    public void removeTab(Tab t) {} public void removeTabAt(int p) {} public void removeAllTabs() {} public void selectTab(Tab t) {} public Tab getSelectedTab() { return null; }
    public Tab getTabAt(int i) { return null; } public int getTabCount() { return 0; }
    public void setStackedBackgroundDrawable(Drawable d) {} public void setSplitBackgroundDrawable(Drawable d) {}
    public void setHomeButtonEnabled(boolean e) {} public android.content.Context getThemedContext() { return null; }
    public boolean isTitleTruncated() { return false; } public void setHomeAsUpIndicator(Drawable d) {} public void setHomeAsUpIndicator(int r) {}
    public void setHomeActionContentDescription(CharSequence c) {} public void setHomeActionContentDescription(int r) {}
    public void setHideOnContentScrollEnabled(boolean e) {} public boolean isHideOnContentScrollEnabled() { return false; }
    public int getHideOffset() { return 0; } public void setHideOffset(int o) {} public void setElevation(float e) {} public float getElevation() { return 0; }
    public boolean onMenuKeyEvent(android.view.KeyEvent e) { return false; } public boolean collapseActionView() { return false; } public void invalidateOptionsMenu() {}
    public void setShowHideAnimationEnabled(boolean e) {}
}
