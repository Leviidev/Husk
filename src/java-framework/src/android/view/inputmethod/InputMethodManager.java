package android.view.inputmethod;
public final class InputMethodManager {
    public static final int SHOW_IMPLICIT = 1, SHOW_FORCED = 2, HIDE_IMPLICIT_ONLY = 1, HIDE_NOT_ALWAYS = 2, RESULT_SHOWN = 2;
    public boolean showSoftInput(android.view.View v, int flags) { return false; }
    public boolean showSoftInput(android.view.View v, int flags, Object result) { return false; }
    public boolean hideSoftInputFromWindow(Object token, int flags) { return false; }
    public void toggleSoftInput(int show, int hide) {}
    public boolean isActive() { return false; }
    public boolean isActive(android.view.View v) { return false; }
    public boolean isAcceptingText() { return false; }
    public void restartInput(android.view.View v) {}
}
