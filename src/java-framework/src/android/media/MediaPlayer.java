package android.media;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import java.io.FileDescriptor;
import java.util.Map;

/**
 * Music and other long sounds: the file is decoded as it plays (husk.Audio's decoder: Ogg Vorbis, MP3, AAC, WAV...) into a stream
 * of Husk's mixer, from a thread of the player's own. Video has no picture: its sound still plays.
 */
public class MediaPlayer implements AudioRouting {
    public interface OnCompletionListener { void onCompletion(MediaPlayer mp); }
    public interface OnPreparedListener { void onPrepared(MediaPlayer mp); }
    public interface OnErrorListener { boolean onError(MediaPlayer mp, int what, int extra); }
    public interface OnSeekCompleteListener { void onSeekComplete(MediaPlayer mp); }
    public interface OnInfoListener { boolean onInfo(MediaPlayer mp, int what, int extra); }
    public interface OnBufferingUpdateListener { void onBufferingUpdate(MediaPlayer mp, int percent); }
    public interface OnVideoSizeChangedListener { void onVideoSizeChanged(MediaPlayer mp, int w, int h); }
    public interface OnTimedTextListener { void onTimedText(MediaPlayer mp, TimedText text); }
    public static final int MEDIA_ERROR_UNKNOWN = 1, MEDIA_ERROR_SERVER_DIED = 100, MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 200, MEDIA_ERROR_IO = -1004, MEDIA_ERROR_MALFORMED = -1007, MEDIA_ERROR_UNSUPPORTED = -1010, MEDIA_ERROR_TIMED_OUT = -110;
    public static final int MEDIA_INFO_UNKNOWN = 1, MEDIA_INFO_VIDEO_RENDERING_START = 3, MEDIA_INFO_BUFFERING_START = 701, MEDIA_INFO_BUFFERING_END = 702, MEDIA_INFO_NOT_SEEKABLE = 801;
    public static final int SEEK_PREVIOUS_SYNC = 0, SEEK_NEXT_SYNC = 1, SEEK_CLOSEST_SYNC = 2, SEEK_CLOSEST = 3;
    public static final int VIDEO_SCALING_MODE_SCALE_TO_FIT = 1, VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING = 2;
    private static final int IDLE = 0, INITIALIZED = 1, PREPARING = 2, PREPARED = 3, STARTED = 4, PAUSED = 5, COMPLETED = 6, STOPPED = 7, ERROR = 8, END = 9;
    private final Object mLock = new Object();
    private final Handler mMain;
    private int mState = IDLE;
    private byte[] mBytes; private int mFd = -1; private long mFdOff, mFdLen;
    private long mDec; private int mRate, mChannels, mDuration;
    private int mTrack = -1;
    private Thread mThread;
    private volatile boolean mLooping, mStopThread;
    private volatile int mSeekTo = -1;
    private long mPosBase; private int mPosBaseMs;
    private float mL = 1, mR = 1;
    private int mSession;
    private OnCompletionListener mOnCompletion; private OnPreparedListener mOnPrepared; private OnErrorListener mOnError; private OnSeekCompleteListener mOnSeek; private OnInfoListener mOnInfo; private MediaPlayer mNext;
    public MediaPlayer() { Looper l = Looper.myLooper(); mMain = new Handler(l != null ? l : Looper.getMainLooper()); }
    public MediaPlayer(Context c) { this(); }
    public static MediaPlayer create(Context c, int resId) { return create(c, resId, null, 0); }
    public static MediaPlayer create(Context c, int resId, AudioAttributes a, int session) {
        try { MediaPlayer mp = new MediaPlayer(); try (java.io.InputStream in = c.getResources().openRawResource(resId)) { mp.mBytes = husk.Audio.readAll(in); } mp.mState = INITIALIZED; mp.prepare(); return mp; }
        catch (Exception e) { android.util.Log.w("MediaPlayer", "create failed for resource " + resId + ": " + e); return null; }
    }
    public static MediaPlayer create(Context c, Uri u) { return create(c, u, null); }
    public static MediaPlayer create(Context c, Uri u, android.view.SurfaceHolder h) { return create(c, u, h, null, 0); }
    public static MediaPlayer create(Context c, Uri u, android.view.SurfaceHolder h, AudioAttributes a, int session) {
        try { MediaPlayer mp = new MediaPlayer(); mp.setDataSource(c, u); mp.prepare(); return mp; }
        catch (Exception e) { android.util.Log.w("MediaPlayer", "create failed for " + u + ": " + e); return null; }
    }
    private void checkIdle() { if (mState != IDLE) throw new IllegalStateException("setDataSource called in state " + mState); }
    public void setDataSource(String path) throws java.io.IOException {
        checkIdle();
        if (path.startsWith("http://") || path.startsWith("https://")) { try (java.io.InputStream in = new java.net.URL(path).openStream()) { mBytes = husk.Audio.readAll(in); } }
        else if (path.startsWith("file://")) setDataSource(path.substring(7));
        else { try (java.io.FileInputStream in = new java.io.FileInputStream(path)) { mBytes = husk.Audio.readAll(in); } }
        mState = INITIALIZED;
    }
    public void setDataSource(String path, Map<String, String> headers) throws java.io.IOException { setDataSource(path); }
    public void setDataSource(Context c, Uri u) throws java.io.IOException {
        checkIdle();
        String s = u.getScheme();
        if (s == null || "file".equals(s)) { setDataSource(u.getPath()); return; }
        if ("http".equals(s) || "https".equals(s)) { setDataSource(u.toString()); return; }
        mBytes = husk.Audio.bytesOf(c, u);
        mState = INITIALIZED;
    }
    public void setDataSource(Context c, Uri u, Map<String, String> headers) throws java.io.IOException { setDataSource(c, u); }
    public void setDataSource(Context c, Uri u, Map<String, String> headers, java.util.List<?> cookies) throws java.io.IOException { setDataSource(c, u); }
    public void setDataSource(FileDescriptor fd) throws java.io.IOException { setDataSource(fd, 0, -1); }
    public void setDataSource(FileDescriptor fd, long offset, long length) throws java.io.IOException {
        checkIdle();
        int n = husk.Audio.fdOf(fd);
        if (n < 0) throw new java.io.IOException("not a file descriptor Husk can read");
        mFd = n; mFdOff = offset; mFdLen = length; mState = INITIALIZED;
    }
    public void setDataSource(AssetFileDescriptor afd) throws java.io.IOException {
        if (afd.getFileDescriptor() != null && husk.Audio.fdOf(afd.getFileDescriptor()) >= 0) setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
        else if (afd.huskAssetName() != null) { checkIdle(); mBytes = husk.Native.readAsset(afd.huskAssetName()); if (mBytes == null) throw new java.io.IOException("no asset " + afd.huskAssetName()); mState = INITIALIZED; }
        else throw new java.io.IOException("unreadable AssetFileDescriptor");
    }
    public void setDataSource(MediaDataSource ds) throws java.io.IOException {
        checkIdle();
        long size = ds.getSize();
        if (size <= 0 || size > (256 << 20)) throw new java.io.IOException("media data source of unknown size");
        mBytes = new byte[(int) size];
        int off = 0;
        while (off < size) { int r = ds.readAt(off, mBytes, off, (int) size - off); if (r <= 0) break; off += r; }
        mState = INITIALIZED;
    }
    public void prepare() throws java.io.IOException {
        if (mState != INITIALIZED && mState != STOPPED) throw new IllegalStateException("prepare called in state " + mState);
        open();
    }
    private void open() throws java.io.IOException {
        long d = mBytes != null ? husk.Audio.decOpen(mBytes) : mFd >= 0 ? husk.Audio.decOpenFd(mFd, mFdOff, mFdLen) : 0;
        if (d == 0) { mState = ERROR; throw new java.io.IOException("Prepare failed.: status=0x1"); }
        int[] info = new int[3];
        husk.Audio.decInfo(d, info);
        synchronized (mLock) { mDec = d; mRate = info[0]; mChannels = info[1]; mDuration = info[2]; mState = PREPARED; mPosBase = 0; mPosBaseMs = 0; }
    }
    public void prepareAsync() {
        if (mState != INITIALIZED && mState != STOPPED) throw new IllegalStateException("prepareAsync called in state " + mState);
        mState = PREPARING;
        new Thread(() -> {
            try { open(); mMain.post(() -> { if (mOnPrepared != null) mOnPrepared.onPrepared(this); }); }
            catch (java.io.IOException e) { mMain.post(() -> fail(MEDIA_ERROR_UNKNOWN, MEDIA_ERROR_IO)); }
        }, "MediaPlayer prepare").start();
    }
    private void fail(int what, int extra) {
        mState = ERROR;
        boolean handled = mOnError != null && mOnError.onError(this, what, extra);
        if (!handled && mOnCompletion != null) mOnCompletion.onCompletion(this);
    }
    public void start() {
        synchronized (mLock) {
            if (mState == COMPLETED) { husk.Audio.decSeek(mDec, 0); resetPosition(0); }
            if (mState != PREPARED && mState != PAUSED && mState != COMPLETED && mState != STARTED) throw new IllegalStateException("start called in state " + mState);
            if (mTrack < 0) { mTrack = husk.Audio.trackOpen(mRate, mChannels); husk.Audio.trackVolume(mTrack, mL, mR); }
            husk.Audio.trackPlay(mTrack, true);
            mState = STARTED;
            if (mThread == null) {
                mStopThread = false;
                mThread = new Thread(this::pump, "MediaPlayer");
                mThread.setDaemon(true);
                mThread.start();
            }
            mLock.notifyAll();
        }
    }
    /** The player's thread: decode, write (paced by the mixer), loop or finish. */
    private void pump() {
        short[] buf = new short[4096 * Math.max(1, mChannels)];
        for (;;) {
            long dec; int track;
            synchronized (mLock) {
                while (!mStopThread && mState != STARTED) { try { mLock.wait(); } catch (InterruptedException e) { return; } }
                if (mStopThread) return;
                if (mSeekTo >= 0) { husk.Audio.trackFlush(mTrack); husk.Audio.decSeek(mDec, mSeekTo); resetPosition(mSeekTo); final int ms = mSeekTo; mSeekTo = -1; mMain.post(() -> { if (mOnSeek != null) mOnSeek.onSeekComplete(this); }); }
                dec = mDec; track = mTrack;
            }
            int n = husk.Audio.decRead(dec, buf);
            if (n <= 0) {
                if (mLooping) { synchronized (mLock) { husk.Audio.decSeek(mDec, 0); } continue; }
                // let what is queued play out, then finish
                while (husk.Audio.trackPending(track) > 0 && !mStopThread && mState == STARTED) { try { Thread.sleep(20); } catch (InterruptedException e) { return; } }
                synchronized (mLock) { if (mState == STARTED) mState = COMPLETED; mThread = null; }
                mMain.post(() -> {
                    if (mNext != null) { try { mNext.start(); } catch (IllegalStateException e) {} }
                    if (mOnCompletion != null) mOnCompletion.onCompletion(this);
                });
                return;
            }
            int off = 0;
            while (off < n && !mStopThread) {
                int w = husk.Audio.trackWrite(track, buf, off, n - off, true);
                if (w <= 0) { if (mState != STARTED) { synchronized (mLock) { while (!mStopThread && mState != STARTED && mSeekTo < 0) { try { mLock.wait(); } catch (InterruptedException e) { return; } } } if (mSeekTo >= 0) break; continue; } break; }
                off += w;
            }
        }
    }
    private void resetPosition(int ms) { mPosBase = mTrack >= 0 ? husk.Audio.trackPosition(mTrack) : 0; mPosBaseMs = ms; }
    public void pause() { synchronized (mLock) { if (mState == STARTED) { mState = PAUSED; if (mTrack >= 0) husk.Audio.trackPlay(mTrack, false); } } }
    public void stop() {
        synchronized (mLock) {
            if (mState == IDLE || mState == END || mState == ERROR) return;
            mStopThread = true; mLock.notifyAll();
            if (mTrack >= 0) { husk.Audio.trackFlush(mTrack); husk.Audio.trackPlay(mTrack, false); }
            if (mDec != 0) { husk.Audio.decClose(mDec); mDec = 0; }
            mThread = null;
            mState = STOPPED;
        }
    }
    public void reset() {
        stop();
        synchronized (mLock) { if (mTrack >= 0) { husk.Audio.trackClose(mTrack); mTrack = -1; } mBytes = null; mFd = -1; mState = IDLE; mSeekTo = -1; }
    }
    public void release() { reset(); mState = END; mOnCompletion = null; mOnPrepared = null; mOnError = null; mOnSeek = null; }
    public boolean isPlaying() { return mState == STARTED; }
    public boolean isLooping() { return mLooping; }
    public void setLooping(boolean l) { mLooping = l; }
    public void setNextMediaPlayer(MediaPlayer next) { mNext = next; }
    public void setVolume(float l, float r) { mL = Math.max(0, Math.min(1, l)); mR = Math.max(0, Math.min(1, r)); if (mTrack >= 0) husk.Audio.trackVolume(mTrack, mL, mR); }
    public void setVolume(float v) { setVolume(v, v); }
    public void seekTo(int ms) {
        synchronized (mLock) {
            if (mDec == 0) return;
            ms = Math.max(0, mDuration > 0 ? Math.min(ms, mDuration) : ms);
            if (mState == STARTED || mThread != null) { mSeekTo = ms; mLock.notifyAll(); }
            else { husk.Audio.decSeek(mDec, ms); if (mTrack >= 0) husk.Audio.trackFlush(mTrack); resetPosition(ms); final int done = ms; mMain.post(() -> { if (mOnSeek != null) mOnSeek.onSeekComplete(this); }); }
            if (mState == COMPLETED) mState = PAUSED;
        }
    }
    public void seekTo(long ms, int mode) { seekTo((int) ms); }
    public int getCurrentPosition() {
        synchronized (mLock) {
            if (mSeekTo >= 0) return mSeekTo;
            if (mState == COMPLETED) return mDuration;
            if (mTrack < 0 || mRate <= 0) return mPosBaseMs;
            long frames = husk.Audio.trackPosition(mTrack) - mPosBase;
            return (int) Math.min(mDuration > 0 ? mDuration : Integer.MAX_VALUE, mPosBaseMs + frames * 1000L / mRate);
        }
    }
    public int getDuration() { return mState == IDLE || mState == INITIALIZED || mState == ERROR ? -1 : mDuration; }
    public int getVideoWidth() { return 0; }
    public int getVideoHeight() { return 0; }
    public void setDisplay(android.view.SurfaceHolder h) {}
    public void setSurface(android.view.Surface s) {}
    public void setVideoScalingMode(int m) {}
    public void setScreenOnWhilePlaying(boolean b) {}
    public void setWakeMode(Context c, int mode) {}
    @Deprecated public void setAudioStreamType(int t) {}
    public void setAudioAttributes(AudioAttributes a) {}
    public void setAudioSessionId(int id) { mSession = id; }
    public int getAudioSessionId() { return mSession; }
    public void setAuxEffectSendLevel(float l) {}
    public void attachAuxEffect(int id) {}
    public void setPlaybackParams(PlaybackParams p) {}
    public PlaybackParams getPlaybackParams() { return new PlaybackParams(); }
    public MediaTimestamp getTimestamp() { return new MediaTimestamp(getCurrentPosition() * 1000L, System.nanoTime(), isPlaying() ? 1f : 0f); }
    public TrackInfo[] getTrackInfo() { return new TrackInfo[] { new TrackInfo() }; }
    public void selectTrack(int i) {} public void deselectTrack(int i) {}
    public void addTimedTextSource(String path, String mime) {}
    public void setOnCompletionListener(OnCompletionListener l) { mOnCompletion = l; }
    public void setOnPreparedListener(OnPreparedListener l) { mOnPrepared = l; }
    public void setOnErrorListener(OnErrorListener l) { mOnError = l; }
    public void setOnSeekCompleteListener(OnSeekCompleteListener l) { mOnSeek = l; }
    public void setOnInfoListener(OnInfoListener l) { mOnInfo = l; }
    public void setOnBufferingUpdateListener(OnBufferingUpdateListener l) {}
    public void setOnVideoSizeChangedListener(OnVideoSizeChangedListener l) {}
    public void setOnTimedTextListener(OnTimedTextListener l) {}
    public AudioDeviceInfo getRoutedDevice() { return AudioDeviceInfo.huskSpeaker(); }
    public boolean setPreferredDevice(AudioDeviceInfo d) { return true; }
    public AudioDeviceInfo getPreferredDevice() { return null; }
    public void addOnRoutingChangedListener(AudioRouting.OnRoutingChangedListener l, android.os.Handler h) {}
    public void removeOnRoutingChangedListener(AudioRouting.OnRoutingChangedListener l) {}
    @Override protected void finalize() throws Throwable { try { if (mState != END) release(); } finally { super.finalize(); } }
    public static class TrackInfo { public static final int MEDIA_TRACK_TYPE_UNKNOWN = 0, MEDIA_TRACK_TYPE_VIDEO = 1, MEDIA_TRACK_TYPE_AUDIO = 2, MEDIA_TRACK_TYPE_TIMEDTEXT = 3, MEDIA_TRACK_TYPE_SUBTITLE = 4; public int getTrackType() { return MEDIA_TRACK_TYPE_AUDIO; } public String getLanguage() { return "und"; } public MediaFormat getFormat() { return null; }
        // ---- generated by tools/compat/fillmembers.py (TrackInfo): the platform's members this class does not write (signatures only)
        public static final int MEDIA_TRACK_TYPE_METADATA = 5;
        public int describeContents() { return 0; }
        public boolean hasHapticChannels() { return false; }
        public void writeToParcel(android.os.Parcel p0, int p1) {}
        // ---- end of generated members (TrackInfo)
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final boolean APPLY_METADATA_FILTER = true;
    public static final boolean BYPASS_METADATA_FILTER = false;
    public static final int MEDIA_ERROR_SYSTEM = -2147483648;
    public static final int MEDIA_INFO_AUDIO_NOT_PLAYING = 804;
    public static final int MEDIA_INFO_BAD_INTERLEAVING = 800;
    public static final int MEDIA_INFO_EXTERNAL_METADATA_UPDATE = 803;
    public static final int MEDIA_INFO_METADATA_UPDATE = 802;
    public static final int MEDIA_INFO_NETWORK_BANDWIDTH = 703;
    public static final int MEDIA_INFO_STARTED_AS_NEXT = 2;
    public static final int MEDIA_INFO_SUBTITLE_TIMED_OUT = 902;
    public static final int MEDIA_INFO_TIMED_TEXT_ERROR = 900;
    public static final int MEDIA_INFO_UNSUPPORTED_SUBTITLE = 901;
    public static final int MEDIA_INFO_VIDEO_NOT_PLAYING = 805;
    public static final int MEDIA_INFO_VIDEO_TRACK_LAGGING = 700;
    public static final java.lang.String MEDIA_MIMETYPE_TEXT_CEA_608 = "text/cea-608";
    public static final java.lang.String MEDIA_MIMETYPE_TEXT_CEA_708 = "text/cea-708";
    public static final java.lang.String MEDIA_MIMETYPE_TEXT_SUBRIP = "application/x-subrip";
    public static final java.lang.String MEDIA_MIMETYPE_TEXT_VTT = "text/vtt";
    public static final boolean METADATA_ALL = false;
    public static final boolean METADATA_UPDATE_ONLY = true;
    public static final int PLAYBACK_RATE_AUDIO_MODE_DEFAULT = 0;
    public static final int PLAYBACK_RATE_AUDIO_MODE_RESAMPLE = 2;
    public static final int PLAYBACK_RATE_AUDIO_MODE_STRETCH = 1;
    public static final int PREPARE_DRM_STATUS_PREPARATION_ERROR = 3;
    public static final int PREPARE_DRM_STATUS_PROVISIONING_NETWORK_ERROR = 1;
    public static final int PREPARE_DRM_STATUS_PROVISIONING_SERVER_ERROR = 2;
    public static final int PREPARE_DRM_STATUS_SUCCESS = 0;
    public static int native_pullBatteryData(android.os.Parcel p0) { return 0; }
    public void addSubtitleSource(java.io.InputStream p0, android.media.MediaFormat p1) {}
    public void addTimedTextSource(android.content.Context p0, android.net.Uri p1, java.lang.String p2) {}
    public void addTimedTextSource(java.io.FileDescriptor p0, long p1, long p2, java.lang.String p3) {}
    public void addTimedTextSource(java.io.FileDescriptor p0, java.lang.String p1) {}
    public void clearOnMediaTimeDiscontinuityListener() {}
    public void clearOnSubtitleDataListener() {}
    public android.media.VolumeShaper createVolumeShaper(android.media.VolumeShaper.Configuration p0) { return null; }
    public android.media.PlaybackParams easyPlaybackParams(float p0, int p1) { return null; }
    public android.media.MediaPlayer.DrmInfo getDrmInfo() { return null; }
    public java.lang.String getDrmPropertyString(java.lang.String p0) { return null; }
    public android.media.MediaDrm.KeyRequest getKeyRequest(byte[] p0, byte[] p1, java.lang.String p2, int p3, java.util.Map p4) { return null; }
    public android.os.PersistableBundle getMetrics() { return null; }
    public java.util.List getRoutedDevices() { return new java.util.ArrayList(); }
    public int getSelectedTrack(int p0) { return 0; }
    public android.media.SyncParams getSyncParams() { return (android.media.SyncParams) huskFill.get("SyncParams"); }
    public void invoke(android.os.Parcel p0, android.os.Parcel p1) {}
    public android.os.Parcel newRequest() { return null; }
    public void notifyAt(long p0) {}
    public void prepareDrm(java.util.UUID p0) {}
    public byte[] provideKeyResponse(byte[] p0, byte[] p1) { return null; }
    public void releaseDrm() {}
    public void restoreKeys(byte[] p0) {}
    public void setDrmPropertyString(java.lang.String p0, java.lang.String p1) {}
    public int setMetadataFilter(java.util.Set p0, java.util.Set p1) { return 0; }
    public void setOnDrmConfigHelper(android.media.MediaPlayer.OnDrmConfigHelper p0) {}
    public void setOnDrmInfoListener(android.media.MediaPlayer.OnDrmInfoListener p0) {}
    public void setOnDrmInfoListener(android.media.MediaPlayer.OnDrmInfoListener p0, android.os.Handler p1) {}
    public void setOnDrmPreparedListener(android.media.MediaPlayer.OnDrmPreparedListener p0) {}
    public void setOnDrmPreparedListener(android.media.MediaPlayer.OnDrmPreparedListener p0, android.os.Handler p1) {}
    public void setOnMediaTimeDiscontinuityListener(android.media.MediaPlayer.OnMediaTimeDiscontinuityListener p0) {}
    public void setOnMediaTimeDiscontinuityListener(android.media.MediaPlayer.OnMediaTimeDiscontinuityListener p0, android.os.Handler p1) {}
    public void setOnRtpRxNoticeListener(android.content.Context p0, java.util.concurrent.Executor p1, android.media.MediaPlayer.OnRtpRxNoticeListener p2) {}
    public void setOnSubtitleDataListener(android.media.MediaPlayer.OnSubtitleDataListener p0) {}
    public void setOnSubtitleDataListener(android.media.MediaPlayer.OnSubtitleDataListener p0, android.os.Handler p1) {}
    public void setOnTimedMetaDataAvailableListener(android.media.MediaPlayer.OnTimedMetaDataAvailableListener p0) {}
    public void setRetransmitEndpoint(java.net.InetSocketAddress p0) {}
    public void setSyncParams(android.media.SyncParams p0) { huskFill.put("SyncParams", p0); }
    // ---- end of generated members
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class DrmInfo {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public java.util.Map getPssh() { return (huskProps.get("Pssh") != null ? (java.util.Map) huskProps.get("Pssh") : new java.util.HashMap()); }
        public java.util.UUID[] getSupportedSchemes() { return (java.util.UUID[]) huskProps.get("SupportedSchemes"); }
        protected DrmInfo() {}
    }
    public static final class MetricsConstants {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public static final java.lang.String CODEC_AUDIO = "android.media.mediaplayer.audio.codec";
        public static final java.lang.String CODEC_VIDEO = "android.media.mediaplayer.video.codec";
        public static final java.lang.String DURATION = "android.media.mediaplayer.durationMs";
        public static final java.lang.String ERRORS = "android.media.mediaplayer.err";
        public static final java.lang.String ERROR_CODE = "android.media.mediaplayer.errcode";
        public static final java.lang.String FRAMES = "android.media.mediaplayer.frames";
        public static final java.lang.String FRAMES_DROPPED = "android.media.mediaplayer.dropped";
        public static final java.lang.String HEIGHT = "android.media.mediaplayer.height";
        public static final java.lang.String MIME_TYPE_AUDIO = "android.media.mediaplayer.audio.mime";
        public static final java.lang.String MIME_TYPE_VIDEO = "android.media.mediaplayer.video.mime";
        public static final java.lang.String PLAYING = "android.media.mediaplayer.playingMs";
        public static final java.lang.String WIDTH = "android.media.mediaplayer.width";
        protected MetricsConstants() {}
    }
    public static final class NoDrmSchemeException extends android.media.MediaDrmException {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public NoDrmSchemeException(java.lang.String p0) { super(p0); }
        NoDrmSchemeException() { this((java.lang.String) null); }
    }
    public interface OnDrmConfigHelper {
        void onDrmConfig(android.media.MediaPlayer p0);
    }
    public interface OnDrmInfoListener {
        void onDrmInfo(android.media.MediaPlayer p0, android.media.MediaPlayer.DrmInfo p1);
    }
    public interface OnDrmPreparedListener {
        void onDrmPrepared(android.media.MediaPlayer p0, int p1);
    }
    public interface OnMediaTimeDiscontinuityListener {
        void onMediaTimeDiscontinuity(android.media.MediaPlayer p0, android.media.MediaTimestamp p1);
    }
    public interface OnRtpRxNoticeListener {
        void onRtpRxNotice(android.media.MediaPlayer p0, int p1, int[] p2);
    }
    public interface OnSubtitleDataListener {
        void onSubtitleData(android.media.MediaPlayer p0, android.media.SubtitleData p1);
    }
    public interface OnTimedMetaDataAvailableListener {
        void onTimedMetaDataAvailable(android.media.MediaPlayer p0, android.media.TimedMetaData p1);
    }
    public static final class ProvisioningNetworkErrorException extends android.media.MediaDrmException {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public ProvisioningNetworkErrorException(java.lang.String p0) { super(p0); }
        ProvisioningNetworkErrorException() { this((java.lang.String) null); }
    }
    public static final class ProvisioningServerErrorException extends android.media.MediaDrmException {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public ProvisioningServerErrorException(java.lang.String p0) { super(p0); }
        ProvisioningServerErrorException() { this((java.lang.String) null); }
    }
    // ---- end of generated nested classes
}
