package android.view;

import android.graphics.Rect;

public interface ViewParent {
    void requestLayout();
    boolean isLayoutRequested();
    void requestTransparentRegion(View child);
    void invalidateChild(View child, Rect r);
    ViewParent invalidateChildInParent(int[] location, Rect r);
    ViewParent getParent();
    void requestChildFocus(View child, View focused);
    void recomputeViewAttributes(View child);
    void clearChildFocus(View child);
    boolean getChildVisibleRect(View child, Rect r, android.graphics.Point offset);
    View focusSearch(View v, int dir);
    void bringChildToFront(View child);
    void focusableViewAvailable(View v);
    boolean showContextMenuForChild(View original);
    void createContextMenu(ContextMenu menu);
    ActionMode startActionModeForChild(View original, ActionMode.Callback cb);
    void childDrawableStateChanged(View child);
    void requestDisallowInterceptTouchEvent(boolean disallow);
    boolean requestChildRectangleOnScreen(View child, Rect r, boolean immediate);
    boolean requestSendAccessibilityEvent(View child, android.view.accessibility.AccessibilityEvent e);
    void childHasTransientStateChanged(View child, boolean has);
    void requestFitSystemWindows();
    ViewParent getParentForAccessibility();
    void notifySubtreeAccessibilityStateChanged(View child, View source, int type);
    boolean canResolveLayoutDirection();
    boolean isLayoutDirectionResolved();
    int getLayoutDirection();
    boolean canResolveTextDirection();
    boolean isTextDirectionResolved();
    int getTextDirection();
    boolean canResolveTextAlignment();
    boolean isTextAlignmentResolved();
    int getTextAlignment();
    boolean onStartNestedScroll(View child, View target, int axes);
    void onNestedScrollAccepted(View child, View target, int axes);
    void onStopNestedScroll(View target);
    void onNestedScroll(View target, int dxc, int dyc, int dxu, int dyu);
    void onNestedPreScroll(View target, int dx, int dy, int[] consumed);
    boolean onNestedFling(View target, float vx, float vy, boolean consumed);
    boolean onNestedPreFling(View target, float vx, float vy);
    boolean onNestedPrePerformAccessibilityAction(View target, int action, android.os.Bundle args);
    default void onDescendantInvalidated(View child, View target) {}
    default void keyboardNavigationClusterSearch(View current, int dir) {}
    default ActionMode startActionModeForChild(View original, ActionMode.Callback cb, int type) { return null; }
}
