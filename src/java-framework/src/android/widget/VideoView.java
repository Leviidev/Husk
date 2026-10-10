package android.widget;

import android.content.Context;
import android.media.MediaPlayer;
import android.net.Uri;
import android.util.AttributeSet;
import android.view.SurfaceView;

/** A video surface: its source is recorded and its listeners are told it cannot play (video playback is not available yet). */
public class VideoView extends SurfaceView implements MediaController.MediaPlayerControl {
    private Uri mUri;
    private MediaPlayer.OnErrorListener mOnError;
    private MediaPlayer.OnPreparedListener mOnPrepared;
    private MediaPlayer.OnCompletionListener mOnCompletion;
    public VideoView(Context c) { super(c); }
    public VideoView(Context c, AttributeSet a) { super(c, a); }
    public VideoView(Context c, AttributeSet a, int s) { super(c, a, s); }
    public VideoView(Context c, AttributeSet a, int s, int r) { super(c, a, s, r); }
    public void setVideoPath(String p) { setVideoURI(Uri.parse(p)); }
    public void setVideoURI(Uri u) { setVideoURI(u, null); }
    public void setVideoURI(Uri u, java.util.Map<String, String> headers) {
        mUri = u;
        android.util.Log.w("VideoView", "video playback is not available: " + u);
        post(() -> { if (mOnError != null) mOnError.onError(null, MediaPlayer.MEDIA_ERROR_UNKNOWN, 0); });
    }
    public void setMediaController(MediaController c) {}
    public void setOnPreparedListener(MediaPlayer.OnPreparedListener l) { mOnPrepared = l; }
    public void setOnCompletionListener(MediaPlayer.OnCompletionListener l) { mOnCompletion = l; }
    public void setOnErrorListener(MediaPlayer.OnErrorListener l) { mOnError = l; }
    public void setOnInfoListener(MediaPlayer.OnInfoListener l) {}
    public void setAudioFocusRequest(int f) {}
    public void stopPlayback() {}
    public void suspend() {} public void resume() {}
    public void start() {} public void pause() {}
    public int getDuration() { return -1; } public int getCurrentPosition() { return 0; } public void seekTo(int ms) {}
    public boolean isPlaying() { return false; } public int getBufferPercentage() { return 0; }
    public boolean canPause() { return true; } public boolean canSeekBackward() { return true; } public boolean canSeekForward() { return true; }
    public int getAudioSessionId() { return 0; }
    public int resolveAdjustedSize(int desired, int spec) { return getDefaultSize(desired, spec); }
}
