package android.media;
public final class AudioFormat {
    public static final int ENCODING_PCM_16BIT = 2, ENCODING_PCM_8BIT = 3, ENCODING_PCM_FLOAT = 4, CHANNEL_OUT_MONO = 4, CHANNEL_OUT_STEREO = 12,
        CHANNEL_CONFIGURATION_MONO = 2, CHANNEL_CONFIGURATION_STEREO = 3, CHANNEL_IN_MONO = 16;
    public static class Builder {
        public Builder setEncoding(int e) { return this; } public Builder setSampleRate(int r) { return this; } public Builder setChannelMask(int m) { return this; }
        public AudioFormat build() { return new AudioFormat(); }
    }
}
