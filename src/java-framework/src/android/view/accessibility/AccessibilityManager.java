package android.view.accessibility;
public final class AccessibilityManager {
    public interface AccessibilityStateChangeListener { void onAccessibilityStateChanged(boolean enabled); }
    public interface TouchExplorationStateChangeListener { void onTouchExplorationStateChanged(boolean enabled); }
    public interface AccessibilityServicesStateChangeListener { void onAccessibilityServicesStateChanged(AccessibilityManager m); }
    public static final int FLAG_CONTENT_ICONS = 1, FLAG_CONTENT_TEXT = 2, FLAG_CONTENT_CONTROLS = 4;
    public boolean isEnabled() { return false; } public boolean isTouchExplorationEnabled() { return false; }
    public java.util.List<android.accessibilityservice.AccessibilityServiceInfo> getEnabledAccessibilityServiceList(int feedback) { return new java.util.ArrayList<>(); }
    public java.util.List<android.accessibilityservice.AccessibilityServiceInfo> getInstalledAccessibilityServiceList() { return new java.util.ArrayList<>(); }
    public void sendAccessibilityEvent(AccessibilityEvent e) {} public void interrupt() {}
    public boolean addAccessibilityStateChangeListener(AccessibilityStateChangeListener l) { return true; } public boolean removeAccessibilityStateChangeListener(AccessibilityStateChangeListener l) { return true; }
    public boolean addTouchExplorationStateChangeListener(TouchExplorationStateChangeListener l) { return true; } public boolean removeTouchExplorationStateChangeListener(TouchExplorationStateChangeListener l) { return true; }
    public void addAccessibilityServicesStateChangeListener(AccessibilityServicesStateChangeListener l) {} public boolean removeAccessibilityServicesStateChangeListener(AccessibilityServicesStateChangeListener l) { return true; }
    public int getRecommendedTimeoutMillis(int original, int flags) { return original; }
    public boolean isAudioDescriptionRequested() { return false; }
}
