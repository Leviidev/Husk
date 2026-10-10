package android.media;
public final class PlaybackParams implements android.os.Parcelable {
    public static final int AUDIO_FALLBACK_MODE_DEFAULT = 0, AUDIO_FALLBACK_MODE_MUTE = 1, AUDIO_FALLBACK_MODE_FAIL = 2;
    private float mSpeed = 1f, mPitch = 1f;
    public PlaybackParams() {}
    public PlaybackParams allowDefaults() { return this; }
    public PlaybackParams setAudioFallbackMode(int m) { return this; } public int getAudioFallbackMode() { return 0; }
    public PlaybackParams setPitch(float p) { mPitch = p; return this; } public float getPitch() { return mPitch; }
    public PlaybackParams setSpeed(float s) { mSpeed = s; return this; } public float getSpeed() { return mSpeed; }
    public int describeContents() { return 0; } public void writeToParcel(android.os.Parcel p, int f) { p.writeFloat(mSpeed); p.writeFloat(mPitch); }
    public static final Creator<PlaybackParams> CREATOR = new Creator<PlaybackParams>() { public PlaybackParams createFromParcel(android.os.Parcel p) { return new PlaybackParams().setSpeed(p.readFloat()).setPitch(p.readFloat()); } public PlaybackParams[] newArray(int n) { return new PlaybackParams[n]; } };
}
