// SPDX-License-Identifier: GPL-2.0-or-later
package android.view;

import android.graphics.Insets;
import android.graphics.Rect;
import java.util.Arrays;
import java.util.List;

public final class DisplayCutoutTest {
    // Same public API sequence used by the supplied VHTG Cocos2dxHelper.getSafeInsets():
    // empty bounding rectangles mean no cutout, even if safe depths are nonzero.
    private static int[] cocosInsets(WindowInsets window) {
        int[] out = new int[4];
        DisplayCutout c = window.getDisplayCutout();
        if (c != null && !c.getBoundingRects().isEmpty()) {
            out[0] = c.getSafeInsetBottom(); out[1] = c.getSafeInsetLeft();
            out[2] = c.getSafeInsetRight(); out[3] = c.getSafeInsetTop();
        }
        return out;
    }

    public static void main(String[] args) {
        assert DisplayCutout.huskFromInsets(2796, 1290, 0, 0, 0, 0) == null;
        assert DisplayCutout.huskFromInsets(0, 1290, 177, 0, 0, 0) == null;
        assert DisplayCutout.huskFromInsets(2796, 1290, -1, 0, 0, 0) == null;
        int[][] cases = { {177, 0, 0, 0}, {0, 0, 177, 0}, {0, 177, 0, 0}, {0, 0, 0, 177} };
        for (int[] inset : cases) {
            DisplayCutout c = DisplayCutout.huskFromInsets(2796, 1290, inset[0], inset[1], inset[2], inset[3]);
            assert c != null && !c.isEmpty() && !c.isBoundsEmpty();
            assert c.getBoundingRects().size() == 1;
            Rect bound = c.getBoundingRects().get(0);
            assert bound.left >= 0 && bound.top >= 0 && bound.right <= 2796 && bound.bottom <= 1290;
            assert !bound.isEmpty();
            // Full-screen window keeps all pixels: only the cutout metadata guides the game's UI.
            WindowInsets fullScreen = WindowInsets.huskOf(0, 0, 0, 0, 0, c);
            assert fullScreen.getSystemWindowInsetLeft() == 0 && fullScreen.getSystemWindowInsetRight() == 0;
            assert fullScreen.getSystemWindowInsetTop() == 0 && fullScreen.getSystemWindowInsetBottom() == 0;
            assert Arrays.equals(cocosInsets(fullScreen), new int[] { inset[3], inset[0], inset[2], inset[1] });
            // Camera side moves, but no synthetic inset appears on the opposite edge or Home indicator.
            int[] actual = cocosInsets(fullScreen);
            int count = 0; for (int v : actual) if (v != 0) count++;
            assert count == 1;
            bound.setEmpty();
            List<Rect> exported = c.getBoundingRects(); exported.clear();
            assert c.getBoundingRects().size() == 1;
        }
        Rect safe = new Rect(12, 0, 0, 0), left = new Rect(0, 50, 12, 100);
        DisplayCutout legacy = new DisplayCutout(safe, Arrays.asList(left));
        safe.left = 100; left.setEmpty();
        assert legacy.getSafeInsetLeft() == 12 && legacy.getBoundingRectLeft().equals(new Rect(0, 50, 12, 100));
        Rect[] bounds = legacy.getBoundingRectsAll(); bounds[0].setEmpty();
        assert !legacy.isBoundsEmpty();
        Rect safeCopy = legacy.getSafeInsets(); safeCopy.left = 999;
        assert legacy.getSafeInsetLeft() == 12;
        DisplayCutout replaced = legacy.replaceSafeInsets(new Rect(8, 0, 0, 0));
        assert legacy.getSafeInsetLeft() == 12 && replaced.getSafeInsetLeft() == 8;
        DisplayCutout empty = new DisplayCutout(Insets.NONE, null, null, null, null);
        assert empty.isEmpty() && empty.isBoundsEmpty() && empty.getBoundingRects().isEmpty();
        assert empty.getBoundingRectBottom() != null && empty.getBoundingRectRight() != null;
        assert DisplayCutout.huskFromInsets(50, 60, 200, 0, 0, 0).getSafeInsetLeft() == 50;
        System.out.println("PASS: production cutout bounds are immutable and nonempty; Cocos reads HUD insets while full-screen window insets stay zero");
    }
}
