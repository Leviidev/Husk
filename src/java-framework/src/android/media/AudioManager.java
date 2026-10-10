package android.media;
public class AudioManager {
    public static final int STREAM_VOICE_CALL = 0, STREAM_SYSTEM = 1, STREAM_RING = 2, STREAM_MUSIC = 3, STREAM_ALARM = 4, STREAM_NOTIFICATION = 5;
    public static final int AUDIOFOCUS_GAIN = 1, AUDIOFOCUS_REQUEST_GRANTED = 1, AUDIOFOCUS_LOSS = -1, RINGER_MODE_NORMAL = 2, ADJUST_RAISE = 1, ADJUST_LOWER = -1, FLAG_SHOW_UI = 1;
    public static final String PROPERTY_OUTPUT_SAMPLE_RATE = "android.media.property.OUTPUT_SAMPLE_RATE", PROPERTY_OUTPUT_FRAMES_PER_BUFFER = "android.media.property.OUTPUT_FRAMES_PER_BUFFER";
    public interface OnAudioFocusChangeListener { void onAudioFocusChange(int c); }
    public int getStreamVolume(int s) { return 10; }
    public int getStreamMaxVolume(int s) { return 15; }
    public void setStreamVolume(int s, int v, int f) {}
    public void adjustStreamVolume(int s, int d, int f) {}
    public int requestAudioFocus(OnAudioFocusChangeListener l, int s, int d) { return AUDIOFOCUS_REQUEST_GRANTED; }
    public int abandonAudioFocus(OnAudioFocusChangeListener l) { return AUDIOFOCUS_REQUEST_GRANTED; }
    public int getRingerMode() { return RINGER_MODE_NORMAL; }
    public boolean isMusicActive() { return false; }
    public String getProperty(String k) { return PROPERTY_OUTPUT_SAMPLE_RATE.equals(k) ? "48000" : PROPERTY_OUTPUT_FRAMES_PER_BUFFER.equals(k) ? "256" : null; }
}
