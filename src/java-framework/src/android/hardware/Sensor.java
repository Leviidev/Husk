package android.hardware;
public final class Sensor {
    public static final int TYPE_ACCELEROMETER = 1, TYPE_MAGNETIC_FIELD = 2, TYPE_ORIENTATION = 3, TYPE_GYROSCOPE = 4, TYPE_LIGHT = 5, TYPE_GRAVITY = 9,
        TYPE_LINEAR_ACCELERATION = 10, TYPE_ROTATION_VECTOR = 11, TYPE_GAME_ROTATION_VECTOR = 15, TYPE_ALL = -1;
    public int getType() { return 0; } public String getName() { return ""; } public float getMaximumRange() { return 0; }
}
