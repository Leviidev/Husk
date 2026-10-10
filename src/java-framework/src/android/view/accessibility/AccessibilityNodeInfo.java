package android.view.accessibility;
public class AccessibilityNodeInfo {
    public static final int ACTION_FOCUS = 1, ACTION_CLEAR_FOCUS = 2, ACTION_SELECT = 4, ACTION_CLEAR_SELECTION = 8, ACTION_CLICK = 16, ACTION_LONG_CLICK = 32,
        ACTION_ACCESSIBILITY_FOCUS = 64, ACTION_CLEAR_ACCESSIBILITY_FOCUS = 128, ACTION_NEXT_AT_MOVEMENT_GRANULARITY = 256, ACTION_PREVIOUS_AT_MOVEMENT_GRANULARITY = 512,
        ACTION_NEXT_HTML_ELEMENT = 1024, ACTION_PREVIOUS_HTML_ELEMENT = 2048, ACTION_SCROLL_FORWARD = 4096, ACTION_SCROLL_BACKWARD = 8192, ACTION_COPY = 16384,
        ACTION_PASTE = 32768, ACTION_CUT = 65536, ACTION_SET_SELECTION = 131072, ACTION_EXPAND = 262144, ACTION_COLLAPSE = 524288, ACTION_DISMISS = 1048576,
        ACTION_SET_TEXT = 2097152, FOCUS_INPUT = 1, FOCUS_ACCESSIBILITY = 2;
    public static final String ACTION_ARGUMENT_SELECTION_START_INT = "ACTION_ARGUMENT_SELECTION_START_INT", ACTION_ARGUMENT_SELECTION_END_INT = "ACTION_ARGUMENT_SELECTION_END_INT",
        ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE = "ACTION_ARGUMENT_SET_TEXT_CHARSEQUENCE";
    public static AccessibilityNodeInfo obtain() { return new AccessibilityNodeInfo(); }
    public static AccessibilityNodeInfo obtain(android.view.View v) { return new AccessibilityNodeInfo(); }
    public static AccessibilityNodeInfo obtain(android.view.View v, int id) { return new AccessibilityNodeInfo(); }
    public static AccessibilityNodeInfo obtain(AccessibilityNodeInfo o) { return new AccessibilityNodeInfo(); }
    public void recycle() {}
    public void addAction(int a) {} public void addAction(AccessibilityAction a) {} public boolean removeAction(AccessibilityAction a) { return false; }
    public void addChild(android.view.View v) {} public void addChild(android.view.View v, int id) {} public void setParent(android.view.View v) {} public void setParent(android.view.View v, int id) {}
    public void setSource(android.view.View v) {} public void setSource(android.view.View v, int id) {}
    public void setBoundsInParent(android.graphics.Rect r) {} public void setBoundsInScreen(android.graphics.Rect r) {} public void getBoundsInParent(android.graphics.Rect r) {} public void getBoundsInScreen(android.graphics.Rect r) {}
    public void setClassName(CharSequence c) {} public CharSequence getClassName() { return null; } public void setPackageName(CharSequence p) {}
    public void setText(CharSequence t) {} public CharSequence getText() { return null; } public void setContentDescription(CharSequence c) {} public CharSequence getContentDescription() { return null; }
    public void setHintText(CharSequence h) {} public void setError(CharSequence e) {} public void setStateDescription(CharSequence s) {} public void setTooltipText(CharSequence t) {}
    public void setClickable(boolean b) {} public void setLongClickable(boolean b) {} public void setFocusable(boolean b) {} public void setFocused(boolean b) {} public void setSelected(boolean b) {}
    public void setEnabled(boolean b) {} public void setCheckable(boolean b) {} public void setChecked(boolean b) {} public void setScrollable(boolean b) {} public void setEditable(boolean b) {}
    public void setPassword(boolean b) {} public void setVisibleToUser(boolean b) {} public void setImportantForAccessibility(boolean b) {} public void setHeading(boolean b) {}
    public void setScreenReaderFocusable(boolean b) {} public void setContentInvalid(boolean b) {} public void setDismissable(boolean b) {} public void setMultiLine(boolean b) {}
    public void setShowingHintText(boolean b) {} public void setTextSelection(int s, int e) {} public void setInputType(int t) {} public void setMaxTextLength(int m) {}
    public void setMovementGranularities(int g) {} public void setLiveRegion(int m) {} public void setCollectionInfo(Object c) {} public void setCollectionItemInfo(Object c) {}
    public void setRangeInfo(Object r) {} public void setLabelFor(android.view.View v) {} public void setLabeledBy(android.view.View v) {} public void setTraversalBefore(android.view.View v) {}
    public void setTraversalAfter(android.view.View v) {} public void setPaneTitle(CharSequence t) {} public void setViewIdResourceName(String n) {} public android.os.Bundle getExtras() { return new android.os.Bundle(); }
    public int getActions() { return 0; } public java.util.List<AccessibilityAction> getActionList() { return new java.util.ArrayList<>(); } public int getChildCount() { return 0; }
    public boolean isClickable() { return false; } public boolean isEnabled() { return true; } public boolean isFocusable() { return false; } public boolean isCheckable() { return false; }
    public boolean isChecked() { return false; } public boolean isVisibleToUser() { return true; } public void setAvailableExtraData(java.util.List<String> l) {} public void setUniqueId(String id) {}
    public void setContainerTitle(CharSequence t) {} public void setTextEntryKey(boolean b) {} public void setRequestInitialAccessibilityFocus(boolean b) {}
    public static final class AccessibilityAction {
        public static final AccessibilityAction ACTION_CLICK = new AccessibilityAction(16, null), ACTION_LONG_CLICK = new AccessibilityAction(32, null),
            ACTION_SCROLL_FORWARD = new AccessibilityAction(4096, null), ACTION_SCROLL_BACKWARD = new AccessibilityAction(8192, null),
            ACTION_SHOW_ON_SCREEN = new AccessibilityAction(0x01020036, null), ACTION_SCROLL_TO_POSITION = new AccessibilityAction(0x01020037, null),
            ACTION_SCROLL_UP = new AccessibilityAction(0x01020038, null), ACTION_SCROLL_DOWN = new AccessibilityAction(0x0102003a, null),
            ACTION_SCROLL_LEFT = new AccessibilityAction(0x01020039, null), ACTION_SCROLL_RIGHT = new AccessibilityAction(0x0102003b, null),
            ACTION_CONTEXT_CLICK = new AccessibilityAction(0x0102003c, null), ACTION_SET_PROGRESS = new AccessibilityAction(0x0102003d, null),
            ACTION_MOVE_WINDOW = new AccessibilityAction(0x01020042, null), ACTION_SHOW_TOOLTIP = new AccessibilityAction(0x01020044, null),
            ACTION_HIDE_TOOLTIP = new AccessibilityAction(0x01020045, null), ACTION_PRESS_AND_HOLD = new AccessibilityAction(0x0102004a, null),
            ACTION_IME_ENTER = new AccessibilityAction(0x01020054, null), ACTION_FOCUS = new AccessibilityAction(1, null), ACTION_DISMISS = new AccessibilityAction(1048576, null),
            ACTION_EXPAND = new AccessibilityAction(262144, null), ACTION_COLLAPSE = new AccessibilityAction(524288, null), ACTION_PAGE_UP = new AccessibilityAction(0x01020046, null),
            ACTION_PAGE_DOWN = new AccessibilityAction(0x01020047, null), ACTION_PAGE_LEFT = new AccessibilityAction(0x01020048, null), ACTION_PAGE_RIGHT = new AccessibilityAction(0x01020049, null),
            ACTION_DRAG_START = new AccessibilityAction(0x0102005b, null), ACTION_DRAG_DROP = new AccessibilityAction(0x0102005c, null), ACTION_DRAG_CANCEL = new AccessibilityAction(0x0102005d, null),
            ACTION_SET_TEXT = new AccessibilityAction(2097152, null), ACTION_SELECT = new AccessibilityAction(4, null), ACTION_COPY = new AccessibilityAction(16384, null);
        private final int mId; private final CharSequence mLabel;
        public AccessibilityAction(int id, CharSequence label) { mId = id; mLabel = label; }
        public int getId() { return mId; } public CharSequence getLabel() { return mLabel; }
    }
    public static final class CollectionInfo { public static CollectionInfo obtain(int r, int c, boolean h) { return new CollectionInfo(); } public static CollectionInfo obtain(int r, int c, boolean h, int s) { return new CollectionInfo(); } public CollectionInfo(int r, int c, boolean h) {} CollectionInfo() {} }
    public static final class CollectionItemInfo { public static CollectionItemInfo obtain(int a, int b, int c, int d, boolean h) { return new CollectionItemInfo(); } public static CollectionItemInfo obtain(int a, int b, int c, int d, boolean h, boolean s) { return new CollectionItemInfo(); } CollectionItemInfo() {} }
    public static final class RangeInfo { public static RangeInfo obtain(int t, float min, float max, float cur) { return new RangeInfo(); } RangeInfo() {} }
    public static final class TouchDelegateInfo {}
}
