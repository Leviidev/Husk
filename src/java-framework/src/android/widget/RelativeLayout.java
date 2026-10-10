package android.widget;
public class RelativeLayout extends android.view.ViewGroup {
    public static final int ALIGN_PARENT_BOTTOM = 12, ALIGN_PARENT_TOP = 10, CENTER_HORIZONTAL = 14, CENTER_IN_PARENT = 13;
    public static class LayoutParams extends android.view.ViewGroup.MarginLayoutParams {
        public LayoutParams(int w, int h) { super(w, h); }
        public void addRule(int verb) {} public void addRule(int verb, int anchor) {}
    }
    public RelativeLayout(android.content.Context c) { super(c); }
}
