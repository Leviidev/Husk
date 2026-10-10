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
        // ---- generated by tools/compat/fillmembers.py (LayoutParams): the platform's members this class does not write (signatures only)
        private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
        public static final int ACCESSIBILITY_ANCHOR_CHANGED = 16777216;
        public static final int ACCESSIBILITY_TITLE_CHANGED = 33554432;
        public static final int ALPHA_CHANGED = 128;
        public static final int ANIMATION_CHANGED = 16;
        public static final int BLUR_BEHIND_RADIUS_CHANGED = 536870912;
        public static final int BUTTON_BRIGHTNESS_CHANGED = 8192;
        public static final int COLOR_MODE_CHANGED = 67108864;
        public static android.os.Parcelable.Creator CREATOR;
        public static final int DIM_AMOUNT_CHANGED = 32;
        public static final int DISPLAY_FLAGS_CHANGED = 4194304;
        public static final int DISPLAY_FLAG_DISABLE_HDR_CONVERSION = 1;
        public static final int FLAG_SLIPPERY = 536870912;
        public static final int FORMAT_CHANGED = 8;
        public static final int INPUT_FEATURES_CHANGED = 65536;
        public static final int INPUT_FEATURE_CAPTURE_KEYBOARD = 32;
        public static final int INPUT_FEATURE_DISABLE_USER_ACTIVITY = 2;
        public static final int INPUT_FEATURE_DISPLAY_TOPOLOGY_AWARE = 16;
        public static final int INPUT_FEATURE_NO_INPUT_CHANNEL = 1;
        public static final int INPUT_FEATURE_SENSITIVE_FOR_PRIVACY = 8;
        public static final int INPUT_FEATURE_SPY = 4;
        public static final int INSET_FLAGS_CHANGED = 134217728;
        public static final int INVALID_WINDOW_TYPE = -1;
        public static final int LAYOUT_CHANGED = 1;
        public static final int MEMORY_TYPE_CHANGED = 256;
        public static final int MEMORY_TYPE_GPU = 2;
        public static final int MEMORY_TYPE_HARDWARE = 1;
        public static final int MEMORY_TYPE_NORMAL = 0;
        public static final int MEMORY_TYPE_PUSH_BUFFERS = 3;
        public static final int MINIMAL_POST_PROCESSING_PREFERENCE_CHANGED = 268435456;
        public static final int PREFERRED_DISPLAY_MODE_ID = 8388608;
        public static final int PREFERRED_MAX_DISPLAY_REFRESH_RATE = -2147483648;
        public static final int PREFERRED_MIN_DISPLAY_REFRESH_RATE = 1073741824;
        public static final int PREFERRED_REFRESH_RATE_CHANGED = 2097152;
        public static final int PRIVATE_FLAGS_CHANGED = 131072;
        public static final int PRIVATE_FLAG_ALLOW_ACTION_KEY_EVENTS = 8388608;
        public static final int PRIVATE_FLAG_APP_PROGRESS_GENERATION_ALLOWED = 128;
        public static final int PRIVATE_FLAG_COLOR_SPACE_AGNOSTIC = 16777216;
        public static final int PRIVATE_FLAG_CONSUME_IME_INSETS = 33554432;
        public static final int PRIVATE_FLAG_DISABLE_WALLPAPER_TOUCH_EVENTS = 1024;
        public static final int PRIVATE_FLAG_EDGE_TO_EDGE_ENFORCED = 2048;
        public static final int PRIVATE_FLAG_EXCLUDE_FROM_SCREEN_MAGNIFICATION = 2097152;
        public static final int PRIVATE_FLAG_FIT_INSETS_CONTROLLED = 268435456;
        public static final int PRIVATE_FLAG_FORCE_DECOR_VIEW_VISIBILITY = 8192;
        public static final int PRIVATE_FLAG_FORCE_DRAW_BAR_BACKGROUNDS = 32768;
        public static final int PRIVATE_FLAG_FORCE_HARDWARE_ACCELERATED = 2;
        public static final int PRIVATE_FLAG_IMMERSIVE_CONFIRMATION_WINDOW = 131072;
        public static final int PRIVATE_FLAG_INPUT_METHOD_WINDOW = 134217728;
        public static final int PRIVATE_FLAG_INSET_PARENT_FRAME_BY_IME = 1073741824;
        public static final int PRIVATE_FLAG_INTERCEPT_GLOBAL_DRAG_AND_DROP = -2147483648;
        public static final int PRIVATE_FLAG_IS_ROUNDED_CORNERS_OVERLAY = 1048576;
        public static final int PRIVATE_FLAG_LAYOUT_CHILD_WINDOW_IN_PARENT_FRAME = 16384;
        public static final int PRIVATE_FLAG_LAYOUT_SIZE_EXTENDED_BY_CUTOUT = 4096;
        public static final int PRIVATE_FLAG_NOT_MAGNIFIABLE = 4194304;
        public static final int PRIVATE_FLAG_NO_MOVE_ANIMATION = 64;
        public static final int PRIVATE_FLAG_OPTIMIZE_MEASURE = 512;
        public static final int PRIVATE_FLAG_OPT_OUT_EDGE_TO_EDGE = 67108864;
        public static final int PRIVATE_FLAG_OVERRIDE_LAYOUT_IN_DISPLAY_CUTOUT_MODE = 262144;
        public static final int PRIVATE_FLAG_SUSTAINED_PERFORMANCE_MODE = 65536;
        public static final int PRIVATE_FLAG_SYSTEM_APPLICATION_OVERLAY = 8;
        public static final int PRIVATE_FLAG_SYSTEM_ERROR = 256;
        public static final int PRIVATE_FLAG_TRUSTED_OVERLAY = 536870912;
        public static final int PRIVATE_FLAG_UNRESTRICTED_GESTURE_EXCLUSION = 32;
        public static final int PRIVATE_FLAG_WANTS_OFFSET_NOTIFICATIONS = 4;
        public static final int ROTATION_ANIMATION_CHANGED = 4096;
        public static final int ROTATION_ANIMATION_UNSPECIFIED = -1;
        public static final int SCREEN_BRIGHTNESS_CHANGED = 2048;
        public static final int SCREEN_ORIENTATION_CHANGED = 1024;
        public static final int SOFT_INPUT_MODE_CHANGED = 512;
        public static final int SURFACE_INSETS_CHANGED = 1048576;
        public static final int SYSTEM_FLAG_HIDE_NON_SYSTEM_OVERLAY_WINDOWS = 524288;
        public static final int SYSTEM_FLAG_SHOW_FOR_ALL_USERS = 16;
        public static final int SYSTEM_UI_LISTENER_CHANGED = 32768;
        public static final int SYSTEM_UI_VISIBILITY_CHANGED = 16384;
        public static final int TRANSLUCENT_FLAGS_CHANGED = 524288;
        public static final int TYPE_ACCESSIBILITY_MAGNIFICATION_OVERLAY = 2039;
        public static final int TYPE_ACCESSIBILITY_OVERLAY = 2032;
        public static final int TYPE_APPLICATION_ABOVE_SUB_PANEL = 1005;
        public static final int TYPE_APPLICATION_MEDIA_OVERLAY = 1004;
        public static final int TYPE_BOOT_PROGRESS = 2021;
        public static final int TYPE_CHANGED = 2;
        public static final int TYPE_DISPLAY_OVERLAY = 2026;
        public static final int TYPE_DOCK_DIVIDER = 2034;
        public static final int TYPE_DRAG = 2016;
        public static final int TYPE_INPUT_CONSUMER = 2022;
        public static final int TYPE_INPUT_METHOD = 2011;
        public static final int TYPE_INPUT_METHOD_DIALOG = 2012;
        public static final int TYPE_KEYGUARD = 2004;
        public static final int TYPE_KEYGUARD_DIALOG = 2009;
        public static final int TYPE_MAGNIFICATION_OVERLAY = 2027;
        public static final int TYPE_NAVIGATION_BAR = 2019;
        public static final int TYPE_NAVIGATION_BAR_PANEL = 2024;
        public static final int TYPE_NOTIFICATION_SHADE = 2040;
        public static final int TYPE_POINTER = 2018;
        public static final int TYPE_PRESENTATION = 2037;
        public static final int TYPE_PRIORITY_PHONE = 2007;
        public static final int TYPE_PRIVATE_PRESENTATION = 2030;
        public static final int TYPE_QS_DIALOG = 2035;
        public static final int TYPE_SCREENSHOT = 2036;
        public static final int TYPE_SEARCH_BAR = 2001;
        public static final int TYPE_SECURE_SYSTEM_OVERLAY = 2015;
        public static final int TYPE_STATUS_BAR_ADDITIONAL = 2041;
        public static final int TYPE_STATUS_BAR_PANEL = 2014;
        public static final int TYPE_STATUS_BAR_SUB_PANEL = 2017;
        public static final int TYPE_SYSTEM_DIALOG = 2008;
        public static final int TYPE_SYSTEM_ERROR = 2010;
        public static final int TYPE_VOICE_INTERACTION = 2031;
        public static final int TYPE_VOICE_INTERACTION_STARTING = 2033;
        public static final int TYPE_VOLUME_OVERLAY = 2020;
        public static final int TYPE_WALLPAPER = 2013;
        public static final int USER_ACTIVITY_TIMEOUT_CHANGED = 262144;
        public long accessibilityIdOfAnchor;
        public java.lang.CharSequence accessibilityTitle;
        public int forciblyShownTypes;
        public boolean hasManualSurfaceInsets;
        public boolean hasSystemUiListeners;
        public long hideTimeoutMilliseconds;
        public int inputFeatures;
        public android.os.IBinder mWindowContextToken;
        public android.view.WindowManager.LayoutParams[] paramsForRotation;
        public boolean preferMinimalPostProcessing;
        public float preferredMaxDisplayRefreshRate;
        public float preferredMinDisplayRefreshRate;
        public boolean preservePreviousSurfaceInsets;
        public boolean receiveInsetsIgnoringZOrder;
        public int subtreeSystemUiVisibility;
        public android.graphics.Rect surfaceInsets;
        public long userActivityTimeout;
        public static boolean isSubWindowType(int p0) { return false; }
        public static boolean isSystemAlertWindowType(int p0) { return false; }
        public static java.lang.String layoutInDisplayCutoutModeToString(int p0) { return null; }
        public boolean areWallpaperTouchEventsEnabled() { return false; }
        public boolean canPlayMoveAnimation() { return false; }
        public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1) {}
        public void dumpDimensions(java.lang.StringBuilder p0) {}
        protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
        public android.view.WindowManager.LayoutParams forRotation(int p0) { return this; }
        public int getBlurBehindRadius() { return 0; }
        public int getColorMode() { return 0; }
        public float getDesiredHdrHeadroom() { return (huskFill.get("DesiredHdrHeadroom") instanceof Float ? (Float) huskFill.get("DesiredHdrHeadroom") : 0f); }
        public int getFitInsetsSides() { return 0; }
        public boolean getFrameRateBoostOnTouchEnabled() { return (huskFill.get("FrameRateBoostOnTouchEnabled") instanceof Boolean ? (Boolean) huskFill.get("FrameRateBoostOnTouchEnabled") : false); }
        public long getUserActivityTimeout() { return (huskFill.get("UserActivityTimeout") instanceof Long ? (Long) huskFill.get("UserActivityTimeout") : 0L); }
        public android.os.IBinder getWindowContextToken() { return (android.os.IBinder) huskFill.get("WindowContextToken"); }
        public boolean hasKeyboardCapture() { return false; }
        public boolean isFrameRatePowerSavingsBalanced() { return (huskFill.get("FrameRatePowerSavingsBalanced") instanceof Boolean ? (Boolean) huskFill.get("FrameRatePowerSavingsBalanced") : false); }
        public boolean isFullscreen() { return false; }
        public boolean isHdrConversionEnabled() { return (huskFill.get("HdrConversionEnabled") instanceof Boolean ? (Boolean) huskFill.get("HdrConversionEnabled") : false); }
        public boolean isModal() { return false; }
        public boolean isSystemApplicationOverlay() { return (huskFill.get("SystemApplicationOverlay") instanceof Boolean ? (Boolean) huskFill.get("SystemApplicationOverlay") : false); }
        public void scale(float p0) {}
        public void setCanPlayMoveAnimation(boolean p0) {}
        public void setDesiredHdrHeadroom(float p0) { huskFill.put("DesiredHdrHeadroom", Float.valueOf(p0)); }
        public void setFrameRateBoostOnTouchEnabled(boolean p0) { huskFill.put("FrameRateBoostOnTouchEnabled", Boolean.valueOf(p0)); }
        public void setFrameRatePowerSavingsBalanced(boolean p0) { huskFill.put("FrameRatePowerSavingsBalanced", Boolean.valueOf(p0)); }
        public void setHdrConversionEnabled(boolean p0) { huskFill.put("HdrConversionEnabled", Boolean.valueOf(p0)); }
        public void setInsetsParams(java.util.List p0) {}
        public void setKeyboardCaptureEnabled(boolean p0) {}
        public void setSurfaceInsets(android.view.View p0, boolean p1, boolean p2) {}
        public void setSystemApplicationOverlay(boolean p0) { huskFill.put("SystemApplicationOverlay", Boolean.valueOf(p0)); }
        public void setTrustedOverlay() {}
        public void setUserActivityTimeout(long p0) { huskFill.put("UserActivityTimeout", Long.valueOf(p0)); }
        public void setWallpaperTouchEventsEnabled(boolean p0) {}
        public void setWindowContextToken(android.os.IBinder p0) { huskFill.put("WindowContextToken", p0); }
        public java.lang.String toString(java.lang.String p0) { return null; }
        // ---- end of generated members (LayoutParams)
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    boolean ACTIVITY_EMBEDDING_GUARD_WITH_ANDROID_15 = false;
    int COMPAT_SMALL_COVER_SCREEN_OPT_IN = 1;
    int DISPLAY_IME_POLICY_FALLBACK_DISPLAY = 1;
    int DISPLAY_IME_POLICY_HIDE = 2;
    int DISPLAY_IME_POLICY_LOCAL = 0;
    int DOCKED_BOTTOM = 4;
    int DOCKED_INVALID = -1;
    int DOCKED_LEFT = 1;
    int DOCKED_RIGHT = 3;
    int DOCKED_TOP = 2;
    long ENABLE_ACTIVITY_EMBEDDING_FOR_ANDROID_15 = 306666082L;
    boolean HAS_WINDOW_EXTENSIONS_ON_DEVICE = false;
    java.lang.String INPUT_CONSUMER_NAVIGATION = "nav_input_consumer";
    java.lang.String INPUT_CONSUMER_PIP = "pip_input_consumer";
    java.lang.String INPUT_CONSUMER_RECENTS_ANIMATION = "recents_animation_input_consumer";
    java.lang.String INPUT_CONSUMER_WALLPAPER = "wallpaper_input_consumer";
    int KEYGUARD_VISIBILITY_TRANSIT_FLAGS = 47360;
    int LARGE_SCREEN_SMALLEST_SCREEN_WIDTH_DP = 600;
    java.lang.String PARCEL_KEY_SHORTCUTS_ARRAY = "shortcuts_array";
    java.lang.String PROPERTY_ACTIVITY_EMBEDDING_ALLOW_SYSTEM_OVERRIDE = "android.window.PROPERTY_ACTIVITY_EMBEDDING_ALLOW_SYSTEM_OVERRIDE";
    java.lang.String PROPERTY_ACTIVITY_EMBEDDING_SPLITS_ENABLED = "android.window.PROPERTY_ACTIVITY_EMBEDDING_SPLITS_ENABLED";
    java.lang.String PROPERTY_ALLOW_UNTRUSTED_ACTIVITY_EMBEDDING_STATE_SHARING = "android.window.PROPERTY_ALLOW_UNTRUSTED_ACTIVITY_EMBEDDING_STATE_SHARING";
    java.lang.String PROPERTY_CAMERA_COMPAT_ALLOW_FORCE_ROTATION = "android.window.PROPERTY_CAMERA_COMPAT_ALLOW_FORCE_ROTATION";
    java.lang.String PROPERTY_CAMERA_COMPAT_ALLOW_REFRESH = "android.window.PROPERTY_CAMERA_COMPAT_ALLOW_REFRESH";
    java.lang.String PROPERTY_CAMERA_COMPAT_ALLOW_SIMULATE_REQUESTED_ORIENTATION = "android.window.PROPERTY_CAMERA_COMPAT_ALLOW_SIMULATE_REQUESTED_ORIENTATION";
    java.lang.String PROPERTY_CAMERA_COMPAT_ENABLE_REFRESH_VIA_PAUSE = "android.window.PROPERTY_CAMERA_COMPAT_ENABLE_REFRESH_VIA_PAUSE";
    java.lang.String PROPERTY_COMPAT_ALLOW_DISPLAY_ORIENTATION_OVERRIDE = "android.window.PROPERTY_COMPAT_ALLOW_DISPLAY_ORIENTATION_OVERRIDE";
    java.lang.String PROPERTY_COMPAT_ALLOW_IGNORING_ORIENTATION_REQUEST_WHEN_LOOP_DETECTED = "android.window.PROPERTY_COMPAT_ALLOW_IGNORING_ORIENTATION_REQUEST_WHEN_LOOP_DETECTED";
    java.lang.String PROPERTY_COMPAT_ALLOW_MIN_ASPECT_RATIO_OVERRIDE = "android.window.PROPERTY_COMPAT_ALLOW_MIN_ASPECT_RATIO_OVERRIDE";
    java.lang.String PROPERTY_COMPAT_ALLOW_ORIENTATION_OVERRIDE = "android.window.PROPERTY_COMPAT_ALLOW_ORIENTATION_OVERRIDE";
    java.lang.String PROPERTY_COMPAT_ALLOW_RESIZEABLE_ACTIVITY_OVERRIDES = "android.window.PROPERTY_COMPAT_ALLOW_RESIZEABLE_ACTIVITY_OVERRIDES";
    java.lang.String PROPERTY_COMPAT_ALLOW_RESTRICTED_RESIZABILITY = "android.window.PROPERTY_COMPAT_ALLOW_RESTRICTED_RESIZABILITY";
    java.lang.String PROPERTY_COMPAT_ALLOW_SAFE_REGION_LETTERBOXING = "android.window.PROPERTY_COMPAT_ALLOW_SAFE_REGION_LETTERBOXING";
    java.lang.String PROPERTY_COMPAT_ALLOW_SANDBOXING_VIEW_BOUNDS_APIS = "android.window.PROPERTY_COMPAT_ALLOW_SANDBOXING_VIEW_BOUNDS_APIS";
    java.lang.String PROPERTY_COMPAT_ALLOW_SMALL_COVER_SCREEN = "android.window.PROPERTY_COMPAT_ALLOW_SMALL_COVER_SCREEN";
    java.lang.String PROPERTY_COMPAT_ALLOW_USER_ASPECT_RATIO_FULLSCREEN_OVERRIDE = "android.window.PROPERTY_COMPAT_ALLOW_USER_ASPECT_RATIO_FULLSCREEN_OVERRIDE";
    java.lang.String PROPERTY_COMPAT_ALLOW_USER_ASPECT_RATIO_OVERRIDE = "android.window.PROPERTY_COMPAT_ALLOW_USER_ASPECT_RATIO_OVERRIDE";
    java.lang.String PROPERTY_COMPAT_ENABLE_FAKE_FOCUS = "android.window.PROPERTY_COMPAT_ENABLE_FAKE_FOCUS";
    java.lang.String PROPERTY_COMPAT_IGNORE_REQUESTED_ORIENTATION = "android.window.PROPERTY_COMPAT_IGNORE_REQUESTED_ORIENTATION";
    java.lang.String PROPERTY_SUPPORTS_MULTI_INSTANCE_SYSTEM_UI = "android.window.PROPERTY_SUPPORTS_MULTI_INSTANCE_SYSTEM_UI";
    int REMOVE_CONTENT_MODE_DESTROY = 2;
    int REMOVE_CONTENT_MODE_MOVE_TO_PRIMARY = 1;
    int REMOVE_CONTENT_MODE_UNDEFINED = 0;
    int SCREEN_RECORDING_STATE_NOT_VISIBLE = 0;
    int SCREEN_RECORDING_STATE_VISIBLE = 1;
    int SHELL_ROOT_LAYER_DIVIDER = 0;
    int SHELL_ROOT_LAYER_PIP = 1;
    int TAKE_SCREENSHOT_FULLSCREEN = 1;
    int TAKE_SCREENSHOT_PROVIDED_IMAGE = 3;
    int TAKE_SCREENSHOT_SELECTED_REGION = 2;
    int TRANSIT_CHANGE = 6;
    int TRANSIT_CLOSE = 2;
    int TRANSIT_CLOSE_PREPARE_BACK_NAVIGATION = 14;
    int TRANSIT_FIRST_CUSTOM = 1000;
    int TRANSIT_FLAG_AOD_APPEARING = 32768;
    int TRANSIT_FLAG_APP_CRASHED = 16;
    int TRANSIT_FLAG_AVOID_MOVE_TO_FRONT = 65536;
    int TRANSIT_FLAG_DISPLAY_LEVEL_TRANSITION = 131072;
    int TRANSIT_FLAG_INVISIBLE = 1024;
    int TRANSIT_FLAG_IS_RECENTS = 128;
    int TRANSIT_FLAG_KEYGUARD_APPEARING = 2048;
    int TRANSIT_FLAG_KEYGUARD_GOING_AWAY = 256;
    int TRANSIT_FLAG_KEYGUARD_GOING_AWAY_NO_ANIMATION = 2;
    int TRANSIT_FLAG_KEYGUARD_GOING_AWAY_SUBTLE_ANIMATION = 8;
    int TRANSIT_FLAG_KEYGUARD_GOING_AWAY_TO_LAUNCHER_CLEAR_SNAPSHOT = 512;
    int TRANSIT_FLAG_KEYGUARD_GOING_AWAY_TO_SHADE = 1;
    int TRANSIT_FLAG_KEYGUARD_GOING_AWAY_WITH_WALLPAPER = 4;
    int TRANSIT_FLAG_KEYGUARD_LOCKED = 64;
    int TRANSIT_FLAG_KEYGUARD_OCCLUDING = 4096;
    int TRANSIT_FLAG_KEYGUARD_UNOCCLUDING = 8192;
    int TRANSIT_FLAG_OPEN_BEHIND = 32;
    int TRANSIT_FLAG_PHYSICAL_DISPLAY_SWITCH = 16384;
    int TRANSIT_KEYGUARD_GOING_AWAY = 7;
    int TRANSIT_KEYGUARD_OCCLUDE = 8;
    int TRANSIT_KEYGUARD_UNOCCLUDE = 9;
    int TRANSIT_NONE = 0;
    int TRANSIT_OLD_ACTIVITY_CLOSE = 7;
    int TRANSIT_OLD_ACTIVITY_OPEN = 6;
    int TRANSIT_OLD_KEYGUARD_GOING_AWAY = 20;
    int TRANSIT_OLD_KEYGUARD_GOING_AWAY_ON_WALLPAPER = 21;
    int TRANSIT_OLD_KEYGUARD_OCCLUDE = 22;
    int TRANSIT_OLD_KEYGUARD_OCCLUDE_BY_DREAM = 33;
    int TRANSIT_OLD_KEYGUARD_UNOCCLUDE = 23;
    int TRANSIT_OLD_NONE = 0;
    int TRANSIT_OLD_TASK_FRAGMENT_CHANGE = 30;
    int TRANSIT_OLD_TASK_FRAGMENT_CLOSE = 29;
    int TRANSIT_OLD_TASK_FRAGMENT_OPEN = 28;
    int TRANSIT_OLD_TASK_OPEN = 8;
    int TRANSIT_OLD_TASK_TO_BACK = 11;
    int TRANSIT_OLD_TASK_TO_FRONT = 10;
    int TRANSIT_OLD_UNSET = -1;
    int TRANSIT_OLD_WALLPAPER_CLOSE = 12;
    int TRANSIT_OLD_WALLPAPER_OPEN = 13;
    int TRANSIT_OPEN = 1;
    int TRANSIT_PIP = 10;
    int TRANSIT_PREPARE_BACK_NAVIGATION = 13;
    int TRANSIT_RELAUNCH = 5;
    int TRANSIT_SLEEP = 12;
    int TRANSIT_START_LOCK_TASK_MODE = 15;
    int TRANSIT_TO_BACK = 4;
    int TRANSIT_TO_FRONT = 3;
    int TRANSIT_WAKE = 11;
    static float fixScale(float p0) { return 0f; }
    static boolean hasWindowExtensionsEnabled() { return false; }
    static java.lang.String transitTypeToString(int p0) { return null; }
    default void addCrossWindowBlurEnabledListener(java.util.concurrent.Executor p0, java.util.function.Consumer p1) {}
    default void addCrossWindowBlurEnabledListener(java.util.function.Consumer p0) {}
    default void addProposedRotationListener(java.util.concurrent.Executor p0, java.util.function.IntConsumer p1) {}
    default int addScreenRecordingCallback(java.util.concurrent.Executor p0, java.util.function.Consumer p1) { return 0; }
    default android.view.WindowManager createLocalWindowManager(android.view.Window p0) { return this; }
    default android.view.KeyboardShortcutGroup getApplicationLaunchKeyboardShortcuts(int p0) { return null; }
    default android.graphics.Region getCurrentImeTouchRegion() { return null; }
    default android.os.IBinder getDefaultToken() { return null; }
    default int getDisplayImePolicy(int p0) { return 0; }
    default java.util.Set getPossibleMaximumWindowMetrics(int p0) { return new java.util.HashSet(); }
    default android.os.IBinder getSurfaceControlInputClientToken(android.view.SurfaceControl p0) { return null; }
    default void holdLock(android.os.IBinder p0, int p1) {}
    default boolean isCrossWindowBlurEnabled() { return false; }
    default boolean isEligibleForDesktopMode(int p0) { return false; }
    default boolean isGlobalKey(int p0) { return false; }
    default boolean isTaskSnapshotSupported() { return false; }
    default java.util.List notifyScreenshotListeners(int p0) { return new java.util.ArrayList(); }
    default android.window.InputTransferToken registerBatchedSurfaceControlInputReceiver(android.window.InputTransferToken p0, android.view.SurfaceControl p1, android.view.Choreographer p2, android.view.SurfaceControlInputReceiver p3) { return null; }
    default void registerTrustedPresentationListener(android.os.IBinder p0, android.window.TrustedPresentationThresholds p1, java.util.concurrent.Executor p2, java.util.function.Consumer p3) {}
    default android.window.InputTransferToken registerUnbatchedSurfaceControlInputReceiver(android.window.InputTransferToken p0, android.view.SurfaceControl p1, android.os.Looper p2, android.view.SurfaceControlInputReceiver p3) { return null; }
    default void removeCrossWindowBlurEnabledListener(java.util.function.Consumer p0) {}
    default void removeProposedRotationListener(java.util.function.IntConsumer p0) {}
    default void removeScreenRecordingCallback(java.util.function.Consumer p0) {}
    default boolean replaceContentOnDisplayWithMirror(int p0, android.view.Window p1) { return false; }
    default boolean replaceContentOnDisplayWithSc(int p0, android.view.SurfaceControl p1) { return false; }
    default void setDisplayImePolicy(int p0, int p1) {}
    default void setParentWindow(android.view.Window p0) {}
    default void setShouldShowWithInsecureKeyguard(int p0, boolean p1) {}
    default boolean shouldShowSystemDecors(int p0) { return false; }
    default android.graphics.Bitmap snapshotTaskForRecents(int p0) { return null; }
    default boolean transferTouchGesture(android.window.InputTransferToken p0, android.window.InputTransferToken p1) { return false; }
    default void unregisterSurfaceControlInputReceiver(android.view.SurfaceControl p0) {}
    default void unregisterTrustedPresentationListener(java.util.function.Consumer p0) {}
    // ---- end of generated members
}
