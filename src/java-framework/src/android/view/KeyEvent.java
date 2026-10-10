package android.view;

public class KeyEvent extends InputEvent {
    public interface Callback { boolean onKeyDown(int code, KeyEvent e); boolean onKeyUp(int code, KeyEvent e); boolean onKeyLongPress(int code, KeyEvent e); boolean onKeyMultiple(int code, int n, KeyEvent e); }
    public static final int ACTION_DOWN = 0, ACTION_UP = 1, ACTION_MULTIPLE = 2, FLAG_SOFT_KEYBOARD = 2, FLAG_KEEP_TOUCH_MODE = 4, FLAG_EDITOR_ACTION = 16;
    public static final int META_SHIFT_ON = 1, META_ALT_ON = 2, META_SYM_ON = 4, META_CTRL_ON = 0x1000, META_META_ON = 0x10000, META_SHIFT_LEFT_ON = 64, META_SHIFT_RIGHT_ON = 128;
    public static final int KEYCODE_UNKNOWN = 0, KEYCODE_SOFT_LEFT = 1, KEYCODE_SOFT_RIGHT = 2, KEYCODE_HOME = 3, KEYCODE_BACK = 4, KEYCODE_CALL = 5,
        KEYCODE_ENDCALL = 6, KEYCODE_0 = 7, KEYCODE_1 = 8, KEYCODE_2 = 9, KEYCODE_3 = 10, KEYCODE_4 = 11, KEYCODE_5 = 12, KEYCODE_6 = 13, KEYCODE_7 = 14,
        KEYCODE_8 = 15, KEYCODE_9 = 16, KEYCODE_STAR = 17, KEYCODE_POUND = 18, KEYCODE_DPAD_UP = 19, KEYCODE_DPAD_DOWN = 20, KEYCODE_DPAD_LEFT = 21,
        KEYCODE_DPAD_RIGHT = 22, KEYCODE_DPAD_CENTER = 23, KEYCODE_VOLUME_UP = 24, KEYCODE_VOLUME_DOWN = 25, KEYCODE_POWER = 26, KEYCODE_CAMERA = 27,
        KEYCODE_CLEAR = 28, KEYCODE_A = 29, KEYCODE_B = 30, KEYCODE_C = 31, KEYCODE_D = 32, KEYCODE_E = 33, KEYCODE_F = 34, KEYCODE_G = 35, KEYCODE_H = 36,
        KEYCODE_I = 37, KEYCODE_J = 38, KEYCODE_K = 39, KEYCODE_L = 40, KEYCODE_M = 41, KEYCODE_N = 42, KEYCODE_O = 43, KEYCODE_P = 44, KEYCODE_Q = 45,
        KEYCODE_R = 46, KEYCODE_S = 47, KEYCODE_T = 48, KEYCODE_U = 49, KEYCODE_V = 50, KEYCODE_W = 51, KEYCODE_X = 52, KEYCODE_Y = 53, KEYCODE_Z = 54,
        KEYCODE_COMMA = 55, KEYCODE_PERIOD = 56, KEYCODE_ALT_LEFT = 57, KEYCODE_ALT_RIGHT = 58, KEYCODE_SHIFT_LEFT = 59, KEYCODE_SHIFT_RIGHT = 60,
        KEYCODE_TAB = 61, KEYCODE_SPACE = 62, KEYCODE_SYM = 63, KEYCODE_EXPLORER = 64, KEYCODE_ENVELOPE = 65, KEYCODE_ENTER = 66, KEYCODE_DEL = 67,
        KEYCODE_GRAVE = 68, KEYCODE_MINUS = 69, KEYCODE_EQUALS = 70, KEYCODE_LEFT_BRACKET = 71, KEYCODE_RIGHT_BRACKET = 72, KEYCODE_BACKSLASH = 73,
        KEYCODE_SEMICOLON = 74, KEYCODE_APOSTROPHE = 75, KEYCODE_SLASH = 76, KEYCODE_AT = 77, KEYCODE_NUM = 78, KEYCODE_HEADSETHOOK = 79, KEYCODE_FOCUS = 80,
        KEYCODE_PLUS = 81, KEYCODE_MENU = 82, KEYCODE_NOTIFICATION = 83, KEYCODE_SEARCH = 84, KEYCODE_MEDIA_PLAY_PAUSE = 85, KEYCODE_MEDIA_STOP = 86,
        KEYCODE_MEDIA_NEXT = 87, KEYCODE_MEDIA_PREVIOUS = 88, KEYCODE_MEDIA_REWIND = 89, KEYCODE_MEDIA_FAST_FORWARD = 90, KEYCODE_MUTE = 91, KEYCODE_PAGE_UP = 92,
        KEYCODE_PAGE_DOWN = 93, KEYCODE_PICTSYMBOLS = 94, KEYCODE_SWITCH_CHARSET = 95, KEYCODE_BUTTON_A = 96, KEYCODE_BUTTON_B = 97, KEYCODE_BUTTON_C = 98,
        KEYCODE_BUTTON_X = 99, KEYCODE_BUTTON_Y = 100, KEYCODE_BUTTON_Z = 101, KEYCODE_BUTTON_L1 = 102, KEYCODE_BUTTON_R1 = 103, KEYCODE_BUTTON_L2 = 104,
        KEYCODE_BUTTON_R2 = 105, KEYCODE_BUTTON_THUMBL = 106, KEYCODE_BUTTON_THUMBR = 107, KEYCODE_BUTTON_START = 108, KEYCODE_BUTTON_SELECT = 109,
        KEYCODE_BUTTON_MODE = 110, KEYCODE_ESCAPE = 111, KEYCODE_FORWARD_DEL = 112, KEYCODE_CTRL_LEFT = 113, KEYCODE_CTRL_RIGHT = 114, KEYCODE_CAPS_LOCK = 115,
        KEYCODE_SCROLL_LOCK = 116, KEYCODE_META_LEFT = 117, KEYCODE_META_RIGHT = 118, KEYCODE_FUNCTION = 119, KEYCODE_SYSRQ = 120, KEYCODE_BREAK = 121,
        KEYCODE_MOVE_HOME = 122, KEYCODE_MOVE_END = 123, KEYCODE_INSERT = 124, KEYCODE_FORWARD = 125, KEYCODE_MEDIA_PLAY = 126, KEYCODE_MEDIA_PAUSE = 127,
        KEYCODE_F1 = 131, KEYCODE_F2 = 132, KEYCODE_F3 = 133, KEYCODE_F4 = 134, KEYCODE_F5 = 135, KEYCODE_F6 = 136, KEYCODE_F7 = 137, KEYCODE_F8 = 138,
        KEYCODE_F9 = 139, KEYCODE_F10 = 140, KEYCODE_F11 = 141, KEYCODE_F12 = 142, KEYCODE_NUM_LOCK = 143, KEYCODE_NUMPAD_0 = 144, KEYCODE_NUMPAD_1 = 145,
        KEYCODE_NUMPAD_2 = 146, KEYCODE_NUMPAD_3 = 147, KEYCODE_NUMPAD_4 = 148, KEYCODE_NUMPAD_5 = 149, KEYCODE_NUMPAD_6 = 150, KEYCODE_NUMPAD_7 = 151,
        KEYCODE_NUMPAD_8 = 152, KEYCODE_NUMPAD_9 = 153, KEYCODE_NUMPAD_DIVIDE = 154, KEYCODE_NUMPAD_MULTIPLY = 155, KEYCODE_NUMPAD_SUBTRACT = 156,
        KEYCODE_NUMPAD_ADD = 157, KEYCODE_NUMPAD_DOT = 158, KEYCODE_NUMPAD_COMMA = 159, KEYCODE_NUMPAD_ENTER = 160, KEYCODE_NUMPAD_EQUALS = 161,
        KEYCODE_NUMPAD_LEFT_PAREN = 162, KEYCODE_NUMPAD_RIGHT_PAREN = 163, KEYCODE_VOLUME_MUTE = 164, KEYCODE_INFO = 165, KEYCODE_BUTTON_1 = 188,
        KEYCODE_BUTTON_16 = 203, KEYCODE_SETTINGS = 176, KEYCODE_APP_SWITCH = 187, KEYCODE_BUTTON_2 = 189;
    public static final int FLAG_WOKE_HERE = 1, FLAG_FROM_SYSTEM = 8, FLAG_VIRTUAL_HARD_KEY = 64, FLAG_LONG_PRESS = 128, FLAG_CANCELED = 32, FLAG_CANCELED_LONG_PRESS = 256,
        FLAG_TRACKING = 512, FLAG_FALLBACK = 1024, META_ALT_LEFT_ON = 16, META_ALT_RIGHT_ON = 32, META_SYM_ON_ = 4, META_FUNCTION_ON = 8, META_CTRL_LEFT_ON = 0x2000,
        META_CTRL_RIGHT_ON = 0x4000, META_META_LEFT_ON = 0x20000, META_META_RIGHT_ON = 0x40000, META_CAPS_LOCK_ON = 0x100000, META_NUM_LOCK_ON = 0x200000,
        META_SCROLL_LOCK_ON = 0x400000, META_ALT_MASK = 50, META_CTRL_MASK = 0x7000, META_META_MASK = 0x70000, META_SHIFT_MASK = 193;
    public static class DispatcherState {
        private int mDownKeyCode; private Object mDownTarget; private final android.util.SparseIntArray mActiveLongPresses = new android.util.SparseIntArray();
        public void reset() { mDownKeyCode = 0; mDownTarget = null; mActiveLongPresses.clear(); }
        public void reset(Object target) { if (mDownTarget == target) { mDownKeyCode = 0; mDownTarget = null; } }
        public void startTracking(KeyEvent e, Object target) { if (e.getAction() != ACTION_DOWN) throw new IllegalArgumentException("Can only start tracking on a down event"); mDownKeyCode = e.getKeyCode(); mDownTarget = target; }
        public boolean isTracking(KeyEvent e) { return mDownKeyCode == e.getKeyCode(); }
        public void performedLongPress(KeyEvent e) { mActiveLongPresses.put(e.getKeyCode(), 1); }
        public void handleUpEvent(KeyEvent e) {
            int code = e.getKeyCode();
            int idx = mActiveLongPresses.indexOfKey(code);
            if (idx >= 0) { e.mFlags |= FLAG_CANCELED | FLAG_CANCELED_LONG_PRESS; mActiveLongPresses.removeAt(idx); }
            if (mDownKeyCode == code) { e.mFlags |= FLAG_TRACKING; mDownKeyCode = 0; mDownTarget = null; }
        }
    }
    private final int action, code, repeat, meta, source;
    private int mDeviceId, mScanCode;
    int mFlags;
    private final long downTime, eventTime;
    private final String chars;
    public KeyEvent(int action, int code) { this(0, 0, action, code, 0, 0); }
    public KeyEvent(long down, long time, int action, int code, int repeat) { this(down, time, action, code, repeat, 0); }
    public KeyEvent(long down, long time, int action, int code, int repeat, int meta) { this(down, time, action, code, repeat, meta, InputDevice.SOURCE_KEYBOARD, null); }
    public KeyEvent(long down, long time, int action, int code, int repeat, int meta, int deviceId, int scancode) { this(down, time, action, code, repeat, meta, InputDevice.SOURCE_KEYBOARD, null); mDeviceId = deviceId; mScanCode = scancode; }
    public KeyEvent(long down, long time, int action, int code, int repeat, int meta, int deviceId, int scancode, int flags) { this(down, time, action, code, repeat, meta, deviceId, scancode); mFlags = flags; }
    public KeyEvent(long down, long time, int action, int code, int repeat, int meta, int deviceId, int scancode, int flags, int source) { this(down, time, action, code, repeat, meta, source, null); mDeviceId = deviceId; mScanCode = scancode; mFlags = flags; }
    public KeyEvent(KeyEvent o) { this(o.downTime, o.eventTime, o.action, o.code, o.repeat, o.meta, o.source, o.chars); mDeviceId = o.mDeviceId; mScanCode = o.mScanCode; mFlags = o.mFlags; }
    @Deprecated public KeyEvent(KeyEvent o, long eventTime, int newRepeat) { this(o.downTime, eventTime, o.action, o.code, newRepeat, o.meta, o.source, o.chars); mDeviceId = o.mDeviceId; mScanCode = o.mScanCode; mFlags = o.mFlags; }
    public KeyEvent(long down, String chars, int device, int flags) { this(down, down, ACTION_MULTIPLE, KEYCODE_UNKNOWN, 0, 0, InputDevice.SOURCE_KEYBOARD, chars); }
    private KeyEvent(long down, long time, int action, int code, int repeat, int meta, int source, String chars) {
        this.downTime = down; this.eventTime = time; this.action = action; this.code = code; this.repeat = repeat; this.meta = meta; this.source = source; this.chars = chars;
    }
    public final int getAction() { return action; }
    public final int getKeyCode() { return code; }
    public final int getRepeatCount() { return repeat; }
    public final int getMetaState() { return meta; }
    public final int getModifiers() { return meta; }
    public final long getDownTime() { return downTime; }
    public final long getEventTime() { return eventTime; }
    public final int getSource() { return source; }
    public final int getDeviceId() { return mDeviceId; }
    public final int getFlags() { return mFlags; }
    public final int getScanCode() { return mScanCode; }
    public final InputDevice getDevice() { return InputDevice.getDevice(1); }
    public final String getCharacters() { return chars; }
    public final boolean isShiftPressed() { return (meta & META_SHIFT_ON) != 0; }
    public final boolean isAltPressed() { return (meta & META_ALT_ON) != 0; }
    public final boolean isCtrlPressed() { return (meta & META_CTRL_ON) != 0; }
    public final boolean isLongPress() { return (mFlags & FLAG_LONG_PRESS) != 0; }
    public final boolean isCanceled() { return (mFlags & FLAG_CANCELED) != 0; }
    public final boolean isTracking() { return (mFlags & FLAG_TRACKING) != 0; }
    public final void startTracking() { mFlags |= FLAG_START_TRACKING; }
    static final int FLAG_START_TRACKING = 0x40000000;
    public final boolean hasModifiers(int m) { return (meta & 0x770ff) == m; }
    public final boolean hasNoModifiers() { return (meta & 0x770ff) == 0; }
    public final boolean isMetaPressed() { return (meta & META_META_ON) != 0; }
    public final boolean isFunctionPressed() { return false; }
    public final boolean isSymPressed() { return false; }
    public final boolean isCapsLockOn() { return false; }
    public final boolean isNumLockOn() { return false; }
    public final boolean isPrintingKey() { return getUnicodeChar() != 0; }
    public final long getEventTimeNanos() { return eventTime * 1000000L; }
    public final int getDisplayIdHusk() { return 0; }
    public static boolean isConfirmKey(int c) { return c == KEYCODE_DPAD_CENTER || c == KEYCODE_ENTER || c == KEYCODE_SPACE || c == KEYCODE_NUMPAD_ENTER; }
    public static boolean isMediaSessionKey(int c) { return c >= KEYCODE_MEDIA_PLAY_PAUSE && c <= KEYCODE_MEDIA_FAST_FORWARD || c == KEYCODE_MEDIA_PLAY || c == KEYCODE_MEDIA_PAUSE; }
    public static boolean metaStateHasNoModifiers(int m) { return (m & 0x770ff) == 0; }
    public static boolean metaStateHasModifiers(int m, int mods) { return (m & 0x770ff) == mods; }
    public static int normalizeMetaState(int m) { return m; }
    public static KeyEvent changeAction(KeyEvent e, int action) { KeyEvent k = new KeyEvent(e.downTime, e.eventTime, action, e.code, e.repeat, e.meta, e.source, e.chars); k.mFlags = e.mFlags; return k; }
    public static KeyEvent changeTimeRepeat(KeyEvent e, long time, int repeat) { return new KeyEvent(e.downTime, time, e.action, e.code, repeat, e.meta, e.source, e.chars); }
    public static KeyEvent changeFlags(KeyEvent e, int flags) { KeyEvent k = changeAction(e, e.action); k.mFlags = flags; return k; }
    /** As KeyEvent.dispatch: to the receiver's onKeyDown/onKeyUp, keeping track of downs for long presses and canceled ups. */
    public final boolean dispatch(Callback receiver, DispatcherState state, Object target) {
        switch (action) {
        case ACTION_DOWN: {
            mFlags &= ~FLAG_START_TRACKING;
            boolean res = receiver.onKeyDown(code, this);
            if (state != null) {
                if (res && repeat == 0 && (mFlags & FLAG_START_TRACKING) != 0) state.startTracking(this, target);
                else if (isLongPress() && state.isTracking(this)) { if (receiver.onKeyLongPress(code, this)) { state.performedLongPress(this); res = true; } }
            }
            return res;
        }
        case ACTION_UP:
            if (state != null) state.handleUpEvent(this);
            return receiver.onKeyUp(code, this);
        case ACTION_MULTIPLE:
            return receiver.onKeyMultiple(code, repeat, this);
        }
        return false;
    }
    public final boolean isSystem() { return code == KEYCODE_BACK || code == KEYCODE_HOME || code == KEYCODE_MENU || code == KEYCODE_VOLUME_UP || code == KEYCODE_VOLUME_DOWN; }
    public static boolean isGamepadButton(int c) { return c >= KEYCODE_BUTTON_A && c <= KEYCODE_BUTTON_MODE; }
    public static boolean isModifierKey(int c) { return c >= KEYCODE_ALT_LEFT && c <= KEYCODE_SHIFT_RIGHT || c == KEYCODE_CTRL_LEFT || c == KEYCODE_CTRL_RIGHT; }
    public int getUnicodeChar() { return getUnicodeChar(meta); }
    public int getUnicodeChar(int m) {
        boolean shift = (m & META_SHIFT_ON) != 0;
        if (code >= KEYCODE_A && code <= KEYCODE_Z) return (shift ? 'A' : 'a') + code - KEYCODE_A;
        if (code >= KEYCODE_0 && code <= KEYCODE_9) return '0' + code - KEYCODE_0;
        switch (code) { case KEYCODE_SPACE: return ' '; case KEYCODE_ENTER: return '\n'; case KEYCODE_PERIOD: return '.'; case KEYCODE_COMMA: return ',';
            case KEYCODE_MINUS: return '-'; case KEYCODE_EQUALS: return '='; case KEYCODE_SLASH: return '/'; case KEYCODE_TAB: return '\t'; default: return 0; }
    }
    public char getDisplayLabel() { return (char) getUnicodeChar(0); }
    public KeyCharacterMap getKeyCharacterMap() { return KeyCharacterMap.load(0); }
    public static String keyCodeToString(int c) { return "KEYCODE_" + c; }
    public static int getMaxKeyCode() { return 316; }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static android.os.Parcelable.Creator CREATOR;
    public static final int FLAG_IS_ACCESSIBILITY_EVENT = 2048;
    public static final int FLAG_LONG_SWIPE = 2048;
    public static final int FLAG_PREDISPATCH = 536870912;
    public static final int FLAG_TAINTED = -2147483648;
    public static final int KEYCODE_11 = 227;
    public static final int KEYCODE_12 = 228;
    public static final int KEYCODE_3D_MODE = 206;
    public static final int KEYCODE_ALL_APPS = 284;
    public static final int KEYCODE_ASSIST = 219;
    public static final int KEYCODE_AVR_INPUT = 182;
    public static final int KEYCODE_AVR_POWER = 181;
    public static final int KEYCODE_BOOKMARK = 174;
    public static final int KEYCODE_BRIGHTNESS_DOWN = 220;
    public static final int KEYCODE_BRIGHTNESS_UP = 221;
    public static final int KEYCODE_BUTTON_10 = 197;
    public static final int KEYCODE_BUTTON_11 = 198;
    public static final int KEYCODE_BUTTON_12 = 199;
    public static final int KEYCODE_BUTTON_13 = 200;
    public static final int KEYCODE_BUTTON_14 = 201;
    public static final int KEYCODE_BUTTON_15 = 202;
    public static final int KEYCODE_BUTTON_3 = 190;
    public static final int KEYCODE_BUTTON_4 = 191;
    public static final int KEYCODE_BUTTON_5 = 192;
    public static final int KEYCODE_BUTTON_6 = 193;
    public static final int KEYCODE_BUTTON_7 = 194;
    public static final int KEYCODE_BUTTON_8 = 195;
    public static final int KEYCODE_BUTTON_9 = 196;
    public static final int KEYCODE_CALCULATOR = 210;
    public static final int KEYCODE_CALENDAR = 208;
    public static final int KEYCODE_CAPTIONS = 175;
    public static final int KEYCODE_CHANNEL_DOWN = 167;
    public static final int KEYCODE_CHANNEL_UP = 166;
    public static final int KEYCODE_CLOSE = 321;
    public static final int KEYCODE_CONTACTS = 207;
    public static final int KEYCODE_COPY = 278;
    public static final int KEYCODE_CUT = 277;
    public static final int KEYCODE_DEMO_APP_1 = 301;
    public static final int KEYCODE_DEMO_APP_2 = 302;
    public static final int KEYCODE_DEMO_APP_3 = 303;
    public static final int KEYCODE_DEMO_APP_4 = 304;
    public static final int KEYCODE_DICTATE = 319;
    public static final int KEYCODE_DO_NOT_DISTURB = 322;
    public static final int KEYCODE_DPAD_DOWN_LEFT = 269;
    public static final int KEYCODE_DPAD_DOWN_RIGHT = 271;
    public static final int KEYCODE_DPAD_UP_LEFT = 268;
    public static final int KEYCODE_DPAD_UP_RIGHT = 270;
    public static final int KEYCODE_DVR = 173;
    public static final int KEYCODE_EISU = 212;
    public static final int KEYCODE_EMOJI_PICKER = 317;
    public static final int KEYCODE_F13 = 326;
    public static final int KEYCODE_F14 = 327;
    public static final int KEYCODE_F15 = 328;
    public static final int KEYCODE_F16 = 329;
    public static final int KEYCODE_F17 = 330;
    public static final int KEYCODE_F18 = 331;
    public static final int KEYCODE_F19 = 332;
    public static final int KEYCODE_F20 = 333;
    public static final int KEYCODE_F21 = 334;
    public static final int KEYCODE_F22 = 335;
    public static final int KEYCODE_F23 = 336;
    public static final int KEYCODE_F24 = 337;
    public static final int KEYCODE_FEATURED_APP_1 = 297;
    public static final int KEYCODE_FEATURED_APP_2 = 298;
    public static final int KEYCODE_FEATURED_APP_3 = 299;
    public static final int KEYCODE_FEATURED_APP_4 = 300;
    public static final int KEYCODE_FULLSCREEN = 325;
    public static final int KEYCODE_GUIDE = 172;
    public static final int KEYCODE_HELP = 259;
    public static final int KEYCODE_HENKAN = 214;
    public static final int KEYCODE_KANA = 218;
    public static final int KEYCODE_KATAKANA_HIRAGANA = 215;
    public static final int KEYCODE_KEYBOARD_BACKLIGHT_DOWN = 305;
    public static final int KEYCODE_KEYBOARD_BACKLIGHT_TOGGLE = 307;
    public static final int KEYCODE_KEYBOARD_BACKLIGHT_UP = 306;
    public static final int KEYCODE_LANGUAGE_SWITCH = 204;
    public static final int KEYCODE_LAST_CHANNEL = 229;
    public static final int KEYCODE_LOCK = 324;
    public static final int KEYCODE_MACRO_1 = 313;
    public static final int KEYCODE_MACRO_2 = 314;
    public static final int KEYCODE_MACRO_3 = 315;
    public static final int KEYCODE_MACRO_4 = 316;
    public static final int KEYCODE_MANNER_MODE = 205;
    public static final int KEYCODE_MEDIA_AUDIO_TRACK = 222;
    public static final int KEYCODE_MEDIA_CLOSE = 128;
    public static final int KEYCODE_MEDIA_EJECT = 129;
    public static final int KEYCODE_MEDIA_RECORD = 130;
    public static final int KEYCODE_MEDIA_SKIP_BACKWARD = 273;
    public static final int KEYCODE_MEDIA_SKIP_FORWARD = 272;
    public static final int KEYCODE_MEDIA_STEP_BACKWARD = 275;
    public static final int KEYCODE_MEDIA_STEP_FORWARD = 274;
    public static final int KEYCODE_MEDIA_TOP_MENU = 226;
    public static final int KEYCODE_MUHENKAN = 213;
    public static final int KEYCODE_MUSIC = 209;
    public static final int KEYCODE_NAVIGATE_IN = 262;
    public static final int KEYCODE_NAVIGATE_NEXT = 261;
    public static final int KEYCODE_NAVIGATE_OUT = 263;
    public static final int KEYCODE_NAVIGATE_PREVIOUS = 260;
    public static final int KEYCODE_NEW = 320;
    public static final int KEYCODE_PAIRING = 225;
    public static final int KEYCODE_PASTE = 279;
    public static final int KEYCODE_PRINT = 323;
    public static final int KEYCODE_PROFILE_SWITCH = 288;
    public static final int KEYCODE_PROG_BLUE = 186;
    public static final int KEYCODE_PROG_GREEN = 184;
    public static final int KEYCODE_PROG_RED = 183;
    public static final int KEYCODE_PROG_YELLOW = 185;
    public static final int KEYCODE_RECENT_APPS = 312;
    public static final int KEYCODE_REFRESH = 285;
    public static final int KEYCODE_RO = 217;
    public static final int KEYCODE_SCREENSHOT = 318;
    public static final int KEYCODE_SLEEP = 223;
    public static final int KEYCODE_SOFT_SLEEP = 276;
    public static final int KEYCODE_STB_INPUT = 180;
    public static final int KEYCODE_STB_POWER = 179;
    public static final int KEYCODE_STEM_1 = 265;
    public static final int KEYCODE_STEM_2 = 266;
    public static final int KEYCODE_STEM_3 = 267;
    public static final int KEYCODE_STEM_PRIMARY = 264;
    public static final int KEYCODE_STYLUS_BUTTON_PRIMARY = 308;
    public static final int KEYCODE_STYLUS_BUTTON_SECONDARY = 309;
    public static final int KEYCODE_STYLUS_BUTTON_TAIL = 311;
    public static final int KEYCODE_STYLUS_BUTTON_TERTIARY = 310;
    public static final int KEYCODE_SYSTEM_NAVIGATION_DOWN = 281;
    public static final int KEYCODE_SYSTEM_NAVIGATION_LEFT = 282;
    public static final int KEYCODE_SYSTEM_NAVIGATION_RIGHT = 283;
    public static final int KEYCODE_SYSTEM_NAVIGATION_UP = 280;
    public static final int KEYCODE_THUMBS_DOWN = 287;
    public static final int KEYCODE_THUMBS_UP = 286;
    public static final int KEYCODE_TV = 170;
    public static final int KEYCODE_TV_ANTENNA_CABLE = 242;
    public static final int KEYCODE_TV_AUDIO_DESCRIPTION = 252;
    public static final int KEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN = 254;
    public static final int KEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP = 253;
    public static final int KEYCODE_TV_CONTENTS_MENU = 256;
    public static final int KEYCODE_TV_DATA_SERVICE = 230;
    public static final int KEYCODE_TV_INPUT = 178;
    public static final int KEYCODE_TV_INPUT_COMPONENT_1 = 249;
    public static final int KEYCODE_TV_INPUT_COMPONENT_2 = 250;
    public static final int KEYCODE_TV_INPUT_COMPOSITE_1 = 247;
    public static final int KEYCODE_TV_INPUT_COMPOSITE_2 = 248;
    public static final int KEYCODE_TV_INPUT_HDMI_1 = 243;
    public static final int KEYCODE_TV_INPUT_HDMI_2 = 244;
    public static final int KEYCODE_TV_INPUT_HDMI_3 = 245;
    public static final int KEYCODE_TV_INPUT_HDMI_4 = 246;
    public static final int KEYCODE_TV_INPUT_VGA_1 = 251;
    public static final int KEYCODE_TV_MEDIA_CONTEXT_MENU = 257;
    public static final int KEYCODE_TV_NETWORK = 241;
    public static final int KEYCODE_TV_NUMBER_ENTRY = 234;
    public static final int KEYCODE_TV_POWER = 177;
    public static final int KEYCODE_TV_RADIO_SERVICE = 232;
    public static final int KEYCODE_TV_SATELLITE = 237;
    public static final int KEYCODE_TV_SATELLITE_BS = 238;
    public static final int KEYCODE_TV_SATELLITE_CS = 239;
    public static final int KEYCODE_TV_SATELLITE_SERVICE = 240;
    public static final int KEYCODE_TV_TELETEXT = 233;
    public static final int KEYCODE_TV_TERRESTRIAL_ANALOG = 235;
    public static final int KEYCODE_TV_TERRESTRIAL_DIGITAL = 236;
    public static final int KEYCODE_TV_TIMER_PROGRAMMING = 258;
    public static final int KEYCODE_TV_ZOOM_MODE = 255;
    public static final int KEYCODE_VIDEO_APP_1 = 289;
    public static final int KEYCODE_VIDEO_APP_2 = 290;
    public static final int KEYCODE_VIDEO_APP_3 = 291;
    public static final int KEYCODE_VIDEO_APP_4 = 292;
    public static final int KEYCODE_VIDEO_APP_5 = 293;
    public static final int KEYCODE_VIDEO_APP_6 = 294;
    public static final int KEYCODE_VIDEO_APP_7 = 295;
    public static final int KEYCODE_VIDEO_APP_8 = 296;
    public static final int KEYCODE_VOICE_ASSIST = 231;
    public static final int KEYCODE_WAKEUP = 224;
    public static final int KEYCODE_WINDOW = 171;
    public static final int KEYCODE_YEN = 216;
    public static final int KEYCODE_ZENKAKU_HANKAKU = 211;
    public static final int KEYCODE_ZOOM_IN = 168;
    public static final int KEYCODE_ZOOM_OUT = 169;
    public static final int LAST_KEYCODE = 337;
    public static final int MAX_KEYCODE = 84;
    public static final int META_ALT_LOCKED = 512;
    public static final int META_CAP_LOCKED = 256;
    public static final int META_SELECTING = 2048;
    public static final int META_SYM_LOCKED = 1024;
    public static java.lang.String actionToString(int p0) { return null; }
    public static android.view.KeyEvent changeTimeRepeat(android.view.KeyEvent p0, long p1, int p2, int p3) { return null; }
    public static android.view.KeyEvent createFromParcelBody(android.os.Parcel p0) { return null; }
    public static int getDeadChar(int p0, int p1) { return 0; }
    public static int getModifierMetaStateMask() { return 0; }
    public static boolean isAltKey(int p0) { return false; }
    public static boolean isMetaKey(int p0) { return false; }
    public static boolean isSystemKey(int p0) { return false; }
    public static boolean isVisibleBackgroundUserAllowedKey(int p0) { return false; }
    public static boolean isWakeKey(int p0) { return false; }
    public static int keyCodeFromString(java.lang.String p0) { return 0; }
    public static java.lang.String metaStateToString(int p0) { return null; }
    public static android.view.KeyEvent obtain(long p0, long p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, int p10, java.lang.String p11) { return null; }
    public static android.view.KeyEvent obtain(long p0, long p1, int p2, int p3, int p4, int p5, int p6, int p7, int p8, int p9, java.lang.String p10) { return null; }
    public static android.view.KeyEvent obtain(android.view.KeyEvent p0) { return null; }
    public void cancel() {}
    public android.view.KeyEvent copy() { return this; }
    public boolean dispatch(android.view.KeyEvent.Callback p0) { return false; }
    public int getDisplayId() { return (huskFill.get("DisplayId") instanceof Integer ? (Integer) huskFill.get("DisplayId") : 0); }
    public int getId() { return 0; }
    public int getKeyboardDevice() { return 0; }
    public char getMatch(char[] p0) { return '\0'; }
    public char getMatch(char[] p0, int p1) { return '\0'; }
    public char getNumber() { return '\0'; }
    public boolean isDown() { return false; }
    public boolean isScrollLockOn() { return false; }
    public boolean isTainted() { return (huskFill.get("Tainted") instanceof Boolean ? (Boolean) huskFill.get("Tainted") : false); }
    public boolean isWakeKey() { return false; }
    public void recycle() {}
    public void recycleIfNeededAfterDispatch() {}
    public void setDisplayId(int p0) { huskFill.put("DisplayId", Integer.valueOf(p0)); }
    public void setFlags(int p0) {}
    public void setSource(int p0) {}
    public void setTainted(boolean p0) { huskFill.put("Tainted", Boolean.valueOf(p0)); }
    public void setTime(long p0, long p1) {}
    public void writeToParcel(android.os.Parcel p0, int p1) {}
    // ---- end of generated members
}
