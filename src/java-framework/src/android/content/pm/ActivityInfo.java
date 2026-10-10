package android.content.pm;
public class ActivityInfo extends ComponentInfo implements android.os.Parcelable {
    public static final int SCREEN_ORIENTATION_UNSET = -2, SCREEN_ORIENTATION_UNSPECIFIED = -1, SCREEN_ORIENTATION_LANDSCAPE = 0, SCREEN_ORIENTATION_PORTRAIT = 1,
        SCREEN_ORIENTATION_USER = 2, SCREEN_ORIENTATION_BEHIND = 3, SCREEN_ORIENTATION_SENSOR = 4, SCREEN_ORIENTATION_NOSENSOR = 5, SCREEN_ORIENTATION_SENSOR_LANDSCAPE = 6,
        SCREEN_ORIENTATION_SENSOR_PORTRAIT = 7, SCREEN_ORIENTATION_REVERSE_LANDSCAPE = 8, SCREEN_ORIENTATION_REVERSE_PORTRAIT = 9,
        SCREEN_ORIENTATION_FULL_SENSOR = 10, SCREEN_ORIENTATION_USER_LANDSCAPE = 11, SCREEN_ORIENTATION_USER_PORTRAIT = 12, SCREEN_ORIENTATION_FULL_USER = 13, SCREEN_ORIENTATION_LOCKED = 14;
    public static final int LAUNCH_MULTIPLE = 0, LAUNCH_SINGLE_TOP = 1, LAUNCH_SINGLE_TASK = 2, LAUNCH_SINGLE_INSTANCE = 3, LAUNCH_SINGLE_INSTANCE_PER_TASK = 4;
    public static final int CONFIG_MCC = 1, CONFIG_MNC = 2, CONFIG_LOCALE = 4, CONFIG_TOUCHSCREEN = 8, CONFIG_KEYBOARD = 16, CONFIG_KEYBOARD_HIDDEN = 32, CONFIG_NAVIGATION = 64,
        CONFIG_ORIENTATION = 128, CONFIG_SCREEN_LAYOUT = 256, CONFIG_UI_MODE = 512, CONFIG_SCREEN_SIZE = 1024, CONFIG_SMALLEST_SCREEN_SIZE = 2048, CONFIG_DENSITY = 4096,
        CONFIG_LAYOUT_DIRECTION = 8192, CONFIG_COLOR_MODE = 16384, CONFIG_GRAMMATICAL_GENDER = 32768, CONFIG_FONT_WEIGHT_ADJUSTMENT = 0x10000000, CONFIG_FONT_SCALE = 0x40000000;
    public static final int FLAG_MULTIPROCESS = 1, FLAG_FINISH_ON_TASK_LAUNCH = 2, FLAG_CLEAR_TASK_ON_LAUNCH = 4, FLAG_ALWAYS_RETAIN_TASK_STATE = 8, FLAG_STATE_NOT_NEEDED = 16,
        FLAG_EXCLUDE_FROM_RECENTS = 32, FLAG_ALLOW_TASK_REPARENTING = 64, FLAG_NO_HISTORY = 128, FLAG_FINISH_ON_CLOSE_SYSTEM_DIALOGS = 256, FLAG_HARDWARE_ACCELERATED = 512,
        FLAG_IMMERSIVE = 0x800, FLAG_RELINQUISH_TASK_IDENTITY = 0x1000, FLAG_AUTO_REMOVE_FROM_RECENTS = 0x2000, FLAG_RESUME_WHILE_PAUSING = 0x4000, FLAG_SINGLE_USER = 0x40000000;
    public static final int UIOPTION_SPLIT_ACTION_BAR_WHEN_NARROW = 1, DOCUMENT_LAUNCH_NONE = 0, PERSIST_ROOT_ONLY = 0, COLOR_MODE_DEFAULT = 0;
    public int theme, launchMode, documentLaunchMode, persistableMode, maxRecents, screenOrientation = SCREEN_ORIENTATION_UNSPECIFIED, configChanges, softInputMode, uiOptions,
        flags, colorMode, rotationAnimation = -1, lockTaskLaunchMode;
    public String permission, taskAffinity, targetActivity, parentActivityName;
    public ActivityInfo() {}
    public ActivityInfo(ActivityInfo o) { super(o); theme = o.theme; launchMode = o.launchMode; screenOrientation = o.screenOrientation; configChanges = o.configChanges; flags = o.flags; parentActivityName = o.parentActivityName; }
    public final int getThemeResource() { return theme != 0 ? theme : applicationInfo.theme; }
    public int describeContents() { return 0; }
    public static final class WindowLayout { public final int width = -1, height = -1, gravity = 0; public final float widthFraction = -1, heightFraction = -1; }
}
