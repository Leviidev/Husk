package husk;
/** Core Location bridge. See husk-tl-location.h for the sample layout. */
public final class GeoNative {
    public static final int PENDING = 0, AUTHORIZED = 1, DENIED = 2, DISABLED = 3, UNAVAILABLE = 4;
    private GeoNative() {}
    public static native int status();
    public static native void enable(boolean enabled);
    public static native long read(double[] values);
    public static native void reset();
}
