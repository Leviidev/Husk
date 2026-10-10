package android.media;

/** Music. Husk does not decode it yet: a player that loads, plays and reports a position, silently. */
public class MediaPlayer {
    public interface OnCompletionListener { void onCompletion(MediaPlayer mp); }
    public interface OnPreparedListener { void onPrepared(MediaPlayer mp); }
    public interface OnErrorListener { boolean onError(MediaPlayer mp, int what, int extra); }
    public interface OnSeekCompleteListener { void onSeekComplete(MediaPlayer mp); }
    public interface OnInfoListener { boolean onInfo(MediaPlayer mp, int what, int extra); }
    public interface OnBufferingUpdateListener { void onBufferingUpdate(MediaPlayer mp, int percent); }
    public interface OnVideoSizeChangedListener { void onVideoSizeChanged(MediaPlayer mp, int w, int h); }
    public interface OnTimedTextListener { void onTimedText(MediaPlayer mp, Object text); }
    public static final int MEDIA_ERROR_UNKNOWN = 1, MEDIA_ERROR_SERVER_DIED = 100, MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 200, MEDIA_ERROR_IO = -1004, MEDIA_ERROR_MALFORMED = -1007, MEDIA_ERROR_UNSUPPORTED = -1010, MEDIA_ERROR_TIMED_OUT = -110;
    public static final int MEDIA_INFO_UNKNOWN = 1, MEDIA_INFO_VIDEO_RENDERING_START = 3, MEDIA_INFO_BUFFERING_START = 701, MEDIA_INFO_BUFFERING_END = 702;
    public static final int SEEK_PREVIOUS_SYNC = 0, SEEK_NEXT_SYNC = 1, SEEK_CLOSEST_SYNC = 2, SEEK_CLOSEST = 3;
    public static final int VIDEO_SCALING_MODE_SCALE_TO_FIT = 1, VIDEO_SCALING_MODE_SCALE_TO_FIT_WITH_CROPPING = 2;
    public void setDisplay(android.view.SurfaceHolder h) {}
    public void setSurface(android.view.Surface s) {}
    public void setVideoScalingMode(int m) {}
    public int getVideoWidth() { return 0; }
    public int getVideoHeight() { return 0; }
    public void setScreenOnWhilePlaying(boolean b) {}
    public void setAudioSessionId(int id) {}
    public int getAudioSessionId() { return 0; }
    public void setAuxEffectSendLevel(float l) {}
    public void attachAuxEffect(int id) {}
    public void seekTo(long ms, int mode) { seekTo((int) ms); }
    public void setNextMediaPlayer(MediaPlayer next) {}
    public void setOnInfoListener(OnInfoListener l) {}
    public void setOnBufferingUpdateListener(OnBufferingUpdateListener l) {}
    public void setOnVideoSizeChangedListener(OnVideoSizeChangedListener l) {}
    public void setDataSource(android.content.Context c, android.net.Uri u, java.util.Map<String, String> headers) {}
    public void setDataSource(String path, java.util.Map<String, String> headers) {}
    public void setPlaybackParams(PlaybackParams p) {}
    public PlaybackParams getPlaybackParams() { return new PlaybackParams(); }
    public static MediaPlayer create(android.content.Context c, android.net.Uri u, android.view.SurfaceHolder h) { return create(c, u); }
    public static MediaPlayer create(android.content.Context c, int resId, AudioAttributes a, int session) { return create(c, resId); }
    private boolean playing, looping;
    private long startedAt, offset;
    private OnPreparedListener prepared;
    public MediaPlayer() {}
    public static MediaPlayer create(android.content.Context c, int resId) { return new MediaPlayer(); }
    public static MediaPlayer create(android.content.Context c, android.net.Uri u) { return new MediaPlayer(); }
    public void setDataSource(String path) {}
    public void setDataSource(java.io.FileDescriptor fd) {}
    public void setDataSource(java.io.FileDescriptor fd, long offset, long length) {}
    public void setDataSource(android.content.Context c, android.net.Uri u) {}
    public void setDataSource(android.content.res.AssetFileDescriptor afd) {}
    public void prepare() {}
    public void prepareAsync() { if (prepared != null) new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> prepared.onPrepared(this)); }
    public void start() { if (!playing) { playing = true; startedAt = System.currentTimeMillis(); } }
    public void pause() { if (playing) { offset += System.currentTimeMillis() - startedAt; playing = false; } }
    public void stop() { playing = false; offset = 0; }
    public void reset() { stop(); }
    public void release() { stop(); }
    public boolean isPlaying() { return playing; }
    public boolean isLooping() { return looping; }
    public void setLooping(boolean l) { looping = l; }
    public void setVolume(float l, float r) {}
    public void seekTo(int ms) { offset = ms; startedAt = System.currentTimeMillis(); }
    public int getCurrentPosition() { return (int) (offset + (playing ? System.currentTimeMillis() - startedAt : 0)); }
    public int getDuration() { return 0; }
    public void setAudioStreamType(int t) {}
    public void setAudioAttributes(AudioAttributes a) {}
    public void setWakeMode(android.content.Context c, int mode) {}
    public void setOnCompletionListener(OnCompletionListener l) {}
    public void setOnPreparedListener(OnPreparedListener l) { prepared = l; }
    public void setOnErrorListener(OnErrorListener l) {}
    public void setOnSeekCompleteListener(OnSeekCompleteListener l) {}
}
