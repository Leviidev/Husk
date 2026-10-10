package android.util;

public class DisplayMetrics {
    public static final int DENSITY_DEFAULT = 160, DENSITY_LOW = 120, DENSITY_MEDIUM = 160, DENSITY_TV = 213, DENSITY_HIGH = 240, DENSITY_XHIGH = 320,
        DENSITY_XXHIGH = 480, DENSITY_XXXHIGH = 640, DENSITY_260 = 260, DENSITY_280 = 280, DENSITY_300 = 300, DENSITY_340 = 340, DENSITY_360 = 360,
        DENSITY_400 = 400, DENSITY_420 = 420, DENSITY_440 = 440, DENSITY_450 = 450, DENSITY_560 = 560, DENSITY_600 = 600;
    public static final int DENSITY_DEVICE_STABLE = (int) (husk.Native.density() * 160);
    public static final float DENSITY_DEFAULT_SCALE = 1.0f / DENSITY_DEFAULT;
    public int widthPixels, heightPixels, densityDpi;
    public float density, scaledDensity, xdpi, ydpi;
    public DisplayMetrics() {}
    public void setToDefaults() {
        widthPixels = husk.Native.screenWidth();
        heightPixels = husk.Native.screenHeight();
        density = husk.Native.density();
        scaledDensity = density;
        densityDpi = (int) (density * 160);
        xdpi = ydpi = densityDpi;
    }
    public boolean equals(DisplayMetrics o) { return o != null && widthPixels == o.widthPixels && heightPixels == o.heightPixels && densityDpi == o.densityDpi && density == o.density; }
    @Override public String toString() { return "DisplayMetrics{density=" + density + ", width=" + widthPixels + ", height=" + heightPixels + ", scaledDensity=" + scaledDensity + "}"; }
    public void setTo(DisplayMetrics o) {
        widthPixels = o.widthPixels; heightPixels = o.heightPixels; densityDpi = o.densityDpi;
        density = o.density; scaledDensity = o.scaledDensity; xdpi = o.xdpi; ydpi = o.ydpi;
    }
}
