package android.view;

import java.util.ArrayList;

public class ViewGroup extends View implements ViewParent {
    public static class LayoutParams {
        public static final int MATCH_PARENT = -1, FILL_PARENT = -1, WRAP_CONTENT = -2;
        public int width, height;
        public LayoutParams(int w, int h) { width = w; height = h; }
        public LayoutParams(LayoutParams o) { width = o.width; height = o.height; }
        public LayoutParams(android.content.Context c, android.util.AttributeSet a) { this(-1, -1); }
    }
    public static class MarginLayoutParams extends LayoutParams {
        public int leftMargin, topMargin, rightMargin, bottomMargin;
        public MarginLayoutParams(int w, int h) { super(w, h); }
        public MarginLayoutParams(LayoutParams o) { super(o); }
        public void setMargins(int l, int t, int r, int b) { leftMargin = l; topMargin = t; rightMargin = r; bottomMargin = b; }
    }
    private final ArrayList<View> children = new ArrayList<>();
    public ViewGroup(android.content.Context c) { super(c); }
    public ViewGroup(android.content.Context c, android.util.AttributeSet a) { super(c); }
    public void addView(View v) { addView(v, -1, v.getLayoutParams() != null ? v.getLayoutParams() : new LayoutParams(-1, -1)); }
    public void addView(View v, int index) { addView(v, index, v.getLayoutParams() != null ? v.getLayoutParams() : new LayoutParams(-1, -1)); }
    public void addView(View v, LayoutParams p) { addView(v, -1, p); }
    public void addView(View v, int w, int h) { addView(v, -1, new LayoutParams(w, h)); }
    public void addView(View v, int index, LayoutParams p) {
        v.setLayoutParams(p);
        v.mParent = this;
        if (index < 0 || index > children.size()) children.add(v); else children.add(index, v);
        if (isAttachedToWindow()) v.huskAttach(getWidth(), getHeight());
    }
    public void removeView(View v) { children.remove(v); v.mParent = null; }
    public void removeViewAt(int i) { View v = children.remove(i); v.mParent = null; }
    public void removeAllViews() { for (View v : children) v.mParent = null; children.clear(); }
    public int getChildCount() { return children.size(); }
    public View getChildAt(int i) { return i >= 0 && i < children.size() ? children.get(i) : null; }
    public int indexOfChild(View v) { return children.indexOf(v); }
    @Override public void huskAttach(int w, int h) { super.huskAttach(w, h); for (View v : new ArrayList<>(children)) v.huskAttach(w, h); }
    @Override public View findViewById(int id) {
        if (id == getId()) return this;
        for (View v : children) { View f = v.findViewById(id); if (f != null) return f; }
        return null;
    }
    @Override public boolean dispatchTouchEvent(MotionEvent e) {
        if (onInterceptTouchEvent(e)) return super.dispatchTouchEvent(e);
        for (int i = children.size() - 1; i >= 0; i--) if (children.get(i).getVisibility() == VISIBLE && children.get(i).dispatchTouchEvent(e)) return true;
        return super.dispatchTouchEvent(e);
    }
    @Override public boolean dispatchKeyEvent(KeyEvent e) {
        for (int i = children.size() - 1; i >= 0; i--) if (children.get(i).dispatchKeyEvent(e)) return true;
        return super.dispatchKeyEvent(e);
    }
    @Override public boolean dispatchGenericMotionEvent(MotionEvent e) {
        for (int i = children.size() - 1; i >= 0; i--) if (children.get(i).dispatchGenericMotionEvent(e)) return true;
        return super.dispatchGenericMotionEvent(e);
    }
    public boolean onInterceptTouchEvent(MotionEvent e) { return false; }
    public void requestDisallowInterceptTouchEvent(boolean b) {}
    public void setDescendantFocusability(int f) {}
    public void setClipChildren(boolean b) {}
}
