package android.view;
public class SoundEffectConstants {
    public static final int CLICK = 0, NAVIGATION_LEFT = 1, NAVIGATION_UP = 2, NAVIGATION_RIGHT = 3, NAVIGATION_DOWN = 4;
    public static int getContantForFocusDirection(int d) { return CLICK; }
    public static int getConstantForFocusDirection(int d, boolean repeat) { return CLICK; }
}
