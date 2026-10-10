package android.view.accessibility;
public abstract class AccessibilityNodeProvider {
    public static final int HOST_VIEW_ID = -1;
    public AccessibilityNodeInfo createAccessibilityNodeInfo(int id) { return null; }
    public boolean performAction(int id, int action, android.os.Bundle args) { return false; }
    public java.util.List<AccessibilityNodeInfo> findAccessibilityNodeInfosByText(String t, int id) { return null; }
    public AccessibilityNodeInfo findFocus(int f) { return null; }
}
