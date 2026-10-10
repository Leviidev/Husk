package husk;
/** The phone's motion sensors (husk-tl-dvm-sensors.m): Android sensor types, Android units. */
public final class Sensors {
    private Sensors() {}
    public static native boolean available(int type);
    public static native void start(int type, int periodUs);
    public static native void stop(int type);
    public static native long read(int type, float[] values);   // the latest sample's time (ns), 0 when none
}
