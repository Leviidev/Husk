package android.view;

import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import java.util.ArrayList;

/**
 * What is drawn over a view after the view itself: drawables, and for a ViewGroup's overlay, views (an animation that moves a
 * view out of its parent). Its views live in a group of its own the size of the host, attached to the host's window.
 */
public class ViewOverlay {
    final View mHost;
    final OverlayViewGroup mGroup;
    private final ArrayList<Drawable> mDrawables = new ArrayList<>();

    ViewOverlay(android.content.Context context, View host) {
        mHost = host;
        mGroup = new OverlayViewGroup(context, host);
    }
    public void add(Drawable drawable) {
        if (drawable == null) throw new IllegalArgumentException("drawable must be non-null");
        if (!mDrawables.contains(drawable)) { mDrawables.add(drawable); drawable.setCallback(mHost); mHost.invalidate(); }
    }
    public void remove(Drawable drawable) {
        if (drawable == null) throw new IllegalArgumentException("drawable must be non-null");
        if (mDrawables.remove(drawable)) { drawable.setCallback(null); mHost.invalidate(); }
    }
    public void clear() {
        for (Drawable d : mDrawables) d.setCallback(null);
        mDrawables.clear();
        mGroup.removeAllViews();
        mHost.invalidate();
    }
    boolean isEmpty() { return mDrawables.isEmpty() && mGroup.getChildCount() == 0; }
    boolean huskHas(Drawable d) { return mDrawables.contains(d); }

    /** Husk: drawn by the host once it has drawn itself. */
    void huskDraw(Canvas c) {
        for (Drawable d : new ArrayList<>(mDrawables)) d.draw(c);
        if (mGroup.getChildCount() > 0) {
            if (mGroup.getWidth() != mHost.getWidth() || mGroup.getHeight() != mHost.getHeight()) mGroup.layout(0, 0, mHost.getWidth(), mHost.getHeight());
            for (int i = 0; i < mGroup.getChildCount(); i++) mGroup.getChildAt(i).drawFromParent(c, mGroup);
        }
    }
    void huskAttached(husk.ViewRoot root) { if (root != null) mGroup.dispatchAttachedToWindow(root, View.VISIBLE); }
    void huskDetached() { mGroup.dispatchDetachedFromWindow(); }

    /** The overlay's views: laid out where they were put, drawn by the host, invalidating it. */
    static class OverlayViewGroup extends ViewGroup {
        final View mHostView;
        OverlayViewGroup(android.content.Context context, View host) { super(context); mHostView = host; }
        @Override protected void onLayout(boolean changed, int l, int t, int r, int b) {}
        @Override public void invalidate() { super.invalidate(); if (mHostView != null) mHostView.invalidate(); }
        @Override public void requestLayout() { if (mHostView != null) mHostView.invalidate(); }
    }
}
