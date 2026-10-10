package android.accessibilityservice;
public class AccessibilityServiceInfo {
    public static final int FEEDBACK_SPOKEN = 1, FEEDBACK_HAPTIC = 2, FEEDBACK_AUDIBLE = 4, FEEDBACK_VISUAL = 8, FEEDBACK_GENERIC = 16, FEEDBACK_ALL_MASK = -1, FEEDBACK_BRAILLE = 32;
    public String getId() { return ""; } public android.content.pm.ResolveInfo getResolveInfo() { return null; }
    public int feedbackType, eventTypes, flags;
}
