// SPDX-License-Identifier: GPL-2.0-or-later
package android.view;

import android.app.Activity;
import android.graphics.Rect;

/**
 * Verifies Android AOSP SurfaceView fixed-size buffer vs full-screen view layout contract:
 * getHolder().setFixedSize(1568, 724) must keep a 1568x724 surface buffer while the view itself
 * measures and lays out across the full 1864x860 parent container (Tru Tien SDL/bgfx sequence).
 */
public final class SurfaceViewFixedSizeTest {
    static void check(boolean cond, String msg) {
        if (!cond) throw new AssertionError(msg);
    }

    static final class Host extends Activity {}

    public static void main(String[] args) {
        Host host = new Host();
        SurfaceView sv = new SurfaceView(host);
        check(!sv.isFixedSize(), "SurfaceView starts non-fixed");
        check(sv.getHolder().getSurface().huskView == sv, "Surface binds back to owning SurfaceView");

        // Simulate SDL/Tru Tien sequence: layout at 1864x860 first, then setFixedSize(1568, 724).
        int atMostW = View.MeasureSpec.makeMeasureSpec(1864, View.MeasureSpec.AT_MOST);
        int atMostH = View.MeasureSpec.makeMeasureSpec(860, View.MeasureSpec.AT_MOST);
        sv.measure(atMostW, atMostH);
        sv.layout(0, 0, sv.getMeasuredWidth(), sv.getMeasuredHeight());
        check(sv.getWidth() == 1864 && sv.getHeight() == 860, "initial full-screen layout is 1864x860");
        check(sv.huskBufW == 1864 && sv.huskBufH == 860, "non-fixed buffer matches 1864x860 layout");
        check(sv.huskViewW == 1864 && sv.huskViewH == 860, "view bounds track 1864x860 layout");

        sv.getHolder().setFixedSize(1568, 724);
        check(sv.isFixedSize(), "isFixedSize() reflects setFixedSize");
        check(sv.huskBufW == 1568 && sv.huskBufH == 724, "live huskBufW/huskBufH switch immediately to 1568x724");
        check(new Rect(0, 0, 1568, 724).equals(sv.getHolder().getSurfaceFrame()), "getSurfaceFrame reports 1568x724");

        // Re-measure with AT_MOST 1864x860 (e.g. RelativeLayout/FrameLayout): view must stay 1864x860!
        sv.measure(atMostW, atMostH);
        sv.layout(0, 0, sv.getMeasuredWidth(), sv.getMeasuredHeight());
        check(sv.getMeasuredWidth() == 1864 && sv.getMeasuredHeight() == 860,
            "AT_MOST measure keeps full 1864x860 parent size instead of shrinking to 1568x724");
        check(sv.huskViewW == 1864 && sv.huskViewH == 860, "huskViewW/huskViewH stay 1864x860 for touch & compositor");
        check(sv.huskBufW == 1568 && sv.huskBufH == 724, "huskBufW/huskBufH remain 1568x724 after re-layout");
        int[] surfSz = sv.huskSurfaceSize();
        check(surfSz[0] == 1568 && surfSz[1] == 724, "huskSurfaceSize returns fixed 1568x724");

        // UNSPECIFIED measure falls back to the fixed buffer size, matching AOSP getDefaultSize().
        int unspec = View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED);
        sv.measure(unspec, unspec);
        check(sv.getMeasuredWidth() == 1568 && sv.getMeasuredHeight() == 724, "UNSPECIFIED measure uses fixed size");

        // Restoring layout size clears fixed buffer override.
        sv.layout(0, 0, 1864, 860);
        sv.getHolder().setSizeFromLayout();
        check(!sv.isFixedSize(), "setSizeFromLayout clears isFixedSize");
        check(sv.huskBufW == 1864 && sv.huskBufH == 860, "setSizeFromLayout restores 1864x860 buffer");

        System.out.println("PASS: SurfaceView fixed buffer size and full-screen view bounds verified");
    }
}

