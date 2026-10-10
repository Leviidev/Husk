package android.os;
public final class UserHandle implements Parcelable {
    private final int mHandle;
    public UserHandle(int h) { mHandle = h; }
    public static UserHandle getUserHandleForUid(int uid) { return new UserHandle(uid / 100000); }
    public static UserHandle of(int id) { return new UserHandle(id); }
    public static int myUserId() { return 0; }
    public int getIdentifier() { return mHandle; }
    public static int getUserId(int uid) { return uid / 100000; }
    public static boolean isApp(int uid) { return uid >= 10000; }
    public int describeContents() { return 0; }
    @Override public boolean equals(Object o) { return o instanceof UserHandle && ((UserHandle) o).mHandle == mHandle; }
    @Override public int hashCode() { return mHandle; }
}
