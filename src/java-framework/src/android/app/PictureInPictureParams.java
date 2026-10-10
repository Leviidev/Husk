package android.app;
public final class PictureInPictureParams implements android.os.Parcelable {
    public int describeContents() { return 0; }
    public static class Builder { public Builder setAspectRatio(android.util.Rational r) { return this; } public Builder setAutoEnterEnabled(boolean b) { return this; } public Builder setSourceRectHint(android.graphics.Rect r) { return this; } public Builder setActions(java.util.List<RemoteAction> a) { return this; } public Builder setSeamlessResizeEnabled(boolean b) { return this; } public PictureInPictureParams build() { return new PictureInPictureParams(); } }
}
