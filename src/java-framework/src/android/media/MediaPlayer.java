package android.media;

/** Music. Husk does not decode it yet: a player that loads, plays and reports a position, silently. */
public class MediaPlayer {
    public interface OnCompletionListener { void onCompletion(MediaPlayer mp); }
    public interface OnPreparedListener { void onPrepared(MediaPlayer mp); }
    public interface OnErrorListener { boolean onError(MediaPlayer mp, int what, int extra); }
    public interface OnSeekCompleteListener { void onSeekComplete(MediaPlayer mp); }
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
