package android.hardware;

import android.os.Handler;
import android.os.Looper;
import java.util.ArrayList;
import java.util.List;

/**
 * The phone's motion sensors. Listeners get events on their handler's thread (the main thread by default) at about the rate they
 * asked for: one sensor thread polls husk.Sensors for new samples and posts them. TYPE_ORIENTATION is computed from gravity and the
 * magnetic field, as Android computes it. The static math (rotation matrices, orientation) is Android's.
 */
public class SensorManager {
    public static final int SENSOR_DELAY_FASTEST = 0, SENSOR_DELAY_GAME = 1, SENSOR_DELAY_UI = 2, SENSOR_DELAY_NORMAL = 3;
    public static final int SENSOR_STATUS_NO_CONTACT = -1, SENSOR_STATUS_UNRELIABLE = 0, SENSOR_STATUS_ACCURACY_LOW = 1, SENSOR_STATUS_ACCURACY_MEDIUM = 2, SENSOR_STATUS_ACCURACY_HIGH = 3;
    public static final float STANDARD_GRAVITY = 9.80665f, GRAVITY_EARTH = 9.80665f, GRAVITY_SUN = 275.0f, GRAVITY_MOON = 1.6f, MAGNETIC_FIELD_EARTH_MAX = 60.0f, MAGNETIC_FIELD_EARTH_MIN = 30.0f,
        PRESSURE_STANDARD_ATMOSPHERE = 1013.25f, LIGHT_SUNLIGHT_MAX = 120000.0f, LIGHT_SUNLIGHT = 110000.0f, LIGHT_SHADE = 20000.0f, LIGHT_OVERCAST = 10000.0f, LIGHT_SUNRISE = 400.0f, LIGHT_CLOUDY = 100.0f, LIGHT_FULLMOON = 0.25f, LIGHT_NO_MOON = 0.001f;
    public static final int AXIS_X = 1, AXIS_Y = 2, AXIS_Z = 3, AXIS_MINUS_X = AXIS_X | 0x80, AXIS_MINUS_Y = AXIS_Y | 0x80, AXIS_MINUS_Z = AXIS_Z | 0x80;
    @Deprecated public static final int SENSOR_ACCELEROMETER = 2, SENSOR_MAGNETIC_FIELD = 8, SENSOR_ORIENTATION = 1, SENSOR_ORIENTATION_RAW = 128, SENSOR_LIGHT = 16, SENSOR_PROXIMITY = 32, SENSOR_TEMPERATURE = 4, SENSOR_TRICORDER = 64, SENSOR_ALL = 127, SENSOR_MIN = SENSOR_ORIENTATION, SENSOR_MAX = ((SENSOR_ALL + 1) >> 1);
    @Deprecated public static final int DATA_X = 0, DATA_Y = 1, DATA_Z = 2, RAW_DATA_INDEX = 3, RAW_DATA_X = 3, RAW_DATA_Y = 4, RAW_DATA_Z = 5;
    public static abstract class DynamicSensorCallback { public void onDynamicSensorConnected(Sensor s) {} public void onDynamicSensorDisconnected(Sensor s) {} }

    private static final ArrayList<Sensor> sSensors = new ArrayList<>();
    static {
        add(Sensor.TYPE_ACCELEROMETER, "Accelerometer", Sensor.STRING_TYPE_ACCELEROMETER, 156.9f, 0.0024f);
        add(Sensor.TYPE_MAGNETIC_FIELD, "Magnetometer", Sensor.STRING_TYPE_MAGNETIC_FIELD, 4912f, 0.15f);
        add(Sensor.TYPE_GYROSCOPE, "Gyroscope", Sensor.STRING_TYPE_GYROSCOPE, 34.9f, 0.0011f);
        add(Sensor.TYPE_GRAVITY, "Gravity", Sensor.STRING_TYPE_GRAVITY, 19.6f, 0.0024f);
        add(Sensor.TYPE_LINEAR_ACCELERATION, "Linear Acceleration", Sensor.STRING_TYPE_LINEAR_ACCELERATION, 156.9f, 0.0024f);
        add(Sensor.TYPE_ROTATION_VECTOR, "Rotation Vector", Sensor.STRING_TYPE_ROTATION_VECTOR, 1f, 5.96e-8f);
        add(Sensor.TYPE_GAME_ROTATION_VECTOR, "Game Rotation Vector", Sensor.STRING_TYPE_GAME_ROTATION_VECTOR, 1f, 5.96e-8f);
        if (husk.Sensors.available(Sensor.TYPE_GRAVITY) && husk.Sensors.available(Sensor.TYPE_MAGNETIC_FIELD)) sSensors.add(new Sensor(Sensor.TYPE_ORIENTATION, "Orientation", Sensor.STRING_TYPE_ORIENTATION, 360f, 0.01f));
    }
    private static void add(int type, String name, String st, float range, float res) { if (husk.Sensors.available(type)) sSensors.add(new Sensor(type, name, st, range, res)); }

    private static final class Reg { SensorEventListener l; Sensor s; int periodUs; Handler h; long last; long lastPost; }
    private static final ArrayList<Reg> sRegs = new ArrayList<>();
    private static Thread sThread;

    public SensorManager() {}
    public List<Sensor> getSensorList(int type) { ArrayList<Sensor> r = new ArrayList<>(); for (Sensor s : sSensors) if (type == Sensor.TYPE_ALL || s.getType() == type) r.add(s); return r; }
    public List<Sensor> getDynamicSensorList(int type) { return new ArrayList<>(); }
    public Sensor getDefaultSensor(int type) { for (Sensor s : sSensors) if (s.getType() == type) return s; return null; }
    public Sensor getDefaultSensor(int type, boolean wakeUp) { return wakeUp ? null : getDefaultSensor(type); }
    public boolean isDynamicSensorDiscoverySupported() { return false; }
    public void registerDynamicSensorCallback(DynamicSensorCallback cb) {}
    public void registerDynamicSensorCallback(DynamicSensorCallback cb, Handler h) {}
    public void unregisterDynamicSensorCallback(DynamicSensorCallback cb) {}
    @Deprecated public int getSensors() { return SENSOR_ACCELEROMETER; }
    @Deprecated public boolean registerListener(SensorListener l, int sensors) { return false; }
    @Deprecated public boolean registerListener(SensorListener l, int sensors, int rate) { return false; }
    @Deprecated public void unregisterListener(SensorListener l) {}
    @Deprecated public void unregisterListener(SensorListener l, int sensors) {}
    private static int periodUs(int delay) {
        switch (delay) { case SENSOR_DELAY_FASTEST: return 5000; case SENSOR_DELAY_GAME: return 20000; case SENSOR_DELAY_UI: return 66667; case SENSOR_DELAY_NORMAL: return 200000; default: return Math.max(5000, delay); }
    }
    public boolean registerListener(SensorEventListener l, Sensor s, int delay) { return registerListener(l, s, delay, (Handler) null); }
    public boolean registerListener(SensorEventListener l, Sensor s, int delay, int maxReportLatencyUs) { return registerListener(l, s, delay, (Handler) null); }
    public boolean registerListener(SensorEventListener l, Sensor s, int delay, int maxReportLatencyUs, Handler h) { return registerListener(l, s, delay, h); }
    public boolean registerListener(SensorEventListener l, Sensor s, int delay, Handler h) {
        if (l == null || s == null) return false;
        synchronized (sRegs) {
            for (Reg r : sRegs) if (r.l == l && r.s == s) return true;
            Reg r = new Reg();
            r.l = l; r.s = s; r.periodUs = periodUs(delay);
            r.h = h != null ? h : new Handler(Looper.getMainLooper());
            sRegs.add(r);
            for (int t : sources(s.getType())) husk.Sensors.start(t, r.periodUs);
            if (sThread == null) { sThread = new Thread(SensorManager::loop, "SensorThread"); sThread.setDaemon(true); sThread.start(); }
            sRegs.notifyAll();
        }
        return true;
    }
    private static int[] sources(int type) { return type == Sensor.TYPE_ORIENTATION ? new int[] { Sensor.TYPE_GRAVITY, Sensor.TYPE_MAGNETIC_FIELD } : new int[] { type }; }
    public void unregisterListener(SensorEventListener l) { unregister(l, null); }
    public void unregisterListener(SensorEventListener l, Sensor s) { unregister(l, s); }
    private static void unregister(SensorEventListener l, Sensor s) {
        synchronized (sRegs) {
            for (int i = sRegs.size() - 1; i >= 0; i--) {
                Reg r = sRegs.get(i);
                if (r.l == l && (s == null || r.s == s)) { sRegs.remove(i); for (int t : sources(r.s.getType())) husk.Sensors.stop(t); }
            }
        }
    }
    public boolean flush(SensorEventListener l) { if (l instanceof SensorEventListener2) { new Handler(Looper.getMainLooper()).post(() -> ((SensorEventListener2) l).onFlushCompleted(null)); } return true; }
    public boolean requestTriggerSensor(TriggerEventListener l, Sensor s) { return false; }
    public boolean cancelTriggerSensor(TriggerEventListener l, Sensor s) { return false; }
    /** The sensor thread: new samples to their listeners, no faster than each asked. */
    private static void loop() {
        float[] buf = new float[5], g = new float[3], m = new float[3], R = new float[9], o = new float[3];
        for (;;) {
            ArrayList<Reg> regs;
            synchronized (sRegs) {
                while (sRegs.isEmpty()) { try { sRegs.wait(); } catch (InterruptedException e) { return; } }
                regs = new ArrayList<>(sRegs);
            }
            long now = System.nanoTime();
            int minPeriod = Integer.MAX_VALUE;
            for (Reg r : regs) {
                minPeriod = Math.min(minPeriod, r.periodUs);
                if ((now - r.lastPost) / 1000 < r.periodUs * 9 / 10) continue;
                int type = r.s.getType();
                float[] values; long ts;
                if (type == Sensor.TYPE_ORIENTATION) {
                    long t1 = husk.Sensors.read(Sensor.TYPE_GRAVITY, g), t2 = husk.Sensors.read(Sensor.TYPE_MAGNETIC_FIELD, m);
                    ts = Math.max(t1, t2);
                    if (t1 == 0 || t2 == 0 || ts == r.last || !getRotationMatrix(R, null, g, m)) continue;
                    getOrientation(R, o);
                    values = new float[] { (float) ((Math.toDegrees(o[0]) + 360) % 360), (float) Math.toDegrees(o[1]), (float) Math.toDegrees(o[2]) };
                } else {
                    ts = husk.Sensors.read(type, buf);
                    if (ts == 0 || ts == r.last) continue;
                    int n = type == Sensor.TYPE_ROTATION_VECTOR ? 5 : type == Sensor.TYPE_GAME_ROTATION_VECTOR ? 4 : 3;
                    values = java.util.Arrays.copyOf(buf, n);
                }
                r.last = ts; r.lastPost = now;
                final SensorEvent e = new SensorEvent(r.s, SENSOR_STATUS_ACCURACY_HIGH, ts, values);
                final SensorEventListener l = r.l;
                r.h.post(() -> { synchronized (sRegs) { boolean live = false; for (Reg x : sRegs) if (x.l == l) live = true; if (!live) return; } l.onSensorChanged(e); });
            }
            try { Thread.sleep(Math.max(2, Math.min(50, minPeriod / 2000))); } catch (InterruptedException ex) { return; }
        }
    }

    // ---- Android's math
    public static boolean getRotationMatrix(float[] R, float[] I, float[] gravity, float[] geomagnetic) {
        float Ax = gravity[0], Ay = gravity[1], Az = gravity[2];
        final float normsqA = (Ax * Ax + Ay * Ay + Az * Az), g = 9.81f, freeFallGravitySquared = 0.01f * g * g;
        if (normsqA < freeFallGravitySquared) return false;
        final float Ex = geomagnetic[0], Ey = geomagnetic[1], Ez = geomagnetic[2];
        float Hx = Ey * Az - Ez * Ay, Hy = Ez * Ax - Ex * Az, Hz = Ex * Ay - Ey * Ax;
        final float normH = (float) Math.sqrt(Hx * Hx + Hy * Hy + Hz * Hz);
        if (normH < 0.1f) return false;
        final float invH = 1.0f / normH;
        Hx *= invH; Hy *= invH; Hz *= invH;
        final float invA = 1.0f / (float) Math.sqrt(Ax * Ax + Ay * Ay + Az * Az);
        Ax *= invA; Ay *= invA; Az *= invA;
        final float Mx = Ay * Hz - Az * Hy, My = Az * Hx - Ax * Hz, Mz = Ax * Hy - Ay * Hx;
        if (R != null) {
            if (R.length == 9) { R[0] = Hx; R[1] = Hy; R[2] = Hz; R[3] = Mx; R[4] = My; R[5] = Mz; R[6] = Ax; R[7] = Ay; R[8] = Az; }
            else if (R.length == 16) { R[0] = Hx; R[1] = Hy; R[2] = Hz; R[3] = 0; R[4] = Mx; R[5] = My; R[6] = Mz; R[7] = 0; R[8] = Ax; R[9] = Ay; R[10] = Az; R[11] = 0; R[12] = 0; R[13] = 0; R[14] = 0; R[15] = 1; }
        }
        if (I != null) {
            final float invE = 1.0f / (float) Math.sqrt(Ex * Ex + Ey * Ey + Ez * Ez);
            final float c = (Ex * Mx + Ey * My + Ez * Mz) * invE, s = (Ex * Ax + Ey * Ay + Ez * Az) * invE;
            if (I.length == 9) { I[0] = 1; I[1] = 0; I[2] = 0; I[3] = 0; I[4] = c; I[5] = s; I[6] = 0; I[7] = -s; I[8] = c; }
            else if (I.length == 16) { I[0] = 1; I[1] = 0; I[2] = 0; I[4] = 0; I[5] = c; I[6] = s; I[8] = 0; I[9] = -s; I[10] = c; I[3] = I[7] = I[11] = I[12] = I[13] = I[14] = 0; I[15] = 1; }
        }
        return true;
    }
    public static float getInclination(float[] I) { if (I.length == 9) return (float) Math.atan2(I[5], I[4]); return (float) Math.atan2(I[6], I[5]); }
    public static boolean remapCoordinateSystem(float[] inR, int X, int Y, float[] outR) {
        if (inR == outR) { final float[] temp = new float[16]; synchronized (temp) { if (remapCoordinateSystemImpl(inR, X, Y, temp)) { System.arraycopy(temp, 0, outR, 0, outR.length); return true; } } }
        return remapCoordinateSystemImpl(inR, X, Y, outR);
    }
    private static boolean remapCoordinateSystemImpl(float[] inR, int X, int Y, float[] outR) {
        final int length = outR.length;
        if (inR.length != length) return false;
        if ((X & 0x7C) != 0 || (Y & 0x7C) != 0) return false;
        if (((X & 0x3) == 0) || ((Y & 0x3) == 0)) return false;
        if ((X & 0x3) == (Y & 0x3)) return false;
        int Z = X ^ Y;
        final int x = (X & 0x3) - 1, y = (Y & 0x3) - 1, z = (Z & 0x3) - 1;
        final int axis_y = (z + 1) % 3, axis_z = (z + 2) % 3;
        if (((x ^ axis_y) | (y ^ axis_z)) != 0) Z ^= 0x80;
        final boolean sx = (X >= 0x80), sy = (Y >= 0x80), sz = (Z >= 0x80);
        final int rowLength = ((length == 16) ? 4 : 3);
        for (int j = 0; j < 3; j++) {
            final int offset = j * rowLength;
            for (int i = 0; i < 3; i++) {
                if (x == i) outR[offset + i] = sx ? -inR[offset + 0] : inR[offset + 0];
                if (y == i) outR[offset + i] = sy ? -inR[offset + 1] : inR[offset + 1];
                if (z == i) outR[offset + i] = sz ? -inR[offset + 2] : inR[offset + 2];
            }
        }
        if (length == 16) { outR[3] = outR[7] = outR[11] = outR[12] = outR[13] = outR[14] = 0; outR[15] = 1; }
        return true;
    }
    public static float[] getOrientation(float[] R, float[] values) {
        if (R.length == 9) { values[0] = (float) Math.atan2(R[1], R[4]); values[1] = (float) Math.asin(-R[7]); values[2] = (float) Math.atan2(-R[6], R[8]); }
        else { values[0] = (float) Math.atan2(R[1], R[5]); values[1] = (float) Math.asin(-R[9]); values[2] = (float) Math.atan2(-R[8], R[10]); }
        return values;
    }
    public static float getAltitude(float p0, float p) { final float coef = 1.0f / 5.255f; return 44330.0f * (1.0f - (float) Math.pow(p / p0, coef)); }
    public static void getAngleChange(float[] angleChange, float[] R, float[] prevR) {
        float rd1, rd4, rd6, rd7, rd8, ri0, ri1, ri2, ri3, ri4, ri5, ri6, ri7, ri8, pri0, pri1, pri2, pri3, pri4, pri5, pri6, pri7, pri8;
        if (R.length == 9) { ri0 = R[0]; ri1 = R[1]; ri2 = R[2]; ri3 = R[3]; ri4 = R[4]; ri5 = R[5]; ri6 = R[6]; ri7 = R[7]; ri8 = R[8]; }
        else { ri0 = R[0]; ri1 = R[1]; ri2 = R[2]; ri3 = R[4]; ri4 = R[5]; ri5 = R[6]; ri6 = R[8]; ri7 = R[9]; ri8 = R[10]; }
        if (prevR.length == 9) { pri0 = prevR[0]; pri1 = prevR[1]; pri2 = prevR[2]; pri3 = prevR[3]; pri4 = prevR[4]; pri5 = prevR[5]; pri6 = prevR[6]; pri7 = prevR[7]; pri8 = prevR[8]; }
        else { pri0 = prevR[0]; pri1 = prevR[1]; pri2 = prevR[2]; pri3 = prevR[4]; pri4 = prevR[5]; pri5 = prevR[6]; pri6 = prevR[8]; pri7 = prevR[9]; pri8 = prevR[10]; }
        rd1 = pri0 * ri1 + pri3 * ri4 + pri6 * ri7; rd4 = pri1 * ri1 + pri4 * ri4 + pri7 * ri7; rd6 = pri2 * ri0 + pri5 * ri3 + pri8 * ri6; rd7 = pri2 * ri1 + pri5 * ri4 + pri8 * ri7; rd8 = pri2 * ri2 + pri5 * ri5 + pri8 * ri8;
        angleChange[0] = (float) Math.atan2(rd1, rd4); angleChange[1] = (float) Math.asin(-rd7); angleChange[2] = (float) Math.atan2(-rd6, rd8);
    }
    public static void getRotationMatrixFromVector(float[] R, float[] rotationVector) {
        float q0, q1 = rotationVector[0], q2 = rotationVector[1], q3 = rotationVector[2];
        if (rotationVector.length >= 4) q0 = rotationVector[3];
        else { q0 = 1 - q1 * q1 - q2 * q2 - q3 * q3; q0 = (q0 > 0) ? (float) Math.sqrt(q0) : 0; }
        float sq_q1 = 2 * q1 * q1, sq_q2 = 2 * q2 * q2, sq_q3 = 2 * q3 * q3, q1_q2 = 2 * q1 * q2, q3_q0 = 2 * q3 * q0, q1_q3 = 2 * q1 * q3, q2_q0 = 2 * q2 * q0, q2_q3 = 2 * q2 * q3, q1_q0 = 2 * q1 * q0;
        if (R.length == 9) {
            R[0] = 1 - sq_q2 - sq_q3; R[1] = q1_q2 - q3_q0; R[2] = q1_q3 + q2_q0;
            R[3] = q1_q2 + q3_q0; R[4] = 1 - sq_q1 - sq_q3; R[5] = q2_q3 - q1_q0;
            R[6] = q1_q3 - q2_q0; R[7] = q2_q3 + q1_q0; R[8] = 1 - sq_q1 - sq_q2;
        } else if (R.length == 16) {
            R[0] = 1 - sq_q2 - sq_q3; R[1] = q1_q2 - q3_q0; R[2] = q1_q3 + q2_q0; R[3] = 0.0f;
            R[4] = q1_q2 + q3_q0; R[5] = 1 - sq_q1 - sq_q3; R[6] = q2_q3 - q1_q0; R[7] = 0.0f;
            R[8] = q1_q3 - q2_q0; R[9] = q2_q3 + q1_q0; R[10] = 1 - sq_q1 - sq_q2; R[11] = 0.0f;
            R[12] = R[13] = R[14] = 0.0f; R[15] = 1.0f;
        }
    }
    public static void getQuaternionFromVector(float[] Q, float[] rv) {
        if (rv.length >= 4) Q[0] = rv[3];
        else { Q[0] = 1 - rv[0] * rv[0] - rv[1] * rv[1] - rv[2] * rv[2]; Q[0] = (Q[0] > 0) ? (float) Math.sqrt(Q[0]) : 0; }
        Q[1] = rv[0]; Q[2] = rv[1]; Q[3] = rv[2];
    }
}
