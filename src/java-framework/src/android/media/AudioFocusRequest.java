package android.media;
public final class AudioFocusRequest {
    private final AudioManager.OnAudioFocusChangeListener mListener; private final AudioAttributes mAttrs; private final int mGain;
    private AudioFocusRequest(AudioManager.OnAudioFocusChangeListener l, AudioAttributes a, int g) { mListener = l; mAttrs = a; mGain = g; }
    public AudioAttributes getAudioAttributes() { return mAttrs; }
    public int getFocusGain() { return mGain; }
    public boolean willPauseWhenDucked() { return false; }
    public boolean acceptsDelayedFocusGain() { return false; }
    public static final class Builder {
        private AudioManager.OnAudioFocusChangeListener l; private AudioAttributes a = new AudioAttributes.Builder().build(); private int g;
        public Builder(int gain) { g = gain; }
        public Builder(AudioFocusRequest r) { l = r.mListener; a = r.mAttrs; g = r.mGain; }
        public Builder setFocusGain(int x) { g = x; return this; }
        public Builder setOnAudioFocusChangeListener(AudioManager.OnAudioFocusChangeListener x) { l = x; return this; }
        public Builder setOnAudioFocusChangeListener(AudioManager.OnAudioFocusChangeListener x, android.os.Handler h) { l = x; return this; }
        public Builder setAudioAttributes(AudioAttributes x) { a = x; return this; }
        public Builder setWillPauseWhenDucked(boolean b) { return this; }
        public Builder setAcceptsDelayedFocusGain(boolean b) { return this; }
        public Builder setForceDucking(boolean b) { return this; }
        public AudioFocusRequest build() { return new AudioFocusRequest(l, a, g); }
    }
}
