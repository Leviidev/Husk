package android.app;
public class KeyguardManager {
    public boolean isKeyguardLocked() { return false; } public boolean isKeyguardSecure() { return false; } public boolean isDeviceSecure() { return true; } public boolean isDeviceLocked() { return false; }
    public void requestDismissKeyguard(Activity a, Object cb) {} public android.content.Intent createConfirmDeviceCredentialIntent(CharSequence t, CharSequence d) { return null; }
}
