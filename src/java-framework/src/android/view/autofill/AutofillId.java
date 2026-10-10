package android.view.autofill;

/** Names a view (and maybe a virtual child of it) for autofill and content capture: Compose asks every view for one. */
public final class AutofillId implements android.os.Parcelable {
    public static final int NO_SESSION = 0;
    public static final AutofillId NO_AUTOFILL_ID = new AutofillId(0);
    private static final int FLAG_IS_VIRTUAL_INT = 0x1, FLAG_IS_VIRTUAL_LONG = 0x2, FLAG_HAS_SESSION = 0x4;
    private final int mViewId, mVirtualIntId;
    private final long mVirtualLongId;
    private int mFlags, mSessionId;
    public AutofillId(int id) { this(0, id, -1, NO_SESSION); }
    public AutofillId(AutofillId hostId, int virtualChildId) { this(FLAG_IS_VIRTUAL_INT, hostId.mViewId, virtualChildId, NO_SESSION); }
    public AutofillId(int hostId, int virtualChildId) { this(FLAG_IS_VIRTUAL_INT, hostId, virtualChildId, NO_SESSION); }
    public AutofillId(AutofillId hostId, long virtualChildId, int sessionId) { this(FLAG_IS_VIRTUAL_LONG | FLAG_HAS_SESSION, hostId.mViewId, virtualChildId, sessionId); }
    public AutofillId(AutofillId hostId, int virtualChildId, int sessionId) { this(FLAG_IS_VIRTUAL_INT | FLAG_HAS_SESSION, hostId.mViewId, virtualChildId, sessionId); }
    private AutofillId(int flags, int parentId, long virtualChildId, int sessionId) {
        mFlags = flags; mViewId = parentId;
        mVirtualIntId = (flags & FLAG_IS_VIRTUAL_INT) != 0 ? (int) virtualChildId : -1;
        mVirtualLongId = (flags & FLAG_IS_VIRTUAL_LONG) != 0 ? virtualChildId : -1;
        mSessionId = sessionId;
    }
    public static AutofillId create(android.view.View host, int virtualId) { return new AutofillId(host.getAutofillId(), virtualId); }
    public static AutofillId withoutSession(AutofillId id) { return new AutofillId(id.mFlags & ~FLAG_HAS_SESSION, id.mViewId, id.isVirtualLong() ? id.mVirtualLongId : id.mVirtualIntId, NO_SESSION); }
    public int getViewId() { return mViewId; }
    public int getVirtualChildIntId() { return mVirtualIntId; }
    public long getVirtualChildLongId() { return mVirtualLongId; }
    public int getAutofillVirtualId() { return mVirtualIntId; }
    public boolean isVirtualInt() { return (mFlags & FLAG_IS_VIRTUAL_INT) != 0; }
    public boolean isVirtualLong() { return (mFlags & FLAG_IS_VIRTUAL_LONG) != 0; }
    public boolean isVirtual() { return isVirtualInt() || isVirtualLong(); }
    public boolean isNonVirtual() { return !isVirtual(); }
    public boolean hasSession() { return (mFlags & FLAG_HAS_SESSION) != 0; }
    public int getSessionId() { return mSessionId; }
    public void setSessionId(int sessionId) { mFlags |= FLAG_HAS_SESSION; mSessionId = sessionId; }
    public void resetSessionId() { mFlags &= ~FLAG_HAS_SESSION; mSessionId = NO_SESSION; }
    public boolean isInAutofillSession() { return hasSession(); }
    public boolean equalsIgnoreSession(AutofillId o) { return o != null && mViewId == o.mViewId && mVirtualIntId == o.mVirtualIntId && mVirtualLongId == o.mVirtualLongId; }
    @Override public int hashCode() { int r = 31 + mViewId; r = 31 * r + mVirtualIntId; r = 31 * r + (int) (mVirtualLongId ^ (mVirtualLongId >>> 32)); return 31 * r + mSessionId; }
    @Override public boolean equals(Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof AutofillId)) return false;
        AutofillId o = (AutofillId) obj;
        return equalsIgnoreSession(o) && mSessionId == o.mSessionId;
    }
    @Override public String toString() {
        StringBuilder b = new StringBuilder().append(mViewId);
        if (isVirtualInt()) b.append(':').append(mVirtualIntId); else if (isVirtualLong()) b.append(':').append(mVirtualLongId);
        if (hasSession()) b.append('@').append(mSessionId);
        return b.toString();
    }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int flags) { p.writeInt(mViewId); p.writeInt(mFlags); p.writeInt(mSessionId); p.writeInt(mVirtualIntId); p.writeLong(mVirtualLongId); }
    public static final Creator<AutofillId> CREATOR = new Creator<AutofillId>() {
        public AutofillId createFromParcel(android.os.Parcel s) {
            int view = s.readInt(), flags = s.readInt(), session = s.readInt(), vi = s.readInt(); long vl = s.readLong();
            return new AutofillId(flags, view, (flags & FLAG_IS_VIRTUAL_LONG) != 0 ? vl : vi, session);
        }
        public AutofillId[] newArray(int size) { return new AutofillId[size]; }
    };
}
