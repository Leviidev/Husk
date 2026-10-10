package android.media;

public final class AudioAttributes implements android.os.Parcelable {
    public static final int USAGE_UNKNOWN = 0, USAGE_MEDIA = 1, USAGE_VOICE_COMMUNICATION = 2, USAGE_VOICE_COMMUNICATION_SIGNALLING = 3, USAGE_ALARM = 4, USAGE_NOTIFICATION = 5, USAGE_NOTIFICATION_RINGTONE = 6,
        USAGE_NOTIFICATION_EVENT = 10, USAGE_ASSISTANCE_ACCESSIBILITY = 11, USAGE_ASSISTANCE_NAVIGATION_GUIDANCE = 12, USAGE_ASSISTANCE_SONIFICATION = 13, USAGE_GAME = 14, USAGE_ASSISTANT = 16;
    public static final int CONTENT_TYPE_UNKNOWN = 0, CONTENT_TYPE_SPEECH = 1, CONTENT_TYPE_MUSIC = 2, CONTENT_TYPE_MOVIE = 3, CONTENT_TYPE_SONIFICATION = 4;
    public static final int FLAG_AUDIBILITY_ENFORCED = 1, FLAG_HW_AV_SYNC = 16, FLAG_LOW_LATENCY = 256;
    public static final int ALLOW_CAPTURE_BY_ALL = 1, ALLOW_CAPTURE_BY_SYSTEM = 2, ALLOW_CAPTURE_BY_NONE = 3;
    private final int mUsage, mContent, mFlags, mStream;
    private AudioAttributes(int u, int c, int f, int s) { mUsage = u; mContent = c; mFlags = f; mStream = s; }
    public int getUsage() { return mUsage; }
    public int getContentType() { return mContent; }
    public int getFlags() { return mFlags; }
    public int getVolumeControlStream() { return mStream; }
    public int getAllowedCapturePolicy() { return ALLOW_CAPTURE_BY_ALL; }
    public boolean areHapticChannelsMuted() { return true; }
    public boolean isContentSpatialized() { return false; }
    public int getSpatializationBehavior() { return 0; }
    public static class Builder {
        private int u, c, f, s = AudioManager.STREAM_MUSIC;
        public Builder() {}
        public Builder(AudioAttributes a) { u = a.mUsage; c = a.mContent; f = a.mFlags; s = a.mStream; }
        public Builder setUsage(int x) { u = x; return this; }
        public Builder setContentType(int x) { c = x; return this; }
        public Builder setFlags(int x) { f |= x; return this; }
        public Builder setLegacyStreamType(int x) { s = x; return this; }
        public Builder setAllowedCapturePolicy(int p) { return this; }
        public Builder setHapticChannelsMuted(boolean m) { return this; }
        public Builder setSpatializationBehavior(int b) { return this; }
        public Builder setIsContentSpatialized(boolean b) { return this; }
        public AudioAttributes build() { return new AudioAttributes(u, c, f, s); }
    }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int fl) { p.writeInt(mUsage); p.writeInt(mContent); p.writeInt(mFlags); p.writeInt(mStream); }
    public static final Creator<AudioAttributes> CREATOR = new Creator<AudioAttributes>() { public AudioAttributes createFromParcel(android.os.Parcel p) { return new AudioAttributes(p.readInt(), p.readInt(), p.readInt(), p.readInt()); } public AudioAttributes[] newArray(int n) { return new AudioAttributes[n]; } };
}
