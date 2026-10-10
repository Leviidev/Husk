package android.util;

public class DisplayMetrics {
    public static final int DENSITY_DEFAULT = 160;
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
    public void setTo(DisplayMetrics o) {
        widthPixels = o.widthPixels; heightPixels = o.heightPixels; densityDpi = o.densityDpi;
        density = o.density; scaledDensity = o.scaledDensity; xdpi = o.xdpi; ydpi = o.ydpi;
    }
}
