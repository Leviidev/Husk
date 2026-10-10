package android.window;
public interface SplashScreen {
    interface OnExitAnimationListener { void onSplashScreenExit(SplashScreenView view); }
    void setOnExitAnimationListener(OnExitAnimationListener l);
    void clearOnExitAnimationListener();
    default void setSplashScreenTheme(int theme) {}
}
