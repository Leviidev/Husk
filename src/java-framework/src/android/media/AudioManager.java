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
        // ---- generated by tools/compat/fillmembers.py (AudioRecordingCallback): the platform's members this class does not write (signatures only)
        public void onRecordingConfigChanged(java.util.List p0) {}
        // ---- end of generated members (AudioRecordingCallback)
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final java.lang.String ACTION_HDMI_AUDIO_PLUG = "android.media.action.HDMI_AUDIO_PLUG";
    public static final java.lang.String ACTION_MICROPHONE_MUTE_CHANGED = "android.media.action.MICROPHONE_MUTE_CHANGED";
    public static final java.lang.String ACTION_SCO_AUDIO_STATE_CHANGED = "android.media.SCO_AUDIO_STATE_CHANGED";
    public static final java.lang.String ACTION_SCO_AUDIO_STATE_UPDATED = "android.media.ACTION_SCO_AUDIO_STATE_UPDATED";
    public static final java.lang.String ACTION_SPEAKERPHONE_STATE_CHANGED = "android.media.action.SPEAKERPHONE_STATE_CHANGED";
    public static final java.lang.String ACTION_VOLUME_CHANGED = "android.media.VOLUME_CHANGED_ACTION";
    public static final int AUDIOFOCUS_FLAGS_APPS = 3;
    public static final int AUDIOFOCUS_FLAGS_SYSTEM = 7;
    public static final int AUDIOFOCUS_FLAG_DELAY_OK = 1;
    public static final int AUDIOFOCUS_FLAG_LOCK = 4;
    public static final int AUDIOFOCUS_FLAG_PAUSES_ON_DUCKABLE_LOSS = 2;
    public static final int AUDIOFOCUS_FLAG_TEST = 8;
    public static final int AUDIOFOCUS_REQUEST_WAITING_FOR_EXT_POLICY = 100;
    public static final int AUDIO_DEVICE_CATEGORY_CARKIT = 4;
    public static final int AUDIO_DEVICE_CATEGORY_HEADPHONES = 3;
    public static final int AUDIO_DEVICE_CATEGORY_HEARING_AID = 6;
    public static final int AUDIO_DEVICE_CATEGORY_OTHER = 1;
    public static final int AUDIO_DEVICE_CATEGORY_RECEIVER = 7;
    public static final int AUDIO_DEVICE_CATEGORY_SPEAKER = 2;
    public static final int AUDIO_DEVICE_CATEGORY_UNKNOWN = 0;
    public static final int AUDIO_DEVICE_CATEGORY_WATCH = 5;
    public static final long CALL_REDIRECTION_AUDIO_MODES = 189472651L;
    public static final int CALL_REDIRECT_NONE = 0;
    public static final int CALL_REDIRECT_PSTN = 1;
    public static final int CALL_REDIRECT_VOIP = 2;
    public static final int CSD_WARNING_ACCUMULATION_START = 4;
    public static final int CSD_WARNING_DOSE_REACHED_1X = 1;
    public static final int CSD_WARNING_DOSE_REPEATED_5X = 2;
    public static final int CSD_WARNING_MOMENTARY_EXPOSURE = 3;
    public static final int DEVICE_CONNECTION_STATE_CONNECTED = 1;
    public static final int DEVICE_CONNECTION_STATE_DISCONNECTED = 0;
    public static final int DEVICE_IN_ANLG_DOCK_HEADSET = -2147483136;
    public static final int DEVICE_IN_BACK_MIC = -2147483520;
    public static final int DEVICE_IN_BLE_HEADSET = -1610612736;
    public static final int DEVICE_IN_BLUETOOTH_SCO_HEADSET = -2147483640;
    public static final int DEVICE_IN_BUILTIN_MIC = -2147483644;
    public static final int DEVICE_IN_DGTL_DOCK_HEADSET = -2147482624;
    public static final int DEVICE_IN_ECHO_REFERENCE = -1879048192;
    public static final int DEVICE_IN_FM_TUNER = -2147475456;
    public static final int DEVICE_IN_HDMI = -2147483616;
    public static final int DEVICE_IN_HDMI_ARC = -2013265920;
    public static final int DEVICE_IN_HDMI_EARC = -2013265919;
    public static final int DEVICE_IN_LINE = -2147450880;
    public static final int DEVICE_IN_LOOPBACK = -2147221504;
    public static final int DEVICE_IN_SPDIF = -2147418112;
    public static final int DEVICE_IN_TELEPHONY_RX = -2147483584;
    public static final int DEVICE_IN_TV_TUNER = -2147467264;
    public static final int DEVICE_IN_USB_ACCESSORY = -2147481600;
    public static final int DEVICE_IN_USB_DEVICE = -2147479552;
    public static final int DEVICE_IN_WIRED_HEADSET = -2147483632;
    public static final int DEVICE_NONE = 0;
    public static final int DEVICE_OUT_ANLG_DOCK_HEADSET = 2048;
    public static final int DEVICE_OUT_AUX_DIGITAL = 1024;
    public static final int DEVICE_OUT_BLE_BROADCAST = 536870914;
    public static final int DEVICE_OUT_BLE_HEADSET = 536870912;
    public static final int DEVICE_OUT_BLE_SPEAKER = 536870913;
    public static final int DEVICE_OUT_BLUETOOTH_A2DP = 128;
    public static final int DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES = 256;
    public static final int DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER = 512;
    public static final int DEVICE_OUT_BLUETOOTH_SCO = 16;
    public static final int DEVICE_OUT_BLUETOOTH_SCO_CARKIT = 64;
    public static final int DEVICE_OUT_BLUETOOTH_SCO_HEADSET = 32;
    public static final int DEVICE_OUT_DEFAULT = 1073741824;
    public static final int DEVICE_OUT_DGTL_DOCK_HEADSET = 4096;
    public static final int DEVICE_OUT_EARPIECE = 1;
    public static final int DEVICE_OUT_ECHO_CANCELLER = 268435456;
    public static final int DEVICE_OUT_FM = 1048576;
    public static final int DEVICE_OUT_HDMI = 1024;
    public static final int DEVICE_OUT_HDMI_ARC = 262144;
    public static final int DEVICE_OUT_HDMI_EARC = 262145;
    public static final int DEVICE_OUT_LINE = 131072;
    public static final int DEVICE_OUT_MULTICHANNEL_GROUP = 8388609;
    public static final int DEVICE_OUT_REMOTE_SUBMIX = 32768;
    public static final int DEVICE_OUT_SPDIF = 524288;
    public static final int DEVICE_OUT_SPEAKER = 2;
    public static final int DEVICE_OUT_TELEPHONY_TX = 65536;
    public static final int DEVICE_OUT_USB_ACCESSORY = 8192;
    public static final int DEVICE_OUT_USB_DEVICE = 16384;
    public static final int DEVICE_OUT_USB_HEADSET = 67108864;
    public static final int DEVICE_OUT_WIRED_HEADPHONE = 8;
    public static final int DEVICE_OUT_WIRED_HEADSET = 4;
    public static final int DEVICE_VOLUME_BEHAVIOR_ABSOLUTE = 3;
    public static final int DEVICE_VOLUME_BEHAVIOR_ABSOLUTE_ADJUST_ONLY = 5;
    public static final int DEVICE_VOLUME_BEHAVIOR_ABSOLUTE_MULTI_MODE = 4;
    public static final int DEVICE_VOLUME_BEHAVIOR_FIXED = 2;
    public static final int DEVICE_VOLUME_BEHAVIOR_FULL = 1;
    public static final int DEVICE_VOLUME_BEHAVIOR_UNSET = -1;
    public static final int DEVICE_VOLUME_BEHAVIOR_VARIABLE = 0;
    public static final int DIRECT_PLAYBACK_BITSTREAM_SUPPORTED = 4;
    public static final int DIRECT_PLAYBACK_NOT_SUPPORTED = 0;
    public static final int DIRECT_PLAYBACK_OFFLOAD_GAPLESS_SUPPORTED = 3;
    public static final int DIRECT_PLAYBACK_OFFLOAD_SUPPORTED = 1;
    public static final int ENCODED_SURROUND_OUTPUT_ALWAYS = 2;
    public static final int ENCODED_SURROUND_OUTPUT_AUTO = 0;
    public static final int ENCODED_SURROUND_OUTPUT_MANUAL = 3;
    public static final int ENCODED_SURROUND_OUTPUT_NEVER = 1;
    public static final int ENCODED_SURROUND_OUTPUT_UNKNOWN = -1;
    public static final int ERROR_BAD_VALUE = -2;
    public static final int ERROR_DEAD_OBJECT = -6;
    public static final int ERROR_INVALID_OPERATION = -3;
    public static final int ERROR_NO_INIT = -5;
    public static final int ERROR_PERMISSION_DENIED = -4;
    public static final java.lang.String EXTRA_AUDIO_PLUG_STATE = "android.media.extra.AUDIO_PLUG_STATE";
    public static final java.lang.String EXTRA_ENCODINGS = "android.media.extra.ENCODINGS";
    public static final java.lang.String EXTRA_MASTER_VOLUME_MUTED = "android.media.EXTRA_MASTER_VOLUME_MUTED";
    public static final java.lang.String EXTRA_MAX_CHANNEL_COUNT = "android.media.extra.MAX_CHANNEL_COUNT";
    public static final java.lang.String EXTRA_PREV_VOLUME_STREAM_DEVICES = "android.media.EXTRA_PREV_VOLUME_STREAM_DEVICES";
    public static final java.lang.String EXTRA_PREV_VOLUME_STREAM_VALUE = "android.media.EXTRA_PREV_VOLUME_STREAM_VALUE";
    public static final java.lang.String EXTRA_RINGER_MODE = "android.media.EXTRA_RINGER_MODE";
    public static final java.lang.String EXTRA_SCO_AUDIO_PREVIOUS_STATE = "android.media.extra.SCO_AUDIO_PREVIOUS_STATE";
    public static final java.lang.String EXTRA_SCO_AUDIO_STATE = "android.media.extra.SCO_AUDIO_STATE";
    public static final java.lang.String EXTRA_STREAM_VOLUME_MUTED = "android.media.EXTRA_STREAM_VOLUME_MUTED";
    public static final java.lang.String EXTRA_VIBRATE_SETTING = "android.media.EXTRA_VIBRATE_SETTING";
    public static final java.lang.String EXTRA_VIBRATE_TYPE = "android.media.EXTRA_VIBRATE_TYPE";
    public static final java.lang.String EXTRA_VOLUME_STREAM_DEVICES = "android.media.EXTRA_VOLUME_STREAM_DEVICES";
    public static final java.lang.String EXTRA_VOLUME_STREAM_TYPE = "android.media.EXTRA_VOLUME_STREAM_TYPE";
    public static final java.lang.String EXTRA_VOLUME_STREAM_TYPE_ALIAS = "android.media.EXTRA_VOLUME_STREAM_TYPE_ALIAS";
    public static final java.lang.String EXTRA_VOLUME_STREAM_VALUE = "android.media.EXTRA_VOLUME_STREAM_VALUE";
    public static final int FLAG_ABSOLUTE_VOLUME = 8192;
    public static final int FLAG_ACTIVE_MEDIA_ONLY = 512;
    public static final int FLAG_BLUETOOTH_ABS_VOLUME = 64;
    public static final int FLAG_FIXED_VOLUME = 32;
    public static final int FLAG_FROM_KEY = 4096;
    public static final int FLAG_HDMI_SYSTEM_AUDIO_VOLUME = 256;
    public static final int FLAG_SHOW_SILENT_HINT = 128;
    public static final int FLAG_SHOW_UI_WARNINGS = 1024;
    public static final int FLAG_SHOW_VIBRATE_HINT = 2048;
    public static final int FX_BACK = 10;
    public static final int FX_FOCUS_NAVIGATION_REPEAT_1 = 12;
    public static final int FX_FOCUS_NAVIGATION_REPEAT_2 = 13;
    public static final int FX_FOCUS_NAVIGATION_REPEAT_3 = 14;
    public static final int FX_FOCUS_NAVIGATION_REPEAT_4 = 15;
    public static final int FX_HOME = 11;
    public static final java.lang.String INTERNAL_RINGER_MODE_CHANGED_ACTION = "android.media.INTERNAL_RINGER_MODE_CHANGED_ACTION";
    public static final java.lang.String MASTER_MUTE_CHANGED_ACTION = "android.media.MASTER_MUTE_CHANGED_ACTION";
    public static final int MODE_ASSISTANT_CONVERSATION = 7;
    public static final int MODE_CALL_REDIRECT = 5;
    public static final int MODE_CALL_SCREENING = 4;
    public static final int MODE_COMMUNICATION_REDIRECT = 6;
    public static final int NUM_NAVIGATION_REPEAT_SOUND_EFFECTS = 4;
    public static final int NUM_SOUND_EFFECTS = 16;
    public static final int NUM_STREAMS = 5;
    public static final int PLAYBACK_OFFLOAD_GAPLESS_SUPPORTED = 2;
    public static final int PLAYBACK_OFFLOAD_NOT_SUPPORTED = 0;
    public static final int PLAYBACK_OFFLOAD_SUPPORTED = 1;
    public static final java.lang.String PROPERTY_SUPPORT_MIC_NEAR_ULTRASOUND = "android.media.property.SUPPORT_MIC_NEAR_ULTRASOUND";
    public static final java.lang.String PROPERTY_SUPPORT_SPEAKER_NEAR_ULTRASOUND = "android.media.property.SUPPORT_SPEAKER_NEAR_ULTRASOUND";
    public static final int RECORDER_STATE_STARTED = 0;
    public static final int RECORDER_STATE_STOPPED = 1;
    public static final int RECORD_CONFIG_EVENT_NONE = -1;
    public static final int RECORD_CONFIG_EVENT_RELEASE = 3;
    public static final int RECORD_CONFIG_EVENT_START = 0;
    public static final int RECORD_CONFIG_EVENT_STOP = 1;
    public static final int RECORD_CONFIG_EVENT_UPDATE = 2;
    public static final int RECORD_RIID_INVALID = -1;
    public static final long RETURN_DEVICE_VOLUME_BEHAVIOR_ABSOLUTE_ADJUST_ONLY = 240663182L;
    public static final java.lang.String RINGER_MODE_CHANGED_ACTION = "android.media.RINGER_MODE_CHANGED";
    public static final int RINGER_MODE_MAX = 2;
    public static final int ROUTE_ALL = -1;
    public static final int ROUTE_BLUETOOTH = 4;
    public static final int ROUTE_BLUETOOTH_A2DP = 16;
    public static final int ROUTE_BLUETOOTH_SCO = 4;
    public static final int ROUTE_EARPIECE = 1;
    public static final int ROUTE_HEADSET = 8;
    public static final int ROUTE_SPEAKER = 2;
    public static final int SCO_AUDIO_STATE_CONNECTED = 1;
    public static final int SCO_AUDIO_STATE_CONNECTING = 2;
    public static final int SCO_AUDIO_STATE_DISCONNECTED = 0;
    public static final int SCO_AUDIO_STATE_ERROR = -1;
    public static final int STREAM_ASSISTANT = 11;
    public static final int STREAM_BLUETOOTH_SCO = 6;
    public static final java.lang.String STREAM_DEVICES_CHANGED_ACTION = "android.media.STREAM_DEVICES_CHANGED_ACTION";
    public static final java.lang.String STREAM_MUTE_CHANGED_ACTION = "android.media.STREAM_MUTE_CHANGED_ACTION";
    public static final int STREAM_SYSTEM_ENFORCED = 7;
    public static final int STREAM_TTS = 9;
    public static final java.lang.String VIBRATE_SETTING_CHANGED_ACTION = "android.media.VIBRATE_SETTING_CHANGED";
    public static final int VIBRATE_SETTING_OFF = 0;
    public static final int VIBRATE_SETTING_ON = 1;
    public static final int VIBRATE_SETTING_ONLY_SILENT = 2;
    public static final int VIBRATE_TYPE_NOTIFICATION = 1;
    public static final int VIBRATE_TYPE_RINGER = 0;
    public static final java.lang.String VOLUME_CACHING_API = "getStreamVolume";
    public static final java.lang.String VOLUME_CHANGED_ACTION = "android.media.VOLUME_CHANGED_ACTION";
    public static final java.lang.String VOLUME_MAX_CACHING_API = "getStreamMaxVolume";
    public static final java.lang.String VOLUME_MIN_CACHING_API = "getStreamMinVolume";
    public static java.lang.String adjustToString(int p0) { return null; }
    public static java.lang.String audioDeviceCategoryToString(int p0) { return null; }
    public static java.lang.String audioFocusToString(int p0) { return null; }
    public static void clearVolumeCache(java.lang.String p0) {}
    public static java.lang.String flagsToString(int p0) { return null; }
    public static java.util.List getAudioProductStrategies() { return new java.util.ArrayList(); }
    public static java.util.List getAudioVolumeGroups() { return new java.util.ArrayList(); }
    public static android.media.AudioDeviceInfo getDeviceForPortId(int p0, int p1) { return null; }
    public static android.media.AudioDeviceInfo getDeviceInfoFromType(int p0) { return null; }
    public static android.media.AudioDeviceInfo getDeviceInfoFromTypeAndAddress(int p0, java.lang.String p1) { return null; }
    public static android.media.AudioDeviceInfo[] getDevicesStatic(int p0) { return null; }
    public static int getDirectPlaybackSupport(android.media.AudioFormat p0, android.media.AudioAttributes p1) { return 0; }
    public static int getNthNavigationRepeatSoundEffect(int p0) { return 0; }
    public static int getPlaybackOffloadSupport(android.media.AudioFormat p0, android.media.AudioAttributes p1) { return 0; }
    public static int[] getPublicStreamTypes() { return null; }
    public static boolean hasHapticChannels(android.content.Context p0, android.net.Uri p1) { return false; }
    public static boolean hasHapticChannelsImpl(android.content.Context p0, android.net.Uri p1) { return false; }
    public static boolean isHapticPlaybackSupported() { return false; }
    public static boolean isInputDevice(int p0) { return false; }
    public static boolean isOutputDevice(int p0) { return false; }
    public static boolean isPublicStreamType(int p0) { return false; }
    public static boolean isValidRingerMode(int p0) { return false; }
    public static boolean isVolumeControlStreamType(int p0) { return false; }
    public static int listAudioDevicePorts(java.util.ArrayList p0) { return 0; }
    public static int listAudioPatches(java.util.ArrayList p0) { return 0; }
    public static int listAudioPorts(java.util.ArrayList p0) { return 0; }
    public static int listPreviousAudioDevicePorts(java.util.ArrayList p0) { return 0; }
    public static int listPreviousAudioPorts(java.util.ArrayList p0) { return 0; }
    public static android.media.MicrophoneInfo microphoneInfoFromAudioDeviceInfo(android.media.AudioDeviceInfo p0) { return null; }
    public static void setPortIdForMicrophones(java.util.ArrayList p0) {}
    public static void setRttEnabled(boolean p0) {}
    public int abandonAudioFocus(android.media.AudioManager.OnAudioFocusChangeListener p0, android.media.AudioAttributes p1) { return 0; }
    public void abandonAudioFocusForCall() {}
    public int abandonAudioFocusForTest(android.media.AudioFocusRequest p0, java.lang.String p1) { return 0; }
    public void addAssistantServicesUids(int[] p0) {}
    public void addOnStreamAliasingChangedListener(java.util.concurrent.Executor p0, java.lang.Runnable p1) {}
    public void adjustStreamVolumeForUid(int p0, int p1, int p2, java.lang.String p3, int p4, int p5, int p6) {}
    public void adjustSuggestedStreamVolumeForUid(int p0, int p1, int p2, java.lang.String p3, int p4, int p5, int p6) {}
    public void adjustVolumeGroupVolume(int p0, int p1, int p2) {}
    public boolean areNavigationRepeatSoundEffectsEnabled() { return false; }
    public void cancelMuteAwaitConnection(android.media.AudioDeviceAttributes p0) {}
    public void clearAudioServerStateCallback() {}
    public void clearCommunicationDevice() {}
    public boolean clearPreferredDevicesForCapturePreset(int p0) { return false; }
    public boolean clearPreferredMixerAttributes(android.media.AudioAttributes p0, android.media.AudioDeviceInfo p1) { return false; }
    public void disableSafeMediaVolume() {}
    public boolean enterAudioFocusFreezeForTest(java.util.List p0) { return false; }
    public boolean exitAudioFocusFreezeForTest() { return false; }
    public void forceComputeCsdOnAllDevices(boolean p0) {}
    public void forceUseFrameworkMel(boolean p0) {}
    public void forceVolumeControlStream(int p0) {}
    public int[] getActiveAssistantServicesUids() { return null; }
    public java.util.List getActivePlaybackConfigurations() { return new java.util.ArrayList(); }
    public java.util.List getActiveRecordingConfigurations() { return new java.util.ArrayList(); }
    public long getAdditionalOutputDeviceDelay(android.media.AudioDeviceInfo p0) { return 0L; }
    public int getAllowedCapturePolicy() { return (huskFill.get("AllowedCapturePolicy") instanceof Integer ? (Integer) huskFill.get("AllowedCapturePolicy") : 0); }
    public int[] getAssistantServicesUids() { return null; }
    public java.util.List getAudioDevicesForAttributes(android.media.AudioAttributes p0) { return new java.util.ArrayList(); }
    public int getAudioHwSyncForSession(int p0) { return 0; }
    public java.util.List getAvailableCommunicationDevices() { return new java.util.ArrayList(); }
    public int getBluetoothAudioDeviceCategory(java.lang.String p0) { return 0; }
    public android.media.AudioRecord getCallDownlinkExtractionAudioRecord(android.media.AudioFormat p0) { return null; }
    public android.media.AudioTrack getCallUplinkInjectionAudioTrack(android.media.AudioFormat p0) { return null; }
    public android.media.AudioDeviceInfo getCommunicationDevice() { return (android.media.AudioDeviceInfo) huskFill.get("CommunicationDevice"); }
    public float getCsd() { return (huskFill.get("Csd") instanceof Float ? (Float) huskFill.get("Csd") : 0f); }
    public int getDeviceVolumeBehavior(android.media.AudioDeviceAttributes p0) { return 0; }
    public java.util.List getDevicesForAttributes(android.media.AudioAttributes p0) { return new java.util.ArrayList(); }
    public int getDevicesForStream(int p0) { return 0; }
    public java.util.List getDirectProfilesForAttributes(android.media.AudioAttributes p0) { return new java.util.ArrayList(); }
    public int getEncodedSurroundMode() { return (huskFill.get("EncodedSurroundMode") instanceof Integer ? (Integer) huskFill.get("EncodedSurroundMode") : 0); }
    public long getFadeOutDurationOnFocusLossMillis(android.media.AudioAttributes p0) { return 0L; }
    public java.util.List getFocusDuckedUidsForTest() { return new java.util.ArrayList(); }
    public long getFocusFadeOutDurationForTest() { return 0L; }
    public int getFocusRampTimeMs(int p0, android.media.AudioAttributes p1) { return 0; }
    public long getFocusUnmuteDelayAfterFadeOutForTest() { return 0L; }
    public java.util.List getHwOffloadFormatsSupportedForA2dp() { return new java.util.ArrayList(); }
    public java.util.List getHwOffloadFormatsSupportedForLeAudio() { return new java.util.ArrayList(); }
    public java.util.List getHwOffloadFormatsSupportedForLeBroadcast() { return new java.util.ArrayList(); }
    public java.util.List getIndependentStreamTypes() { return new java.util.ArrayList(); }
    public int getLastAudibleStreamVolume(int p0) { return 0; }
    public int getLastAudibleVolumeForVolumeGroup(int p0) { return 0; }
    public long getMaxAdditionalOutputDeviceDelay(android.media.AudioDeviceInfo p0) { return 0L; }
    public int getMaxVolumeIndexForAttributes(android.media.AudioAttributes p0) { return 0; }
    public java.util.List getMicrophones() { return new java.util.ArrayList(); }
    public int getMinVolumeIndexForAttributes(android.media.AudioAttributes p0) { return 0; }
    public android.media.AudioDeviceAttributes getMutingExpectedDevice() { return null; }
    public int getOutputLatency(int p0) { return 0; }
    public java.util.List getPreferredDevicesForCapturePreset(int p0) { return new java.util.ArrayList(); }
    public android.media.AudioMixerAttributes getPreferredMixerAttributes(android.media.AudioAttributes p0, android.media.AudioDeviceInfo p1) { return null; }
    public java.util.List getRegisteredPolicyMixes() { return new java.util.ArrayList(); }
    public java.util.List getReportedSurroundFormats() { return new java.util.ArrayList(); }
    public int getRingerModeInternal() { return (huskFill.get("RingerModeInternal") instanceof Integer ? (Integer) huskFill.get("RingerModeInternal") : 0); }
    public int getRouting(int p0) { return 0; }
    public float getRs2Value() { return (huskFill.get("Rs2Value") instanceof Float ? (Float) huskFill.get("Rs2Value") : 0f); }
    public android.media.Spatializer getSpatializer() { return null; }
    public int getStreamMinVolumeInt(int p0) { return 0; }
    public int getStreamTypeAlias(int p0) { return 0; }
    public java.util.Set getSupportedDeviceTypes(int p0) { return new java.util.HashSet(); }
    public java.util.List getSupportedMixerAttributes(android.media.AudioDeviceInfo p0) { return new java.util.ArrayList(); }
    public int[] getSupportedSystemUsages() { return (int[]) huskFill.get("SupportedSystemUsages"); }
    public java.util.Map getSurroundFormats() { return new java.util.HashMap(); }
    public int getUiSoundsStreamType() { return 0; }
    public int getVibrateSetting(int p0) { return 0; }
    public int getVolumeGroupIdForAttributes(android.media.AudioAttributes p0) { return 0; }
    public int getVolumeGroupIdForAttributes(android.media.AudioAttributes p0, int p1) { return 0; }
    public int getVolumeGroupMaxVolumeIndex(int p0) { return 0; }
    public int getVolumeGroupMinVolumeIndex(int p0) { return 0; }
    public int getVolumeGroupVolumeIndex(int p0) { return 0; }
    public int getVolumeIndexForAttributes(android.media.AudioAttributes p0) { return 0; }
    public boolean hasAudioFocus(java.lang.String p0) { return false; }
    public boolean hasRegisteredDynamicPolicy() { return false; }
    public boolean isAudioFocusExclusive() { return false; }
    public boolean isAudioServerRunning() { return false; }
    public boolean isBluetoothAudioDeviceCategoryFixed(java.lang.String p0) { return false; }
    public boolean isBluetoothScoAvailableOffCall() { return false; }
    public boolean isBluetoothVariableLatencyEnabled() { return (huskFill.get("BluetoothVariableLatencyEnabled") instanceof Boolean ? (Boolean) huskFill.get("BluetoothVariableLatencyEnabled") : false); }
    public boolean isCallScreeningModeSupported() { return false; }
    public boolean isCsdAsAFeatureAvailable() { return false; }
    public boolean isCsdAsAFeatureEnabled() { return (huskFill.get("CsdAsAFeatureEnabled") instanceof Boolean ? (Boolean) huskFill.get("CsdAsAFeatureEnabled") : false); }
    public boolean isCsdEnabled() { return false; }
    public boolean isHdmiSystemAudioSupported() { return (huskFill.get("HdmiSystemAudioSupported") instanceof Boolean ? (Boolean) huskFill.get("HdmiSystemAudioSupported") : false); }
    public boolean isHomeSoundEffectEnabled() { return (huskFill.get("HomeSoundEffectEnabled") instanceof Boolean ? (Boolean) huskFill.get("HomeSoundEffectEnabled") : false); }
    public boolean isHotwordStreamSupported(boolean p0) { return false; }
    public boolean isMasterMute() { return (huskFill.get("MasterMute") instanceof Boolean ? (Boolean) huskFill.get("MasterMute") : false); }
    public boolean isMultiAudioFocusEnabled() { return (huskFill.get("MultiAudioFocusEnabled") instanceof Boolean ? (Boolean) huskFill.get("MultiAudioFocusEnabled") : false); }
    public boolean isMusicActiveRemotely() { return false; }
    public boolean isPstnCallAudioInterceptable() { return false; }
    public boolean isRampingRingerEnabled() { return (huskFill.get("RampingRingerEnabled") instanceof Boolean ? (Boolean) huskFill.get("RampingRingerEnabled") : false); }
    public boolean isScoManagedByAudio() { return false; }
    public boolean isSilentMode() { return false; }
    public boolean isStreamAffectedByMute(int p0) { return false; }
    public boolean isStreamAffectedByRingerMode(int p0) { return false; }
    public boolean isStreamMutableByUi(int p0) { return false; }
    public boolean isSurroundFormatEnabled(int p0) { return false; }
    public boolean isUltrasoundSupported() { return false; }
    public boolean isVolumeControlUsingVolumeGroups() { return false; }
    public boolean isVolumeGroupMuted(int p0) { return false; }
    public void lowerVolumeToRs1() {}
    public void muteAwaitConnection(int[] p0, android.media.AudioDeviceAttributes p1, long p2, java.util.concurrent.TimeUnit p3) {}
    public void permissionUpdateBarrier() {}
    public void playSoundEffect(int p0, int p1) {}
    public void preDispatchKeyEvent(android.view.KeyEvent p0, int p1) {}
    public void registerAudioFocusRequest(android.media.AudioFocusRequest p0) {}
    public int registerAudioPolicy(android.media.audiopolicy.AudioPolicy p0) { return 0; }
    public void registerAudioRecordingCallback(android.media.AudioManager.AudioRecordingCallback p0, android.os.Handler p1) {}
    public void registerMediaButtonEventReceiver(android.app.PendingIntent p0) {}
    public void registerMediaButtonEventReceiver(android.content.ComponentName p0) {}
    public void registerMediaButtonIntent(android.app.PendingIntent p0, android.content.ComponentName p1) {}
    public void registerRemoteControlClient(android.media.RemoteControlClient p0) {}
    public boolean registerRemoteController(android.media.RemoteController p0) { return false; }
    public void reloadAudioSettings() {}
    public void removeAssistantServicesUids(int[] p0) {}
    public void removeOnStreamAliasingChangedListener(java.lang.Runnable p0) {}
    public int requestAudioFocus(android.media.AudioFocusRequest p0, android.media.audiopolicy.AudioPolicy p1) { return 0; }
    public int requestAudioFocus(android.media.AudioManager.OnAudioFocusChangeListener p0, android.media.AudioAttributes p1, int p2, int p3) { return 0; }
    public int requestAudioFocus(android.media.AudioManager.OnAudioFocusChangeListener p0, android.media.AudioAttributes p1, int p2, int p3, android.media.audiopolicy.AudioPolicy p4) { return 0; }
    public void requestAudioFocusForCall(int p0, int p1) {}
    public int requestAudioFocusForTest(android.media.AudioFocusRequest p0, java.lang.String p1, int p2, int p3) { return 0; }
    public void setA2dpSuspended(boolean p0) {}
    public void setActiveAssistantServiceUids(int[] p0) {}
    public boolean setAdditionalOutputDeviceDelay(android.media.AudioDeviceInfo p0, long p1) { return false; }
    public void setAllowedCapturePolicy(int p0) { huskFill.put("AllowedCapturePolicy", Integer.valueOf(p0)); }
    public void setBluetoothA2dpOn(boolean p0) {}
    public boolean setBluetoothAudioDeviceCategory(java.lang.String p0, int p1) { return false; }
    public void setBluetoothHeadsetProperties(java.lang.String p0, boolean p1, boolean p2) {}
    public void setBluetoothVariableLatencyEnabled(boolean p0) { huskFill.put("BluetoothVariableLatencyEnabled", Boolean.valueOf(p0)); }
    public boolean setCommunicationDevice(android.media.AudioDeviceInfo p0) { return false; }
    public void setCsd(float p0) { huskFill.put("Csd", Float.valueOf(p0)); }
    public void setCsdAsAFeatureEnabled(boolean p0) { huskFill.put("CsdAsAFeatureEnabled", Boolean.valueOf(p0)); }
    public void setDeviceVolumeBehavior(android.media.AudioDeviceAttributes p0, int p1) {}
    public void setEnableHardening(boolean p0) {}
    public boolean setEncodedSurroundMode(int p0) { return false; }
    public int setHdmiSystemAudioSupported(boolean p0) { return 0; }
    public void setHfpEnabled(boolean p0) {}
    public void setHfpSamplingRate(int p0) {}
    public void setHfpVolume(int p0) {}
    public void setHomeSoundEffectEnabled(boolean p0) { huskFill.put("HomeSoundEffectEnabled", Boolean.valueOf(p0)); }
    public void setLeAudioSuspended(boolean p0) {}
    public void setMasterMute(boolean p0, int p1) {}
    public void setMicrophoneMuteFromSwitch(boolean p0) {}
    public void setMultiAudioFocusEnabled(boolean p0) { huskFill.put("MultiAudioFocusEnabled", Boolean.valueOf(p0)); }
    public void setNavigationRepeatSoundEffectsEnabled(boolean p0) {}
    public void setNotifAliasRingForTest(boolean p0) {}
    public void setParameter(java.lang.String p0, java.lang.String p1) {}
    public boolean setPreferredDeviceForCapturePreset(int p0, android.media.AudioDeviceAttributes p1) { return false; }
    public boolean setPreferredMixerAttributes(android.media.AudioAttributes p0, android.media.AudioDeviceInfo p1, android.media.AudioMixerAttributes p2) { return false; }
    public void setRampingRingerEnabled(boolean p0) { huskFill.put("RampingRingerEnabled", Boolean.valueOf(p0)); }
    public void setRingerModeInternal(int p0) { huskFill.put("RingerModeInternal", Integer.valueOf(p0)); }
    public void setRouting(int p0, int p1, int p2) {}
    public void setRs2Value(float p0) { huskFill.put("Rs2Value", Float.valueOf(p0)); }
    public void setStreamVolumeForUid(int p0, int p1, int p2, java.lang.String p3, int p4, int p5, int p6) {}
    public void setSupportedSystemUsages(int[] p0) { huskFill.put("SupportedSystemUsages", p0); }
    public boolean setSurroundFormatEnabled(int p0, boolean p1) { return false; }
    public void setTestDeviceConnectionState(android.media.AudioDeviceAttributes p0, boolean p1) {}
    public void setVibrateSetting(int p0, int p1) {}
    public void setVolumeControllerLongPressTimeoutEnabled(boolean p0) {}
    public void setVolumeGroupVolumeIndex(int p0, int p1, int p2) {}
    public void setVolumeIndexForAttributes(android.media.AudioAttributes p0, int p1, int p2) {}
    public void setWiredDeviceConnectionState(int p0, int p1, java.lang.String p2, java.lang.String p3) {}
    public void setWiredDeviceConnectionState(android.media.AudioDeviceAttributes p0, int p1) {}
    public void setWiredHeadsetOn(boolean p0) {}
    public boolean shouldNotificationSoundPlay(android.media.AudioAttributes p0) { return false; }
    public boolean shouldVibrate(int p0) { return false; }
    public void startBluetoothScoVirtualCall() {}
    public boolean supportsBluetoothVariableLatency() { return false; }
    public void unregisterAudioFocusRequest(android.media.AudioManager.OnAudioFocusChangeListener p0) {}
    public void unregisterAudioPolicy(android.media.audiopolicy.AudioPolicy p0) {}
    public void unregisterAudioPolicyAsync(android.media.audiopolicy.AudioPolicy p0) {}
    public void unregisterAudioRecordingCallback(android.media.AudioManager.AudioRecordingCallback p0) {}
    public void unregisterMediaButtonEventReceiver(android.app.PendingIntent p0) {}
    public void unregisterMediaButtonEventReceiver(android.content.ComponentName p0) {}
    public void unregisterMediaButtonIntent(android.app.PendingIntent p0) {}
    public void unregisterRemoteControlClient(android.media.RemoteControlClient p0) {}
    public void unregisterRemoteController(android.media.RemoteController p0) {}
    public void waitForAudioHandlerBarrier() {}
    // ---- end of generated members
}
