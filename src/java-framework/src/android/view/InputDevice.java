package android.view;
public final class InputDevice {
    public static final int SOURCE_CLASS_BUTTON = 1, SOURCE_CLASS_POINTER = 2, SOURCE_CLASS_JOYSTICK = 16, SOURCE_KEYBOARD = 0x101, SOURCE_DPAD = 0x201,
        SOURCE_GAMEPAD = 0x401, SOURCE_TOUCHSCREEN = 0x1002, SOURCE_MOUSE = 0x2002, SOURCE_STYLUS = 0x4002, SOURCE_TOUCHPAD = 0x100008,
        SOURCE_JOYSTICK = 0x1000010, SOURCE_ANY = 0xffffff00, KEYBOARD_TYPE_NONE = 0, KEYBOARD_TYPE_ALPHABETIC = 2;
    private final int id;
    private InputDevice(int id) { this.id = id; }
    public static InputDevice getDevice(int id) { return id == 1 ? new InputDevice(1) : null; }
    public static int[] getDeviceIds() { return new int[] { 1 }; }
    public int getId() { return id; }
    public String getName() { return "touchscreen"; }
    public String getDescriptor() { return "husk-touch"; }
    public int getSources() { return SOURCE_TOUCHSCREEN; }
    public int getKeyboardType() { return KEYBOARD_TYPE_NONE; }
    public boolean isVirtual() { return false; }
    public boolean isExternal() { return false; }
    public int getVendorId() { return 0; }
    public int getProductId() { return 0; }
    public java.util.List<Object> getMotionRanges() { return new java.util.ArrayList<>(); }
    public Object getMotionRange(int axis) { return null; }
    public boolean supportsSource(int s) { return (s & SOURCE_TOUCHSCREEN) == s; }
    public boolean[] hasKeys(int... keys) { return new boolean[keys.length]; }
    public android.os.Vibrator getVibrator() { return new android.os.Vibrator(); }
}
