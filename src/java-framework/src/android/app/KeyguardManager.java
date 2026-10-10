package android.app;
public class KeyguardManager {
    public boolean isKeyguardLocked() { return false; } public boolean isKeyguardSecure() { return false; } public boolean isDeviceSecure() { return true; } public boolean isDeviceLocked() { return false; }
    public void requestDismissKeyguard(Activity a, KeyguardDismissCallback cb) { if (cb != null) cb.onDismissSucceeded(); } public android.content.Intent createConfirmDeviceCredentialIntent(CharSequence t, CharSequence d) { return null; }
    // ---- platform API stubs (tools/compat/genstubs.py)
    public static abstract class KeyguardDismissCallback {
        public KeyguardDismissCallback() {}
        public void onDismissCancelled() {}
        public void onDismissError() {}
        public void onDismissSucceeded() {}
    }
}
