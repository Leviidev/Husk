package android.hardware;
@Deprecated public interface SensorListener { void onSensorChanged(int sensor, float[] values); void onAccuracyChanged(int sensor, int accuracy); }
