package android.view;

public interface WindowManager extends ViewManager {
    Display getDefaultDisplay();
    WindowMetrics getCurrentWindowMetrics();
    WindowMetrics getMaximumWindowMetrics();
    class LayoutParams extends ViewGroup.LayoutParams {
        public static final int FLAG_FULLSCREEN = 0x400, FLAG_KEEP_SCREEN_ON = 0x80, FLAG_LAYOUT_NO_LIMITS = 0x200, FLAG_FORCE_NOT_FULLSCREEN = 0x800,
            FLAG_LAYOUT_IN_SCREEN = 0x100, FLAG_TRANSLUCENT_STATUS = 0x4000000, FLAG_TRANSLUCENT_NAVIGATION = 0x8000000,
            FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = 0x80000000, FLAG_NOT_FOCUSABLE = 8, FLAG_HARDWARE_ACCELERATED = 0x1000000;
        public static final int LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT = 0, LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES = 1, LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER = 2,
            LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS = 3, SOFT_INPUT_ADJUST_PAN = 0x20, SOFT_INPUT_ADJUST_RESIZE = 0x10, SOFT_INPUT_STATE_HIDDEN = 2;
        public int flags, layoutInDisplayCutoutMode, softInputMode, type, format;
        public float screenBrightness = -1f;
        public LayoutParams() { super(-1, -1); }
    }
}
