package android.view;

/** A touch: the pointers down now, the action and which pointer it is about. Made by Husk from the screen's touches. */
public final class MotionEvent extends InputEvent {
    public static final int ACTION_DOWN = 0, ACTION_UP = 1, ACTION_MOVE = 2, ACTION_CANCEL = 3, ACTION_OUTSIDE = 4, ACTION_POINTER_DOWN = 5,
        ACTION_POINTER_UP = 6, ACTION_HOVER_MOVE = 7, ACTION_SCROLL = 8, ACTION_HOVER_ENTER = 9, ACTION_HOVER_EXIT = 10, ACTION_BUTTON_PRESS = 11,
        ACTION_BUTTON_RELEASE = 12, ACTION_MASK = 0xff, ACTION_POINTER_INDEX_MASK = 0xff00, ACTION_POINTER_INDEX_SHIFT = 8, ACTION_POINTER_ID_MASK = 0xff00,
        ACTION_POINTER_ID_SHIFT = 8, ACTION_POINTER_1_DOWN = 5, ACTION_POINTER_2_DOWN = 0x105, ACTION_POINTER_1_UP = 6, ACTION_POINTER_2_UP = 0x106;
    public static final int AXIS_X = 0, AXIS_Y = 1, AXIS_PRESSURE = 2, AXIS_SIZE = 3, AXIS_VSCROLL = 9, AXIS_HSCROLL = 10, AXIS_Z = 11, AXIS_RX = 12,
        AXIS_RY = 13, AXIS_RZ = 14, AXIS_HAT_X = 15, AXIS_HAT_Y = 16, AXIS_LTRIGGER = 17, AXIS_RTRIGGER = 18, AXIS_GAS = 22, AXIS_BRAKE = 23;
    public static final int BUTTON_PRIMARY = 1, BUTTON_SECONDARY = 2, BUTTON_TERTIARY = 4, BUTTON_BACK = 8, BUTTON_FORWARD = 16;
    public static final int TOOL_TYPE_UNKNOWN = 0, TOOL_TYPE_FINGER = 1, TOOL_TYPE_STYLUS = 2, TOOL_TYPE_MOUSE = 3;
    public static final int EDGE_TOP = 1, EDGE_BOTTOM = 2, EDGE_LEFT = 4, EDGE_RIGHT = 8, FLAG_WINDOW_IS_OBSCURED = 1;

    private int action, count, source = InputDevice.SOURCE_TOUCHSCREEN, buttonState;
    private int[] ids;
    private float[] xs, ys;
    private long downTime, eventTime;
    private float offX, offY;

    private MotionEvent() {}
    /** Husk's driver: a whole event at once. */
    public static MotionEvent huskObtain(long downTime, long eventTime, int action, int count, int[] ids, float[] xs, float[] ys) {
        MotionEvent e = new MotionEvent();
        e.downTime = downTime; e.eventTime = eventTime; e.action = action; e.count = count; e.ids = ids; e.xs = xs; e.ys = ys;
        return e;
    }
    public static MotionEvent obtain(long downTime, long eventTime, int action, float x, float y, int metaState) {
        return huskObtain(downTime, eventTime, action, 1, new int[] { 0 }, new float[] { x }, new float[] { y });
    }
    public static MotionEvent obtain(MotionEvent o) {
        return huskObtain(o.downTime, o.eventTime, o.action, o.count, o.ids.clone(), o.xs.clone(), o.ys.clone());
    }
    public void recycle() {}
    public int getAction() { return action; }
    public int getActionMasked() { return action & ACTION_MASK; }
    public int getActionIndex() { return (action & ACTION_POINTER_INDEX_MASK) >> ACTION_POINTER_INDEX_SHIFT; }
    public void setAction(int a) { action = a; }
    public int getPointerCount() { return count; }
    public int getPointerId(int i) { return ids[i]; }
    public int findPointerIndex(int id) { for (int i = 0; i < count; i++) if (ids[i] == id) return i; return -1; }
    public float getX() { return xs[0] + offX; }
    public float getY() { return ys[0] + offY; }
    public float getX(int i) { return xs[i] + offX; }
    public float getY(int i) { return ys[i] + offY; }
    public float getRawX() { return xs[0]; }
    public float getRawY() { return ys[0]; }
    public float getRawX(int i) { return xs[i]; }
    public float getRawY(int i) { return ys[i]; }
    public float getPressure() { return action == ACTION_UP ? 0f : 1f; }
    public float getPressure(int i) { return getPressure(); }
    public float getSize() { return 0.1f; }
    public float getSize(int i) { return 0.1f; }
    public float getTouchMajor() { return 10f; }
    public float getTouchMajor(int i) { return 10f; }
    public float getAxisValue(int axis) { return axis == AXIS_X ? getX() : axis == AXIS_Y ? getY() : 0f; }
    public float getAxisValue(int axis, int i) { return axis == AXIS_X ? getX(i) : axis == AXIS_Y ? getY(i) : 0f; }
    public int getToolType(int i) { return TOOL_TYPE_FINGER; }
    public int getButtonState() { return buttonState; }
    public int getHistorySize() { return 0; }
    public float getHistoricalX(int p, int h) { return getX(p); }
    public float getHistoricalY(int p, int h) { return getY(p); }
    public long getHistoricalEventTime(int h) { return eventTime; }
    public long getDownTime() { return downTime; }
    public long getEventTime() { return eventTime; }
    public long getEventTimeNanos() { return eventTime * 1000000L; }
    public int getSource() { return source; }
    public void setSource(int s) { source = s; }
    public boolean isFromSource(int s) { return (source & s) == s; }
    public int getMetaState() { return 0; }
    public int getFlags() { return 0; }
    public int getEdgeFlags() { return 0; }
    public int getDeviceId() { return 1; }
    public InputDevice getDevice() { return InputDevice.getDevice(1); }
    public void offsetLocation(float dx, float dy) { offX += dx; offY += dy; }
    public void setLocation(float x, float y) { offX = x - xs[0]; offY = y - ys[0]; }
    public boolean isButtonPressed(int b) { return (buttonState & b) != 0; }
}
