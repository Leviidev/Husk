package android.view;
public final class SurfaceControl implements android.os.Parcelable {
    public static class Transaction implements java.io.Closeable { public Transaction apply() { return this; } public void apply(boolean sync) {} public void close() {} public Transaction setVisibility(SurfaceControl s, boolean v) { return this; } public Transaction setAlpha(SurfaceControl s, float a) { return this; } public Transaction setLayer(SurfaceControl s, int z) { return this; } public Transaction setBufferSize(SurfaceControl s, int w, int h) { return this; } public Transaction reparent(SurfaceControl s, SurfaceControl p) { return this; } public Transaction merge(Transaction o) { return this; } }
    public static class Builder { public Builder setName(String n) { return this; } public Builder setBufferSize(int w, int h) { return this; } public Builder setParent(SurfaceControl p) { return this; } public Builder setFormat(int f) { return this; } public Builder setOpaque(boolean o) { return this; } public Builder setHidden(boolean h) { return this; } public SurfaceControl build() { return new SurfaceControl(); } }
    public boolean isValid() { return true; }
    public void release() {}
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel p, int f) {}
    public static final Creator<SurfaceControl> CREATOR = new Creator<SurfaceControl>() { public SurfaceControl createFromParcel(android.os.Parcel p) { return new SurfaceControl(); } public SurfaceControl[] newArray(int n) { return new SurfaceControl[n]; } };
}
