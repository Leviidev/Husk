package android.os;

public class Build {
    public static final String MODEL = "Pixel 8", MANUFACTURER = "Google", BRAND = "google", DEVICE = "shiba", PRODUCT = "shiba";
    public static final String HARDWARE = "husk", BOARD = "shiba", DISPLAY = "AP1A", ID = "AP1A", TAGS = "release-keys", TYPE = "user";
    public static final String FINGERPRINT = "google/shiba/shiba:14/AP1A/1:user/release-keys", HOST = "husk", USER = "husk";
    public static final String SERIAL = "unknown", BOOTLOADER = "unknown", RADIO = "unknown";
    public static final String CPU_ABI = "arm64-v8a", CPU_ABI2 = "";
    public static final String[] SUPPORTED_ABIS = { "arm64-v8a" }, SUPPORTED_64_BIT_ABIS = { "arm64-v8a" }, SUPPORTED_32_BIT_ABIS = {};
    public static final long TIME = 0;
    public static String getSerial() { return SERIAL; }
    public static class VERSION {
        public static final int SDK_INT = 34, PREVIEW_SDK_INT = 0;
        public static final String RELEASE = "14", CODENAME = "REL", INCREMENTAL = "1", SDK = "34", SECURITY_PATCH = "2024-01-01";
        public static final String BASE_OS = "";
    }
    public static class VERSION_CODES {
        public static final int BASE = 1, CUPCAKE = 3, DONUT = 4, ECLAIR = 5, FROYO = 8, GINGERBREAD = 9, HONEYCOMB = 11,
            HONEYCOMB_MR1 = 12, HONEYCOMB_MR2 = 13, ICE_CREAM_SANDWICH = 14, JELLY_BEAN = 16, JELLY_BEAN_MR1 = 17, JELLY_BEAN_MR2 = 18,
            KITKAT = 19, KITKAT_WATCH = 20, LOLLIPOP = 21, LOLLIPOP_MR1 = 22, M = 23, N = 24, N_MR1 = 25, O = 26, O_MR1 = 27, P = 28,
            Q = 29, R = 30, S = 31, S_V2 = 32, TIRAMISU = 33, UPSIDE_DOWN_CAKE = 34, CUR_DEVELOPMENT = 10000;
    }
}
