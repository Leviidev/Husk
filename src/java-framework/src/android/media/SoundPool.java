package android.media;

import java.util.HashMap;

/** Short sounds: decoded and mixed by Husk (husk.Native's sound calls); until a sound loads it plays nothing. */
public class SoundPool {
    public interface OnLoadCompleteListener { void onLoadComplete(SoundPool pool, int sampleId, int status); }
    public static class Builder {
        private int max = 8;
        public Builder setMaxStreams(int m) { max = m; return this; }
        public Builder setAudioAttributes(AudioAttributes a) { return this; }
        public SoundPool build() { return new SoundPool(max, 3, 0); }
    }
    private int next = 1, nextStream = 1;
    private OnLoadCompleteListener listener;
    private final HashMap<Integer, String> sounds = new HashMap<>();
    public SoundPool(int maxStreams, int streamType, int quality) {}
    public void setOnLoadCompleteListener(OnLoadCompleteListener l) { listener = l; }
    private int loaded(String what) {
        int id = next++;
        sounds.put(id, what);
        if (listener != null) { final int i = id; new android.os.Handler(android.os.Looper.getMainLooper()).post(() -> listener.onLoadComplete(this, i, 0)); }
        return id;
    }
    public int load(String path, int priority) { return loaded(path); }
    public int load(android.content.res.AssetFileDescriptor afd, int priority) { return loaded(afd.huskAssetName()); }
    public int load(android.content.Context c, int resId, int priority) { return loaded("res:" + resId); }
    public int load(java.io.FileDescriptor fd, long offset, long length, int priority) { return loaded("fd"); }
    public boolean unload(int id) { return sounds.remove(id) != null; }
    public int play(int soundId, float left, float right, int priority, int loop, float rate) { return sounds.containsKey(soundId) ? nextStream++ : 0; }
    public void stop(int stream) {} public void pause(int stream) {} public void resume(int stream) {}
    public void autoPause() {} public void autoResume() {}
    public void setVolume(int stream, float l, float r) {} public void setRate(int stream, float rate) {} public void setLoop(int stream, int loop) {} public void setPriority(int stream, int p) {}
    public void release() {}
}
