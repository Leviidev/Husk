package android.widget;
public class LinearLayout extends android.view.ViewGroup {
    public static final int HORIZONTAL = 0, VERTICAL = 1;
    public static class LayoutParams extends android.view.ViewGroup.MarginLayoutParams {
        public float weight; public int gravity = -1;
        public LayoutParams(int w, int h) { super(w, h); } public LayoutParams(int w, int h, float weight) { super(w, h); this.weight = weight; }
    }
    public LinearLayout(android.content.Context c) { super(c); }
    public void setOrientation(int o) {} public void setGravity(int g) {}
}
