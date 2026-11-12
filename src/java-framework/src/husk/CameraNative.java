package husk;

/** AVFoundation bridge. Opening requests video access; enumeration never requests permission. No microphone is used. */
public final class CameraNative {
    public static native String[] ids();
    public static native long open(String id);
    /** 0 waiting, 1 running, 2 denied, 3 absent, 4 failed/closed. */
    public static native int status(long camera);
    public static native void close(long camera);
    /** timestamp (monotonic ns), exposure ns, ISO; null before a real frame. */
    public static native long[] frame(long camera);
    /** JPEG (256) or planar YUV_420_888 (35), at the requested dimensions. */
    public static native byte[] image(long camera, int format, int width, int height, int rotation);
    public static native void preview(long camera, android.view.Surface target, int width, int height);
    public static native void focus(long camera, boolean infinity);
    public static native long photo(String destination);
    /** 0 waiting, 1 saved, 2 cancelled, 3 unavailable, 4 failed. */
    public static native int photoStatus(long request);
    public static native void cancelPhoto(long request);
}
