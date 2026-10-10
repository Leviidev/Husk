package android.content.pm;

/** What the device offers: the GL ES version above all (3.2 through ANGLE), a touch screen, no keyboard or navigation keys. */
public class ConfigurationInfo implements android.os.Parcelable {
    public static final int GL_ES_VERSION_UNDEFINED = 0;
    public static final int INPUT_FEATURE_FIVE_WAY_NAV = 2;
    public static final int INPUT_FEATURE_HARD_KEYBOARD = 1;
    public int reqGlEsVersion, reqInputFeatures, reqKeyboardType, reqNavigation, reqTouchScreen;
    public ConfigurationInfo() {}
    public ConfigurationInfo(ConfigurationInfo o) {
        reqGlEsVersion = o.reqGlEsVersion; reqInputFeatures = o.reqInputFeatures; reqKeyboardType = o.reqKeyboardType;
        reqNavigation = o.reqNavigation; reqTouchScreen = o.reqTouchScreen;
    }
    public String getGlEsVersion() { return ((reqGlEsVersion & 0xffff0000) >> 16) + "." + (reqGlEsVersion & 0x0000ffff); }
    public int describeContents() { return 0; }
    public void writeToParcel(android.os.Parcel dest, int flags) {
        dest.writeInt(reqTouchScreen); dest.writeInt(reqKeyboardType); dest.writeInt(reqNavigation); dest.writeInt(reqInputFeatures); dest.writeInt(reqGlEsVersion);
    }
    public static final Creator<ConfigurationInfo> CREATOR = new Creator<ConfigurationInfo>() {
        public ConfigurationInfo createFromParcel(android.os.Parcel s) {
            ConfigurationInfo c = new ConfigurationInfo();
            c.reqTouchScreen = s.readInt(); c.reqKeyboardType = s.readInt(); c.reqNavigation = s.readInt(); c.reqInputFeatures = s.readInt(); c.reqGlEsVersion = s.readInt();
            return c;
        }
        public ConfigurationInfo[] newArray(int size) { return new ConfigurationInfo[size]; }
    };
    /** Husk's device. */
    public static ConfigurationInfo huskDevice() {
        ConfigurationInfo c = new ConfigurationInfo();
        c.reqGlEsVersion = 0x00030002;
        c.reqTouchScreen = android.content.res.Configuration.TOUCHSCREEN_FINGER;
        c.reqKeyboardType = android.content.res.Configuration.KEYBOARD_NOKEYS;
        c.reqNavigation = android.content.res.Configuration.NAVIGATION_NONAV;
        return c;
    }
}
