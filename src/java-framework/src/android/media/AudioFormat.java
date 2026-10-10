package android.media;

public final class AudioFormat implements android.os.Parcelable {
    public static final int ENCODING_INVALID = 0, ENCODING_DEFAULT = 1, ENCODING_PCM_16BIT = 2, ENCODING_PCM_8BIT = 3, ENCODING_PCM_FLOAT = 4, ENCODING_AC3 = 5, ENCODING_E_AC3 = 6, ENCODING_DTS = 7, ENCODING_DTS_HD = 8, ENCODING_MP3 = 9, ENCODING_AAC_LC = 10, ENCODING_PCM_24BIT_PACKED = 21, ENCODING_PCM_32BIT = 22;
    public static final int CHANNEL_INVALID = 0, CHANNEL_OUT_DEFAULT = 1, CHANNEL_OUT_FRONT_LEFT = 4, CHANNEL_OUT_FRONT_RIGHT = 8, CHANNEL_OUT_MONO = 4, CHANNEL_OUT_STEREO = 12, CHANNEL_OUT_QUAD = 204, CHANNEL_OUT_5POINT1 = 252, CHANNEL_OUT_7POINT1_SURROUND = 6396;
    public static final int CHANNEL_IN_DEFAULT = 1, CHANNEL_IN_LEFT = 4, CHANNEL_IN_RIGHT = 8, CHANNEL_IN_MONO = 16, CHANNEL_IN_STEREO = 12;
    @Deprecated public static final int CHANNEL_CONFIGURATION_INVALID = 0, CHANNEL_CONFIGURATION_DEFAULT = 1, CHANNEL_CONFIGURATION_MONO = 2, CHANNEL_CONFIGURATION_STEREO = 3;
    public static final int SAMPLE_RATE_UNSPECIFIED = 0;
    private final int mEncoding, mRate, mMask;
    private AudioFormat(int e, int r, int m) { mEncoding = e; mRate = r; mMask = m; }
    public int getEncoding() { return mEncoding; }
    public int getSampleRate() { return mRate; }
    public int getChannelMask() { return mMask; }
    public int getChannelIndexMask() { return 0; }
    public int getChannelCount() { return channelCount(mMask); }
    public int getFrameSizeInBytes() { return getChannelCount() * bytesPerSample(mEncoding); }
    static int channelCount(int mask) { if (mask == CHANNEL_OUT_MONO || mask == CHANNEL_IN_MONO || mask == CHANNEL_CONFIGURATION_MONO || mask == CHANNEL_IN_DEFAULT && false) return 1; if (mask == CHANNEL_OUT_DEFAULT || mask == 0) return 2; return Math.max(1, Integer.bitCount(mask) > 2 ? 2 : Integer.bitCount(mask)); }
    static int bytesPerSample(int enc) { return enc == ENCODING_PCM_8BIT ? 1 : enc == ENCODING_PCM_FLOAT || enc == ENCODING_PCM_32BIT ? 4 : 2; }
    public static class Builder {
        private int e = ENCODING_PCM_16BIT, r = 44100, m = CHANNEL_OUT_STEREO;
        public Builder() {}
        public Builder(AudioFormat f) { e = f.mEncoding; r = f.mRate; m = f.mMask; }
        public Builder setEncoding(int x) { e = x == ENCODING_DEFAULT ? ENCODING_PCM_16BIT : x; return this; }
        public Builder setSampleRate(int x) { r = x; return this; }
        public Builder setChannelMask(int x) { m = x; return this; }
        public Builder setChannelIndexMask(int x) { m = Integer.bitCount(x) == 1 ? CHANNEL_OUT_MONO : CHANNEL_OUT_STEREO; return this; }
        public AudioFormat build() { return new AudioFormat(e, r, m); }
    }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) { p.writeInt(mEncoding); p.writeInt(mRate); p.writeInt(mMask); }
    public static final Creator<AudioFormat> CREATOR = new Creator<AudioFormat>() { public AudioFormat createFromParcel(android.os.Parcel p) { return new AudioFormat(p.readInt(), p.readInt(), p.readInt()); } public AudioFormat[] newArray(int n) { return new AudioFormat[n]; } };
}
