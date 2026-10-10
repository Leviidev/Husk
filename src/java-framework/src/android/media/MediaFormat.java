package android.media;
public final class MediaFormat {
    public static final String KEY_MIME = "mime", KEY_SAMPLE_RATE = "sample-rate", KEY_CHANNEL_COUNT = "channel-count", KEY_WIDTH = "width", KEY_HEIGHT = "height", KEY_DURATION = "durationUs", KEY_BIT_RATE = "bitrate", KEY_FRAME_RATE = "frame-rate", KEY_LANGUAGE = "language", KEY_MAX_INPUT_SIZE = "max-input-size", KEY_COLOR_FORMAT = "color-format", KEY_I_FRAME_INTERVAL = "i-frame-interval";
    public static final String MIMETYPE_AUDIO_AAC = "audio/mp4a-latm", MIMETYPE_AUDIO_MPEG = "audio/mpeg", MIMETYPE_AUDIO_VORBIS = "audio/vorbis", MIMETYPE_AUDIO_OPUS = "audio/opus", MIMETYPE_AUDIO_RAW = "audio/raw", MIMETYPE_VIDEO_AVC = "video/avc", MIMETYPE_VIDEO_HEVC = "video/hevc", MIMETYPE_VIDEO_VP8 = "video/x-vnd.on2.vp8", MIMETYPE_VIDEO_VP9 = "video/x-vnd.on2.vp9";
    private final java.util.HashMap<String, Object> mMap = new java.util.HashMap<>();
    public MediaFormat() {}
    public MediaFormat(MediaFormat o) { mMap.putAll(o.mMap); }
    public static MediaFormat createAudioFormat(String mime, int rate, int channels) { MediaFormat f = new MediaFormat(); f.setString(KEY_MIME, mime); f.setInteger(KEY_SAMPLE_RATE, rate); f.setInteger(KEY_CHANNEL_COUNT, channels); return f; }
    public static MediaFormat createVideoFormat(String mime, int w, int h) { MediaFormat f = new MediaFormat(); f.setString(KEY_MIME, mime); f.setInteger(KEY_WIDTH, w); f.setInteger(KEY_HEIGHT, h); return f; }
    public boolean containsKey(String k) { return mMap.containsKey(k); }
    public int getInteger(String k) { return ((Number) mMap.get(k)).intValue(); }
    public int getInteger(String k, int d) { Object v = mMap.get(k); return v instanceof Number ? ((Number) v).intValue() : d; }
    public long getLong(String k) { return ((Number) mMap.get(k)).longValue(); }
    public long getLong(String k, long d) { Object v = mMap.get(k); return v instanceof Number ? ((Number) v).longValue() : d; }
    public float getFloat(String k) { return ((Number) mMap.get(k)).floatValue(); }
    public float getFloat(String k, float d) { Object v = mMap.get(k); return v instanceof Number ? ((Number) v).floatValue() : d; }
    public String getString(String k) { return (String) mMap.get(k); }
    public String getString(String k, String d) { Object v = mMap.get(k); return v instanceof String ? (String) v : d; }
    public java.nio.ByteBuffer getByteBuffer(String k) { return (java.nio.ByteBuffer) mMap.get(k); }
    public void setInteger(String k, int v) { mMap.put(k, v); }
    public void setLong(String k, long v) { mMap.put(k, v); }
    public void setFloat(String k, float v) { mMap.put(k, v); }
    public void setString(String k, String v) { mMap.put(k, v); }
    public void setByteBuffer(String k, java.nio.ByteBuffer v) { mMap.put(k, v); }
    public void removeKey(String k) { mMap.remove(k); }
    public java.util.Set<String> getKeys() { return mMap.keySet(); }
    @Override public String toString() { return mMap.toString(); }
}
