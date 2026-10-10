package android.media;
public final class AudioDeviceInfo {
    public static final int TYPE_UNKNOWN = 0, TYPE_BUILTIN_EARPIECE = 1, TYPE_BUILTIN_SPEAKER = 2, TYPE_WIRED_HEADSET = 3, TYPE_WIRED_HEADPHONES = 4, TYPE_BLUETOOTH_SCO = 7, TYPE_BLUETOOTH_A2DP = 8, TYPE_HDMI = 9, TYPE_USB_DEVICE = 11, TYPE_USB_HEADSET = 22, TYPE_BUILTIN_MIC = 15, TYPE_BLE_HEADSET = 26;
    private final int mType;
    private AudioDeviceInfo(int t) { mType = t; }
    static AudioDeviceInfo huskSpeaker() { return new AudioDeviceInfo(TYPE_BUILTIN_SPEAKER); }
    public int getId() { return mType; }
    public CharSequence getProductName() { return "iPhone"; }
    public String getAddress() { return ""; }
    public boolean isSource() { return mType == TYPE_BUILTIN_MIC; }
    public boolean isSink() { return !isSource(); }
    public int[] getSampleRates() { return new int[] { 44100, 48000 }; }
    public int[] getChannelMasks() { return new int[] { AudioFormat.CHANNEL_OUT_STEREO }; }
    public int[] getChannelIndexMasks() { return new int[0]; }
    public int[] getChannelCounts() { return new int[] { 1, 2 }; }
    public int[] getEncodings() { return new int[] { AudioFormat.ENCODING_PCM_16BIT, AudioFormat.ENCODING_PCM_FLOAT }; }
    public int getType() { return mType; }
}
