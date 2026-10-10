package android.view;

import android.util.DisplayMetrics;

public class Display {
    public static final int DEFAULT_DISPLAY = 0, INVALID_DISPLAY = -1, STATE_UNKNOWN = 0, STATE_OFF = 1, STATE_ON = 2, FLAG_SECURE = 2, FLAG_ROUND = 16;
    public void getMetrics(DisplayMetrics m) { m.setToDefaults(); }
    public void getRealMetrics(DisplayMetrics m) { m.setToDefaults(); }
    public void getSize(android.graphics.Point p) { p.x = husk.Native.screenWidth(); p.y = husk.Native.screenHeight(); }
    public void getRealSize(android.graphics.Point p) { getSize(p); }
    public void getRectSize(android.graphics.Rect r) { r.set(0, 0, husk.Native.screenWidth(), husk.Native.screenHeight()); }
    public void getCurrentSizeRange(android.graphics.Point small, android.graphics.Point large) { int w = husk.Native.screenWidth(), h = husk.Native.screenHeight(); small.set(Math.min(w, h), Math.min(w, h)); large.set(Math.max(w, h), Math.max(w, h)); }
    public int getWidth() { return husk.Native.screenWidth(); }
    public int getHeight() { return husk.Native.screenHeight(); }
    public int getRotation() { return 0; }
    @Deprecated public int getOrientation() { return 0; }
    public float getRefreshRate() { return 60f; }
    public float[] getSupportedRefreshRates() { return new float[] { 60f }; }
    public int getDisplayId() { return 0; }
    public String getName() { return "Built-in Screen"; }
    public int getState() { return STATE_ON; }
    public int getFlags() { return 0; }
    public boolean isValid() { return true; }
    public boolean isHdr() { return false; }
    public boolean isWideColorGamut() { return false; }
    public int getPixelFormat() { return android.graphics.PixelFormat.RGBA_8888; }
    public long getAppVsyncOffsetNanos() { return 0; }
    public long getPresentationDeadlineNanos() { return 16_666_667L; }
    public DisplayCutout getCutout() { return husk.ViewRoot.cutout(); }
    public HdrCapabilities getHdrCapabilities() { return null; }
    public static final class HdrCapabilities { public int[] getSupportedHdrTypes() { return new int[0]; } }
    public Mode getMode() { return new Mode(); }
    public Mode[] getSupportedModes() { return new Mode[] { new Mode() }; }
    public static final class Mode {
        public int getModeId() { return 1; } public int getPhysicalWidth() { return husk.Native.screenWidth(); }
        public int getPhysicalHeight() { return husk.Native.screenHeight(); } public float getRefreshRate() { return 60f; }
        public float[] getAlternativeRefreshRates() { return new float[0]; }
    }
}
