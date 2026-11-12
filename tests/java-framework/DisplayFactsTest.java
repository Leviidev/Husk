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
        System.out.println("PASS: display, mode, vsync and presentation deadline share the same host refresh bridge and bounded JVM fallback");
    }
}
