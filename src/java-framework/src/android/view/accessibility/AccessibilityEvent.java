package android.view.accessibility;
public final class AccessibilityEvent extends AccessibilityRecord implements android.os.Parcelable {
    public static final int TYPE_VIEW_CLICKED = 1, TYPE_VIEW_LONG_CLICKED = 2, TYPE_VIEW_SELECTED = 4, TYPE_VIEW_FOCUSED = 8, TYPE_VIEW_TEXT_CHANGED = 16,
        TYPE_WINDOW_STATE_CHANGED = 32, TYPE_NOTIFICATION_STATE_CHANGED = 64, TYPE_VIEW_HOVER_ENTER = 128, TYPE_VIEW_HOVER_EXIT = 256, TYPE_TOUCH_EXPLORATION_GESTURE_START = 512,
        TYPE_TOUCH_EXPLORATION_GESTURE_END = 1024, TYPE_WINDOW_CONTENT_CHANGED = 2048, TYPE_VIEW_SCROLLED = 4096, TYPE_VIEW_TEXT_SELECTION_CHANGED = 8192, TYPE_ANNOUNCEMENT = 16384,
        TYPE_VIEW_ACCESSIBILITY_FOCUSED = 32768, TYPE_VIEW_ACCESSIBILITY_FOCUS_CLEARED = 65536, TYPES_ALL_MASK = -1, CONTENT_CHANGE_TYPE_SUBTREE = 1, CONTENT_CHANGE_TYPE_TEXT = 2,
        CONTENT_CHANGE_TYPE_CONTENT_DESCRIPTION = 4, CONTENT_CHANGE_TYPE_UNDEFINED = 0, CONTENT_CHANGE_TYPE_PANE_TITLE = 8, CONTENT_CHANGE_TYPE_PANE_APPEARED = 16, CONTENT_CHANGE_TYPE_PANE_DISAPPEARED = 32;
    private int mType;
    public AccessibilityEvent() {}
    public AccessibilityEvent(int t) { mType = t; }
    public static AccessibilityEvent obtain(int t) { return new AccessibilityEvent(t); }
    public static AccessibilityEvent obtain() { return new AccessibilityEvent(); }
    public int getEventType() { return mType; } public void setEventType(int t) { mType = t; }
    public void setContentChangeTypes(int t) {} public int getContentChangeTypes() { return 0; } public void setPackageName(CharSequence p) {} public CharSequence getPackageName() { return null; }
    public long getEventTime() { return 0; } public void setEventTime(long t) {} public int getAction() { return 0; } public void setAction(int a) {}
    public int describeContents() { return 0; }
}
