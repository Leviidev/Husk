package android.media;

/** Recording: no microphone is connected through Husk, so a record produces silence at the pace a microphone would. */
public class AudioRecord implements AudioRouting {
    public static final int STATE_UNINITIALIZED = 0, STATE_INITIALIZED = 1, RECORDSTATE_STOPPED = 1, RECORDSTATE_RECORDING = 3;
    public static final int SUCCESS = 0, ERROR = -1, ERROR_BAD_VALUE = -2, ERROR_INVALID_OPERATION = -3, ERROR_DEAD_OBJECT = -6, READ_BLOCKING = 0, READ_NON_BLOCKING = 1;
    public interface OnRecordPositionUpdateListener { void onMarkerReached(AudioRecord r); void onPeriodicNotification(AudioRecord r); }
    private final int mRate, mChannels, mEncoding;
    private int mState = STATE_INITIALIZED, mRecording = RECORDSTATE_STOPPED;
    private long mStart;
    public AudioRecord(int source, int sampleRate, int channelConfig, int audioFormat, int bufferSizeInBytes) {
        mRate = sampleRate > 0 ? sampleRate : 44100; mChannels = channelConfig == AudioFormat.CHANNEL_IN_STEREO ? 2 : 1; mEncoding = audioFormat;
    }
    public static class Builder {
        private AudioFormat f = new AudioFormat.Builder().setChannelMask(AudioFormat.CHANNEL_IN_MONO).build(); private int size = 4096;
        public Builder setAudioSource(int s) { return this; }
        public Builder setAudioFormat(AudioFormat x) { f = x; return this; }
        public Builder setBufferSizeInBytes(int x) { size = x; return this; }
        public Builder setContext(android.content.Context c) { return this; }
        public AudioRecord build() { return new AudioRecord(0, f.getSampleRate() > 0 ? f.getSampleRate() : 44100, f.getChannelMask(), f.getEncoding(), size); }
    }
    public static int getMinBufferSize(int sampleRate, int channelConfig, int audioFormat) { return Math.max(1024, sampleRate / 10) * (channelConfig == AudioFormat.CHANNEL_IN_STEREO ? 2 : 1) * 2; }
    public int getSampleRate() { return mRate; }
    public int getAudioFormat() { return mEncoding; }
    public int getChannelCount() { return mChannels; }
    public int getState() { return mState; }
    public int getRecordingState() { return mRecording; }
    public int getAudioSessionId() { return 0; }
    public int getAudioSource() { return 0; }
    public void startRecording() { mRecording = RECORDSTATE_RECORDING; mStart = System.nanoTime(); }
    public void stop() { mRecording = RECORDSTATE_STOPPED; }
    public void release() { stop(); mState = STATE_UNINITIALIZED; }
    private void pace(int frames) { try { Thread.sleep(Math.max(1, frames * 1000L / mRate)); } catch (InterruptedException e) { Thread.currentThread().interrupt(); } }
    public int read(byte[] b, int off, int n) { return read(b, off, n, READ_BLOCKING); }
    public int read(byte[] b, int off, int n, int mode) { if (mRecording != RECORDSTATE_RECORDING) return ERROR_INVALID_OPERATION; java.util.Arrays.fill(b, off, off + n, (byte) 0); if (mode == READ_BLOCKING) pace(n / 2 / mChannels); return n; }
    public int read(short[] b, int off, int n) { return read(b, off, n, READ_BLOCKING); }
    public int read(short[] b, int off, int n, int mode) { if (mRecording != RECORDSTATE_RECORDING) return ERROR_INVALID_OPERATION; java.util.Arrays.fill(b, off, off + n, (short) 0); if (mode == READ_BLOCKING) pace(n / mChannels); return n; }
    public int read(float[] b, int off, int n, int mode) { if (mRecording != RECORDSTATE_RECORDING) return ERROR_INVALID_OPERATION; java.util.Arrays.fill(b, off, off + n, 0f); if (mode == READ_BLOCKING) pace(n / mChannels); return n; }
    public int read(java.nio.ByteBuffer b, int n) { return read(b, n, READ_BLOCKING); }
    public int read(java.nio.ByteBuffer b, int n, int mode) { if (mRecording != RECORDSTATE_RECORDING) return ERROR_INVALID_OPERATION; for (int i = 0; i < n; i++) b.put(b.position() + i, (byte) 0); if (mode == READ_BLOCKING) pace(n / 2 / mChannels); return n; }
    public void setRecordPositionUpdateListener(OnRecordPositionUpdateListener l) {}
    public void setRecordPositionUpdateListener(OnRecordPositionUpdateListener l, android.os.Handler h) {}
    public int setNotificationMarkerPosition(int m) { return SUCCESS; }
    public int setPositionNotificationPeriod(int p) { return SUCCESS; }
    public AudioDeviceInfo getRoutedDevice() { return null; }
    public boolean setPreferredDevice(AudioDeviceInfo d) { return true; }
    public AudioDeviceInfo getPreferredDevice() { return null; }
    public void addOnRoutingChangedListener(AudioRouting.OnRoutingChangedListener l, android.os.Handler h) {}
    public void removeOnRoutingChangedListener(AudioRouting.OnRoutingChangedListener l) {}
}
