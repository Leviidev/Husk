package android.hardware;
public interface SensorEventListener { void onSensorChanged(SensorEvent e); void onAccuracyChanged(Sensor s, int accuracy); }
