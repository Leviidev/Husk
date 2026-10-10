package android.os;
public class BatteryManager {
    public static final int BATTERY_PROPERTY_CHARGE_COUNTER = 1, BATTERY_PROPERTY_CURRENT_NOW = 2, BATTERY_PROPERTY_CURRENT_AVERAGE = 3,
        BATTERY_PROPERTY_CAPACITY = 4, BATTERY_PROPERTY_ENERGY_COUNTER = 5, BATTERY_PROPERTY_STATUS = 6, BATTERY_STATUS_UNKNOWN = 1,
        BATTERY_STATUS_CHARGING = 2, BATTERY_STATUS_DISCHARGING = 3, BATTERY_STATUS_NOT_CHARGING = 4, BATTERY_STATUS_FULL = 5,
        BATTERY_PLUGGED_AC = 1, BATTERY_PLUGGED_USB = 2, BATTERY_PLUGGED_WIRELESS = 4, BATTERY_HEALTH_GOOD = 2;
    public static final String EXTRA_LEVEL = "level", EXTRA_SCALE = "scale", EXTRA_STATUS = "status", EXTRA_PLUGGED = "plugged", EXTRA_HEALTH = "health",
        EXTRA_PRESENT = "present", EXTRA_TEMPERATURE = "temperature", EXTRA_VOLTAGE = "voltage", EXTRA_TECHNOLOGY = "technology";
    public int getIntProperty(int id) { return id == BATTERY_PROPERTY_CAPACITY ? 100 : id == BATTERY_PROPERTY_STATUS ? BATTERY_STATUS_FULL : 0; }
    public long getLongProperty(int id) { return getIntProperty(id); }
    public boolean isCharging() { return false; }
    public long computeChargeTimeRemaining() { return -1; }
}
