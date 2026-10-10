package android.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;

public class ViewSwitcher extends ViewAnimator {
    public interface ViewFactory { View makeView(); }
    private ViewFactory mFactory;
    public ViewSwitcher(Context c) { super(c); }
    public ViewSwitcher(Context c, AttributeSet a) { super(c, a); }
    @Override public void addView(View child, int index, ViewGroup.LayoutParams params) { if (getChildCount() >= 2) throw new IllegalStateException("Can't add more than 2 views to a ViewSwitcher"); super.addView(child, index, params); }
    public View getNextView() { int which = mWhichChild == 0 ? 1 : 0; return getChildAt(which); }
    private View obtainView() { View child = mFactory.makeView(); FrameLayout.LayoutParams lp = (FrameLayout.LayoutParams) child.getLayoutParams(); if (lp == null) lp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT); addView(child, lp); return child; }
    public void setFactory(ViewFactory f) { mFactory = f; obtainView(); obtainView(); }
    public void reset() { mFirstTimeReset(); }
    private void mFirstTimeReset() { View v = getChildAt(0); if (v != null) v.setVisibility(View.GONE); v = getChildAt(1); if (v != null) v.setVisibility(View.GONE); }
    @Override public CharSequence getAccessibilityClassName() { return ViewSwitcher.class.getName(); }
}
