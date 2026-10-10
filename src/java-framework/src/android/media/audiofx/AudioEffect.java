// AudioEffect: no effects are offered by the device; the byte helpers convert like Android's (native byte order).
package android.media.audiofx;

@SuppressWarnings({"unchecked", "rawtypes", "deprecation"})
public class AudioEffect {
    private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
    public static final java.lang.String ACTION_CLOSE_AUDIO_EFFECT_CONTROL_SESSION = "android.media.action.CLOSE_AUDIO_EFFECT_CONTROL_SESSION";
    public static final java.lang.String ACTION_DISPLAY_AUDIO_EFFECT_CONTROL_PANEL = "android.media.action.DISPLAY_AUDIO_EFFECT_CONTROL_PANEL";
    public static final java.lang.String ACTION_OPEN_AUDIO_EFFECT_CONTROL_SESSION = "android.media.action.OPEN_AUDIO_EFFECT_CONTROL_SESSION";
    public static final int ALREADY_EXISTS = -2;
    public static final int CONTENT_TYPE_GAME = 2;
    public static final int CONTENT_TYPE_MOVIE = 1;
    public static final int CONTENT_TYPE_MUSIC = 0;
    public static final int CONTENT_TYPE_VOICE = 3;
    public static final java.lang.String EFFECT_AUXILIARY = "Auxiliary";
    public static final java.lang.String EFFECT_INSERT = "Insert";
    public static final java.lang.String EFFECT_POST_PROCESSING = "Post Processing";
    public static final java.lang.String EFFECT_PRE_PROCESSING = "Pre Processing";
    public static java.util.UUID EFFECT_TYPE_AEC;
    public static java.util.UUID EFFECT_TYPE_AGC;
    public static java.util.UUID EFFECT_TYPE_BASS_BOOST;
    public static java.util.UUID EFFECT_TYPE_DYNAMICS_PROCESSING;
    public static java.util.UUID EFFECT_TYPE_ENV_REVERB;
    public static java.util.UUID EFFECT_TYPE_EQUALIZER;
    public static java.util.UUID EFFECT_TYPE_HAPTIC_GENERATOR;
    public static java.util.UUID EFFECT_TYPE_LOUDNESS_ENHANCER;
    public static java.util.UUID EFFECT_TYPE_NS;
    public static java.util.UUID EFFECT_TYPE_NULL;
    public static java.util.UUID EFFECT_TYPE_PRESET_REVERB;
    public static java.util.UUID EFFECT_TYPE_SPATIALIZER;
    public static java.util.UUID EFFECT_TYPE_VIRTUALIZER;
    public static final int ERROR = -1;
    public static final int ERROR_BAD_VALUE = -4;
    public static final int ERROR_DEAD_OBJECT = -7;
    public static final int ERROR_INVALID_OPERATION = -5;
    public static final int ERROR_NO_INIT = -3;
    public static final int ERROR_NO_MEMORY = -6;
    public static final java.lang.String EXTRA_AUDIO_SESSION = "android.media.extra.AUDIO_SESSION";
    public static final java.lang.String EXTRA_CONTENT_TYPE = "android.media.extra.CONTENT_TYPE";
    public static final java.lang.String EXTRA_PACKAGE_NAME = "android.media.extra.PACKAGE_NAME";
    public static final int NATIVE_EVENT_CONTROL_STATUS = 0;
    public static final int NATIVE_EVENT_ENABLED_STATUS = 1;
    public static final int NATIVE_EVENT_PARAMETER_CHANGED = 2;
    public static final int STATE_INITIALIZED = 1;
    public static final int STATE_UNINITIALIZED = 0;
    public static final int SUCCESS = 0;
    public java.lang.Object mListenerLock;
    public android.media.audiofx.AudioEffect.NativeEventHandler mNativeEventHandler;
    public AudioEffect(java.util.UUID p0, android.media.AudioDeviceAttributes p1) {}
    public AudioEffect(java.util.UUID p0, java.util.UUID p1, int p2, int p3) {}
    public static float byteArrayToFloat(byte[] p0) { return 0f; }
    public static float byteArrayToFloat(byte[] p0, int p1) { return 0f; }
    public static int byteArrayToInt(byte[] p0) { return 0; }
    public static int byteArrayToInt(byte[] p0, int p1) { return 0; }
    public static short byteArrayToShort(byte[] p0) { return (short) 0; }
    public static short byteArrayToShort(byte[] p0, int p1) { return (short) 0; }
    public static byte[] concatArrays(byte[]... p0) { int n = 0; for (byte[] a : p0) n += a.length; byte[] r = new byte[n]; n = 0; for (byte[] a : p0) { System.arraycopy(a, 0, r, n, a.length); n += a.length; } return r; }
    public static byte[] floatToByteArray(float p0) { return java.nio.ByteBuffer.allocate(4).order(java.nio.ByteOrder.nativeOrder()).putFloat(p0).array(); }
    public static byte[] intToByteArray(int p0) { return java.nio.ByteBuffer.allocate(4).order(java.nio.ByteOrder.nativeOrder()).putInt(p0).array(); }
    public static boolean isEffectSupportedForDevice(java.util.UUID p0, android.media.AudioDeviceAttributes p1) { return false; }
    public static boolean isEffectTypeAvailable(java.util.UUID p0) { return false; }
    public static boolean isError(int p0) { return false; }
    public static android.media.audiofx.AudioEffect.Descriptor[] queryEffects() { return new Descriptor[0]; }
    public static android.media.audiofx.AudioEffect.Descriptor[] queryPreProcessings(int p0) { return new Descriptor[0]; }
    public static byte[] shortToByteArray(short p0) { return java.nio.ByteBuffer.allocate(2).order(java.nio.ByteOrder.nativeOrder()).putShort(p0).array(); }
    public void checkState(java.lang.String p0) {}
    public void checkStatus(int p0) {}
    public int command(int p0, byte[] p1, byte[] p2) { return 0; }
    protected void finalize() {}
    public android.media.audiofx.AudioEffect.Descriptor getDescriptor() { return (android.media.audiofx.AudioEffect.Descriptor) huskProps.get("Descriptor"); }
    public boolean getEnabled() { return (huskProps.get("Enabled") instanceof Boolean ? (Boolean) huskProps.get("Enabled") : false); }
    public int getId() { return (huskProps.get("Id") instanceof Integer ? (Integer) huskProps.get("Id") : 0); }
    public int getParameter(int p0, byte[] p1) { return 0; }
    public int getParameter(int p0, int[] p1) { return 0; }
    public int getParameter(int p0, short[] p1) { return 0; }
    public int getParameter(byte[] p0, byte[] p1) { return 0; }
    public int getParameter(int[] p0, byte[] p1) { return 0; }
    public int getParameter(int[] p0, int[] p1) { return 0; }
    public int getParameter(int[] p0, short[] p1) { return 0; }
    public boolean hasControl() { return false; }
    public void release() {}
    public void setControlStatusListener(android.media.audiofx.AudioEffect.OnControlStatusChangeListener p0) { huskProps.put("ControlStatusListener", p0); }
    public void setEnableStatusListener(android.media.audiofx.AudioEffect.OnEnableStatusChangeListener p0) { huskProps.put("EnableStatusListener", p0); }
    public int setEnabled(boolean p0) { return 0; }
    public int setParameter(int p0, int p1) { return 0; }
    public int setParameter(int p0, short p1) { return 0; }
    public int setParameter(int p0, byte[] p1) { return 0; }
    public int setParameter(byte[] p0, byte[] p1) { return 0; }
    public int setParameter(int[] p0, byte[] p1) { return 0; }
    public int setParameter(int[] p0, int[] p1) { return 0; }
    public int setParameter(int[] p0, short[] p1) { return 0; }
    public void setParameterListener(android.media.audiofx.AudioEffect.OnParameterChangeListener p0) { huskProps.put("ParameterListener", p0); }
    AudioEffect() { this((java.util.UUID) null, (android.media.AudioDeviceAttributes) null); }
    public static class Descriptor {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public java.lang.String connectMode;
        public java.lang.String implementor;
        public java.lang.String name;
        public java.util.UUID type;
        public java.util.UUID uuid;
        public Descriptor() {}
        public Descriptor(android.os.Parcel p0) {}
        public Descriptor(java.lang.String p0, java.lang.String p1, java.lang.String p2, java.lang.String p3, java.lang.String p4) {}
        public void writeToParcel(android.os.Parcel p0) {}
    }
    public static class NativeEventHandler extends android.os.Handler {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public NativeEventHandler(android.media.audiofx.AudioEffect p0, android.media.audiofx.AudioEffect p1, android.os.Looper p2) { super(); }
        public void handleMessage(android.os.Message p0) {}
        NativeEventHandler() { this((android.media.audiofx.AudioEffect) null, (android.media.audiofx.AudioEffect) null, (android.os.Looper) null); }
    }
    public interface OnControlStatusChangeListener {
        void onControlStatusChange(android.media.audiofx.AudioEffect p0, boolean p1);
    }
    public interface OnEnableStatusChangeListener {
        void onEnableStatusChange(android.media.audiofx.AudioEffect p0, boolean p1);
    }
    public interface OnParameterChangeListener {
        void onParameterChange(android.media.audiofx.AudioEffect p0, int p1, byte[] p2, byte[] p3);
    }
}
