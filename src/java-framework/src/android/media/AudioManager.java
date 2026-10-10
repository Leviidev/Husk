package android.media;

/** Volumes and focus: the app always has focus, and volume is the phone's own (the app's settings are remembered, not applied). */
public class AudioManager {
    public static final int STREAM_VOICE_CALL = 0, STREAM_SYSTEM = 1, STREAM_RING = 2, STREAM_MUSIC = 3, STREAM_ALARM = 4, STREAM_NOTIFICATION = 5, STREAM_DTMF = 8, STREAM_ACCESSIBILITY = 10, USE_DEFAULT_STREAM_TYPE = Integer.MIN_VALUE;
    public static final int AUDIOFOCUS_NONE = 0, AUDIOFOCUS_GAIN = 1, AUDIOFOCUS_GAIN_TRANSIENT = 2, AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK = 3, AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE = 4,
        AUDIOFOCUS_LOSS = -1, AUDIOFOCUS_LOSS_TRANSIENT = -2, AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK = -3, AUDIOFOCUS_REQUEST_FAILED = 0, AUDIOFOCUS_REQUEST_GRANTED = 1, AUDIOFOCUS_REQUEST_DELAYED = 2;
    public static final int RINGER_MODE_SILENT = 0, RINGER_MODE_VIBRATE = 1, RINGER_MODE_NORMAL = 2;
    public static final int ADJUST_RAISE = 1, ADJUST_LOWER = -1, ADJUST_SAME = 0, ADJUST_MUTE = -100, ADJUST_UNMUTE = 100, ADJUST_TOGGLE_MUTE = 101;
    public static final int FLAG_SHOW_UI = 1, FLAG_ALLOW_RINGER_MODES = 2, FLAG_PLAY_SOUND = 4, FLAG_REMOVE_SOUND_AND_VIBRATE = 8, FLAG_VIBRATE = 16;
    public static final int MODE_INVALID = -2, MODE_CURRENT = -1, MODE_NORMAL = 0, MODE_RINGTONE = 1, MODE_IN_CALL = 2, MODE_IN_COMMUNICATION = 3;
    public static final int FX_KEY_CLICK = 0, FX_FOCUS_NAVIGATION_UP = 1, FX_FOCUS_NAVIGATION_DOWN = 2, FX_FOCUS_NAVIGATION_LEFT = 3, FX_FOCUS_NAVIGATION_RIGHT = 4, FX_KEYPRESS_STANDARD = 5, FX_KEYPRESS_SPACEBAR = 6, FX_KEYPRESS_DELETE = 7, FX_KEYPRESS_RETURN = 8, FX_KEYPRESS_INVALID = 9;
    public static final int GET_DEVICES_INPUTS = 1, GET_DEVICES_OUTPUTS = 2, GET_DEVICES_ALL = 3, ERROR = -1, SUCCESS = 0, AUDIO_SESSION_ID_GENERATE = 0;
    public static final String ACTION_AUDIO_BECOMING_NOISY = "android.media.AUDIO_BECOMING_NOISY", ACTION_HEADSET_PLUG = "android.intent.action.HEADSET_PLUG";
    public static final String PROPERTY_OUTPUT_SAMPLE_RATE = "android.media.property.OUTPUT_SAMPLE_RATE", PROPERTY_OUTPUT_FRAMES_PER_BUFFER = "android.media.property.OUTPUT_FRAMES_PER_BUFFER", PROPERTY_SUPPORT_AUDIO_SOURCE_UNPROCESSED = "android.media.property.SUPPORT_AUDIO_SOURCE_UNPROCESSED";
    public interface OnAudioFocusChangeListener { void onAudioFocusChange(int focusChange); }
    public static abstract class AudioPlaybackCallback { public void onPlaybackConfigChanged(java.util.List<?> configs) {} }
    private static final int[] sVolume = { 5, 7, 5, 11, 6, 5, 0, 0, 11, 0, 11 };
    private static int sMode = MODE_NORMAL, sSession = 1;
    public AudioManager() {}
    public AudioManager(android.content.Context c) {}
    public int getStreamVolume(int s) { return s >= 0 && s < sVolume.length ? sVolume[s] : 11; }
    public int getStreamMaxVolume(int s) { return 15; }
    public int getStreamMinVolume(int s) { return 0; }
    public float getStreamVolumeDb(int s, int i, int d) { return 0; }
    public void setStreamVolume(int s, int v, int f) { if (s >= 0 && s < sVolume.length) sVolume[s] = Math.max(0, Math.min(15, v)); }
    public void adjustStreamVolume(int s, int d, int f) { if (s >= 0 && s < sVolume.length) setStreamVolume(s, sVolume[s] + (d == ADJUST_RAISE ? 1 : d == ADJUST_LOWER ? -1 : 0), f); }
    public void adjustVolume(int d, int f) { adjustStreamVolume(STREAM_MUSIC, d, f); }
    public void adjustSuggestedStreamVolume(int d, int s, int f) { adjustStreamVolume(s, d, f); }
    public boolean isStreamMute(int s) { return false; }
    @Deprecated public void setStreamMute(int s, boolean m) {}
    @Deprecated public void setStreamSolo(int s, boolean m) {}
    public boolean isVolumeFixed() { return true; }
    public int requestAudioFocus(OnAudioFocusChangeListener l, int s, int d) { return AUDIOFOCUS_REQUEST_GRANTED; }
    public int requestAudioFocus(AudioFocusRequest r) { return AUDIOFOCUS_REQUEST_GRANTED; }
    public int abandonAudioFocus(OnAudioFocusChangeListener l) { return AUDIOFOCUS_REQUEST_GRANTED; }
    public int abandonAudioFocusRequest(AudioFocusRequest r) { return AUDIOFOCUS_REQUEST_GRANTED; }
    public int getRingerMode() { return RINGER_MODE_NORMAL; }
    public void setRingerMode(int m) {}
    public int getMode() { return sMode; }
    public void setMode(int m) { sMode = m; }
    public boolean isMusicActive() { return false; }
    public boolean isSpeakerphoneOn() { return true; }
    public void setSpeakerphoneOn(boolean on) {}
    public boolean isMicrophoneMute() { return false; }
    public void setMicrophoneMute(boolean on) {}
    public boolean isBluetoothScoOn() { return false; }
    public boolean isBluetoothA2dpOn() { return false; }
    public boolean isWiredHeadsetOn() { return false; }
    public void startBluetoothSco() {} public void stopBluetoothSco() {} public void setBluetoothScoOn(boolean o) {}
    public int generateAudioSessionId() { return ++sSession; }
    public void playSoundEffect(int effect) {}
    public void playSoundEffect(int effect, float volume) {}
    public void loadSoundEffects() {} public void unloadSoundEffects() {}
    public AudioDeviceInfo[] getDevices(int flags) { return (flags & GET_DEVICES_OUTPUTS) != 0 ? new AudioDeviceInfo[] { AudioDeviceInfo.huskSpeaker() } : new AudioDeviceInfo[0]; }
    public void registerAudioDeviceCallback(AudioDeviceCallback cb, android.os.Handler h) {}
    public void unregisterAudioDeviceCallback(AudioDeviceCallback cb) {}
    public void registerAudioPlaybackCallback(AudioPlaybackCallback cb, android.os.Handler h) {}
    public void unregisterAudioPlaybackCallback(AudioPlaybackCallback cb) {}
    public void dispatchMediaKeyEvent(android.view.KeyEvent e) {}
    public String getParameters(String keys) { return ""; }
    public void setParameters(String kv) {}
    public String getProperty(String k) { return PROPERTY_OUTPUT_SAMPLE_RATE.equals(k) ? "44100" : PROPERTY_OUTPUT_FRAMES_PER_BUFFER.equals(k) ? "512" : null; }
    public boolean isStreamMuteHusk() { return false; }
    public static boolean isOffloadedPlaybackSupported(AudioFormat f, AudioAttributes a) { return false; }
    // ---- generated by tools/compat/genstubs.py: signatures only
    public static abstract class AudioRecordingCallback {
        protected AudioRecordingCallback() {}
    }
}
