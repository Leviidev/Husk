package android.os;
public final class VibrationAttributes implements Parcelable {
    public static final int USAGE_UNKNOWN = 0, USAGE_ALARM = 17, USAGE_RINGTONE = 33, USAGE_NOTIFICATION = 49, USAGE_COMMUNICATION_REQUEST = 65, USAGE_TOUCH = 18, USAGE_PHYSICAL_EMULATION = 34, USAGE_HARDWARE_FEEDBACK = 50, USAGE_ACCESSIBILITY = 66, USAGE_MEDIA = 19;
    private final int mUsage;
    private VibrationAttributes(int u) { mUsage = u; }
    public static VibrationAttributes createForUsage(int u) { return new VibrationAttributes(u); }
    public int getUsage() { return mUsage; }
    public int getFlags() { return 0; }
    public static final class Builder { private int u; public Builder() {} public Builder(VibrationAttributes a) { u = a.mUsage; } public Builder setUsage(int x) { u = x; return this; } public Builder setFlags(int f, int m) { return this; } public VibrationAttributes build() { return new VibrationAttributes(u); } }
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel p, int f) { p.writeInt(mUsage); }
    public static final Creator<VibrationAttributes> CREATOR = new Creator<VibrationAttributes>() { public VibrationAttributes createFromParcel(Parcel p) { return new VibrationAttributes(p.readInt()); } public VibrationAttributes[] newArray(int n) { return new VibrationAttributes[n]; } };
}
