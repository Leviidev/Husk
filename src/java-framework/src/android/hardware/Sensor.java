package android.hardware;

/** A sensor of the phone's (CoreMotion's, through husk.Sensors), described as Android describes it. */
public final class Sensor {
    public static final int TYPE_ACCELEROMETER = 1, TYPE_MAGNETIC_FIELD = 2, TYPE_ORIENTATION = 3, TYPE_GYROSCOPE = 4, TYPE_LIGHT = 5, TYPE_PRESSURE = 6, TYPE_TEMPERATURE = 7,
        TYPE_PROXIMITY = 8, TYPE_GRAVITY = 9, TYPE_LINEAR_ACCELERATION = 10, TYPE_ROTATION_VECTOR = 11, TYPE_RELATIVE_HUMIDITY = 12, TYPE_AMBIENT_TEMPERATURE = 13,
        TYPE_MAGNETIC_FIELD_UNCALIBRATED = 14, TYPE_GAME_ROTATION_VECTOR = 15, TYPE_GYROSCOPE_UNCALIBRATED = 16, TYPE_SIGNIFICANT_MOTION = 17, TYPE_STEP_DETECTOR = 18,
        TYPE_STEP_COUNTER = 19, TYPE_GEOMAGNETIC_ROTATION_VECTOR = 20, TYPE_HEART_RATE = 21, TYPE_POSE_6DOF = 28, TYPE_STATIONARY_DETECT = 29, TYPE_MOTION_DETECT = 30,
        TYPE_LOW_LATENCY_OFFBODY_DETECT = 34, TYPE_ACCELEROMETER_UNCALIBRATED = 35, TYPE_HINGE_ANGLE = 36, TYPE_ALL = -1, TYPE_DEVICE_PRIVATE_BASE = 0x10000;
    public static final String STRING_TYPE_ACCELEROMETER = "android.sensor.accelerometer", STRING_TYPE_MAGNETIC_FIELD = "android.sensor.magnetic_field", STRING_TYPE_GYROSCOPE = "android.sensor.gyroscope",
        STRING_TYPE_GRAVITY = "android.sensor.gravity", STRING_TYPE_LINEAR_ACCELERATION = "android.sensor.linear_acceleration", STRING_TYPE_ROTATION_VECTOR = "android.sensor.rotation_vector",
        STRING_TYPE_GAME_ROTATION_VECTOR = "android.sensor.game_rotation_vector", STRING_TYPE_ORIENTATION = "android.sensor.orientation";
    public static final int REPORTING_MODE_CONTINUOUS = 0, REPORTING_MODE_ON_CHANGE = 1, REPORTING_MODE_ONE_SHOT = 2, REPORTING_MODE_SPECIAL_TRIGGER = 3;
    private final int mType;
    private final String mName, mStringType;
    private final float mRange, mResolution;
    Sensor(int type, String name, String stringType, float range, float resolution) { mType = type; mName = name; mStringType = stringType; mRange = range; mResolution = resolution; }
    public int getType() { return mType; }
    public String getName() { return mName; }
    public String getVendor() { return "Apple"; }
    public int getVersion() { return 1; }
    public String getStringType() { return mStringType; }
    public float getMaximumRange() { return mRange; }
    public float getResolution() { return mResolution; }
    public float getPower() { return 0.5f; }
    public int getMinDelay() { return 5000; }
    public int getMaxDelay() { return 200000; }
    public int getFifoReservedEventCount() { return 0; }
    public int getFifoMaxEventCount() { return 0; }
    public int getReportingMode() { return REPORTING_MODE_CONTINUOUS; }
    public int getId() { return mType; }
    public boolean isWakeUpSensor() { return false; }
    public boolean isDynamicSensor() { return false; }
    public boolean isAdditionalInfoSupported() { return false; }
    public int getHighestDirectReportRateLevel() { return 0; }
    public boolean isDirectChannelTypeSupported(int t) { return false; }
    @Override public String toString() { return "{Sensor name=\"" + mName + "\", vendor=\"Apple\", version=1, type=" + mType + ", maxRange=" + mRange + ", resolution=" + mResolution + "}"; }
}
