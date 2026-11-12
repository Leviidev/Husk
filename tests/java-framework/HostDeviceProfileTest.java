// SPDX-License-Identifier: GPL-2.0-or-later
package android.os;

public final class HostDeviceProfileTest {
    private static void check(boolean value, String message) { if (!value) throw new AssertionError(message); }
    public static void main(String[] args) {
        // No native host bridge on the JVM: truthful neutral fallbacks must
        // replace the old Pixel/Tensor profile without class-init failure.
        check(Build.MODEL.equals("Apple device"), "neutral fallback when native bridge is unavailable");
        check(Build.MANUFACTURER.equals("Apple") && Build.SOC_MANUFACTURER.equals("Apple"), "Apple manufacturer");
        check(Build.SOC_MODEL.equals("Apple ARM64"), "unknown SoC does not invent an A-series tier");
        check(Build.VERSION.SDK_INT == 34 && Build.VERSION.RELEASE.equals("14"), "Android API compatibility remains separate from host identity");
        check(Build.CPU_ABI.equals("arm64-v8a"), "guest ABI remains Android ABI");
        String[][] identities = {
            {"ro.product.model", Build.MODEL}, {"ro.product.name", Build.PRODUCT}, {"ro.product.device", Build.DEVICE},
            {"ro.product.manufacturer", Build.MANUFACTURER}, {"ro.product.board", Build.BOARD},
            {"ro.soc.manufacturer", Build.SOC_MANUFACTURER}, {"ro.soc.model", Build.SOC_MODEL},
            {"ro.hardware", Build.HARDWARE}, {"ro.board.platform", Build.BOARD},
            {"ro.build.id", Build.ID}, {"ro.build.display.id", Build.DISPLAY},
            {"ro.build.fingerprint", Build.FINGERPRINT}, {"ro.vendor.build.fingerprint", Build.FINGERPRINT},
            {"ro.boot.hardware.sku", Build.SKU}, {"ro.boot.product.hardware.sku", Build.ODM_SKU}
        };
        for (String[] identity : identities)
            check(SystemProperties.get(identity[0]).equals(identity[1]), "Java Build / SystemProperties agree: " + identity[0]);
        check(!Build.FINGERPRINT.contains("google") && !Build.HARDWARE.contains("shiba"), "no inherited Pixel identity");
        check(SystemProperties.get("ro.sf.lcd_density", "unavailable").equals("unavailable"), "no fabricated DPI when JVM has no native screen");
        System.out.println("PASS: Java Build host fallback is truthful, property identity is coherent and Android SDK/ABI remains compatible");
    }
}
