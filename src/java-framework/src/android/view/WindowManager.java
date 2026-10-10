package android.view;

import android.os.Parcelable;

public interface WindowManager extends ViewManager {
    class BadTokenException extends RuntimeException { public BadTokenException() {} public BadTokenException(String s) { super(s); } }
    class InvalidDisplayException extends RuntimeException { public InvalidDisplayException() {} public InvalidDisplayException(String s) { super(s); } }
    Display getDefaultDisplay();
    void removeViewImmediate(View v);
    default WindowMetrics getCurrentWindowMetrics() { return new WindowMetrics(new android.graphics.Rect(0, 0, husk.Native.screenWidth(), husk.Native.screenHeight()), husk.ViewRoot.screenInsets()); }
    default WindowMetrics getMaximumWindowMetrics() { return getCurrentWindowMetrics(); }
    class LayoutParams extends ViewGroup.LayoutParams implements Parcelable {
        public static final int FIRST_APPLICATION_WINDOW = 1, TYPE_BASE_APPLICATION = 1, TYPE_APPLICATION = 2, TYPE_APPLICATION_STARTING = 3, TYPE_DRAWN_APPLICATION = 4,
            LAST_APPLICATION_WINDOW = 99, FIRST_SUB_WINDOW = 1000, TYPE_APPLICATION_PANEL = 1000, TYPE_APPLICATION_MEDIA = 1001, TYPE_APPLICATION_SUB_PANEL = 1002,
            TYPE_APPLICATION_ATTACHED_DIALOG = 1003, LAST_SUB_WINDOW = 1999, FIRST_SYSTEM_WINDOW = 2000, TYPE_STATUS_BAR = 2000, TYPE_TOAST = 2005,
            TYPE_SYSTEM_ALERT = 2003, TYPE_PHONE = 2002, TYPE_SYSTEM_OVERLAY = 2006, TYPE_APPLICATION_OVERLAY = 2038, LAST_SYSTEM_WINDOW = 2999;
        public static final int FLAG_ALLOW_LOCK_WHILE_SCREEN_ON = 1, FLAG_DIM_BEHIND = 2, FLAG_BLUR_BEHIND = 4, FLAG_NOT_FOCUSABLE = 8, FLAG_NOT_TOUCHABLE = 16,
            FLAG_NOT_TOUCH_MODAL = 32, FLAG_TOUCHABLE_WHEN_WAKING = 64, FLAG_KEEP_SCREEN_ON = 128, FLAG_LAYOUT_IN_SCREEN = 256, FLAG_LAYOUT_NO_LIMITS = 512,
            FLAG_FULLSCREEN = 1024, FLAG_FORCE_NOT_FULLSCREEN = 2048, FLAG_DITHER = 4096, FLAG_SECURE = 8192, FLAG_SCALED = 16384,
            FLAG_IGNORE_CHEEK_PRESSES = 32768, FLAG_LAYOUT_INSET_DECOR = 65536, FLAG_ALT_FOCUSABLE_IM = 131072, FLAG_WATCH_OUTSIDE_TOUCH = 262144,
            FLAG_SHOW_WHEN_LOCKED = 524288, FLAG_SHOW_WALLPAPER = 1048576, FLAG_TURN_SCREEN_ON = 2097152, FLAG_DISMISS_KEYGUARD = 4194304,
            FLAG_SPLIT_TOUCH = 8388608, FLAG_HARDWARE_ACCELERATED = 16777216, FLAG_LAYOUT_IN_OVERSCAN = 33554432, FLAG_TRANSLUCENT_STATUS = 67108864,
            FLAG_TRANSLUCENT_NAVIGATION = 134217728, FLAG_LOCAL_FOCUS_MODE = 268435456, FLAG_LAYOUT_ATTACHED_IN_DECOR = 1073741824,
            FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS = 0x80000000;
        public static final int SOFT_INPUT_MASK_STATE = 15, SOFT_INPUT_STATE_UNSPECIFIED = 0, SOFT_INPUT_STATE_UNCHANGED = 1, SOFT_INPUT_STATE_HIDDEN = 2,
            SOFT_INPUT_STATE_ALWAYS_HIDDEN = 3, SOFT_INPUT_STATE_VISIBLE = 4, SOFT_INPUT_STATE_ALWAYS_VISIBLE = 5, SOFT_INPUT_MASK_ADJUST = 240,
            SOFT_INPUT_ADJUST_UNSPECIFIED = 0, SOFT_INPUT_ADJUST_RESIZE = 16, SOFT_INPUT_ADJUST_PAN = 32, SOFT_INPUT_ADJUST_NOTHING = 48, SOFT_INPUT_IS_FORWARD_NAVIGATION = 256;
        public static final int LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT = 0, LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES = 1, LAYOUT_IN_DISPLAY_CUTOUT_MODE_NEVER = 2,
            LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS = 3, ROTATION_ANIMATION_ROTATE = 0, ROTATION_ANIMATION_CROSSFADE = 1, ROTATION_ANIMATION_JUMPCUT = 2,
            ROTATION_ANIMATION_SEAMLESS = 3;
        public static final float BRIGHTNESS_OVERRIDE_NONE = -1f, BRIGHTNESS_OVERRIDE_OFF = 0f, BRIGHTNESS_OVERRIDE_FULL = 1f;
        public static final int TITLE_CHANGED = 64, FLAGS_CHANGED = 4;
        public int x, y, type = TYPE_APPLICATION, flags, privateFlags, softInputMode, gravity, format, windowAnimations, layoutInDisplayCutoutMode,
            screenOrientation = -1, systemUiVisibility, rotationAnimation, preferredDisplayModeId, memoryType;
        public float horizontalWeight, verticalWeight, horizontalMargin, verticalMargin, alpha = 1f, dimAmount = 1f, screenBrightness = -1f, buttonBrightness = -1f, preferredRefreshRate;
        public android.os.IBinder token;
        public String packageName;
        private CharSequence mTitle = "";
        private boolean mFitInsets = true;
        public LayoutParams() { super(MATCH_PARENT, MATCH_PARENT); }
        public LayoutParams(int type) { this(); this.type = type; }
        public LayoutParams(int type, int flags) { this(); this.type = type; this.flags = flags; }
        public LayoutParams(int type, int flags, int format) { this(type, flags); this.format = format; }
        public LayoutParams(int w, int h, int type, int flags, int format) { super(w, h); this.type = type; this.flags = flags; this.format = format; }
        public LayoutParams(int w, int h, int x, int y, int type, int flags, int format) { this(w, h, type, flags, format); this.x = x; this.y = y; }
        public LayoutParams(android.os.Parcel in) { this(); }
        public final void setTitle(CharSequence t) { mTitle = t == null ? "" : t; }
        public final CharSequence getTitle() { return mTitle; }
        public final int copyFrom(LayoutParams o) {
            width = o.width; height = o.height; x = o.x; y = o.y; type = o.type; flags = o.flags; softInputMode = o.softInputMode; gravity = o.gravity;
            format = o.format; windowAnimations = o.windowAnimations; alpha = o.alpha; dimAmount = o.dimAmount; screenBrightness = o.screenBrightness;
            layoutInDisplayCutoutMode = o.layoutInDisplayCutoutMode; screenOrientation = o.screenOrientation; systemUiVisibility = o.systemUiVisibility;
            horizontalMargin = o.horizontalMargin; verticalMargin = o.verticalMargin; token = o.token; mTitle = o.mTitle;
            return 0xffffffff;
        }
        public void setFitInsetsTypes(int t) { mFitInsets = t != 0; }
        public void setFitInsetsSides(int s) {}
        public void setFitInsetsIgnoringVisibility(boolean b) {}
        public int getFitInsetsTypes() { return mFitInsets ? WindowInsets.Type.systemBars() : 0; }
        public boolean isFitInsetsIgnoringVisibility() { return false; }
        public void setBlurBehindRadius(int r) {}
        public void setColorMode(int m) {}
        public int describeContents() { return 0; }
        public static boolean mayUseInputMethod(int flags) { return true; }
        @Override public String debug(String p) { return p + "WM.LayoutParams{(" + x + "," + y + ")(" + sizeToString(width) + "x" + sizeToString(height) + ") gr=" + gravity + " ty=" + type + " fl=#" + Integer.toHexString(flags) + "}"; }
    }
}
