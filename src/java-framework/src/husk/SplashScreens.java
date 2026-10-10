package husk;

import android.window.SplashScreen;

/** No splash screen is shown: the exit listener is told at once, with an empty view, so apps waiting on it carry on. */
public final class SplashScreens {
    private SplashScreens() {}
    public static SplashScreen of(android.app.Activity a) {
        return new SplashScreen() {
            public void setOnExitAnimationListener(OnExitAnimationListener l) {
                ViewRoot.mainHandler().post(() -> {
                    android.window.SplashScreenView v = new android.window.SplashScreenView(a);
                    l.onSplashScreenExit(v);
                });
            }
            public void clearOnExitAnimationListener() {}
        };
    }
}
