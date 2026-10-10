package android.view;

import android.util.DisplayMetrics;

public class Display {
    public static final int DEFAULT_DISPLAY = 0;
    public void getMetrics(DisplayMetrics m) { m.setToDefaults(); }
    public void getRealMetrics(DisplayMetrics m) { m.setToDefaults(); }
    public void getSize(android.graphics.Point p) { p.x = husk.Native.screenWidth(); p.y = husk.Native.screenHeight(); }
    public void getRealSize(android.graphics.Point p) { getSize(p); }
    public void getRectSize(android.graphics.Rect r) { r.set(0, 0, husk.Native.screenWidth(), husk.Native.screenHeight()); }
    public int getWidth() { return husk.Native.screenWidth(); }
    public int getHeight() { return husk.Native.screenHeight(); }
    public int getRotation() { return 0; }
    public int getOrientation() { return 0; }
    public float getRefreshRate() { return 60f; }
    public int getDisplayId() { return 0; }
    public String getName() { return "Built-in Screen"; }
    public int getState() { return 2; }
    public DisplayCutout getCutout() { return null; }
    public Mode getMode() { return new Mode(); }
    public Mode[] getSupportedModes() { return new Mode[] { new Mode() }; }
    public static final class Mode {
        public int getModeId() { return 1; } public int getPhysicalWidth() { return husk.Native.screenWidth(); }
        public int getPhysicalHeight() { return husk.Native.screenHeight(); } public float getRefreshRate() { return 60f; }
    }
}
