package android.window;
public interface OnBackAnimationCallback extends OnBackInvokedCallback {
    default void onBackStarted(BackEvent e) {} default void onBackProgressed(BackEvent e) {} default void onBackCancelled() {}
}
