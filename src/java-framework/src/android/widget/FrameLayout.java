package android.widget;

import android.content.Context;
import android.view.ViewGroup;

public class FrameLayout extends ViewGroup {
    public static class LayoutParams extends ViewGroup.MarginLayoutParams {
        public int gravity = -1;
        public LayoutParams(int w, int h) { super(w, h); }
        public LayoutParams(int w, int h, int gravity) { super(w, h); this.gravity = gravity; }
        public LayoutParams(ViewGroup.LayoutParams o) { super(o); }
    }
    public FrameLayout(Context c) { super(c); }
    public FrameLayout(Context c, android.util.AttributeSet a) { super(c); }
}
