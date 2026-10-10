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
    private final int action, code, repeat, meta, source;
    private final long downTime, eventTime;
    private final String chars;
    public KeyEvent(int action, int code) { this(0, 0, action, code, 0, 0); }
    public KeyEvent(long down, long time, int action, int code, int repeat) { this(down, time, action, code, repeat, 0); }
    public KeyEvent(long down, long time, int action, int code, int repeat, int meta) { this(down, time, action, code, repeat, meta, InputDevice.SOURCE_KEYBOARD, null); }
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
    public final int getDeviceId() { return 0; }
    public final int getFlags() { return 0; }
    public final int getScanCode() { return 0; }
    public final InputDevice getDevice() { return InputDevice.getDevice(1); }
    public final String getCharacters() { return chars; }
    public final boolean isShiftPressed() { return (meta & META_SHIFT_ON) != 0; }
    public final boolean isAltPressed() { return (meta & META_ALT_ON) != 0; }
    public final boolean isCtrlPressed() { return (meta & META_CTRL_ON) != 0; }
    public final boolean isLongPress() { return false; }
    public final boolean isCanceled() { return false; }
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
}
