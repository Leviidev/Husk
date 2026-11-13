// SPDX-License-Identifier: GPL-2.0-or-later
package android.view;

/** The standalone JVM has no host-native bridge; all display accessors must
 * agree on a bounded fallback rather than mixing a real mode with a fake Hz. */
public final class DisplayFactsTest {
    static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }
    public static void main(String[] args) {
        Display display = new Display();
        check(display.getRefreshRate() == 60f, "missing native bridge uses 60 Hz fallback");
        float[] supported = display.getSupportedRefreshRates();
        check(supported.length == 1 && supported[0] == display.getRefreshRate(), "supported refresh agrees with display");
        check(display.getMode().getRefreshRate() == display.getRefreshRate(), "mode refresh agrees with display");
        check(display.getMode().getVsyncRate() == display.getRefreshRate(), "vsync rate agrees with display");
        check(display.getPresentationDeadlineNanos() == 16_666_667L, "deadline derives from reported refresh");
        check(!display.isWideColorGamut() && display.getPreferredWideGamutColorSpace() == null, "JVM fallback without native display stays conservative");
        check(display.getColorMode() == Display.COLOR_MODE_SRGB, "JVM fallback color mode stays sRGB");
        android.graphics.ColorSpace p3 = android.graphics.ColorSpace.get(android.graphics.ColorSpace.Named.DISPLAY_P3);
        android.graphics.ColorSpace srgb = android.graphics.ColorSpace.get(android.graphics.ColorSpace.Named.SRGB);
        check(p3.isWideGamut() && !p3.isSrgb() && "Display P3".equals(p3.getName()), "DISPLAY_P3 ColorSpace reports wide gamut");
        check(!srgb.isWideGamut() && srgb.isSrgb(), "SRGB ColorSpace reports standard sRGB");
        System.out.println("PASS: display, mode, vsync, color space and presentation deadline share the same host bridge and bounded JVM fallback");
    }
}
