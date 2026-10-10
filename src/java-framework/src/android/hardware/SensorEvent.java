package android.hardware;
public class SensorEvent {
    public final float[] values;
    public Sensor sensor;
    public int accuracy;
    public long timestamp;
    public boolean firstEventAfterDiscontinuity;
    public SensorEvent(int valueSize) { values = new float[valueSize]; }
    public SensorEvent(Sensor s, int accuracy, long timestamp, float[] values) { this.sensor = s; this.accuracy = accuracy; this.timestamp = timestamp; this.values = values; }
    SensorEvent() { this(3); }
}
