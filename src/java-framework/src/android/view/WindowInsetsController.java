package android.view;
public interface WindowInsetsController {
    int BEHAVIOR_SHOW_BARS_BY_TOUCH = 0, BEHAVIOR_SHOW_BARS_BY_SWIPE = 1, BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE = 2, BEHAVIOR_DEFAULT = 1;
    int APPEARANCE_OPAQUE_STATUS_BARS = 1, APPEARANCE_OPAQUE_NAVIGATION_BARS = 2, APPEARANCE_LOW_PROFILE_BARS = 4, APPEARANCE_LIGHT_STATUS_BARS = 8,
        APPEARANCE_LIGHT_NAVIGATION_BARS = 16;
    interface OnControllableInsetsChangedListener { void onControllableInsetsChanged(WindowInsetsController c, int types); }
    void show(int types);
    void hide(int types);
    void setSystemBarsAppearance(int appearance, int mask);
    int getSystemBarsAppearance();
    void setSystemBarsBehavior(int behavior);
    int getSystemBarsBehavior();
    default void addOnControllableInsetsChangedListener(OnControllableInsetsChangedListener l) {}
    default void removeOnControllableInsetsChangedListener(OnControllableInsetsChangedListener l) {}
    default void controlWindowInsetsAnimation(int types, long durationMs, android.view.animation.Interpolator i, android.os.CancellationSignal c, Object listener) {}
}
