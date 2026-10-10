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
    /** The touch screen's ranges (the screen's pixels), as a phone's touch screen reports them. */
    public java.util.List<MotionRange> getMotionRanges() { java.util.ArrayList<MotionRange> l = new java.util.ArrayList<>(); MotionRange x = getMotionRange(MotionEvent.AXIS_X), y = getMotionRange(MotionEvent.AXIS_Y); if (x != null) l.add(x); if (y != null) l.add(y); return l; }
    public MotionRange getMotionRange(int axis) { return getMotionRange(axis, SOURCE_TOUCHSCREEN); }
    public MotionRange getMotionRange(int axis, int source) {
        if ((getSources() & source) != source || (source & SOURCE_TOUCHSCREEN) != SOURCE_TOUCHSCREEN) return null;
        if (axis == MotionEvent.AXIS_X) return new MotionRange(axis, source, 0, husk.Native.screenWidth() - 1);
        if (axis == MotionEvent.AXIS_Y) return new MotionRange(axis, source, 0, husk.Native.screenHeight() - 1);
        if (axis == MotionEvent.AXIS_PRESSURE || axis == MotionEvent.AXIS_SIZE) return new MotionRange(axis, source, 0, 1);
        return null;
    }
    public boolean supportsSource(int s) { return (s & SOURCE_TOUCHSCREEN) == s; }
    public boolean[] hasKeys(int... keys) { return new boolean[keys.length]; }
    public android.os.Vibrator getVibrator() { return new android.os.Vibrator(); }
    // ---- platform API stubs (tools/compat/genstubs.py)
    public static final class MotionRange {
        private final int mAxis, mSource; private final float mMin, mMax;
        MotionRange(int axis, int source, float min, float max) { mAxis = axis; mSource = source; mMin = min; mMax = max; }
        public int getAxis() { return mAxis; }
        public int getSource() { return mSource; }
        public boolean isFromSource(int source) { return (mSource & source) == source; }
        public float getMin() { return mMin; }
        public float getMax() { return mMax; }
        public float getRange() { return mMax - mMin; }
        public float getFlat() { return 0f; }
        public float getFuzz() { return 0f; }
        public float getResolution() { return 0f; }
    }
    public int getControllerNumber() { return 0; }
}
