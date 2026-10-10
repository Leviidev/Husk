package android.hardware;
public class SensorManager {
    public static final int SENSOR_DELAY_FASTEST = 0, SENSOR_DELAY_GAME = 1, SENSOR_DELAY_UI = 2, SENSOR_DELAY_NORMAL = 3;
    public static final float GRAVITY_EARTH = 9.80665f;
    public Sensor getDefaultSensor(int type) { return null; }
    public java.util.List<Sensor> getSensorList(int type) { return new java.util.ArrayList<>(); }
    public boolean registerListener(SensorEventListener l, Sensor s, int delay) { return false; }
    public boolean registerListener(SensorEventListener l, Sensor s, int delay, android.os.Handler h) { return false; }
    public void unregisterListener(SensorEventListener l) {}
    public void unregisterListener(SensorEventListener l, Sensor s) {}
    public static boolean getRotationMatrix(float[] R, float[] I, float[] g, float[] m) { return false; }
    public static float[] getOrientation(float[] R, float[] v) { return v; }
    public static void getRotationMatrixFromVector(float[] R, float[] v) {}
    public static boolean remapCoordinateSystem(float[] in, int x, int y, float[] out) { return false; }
}
