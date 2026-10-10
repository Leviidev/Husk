package android.media;

import java.nio.ByteBuffer;

/**
 * PCM playback through Husk's mixer. Streaming tracks block on write while the mixer's ring is full (as Android's do); static tracks
 * keep what was written and play it from a thread of their own, looping when asked.
 */
public class AudioTrack implements AudioRouting {
    public static final int PLAYSTATE_STOPPED = 1, PLAYSTATE_PAUSED = 2, PLAYSTATE_PLAYING = 3;
    public static final int MODE_STATIC = 0, MODE_STREAM = 1;
    public static final int STATE_UNINITIALIZED = 0, STATE_INITIALIZED = 1, STATE_NO_STATIC_DATA = 2;
    public static final int SUCCESS = 0, ERROR = -1, ERROR_BAD_VALUE = -2, ERROR_INVALID_OPERATION = -3, ERROR_DEAD_OBJECT = -6;
    public static final int WRITE_BLOCKING = 0, WRITE_NON_BLOCKING = 1;
    public static final int PERFORMANCE_MODE_NONE = 0, PERFORMANCE_MODE_LOW_LATENCY = 1, PERFORMANCE_MODE_POWER_SAVING = 2;
    public static final int ENCAPSULATION_MODE_NONE = 0;
    public interface OnPlaybackPositionUpdateListener { void onMarkerReached(AudioTrack t); void onPeriodicNotification(AudioTrack t); }
    public interface OnRoutingChangedListener extends AudioRouting.OnRoutingChangedListener {}
    public static abstract class StreamEventCallback { public void onTearDown(AudioTrack t) {} public void onPresentationEnded(AudioTrack t) {} public void onDataRequest(AudioTrack t, int sizeInFrames) {} }
    private final int mRate, mChannels, mEncoding, mMode, mBufferBytes, mSession;
    private int mId = -1, mState, mPlayState = PLAYSTATE_STOPPED, mLoopCount, mMarker, mPeriod;
    private float mVolume = 1, mL = 1, mR = 1;
    private byte[] mStatic; private int mStaticLen;
    private Thread mStaticThread;
    private long mBase;
    private OnPlaybackPositionUpdateListener mPosListener;
    private android.os.Handler mPosHandler;
    @Deprecated public AudioTrack(int streamType, int sampleRate, int channelConfig, int audioFormat, int bufferSizeInBytes, int mode) { this(streamType, sampleRate, channelConfig, audioFormat, bufferSizeInBytes, mode, 0); }
    @Deprecated public AudioTrack(int streamType, int sampleRate, int channelConfig, int audioFormat, int bufferSizeInBytes, int mode, int sessionId) {
        mRate = sampleRate > 0 ? sampleRate : 44100;
        mChannels = AudioFormat.channelCount(channelConfig);
        mEncoding = audioFormat == AudioFormat.ENCODING_DEFAULT ? AudioFormat.ENCODING_PCM_16BIT : audioFormat;
        mMode = mode; mBufferBytes = bufferSizeInBytes; mSession = sessionId;
        if (mode == MODE_STREAM) { mId = husk.Audio.trackOpen(mRate, mChannels); mState = mId >= 0 ? STATE_INITIALIZED : STATE_UNINITIALIZED; }
        else { mStatic = new byte[Math.max(0, bufferSizeInBytes)]; mState = STATE_NO_STATIC_DATA; }
    }
    public AudioTrack(AudioAttributes attributes, AudioFormat format, int bufferSizeInBytes, int mode, int sessionId) {
        this(AudioManager.STREAM_MUSIC, format.getSampleRate() > 0 ? format.getSampleRate() : 44100, format.getChannelMask(), format.getEncoding(), bufferSizeInBytes, mode, sessionId);
    }
    public static class Builder {
        private AudioAttributes a; private AudioFormat f = new AudioFormat.Builder().build(); private int size = 8192, mode = MODE_STREAM, session;
        public Builder() {}
        public Builder setAudioAttributes(AudioAttributes x) { a = x; return this; }
        public Builder setAudioFormat(AudioFormat x) { f = x; return this; }
        public Builder setBufferSizeInBytes(int x) { size = x; return this; }
        public Builder setTransferMode(int x) { mode = x; return this; }
        public Builder setSessionId(int x) { session = x; return this; }
        public Builder setPerformanceMode(int x) { return this; }
        public Builder setOffloadedPlayback(boolean b) { return this; }
        public Builder setEncapsulationMode(int m) { return this; }
        public Builder setContext(android.content.Context c) { return this; }
        public AudioTrack build() { return new AudioTrack(a, f, size, mode, session); }
    }
    public static int getMinBufferSize(int sampleRate, int channelConfig, int audioFormat) {
        if (sampleRate < 4000 || sampleRate > 192000) return ERROR_BAD_VALUE;
        return Math.max(1024, sampleRate / 20) * AudioFormat.channelCount(channelConfig) * AudioFormat.bytesPerSample(audioFormat == AudioFormat.ENCODING_DEFAULT ? AudioFormat.ENCODING_PCM_16BIT : audioFormat);
    }
    public static float getMinVolume() { return 0f; }
    public static float getMaxVolume() { return 1f; }
    public static int getNativeOutputSampleRate(int streamType) { return 44100; }
    public int getSampleRate() { return mRate; }
    public int getPlaybackRate() { return mRate; }
    public int setPlaybackRate(int r) { return SUCCESS; }
    public PlaybackParams getPlaybackParams() { return new PlaybackParams(); }
    public void setPlaybackParams(PlaybackParams p) {}
    public int getAudioFormat() { return mEncoding; }
    public AudioFormat getFormat() { return new AudioFormat.Builder().setEncoding(mEncoding).setSampleRate(mRate).setChannelMask(mChannels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO).build(); }
    public int getStreamType() { return AudioManager.STREAM_MUSIC; }
    public int getChannelConfiguration() { return mChannels == 1 ? AudioFormat.CHANNEL_OUT_MONO : AudioFormat.CHANNEL_OUT_STEREO; }
    public int getChannelCount() { return mChannels; }
    public int getState() { return mState; }
    public int getPlayState() { return mPlayState; }
    public int getBufferSizeInFrames() { return Math.max(1, mBufferBytes / frameBytes()); }
    public int setBufferSizeInFrames(int f) { return getBufferSizeInFrames(); }
    public int getBufferCapacityInFrames() { return getBufferSizeInFrames(); }
    public int getUnderrunCount() { return 0; }
    public int getPerformanceMode() { return PERFORMANCE_MODE_NONE; }
    public int getAudioSessionId() { return mSession; }
    public boolean isOffloadedPlayback() { return false; }
    public int getNotificationMarkerPosition() { return mMarker; }
    public int getPositionNotificationPeriod() { return mPeriod; }
    public int setNotificationMarkerPosition(int m) { mMarker = m; return SUCCESS; }
    public int setPositionNotificationPeriod(int p) { mPeriod = p; return SUCCESS; }
    public void setPlaybackPositionUpdateListener(OnPlaybackPositionUpdateListener l) { setPlaybackPositionUpdateListener(l, null); }
    public void setPlaybackPositionUpdateListener(OnPlaybackPositionUpdateListener l, android.os.Handler h) { mPosListener = l; mPosHandler = h; }
    private int frameBytes() { return mChannels * AudioFormat.bytesPerSample(mEncoding); }
    public int getPlaybackHeadPosition() { return mId >= 0 ? (int) (husk.Audio.trackPosition(mId) - mBase) : 0; }
    public boolean getTimestamp(AudioTimestamp ts) { if (mId < 0) return false; ts.framePosition = getPlaybackHeadPosition(); ts.nanoTime = System.nanoTime(); return true; }
    public int setLoopPoints(int start, int end, int loopCount) { mLoopCount = loopCount; return SUCCESS; }
    public int reloadStaticData() { return SUCCESS; }
    public int setStereoVolume(float l, float r) { mL = l; mR = r; applyVolume(); return SUCCESS; }
    public int setVolume(float v) { mVolume = v; applyVolume(); return SUCCESS; }
    private void applyVolume() { if (mId >= 0) husk.Audio.trackVolume(mId, Math.max(0, Math.min(1, mVolume * mL)), Math.max(0, Math.min(1, mVolume * mR))); }
    public int setAuxEffectSendLevel(float l) { return SUCCESS; }
    public int attachAuxEffect(int id) { return SUCCESS; }
    public int setPresentation(AudioPresentation p) { return SUCCESS; }
    public void play() {
        if (mState == STATE_UNINITIALIZED) throw new IllegalStateException("play() called on uninitialized AudioTrack.");
        mPlayState = PLAYSTATE_PLAYING;
        if (mMode == MODE_STATIC) { startStatic(); return; }
        husk.Audio.trackPlay(mId, true);
    }
    public void pause() { if (mPlayState == PLAYSTATE_PLAYING) { mPlayState = PLAYSTATE_PAUSED; if (mId >= 0) husk.Audio.trackPlay(mId, false); } }
    public void stop() {
        if (mState == STATE_UNINITIALIZED) throw new IllegalStateException("stop() called on uninitialized AudioTrack.");
        mPlayState = PLAYSTATE_STOPPED;
        if (mStaticThread != null) { mStaticThread.interrupt(); mStaticThread = null; }
        if (mId >= 0) { husk.Audio.trackFlush(mId); husk.Audio.trackPlay(mId, false); mBase = husk.Audio.trackPosition(mId); }
    }
    public void flush() { if (mId >= 0 && mMode == MODE_STREAM) husk.Audio.trackFlush(mId); }
    public void release() { stop0(); if (mId >= 0) { husk.Audio.trackClose(mId); mId = -1; } mState = STATE_UNINITIALIZED; }
    private void stop0() { try { if (mState != STATE_UNINITIALIZED) stop(); } catch (IllegalStateException e) {} }
    @Override protected void finalize() throws Throwable { try { release(); } finally { super.finalize(); } }
    private void startStatic() {
        if (mStaticThread != null) return;
        if (mId < 0) { mId = husk.Audio.trackOpen(mRate, mChannels); applyVolume(); }
        husk.Audio.trackPlay(mId, true);
        final byte[] data = mStatic; final int len = mStaticLen; final int loops = mLoopCount;
        mStaticThread = new Thread(() -> {
            int left = loops;
            do {
                int off = 0;
                while (off < len && !Thread.currentThread().isInterrupted()) { int w = husk.Audio.trackWriteBytes(mId, data, off, len - off, mEncoding, true); if (w <= 0) break; off += w; }
                if (left > 0) left--;
            } while ((left != 0) && !Thread.currentThread().isInterrupted());
        }, "AudioTrack static");
        mStaticThread.setDaemon(true);
        mStaticThread.start();
    }
    private int writeStatic(byte[] b, int off, int n) {
        if (mStatic.length < mStaticLen + n) mStatic = java.util.Arrays.copyOf(mStatic, mStaticLen + n);
        System.arraycopy(b, off, mStatic, mStaticLen, n);
        mStaticLen += n;
        mState = STATE_INITIALIZED;
        return n;
    }
    public int write(byte[] data, int off, int size) { return write(data, off, size, WRITE_BLOCKING); }
    public int write(byte[] data, int off, int size, int mode) {
        if (mState == STATE_UNINITIALIZED || mEncoding == AudioFormat.ENCODING_PCM_FLOAT) return ERROR_INVALID_OPERATION;
        if (data == null || off < 0 || size < 0 || off + size > data.length) return ERROR_BAD_VALUE;
        if (mMode == MODE_STATIC) return writeStatic(data, off, size);
        return husk.Audio.trackWriteBytes(mId, data, off, size, mEncoding, mode == WRITE_BLOCKING);
    }
    public int write(short[] data, int off, int size) { return write(data, off, size, WRITE_BLOCKING); }
    public int write(short[] data, int off, int size, int mode) {
        if (mState == STATE_UNINITIALIZED || mEncoding != AudioFormat.ENCODING_PCM_16BIT) return ERROR_INVALID_OPERATION;
        if (data == null || off < 0 || size < 0 || off + size > data.length) return ERROR_BAD_VALUE;
        if (mMode == MODE_STATIC) { byte[] b = new byte[size * 2]; for (int i = 0; i < size; i++) { b[2 * i] = (byte) data[off + i]; b[2 * i + 1] = (byte) (data[off + i] >> 8); } return writeStatic(b, 0, b.length) / 2; }
        return husk.Audio.trackWrite(mId, data, off, size, mode == WRITE_BLOCKING);
    }
    public int write(float[] data, int off, int size, int mode) {
        if (mState == STATE_UNINITIALIZED || mEncoding != AudioFormat.ENCODING_PCM_FLOAT) return ERROR_INVALID_OPERATION;
        if (data == null || off < 0 || size < 0 || off + size > data.length) return ERROR_BAD_VALUE;
        return husk.Audio.trackWriteFloat(mId, data, off, size, mode == WRITE_BLOCKING);
    }
    public int write(ByteBuffer data, int size, int mode) {
        if (data == null || size < 0 || size > data.remaining()) return ERROR_BAD_VALUE;
        byte[] b = new byte[size];
        int pos = data.position();
        data.get(b);
        int w = write(b, 0, size, mode);
        data.position(pos + Math.max(0, w));
        return w;
    }
    public int write(ByteBuffer data, int size, int mode, long ts) { return write(data, size, mode); }
    public AudioDeviceInfo getRoutedDevice() { return AudioDeviceInfo.huskSpeaker(); }
    public boolean setPreferredDevice(AudioDeviceInfo d) { return true; }
    public AudioDeviceInfo getPreferredDevice() { return null; }
    public void addOnRoutingChangedListener(AudioRouting.OnRoutingChangedListener l, android.os.Handler h) {}
    public void removeOnRoutingChangedListener(AudioRouting.OnRoutingChangedListener l) {}
    public void registerStreamEventCallback(java.util.concurrent.Executor e, StreamEventCallback cb) {}
    public void unregisterStreamEventCallback(StreamEventCallback cb) {}
}
