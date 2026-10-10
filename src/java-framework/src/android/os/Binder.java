package android.os;

/** A local binder: everything runs in one process here, so transact() calls onTransact() directly. */
public class Binder implements IBinder {
    private IInterface mOwner;
    private String mDescriptor;
    public Binder() {}
    public Binder(String descriptor) { mDescriptor = descriptor; }
    public static final int getCallingPid() { return android.os.Process.myPid(); }
    public static final int getCallingUid() { return android.os.Process.myUid(); }
    public static final int getCallingUidOrThrow() { return getCallingUid(); }
    public static final UserHandle getCallingUserHandle() { return UserHandle.getUserHandleForUid(getCallingUid()); }
    public static final long clearCallingIdentity() { return 0; }
    public static final void restoreCallingIdentity(long token) {}
    public static final int getThreadStrictModePolicy() { return 0; }
    public static final void setThreadStrictModePolicy(int policyMask) {}
    public static final void flushPendingCommands() {}
    public static final void joinThreadPool() {}
    public static boolean isProxy(IInterface iface) { return false; }
    public void attachInterface(IInterface owner, String descriptor) { mOwner = owner; mDescriptor = descriptor; }
    public String getInterfaceDescriptor() { return mDescriptor; }
    public boolean pingBinder() { return true; }
    public boolean isBinderAlive() { return true; }
    public IInterface queryLocalInterface(String descriptor) { if (mDescriptor != null && mDescriptor.equals(descriptor)) return mOwner; return null; }
    protected boolean onTransact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
        if (code == INTERFACE_TRANSACTION) { if (reply != null) reply.writeString(getInterfaceDescriptor()); return true; }
        return false;
    }
    public void dump(java.io.FileDescriptor fd, String[] args) {}
    public void dumpAsync(java.io.FileDescriptor fd, String[] args) {}
    protected void dump(java.io.FileDescriptor fd, java.io.PrintWriter fout, String[] args) {}
    public final boolean transact(int code, Parcel data, Parcel reply, int flags) throws RemoteException {
        if (data != null) data.setDataPosition(0);
        boolean r = onTransact(code, data, reply, flags);
        if (reply != null) reply.setDataPosition(0);
        return r;
    }
    public void linkToDeath(DeathRecipient recipient, int flags) {}
    public boolean unlinkToDeath(DeathRecipient recipient, int flags) { return true; }
}
