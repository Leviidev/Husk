// SPDX-License-Identifier: GPL-2.0-or-later
package husk;

import android.app.Activity;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;

/** Consecutive MOVE events coalesce into one multi-pointer frame without altering DOWN/UP ordering. */
public final class InputDeliveryTest {
    private static final class Host extends Activity {}

    private static final class Record {
        final int actionMasked;
        final int actionIndex;
        final int count;
        final int[] ids;
        final float[] xs;
        final float[] ys;

        Record(MotionEvent e) {
            actionMasked = e.getActionMasked();
            actionIndex = e.getActionIndex();
            count = e.getPointerCount();
            ids = new int[count];
            xs = new float[count];
            ys = new float[count];
            for (int i = 0; i < count; i++) {
                ids[i] = e.getPointerId(i);
                xs[i] = e.getX(i);
                ys[i] = e.getY(i);
            }
        }
    }

    private static void check(boolean cond, String msg) {
        if (!cond) throw new AssertionError(msg);
    }

    private static int bits(float f) { return Float.floatToRawIntBits(f); }

    private static int[] pack(int... vals) { return vals; }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        AppRunner.resetSession();
        ViewRoot.resetSession();
        List<Record> seen = new ArrayList<>();
        View surface = new View(new Host()) {
            @Override
            public boolean onTouchEvent(MotionEvent e) {
                seen.add(new Record(e));
                return true;
            }
        };
        surface.layout(0, 0, 1864, 860);
        WindowManager.LayoutParams lp = new WindowManager.LayoutParams();
        lp.width = 1864;
        lp.height = 860;
        Constructor<ViewRoot> ctor = ViewRoot.class.getDeclaredConstructor(View.class, WindowManager.LayoutParams.class);
        ctor.setAccessible(true);
        ViewRoot root = ctor.newInstance(surface, lp);
        Field wField = ViewRoot.class.getDeclaredField("mW");
        Field hField = ViewRoot.class.getDeclaredField("mH");
        Field rootsField = ViewRoot.class.getDeclaredField("sRoots");
        wField.setAccessible(true);
        hField.setAccessible(true);
        rootsField.setAccessible(true);
        wField.setInt(root, 1864);
        hField.setInt(root, 860);
        ((List<ViewRoot>) rootsField.get(null)).add(root);

        AppRunner.deliver(pack(
            4,
            0, 0, bits(100f), bits(200f),
            1, 0, bits(110f), bits(210f),
            1, 0, bits(125f), bits(220f),
            1, 0, bits(140.5f), bits(230.25f)
        ));
        check(seen.size() == 2, "expected DOWN + single coalesced MOVE, got " + seen.size());
        check(seen.get(0).actionMasked == MotionEvent.ACTION_DOWN, "first event is DOWN");
        check(seen.get(1).actionMasked == MotionEvent.ACTION_MOVE, "second event is MOVE");
        check(seen.get(1).xs[0] == 140.5f && seen.get(1).ys[0] == 230.25f, "MOVE keeps final position");

        seen.clear();
        AppRunner.deliver(pack(
            6,
            0, 1, bits(1200f), bits(400f),
            1, 0, bits(145f), bits(235f),
            1, 1, bits(1215f), bits(405f),
            1, 1, bits(1242.75f), bits(418.5f),
            2, 0, bits(145f), bits(235f),
            1, 1, bits(1255f), bits(422f)
        ));
        check(seen.size() == 4, "expected POINTER_DOWN, coalesced MOVE(0+1), POINTER_UP(0), MOVE(1), got " + seen.size());
        check(seen.get(0).actionMasked == MotionEvent.ACTION_POINTER_DOWN, "pointer 1 down");
        Record multiMove = seen.get(1);
        check(multiMove.actionMasked == MotionEvent.ACTION_MOVE && multiMove.count == 2, "coalesced 2-pointer MOVE");
        check(multiMove.ids[0] == 0 && multiMove.xs[0] == 145f && multiMove.ys[0] == 235f, "pointer 0 latest in multiMove");
        check(multiMove.ids[1] == 1 && multiMove.xs[1] == 1242.75f && multiMove.ys[1] == 418.5f, "pointer 1 latest in multiMove");
        check(seen.get(2).actionMasked == MotionEvent.ACTION_POINTER_UP && seen.get(2).ids[seen.get(2).actionIndex] == 0, "pointer 0 lifted in order");
        Record postUpMove = seen.get(3);
        check(postUpMove.actionMasked == MotionEvent.ACTION_MOVE && postUpMove.count == 1, "post-UP MOVE only has pointer 1");
        check(postUpMove.ids[0] == 1 && postUpMove.xs[0] == 1255f && postUpMove.ys[0] == 422f, "pointer 1 continues smoothly after pointer 0 lifts");

        ViewRoot.resetSession();
        AppRunner.resetSession();
        System.out.println("PASS: AppRunner coalesces burst moves across active pointers while preserving multi-touch order");
    }
}
