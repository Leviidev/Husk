package android.view;

/** A ViewGroup's overlay: views too, taken from wherever they are and kept where they showed on screen. */
public class ViewGroupOverlay extends ViewOverlay {
    ViewGroupOverlay(android.content.Context context, View host) { super(context, host); }
    public void add(View view) {
        if (view == null) throw new IllegalArgumentException("view must be non-null");
        ViewParent p = view.getParent();
        if (p instanceof ViewGroup && p != mGroup) {
            ViewGroup parent = (ViewGroup) p;
            int[] hostLoc = new int[2], parentLoc = new int[2];
            mHost.getLocationOnScreen(hostLoc);
            parent.getLocationOnScreen(parentLoc);
            int dx = parentLoc[0] - hostLoc[0], dy = parentLoc[1] - hostLoc[1];
            parent.removeView(view);
            if (dx != 0 || dy != 0) { view.offsetLeftAndRight(dx); view.offsetTopAndBottom(dy); }
        }
        mGroup.addView(view);
        if (view.getWidth() == 0 && view.getHeight() == 0 && (view.getMeasuredWidth() > 0 || view.getMeasuredHeight() > 0))
            view.layout(view.getLeft(), view.getTop(), view.getLeft() + view.getMeasuredWidth(), view.getTop() + view.getMeasuredHeight());
        mHost.invalidate();
    }
    public void remove(View view) {
        if (view == null) throw new IllegalArgumentException("view must be non-null");
        mGroup.removeView(view);
        mHost.invalidate();
    }
}
