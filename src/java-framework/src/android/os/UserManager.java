package android.os;
public class UserManager {
    public static boolean supportsMultipleUsers() { return false; }
    public boolean isUserUnlocked() { return true; }
    public boolean isSystemUser() { return true; }
    public boolean isUserAGoat() { return false; }
    public boolean isDemoUser() { return false; }
    public boolean isManagedProfile() { return false; }
    public boolean hasUserRestriction(String r) { return false; }
    public Bundle getUserRestrictions() { return new Bundle(); }
    public Bundle getApplicationRestrictions(String pkg) { return new Bundle(); }
    public long getSerialNumberForUser(UserHandle u) { return 0; }
    public java.util.List<UserHandle> getUserProfiles() { java.util.ArrayList<UserHandle> l = new java.util.ArrayList<>(); l.add(UserHandle.getUserHandleForUid(0)); return l; }
    public String getUserName() { return "Owner"; }
}
