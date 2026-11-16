// SPDX-License-Identifier: GPL-2.0-or-later
package android.view;

import java.lang.management.ManagementFactory;

/** Split touches keep pointer identity, action routing, coordinates and event metadata. */
public final class MotionEventSplitTest {
    private static volatile MotionEvent allocationSink;

    private static void check(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    private static void equal(float actual, float expected, String message) {
        check(Float.floatToIntBits(actual) == Float.floatToIntBits(expected), message);
    }

    private static MotionEvent event(int action) {
        MotionEvent.PointerProperties[] properties = new MotionEvent.PointerProperties[3];
        MotionEvent.PointerCoords[] coordinates = new MotionEvent.PointerCoords[3];
        int[] ids = { 3, 7, 12 };
        for (int i = 0; i < 3; i++) {
            properties[i] = new MotionEvent.PointerProperties();
            properties[i].id = ids[i];
            properties[i].toolType = MotionEvent.TOOL_TYPE_FINGER;
            coordinates[i] = new MotionEvent.PointerCoords();
            coordinates[i].x = 30 + 40 * i;
            coordinates[i].y = 60 + 20 * i;
        }
        return MotionEvent.obtain(100L, 125L, action, 3, properties, coordinates,
                17, MotionEvent.BUTTON_PRIMARY, 1f, 1f, 1, 0,
                InputDevice.SOURCE_MOUSE, MotionEvent.FLAG_WINDOW_IS_OBSCURED);
    }

    private static void testPointerActions() {
        MotionEvent down = event(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
        MotionEvent selected = down.split((1 << 7) | (1 << 12));
        check(selected.getPointerCount() == 2, "two selected pointers");
        check(selected.getPointerId(0) == 7 && selected.getPointerId(1) == 12,
                "selected IDs retain their original ordering");
        check(selected.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN && selected.getActionIndex() == 0,
                "pointer DOWN action index is remapped to its selected position");
        check(down.split(1 << 7).getAction() == MotionEvent.ACTION_DOWN,
                "the sole selected pointer goes DOWN");
        check(down.split((1 << 3) | (1 << 12)).getAction() == MotionEvent.ACTION_MOVE,
                "a child excluding the newly down pointer receives MOVE");

        MotionEvent up = event(MotionEvent.ACTION_POINTER_UP | (2 << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
        selected = up.split((1 << 3) | (1 << 12));
        check(selected.getActionMasked() == MotionEvent.ACTION_POINTER_UP && selected.getActionIndex() == 1,
                "pointer UP action index is remapped");
        check(up.split(1 << 12).getAction() == MotionEvent.ACTION_UP,
                "the sole selected pointer goes UP");
        check(up.split((1 << 3) | (1 << 7)).getAction() == MotionEvent.ACTION_MOVE,
                "a child excluding the lifted pointer receives MOVE");

        MotionEvent cancel = event(MotionEvent.ACTION_CANCEL).split(1 << 7);
        check(cancel.getAction() == MotionEvent.ACTION_CANCEL, "CANCEL survives splitting");
        check(cancel.getPointerCount() == 1 && cancel.getPointerId(0) == 7,
                "CANCEL retains the selected pointer");
    }

    private static void testCoordinatesAndMetadata() {
        MotionEvent original = event(MotionEvent.ACTION_MOVE);
        original.offsetLocation(-10f, 25f);
        MotionEvent copy = original.split((1 << 3) | (1 << 12));
        equal(copy.getX(0), 20f, "first local X includes the receiving view offset");
        equal(copy.getY(0), 85f, "first local Y includes the receiving view offset");
        equal(copy.getX(1), 100f, "last selected local X");
        equal(copy.getY(1), 125f, "last selected local Y");
        equal(copy.getRawX(0), 30f, "first screen X is independent of local coordinates");
        equal(copy.getRawY(0), 60f, "first screen Y is independent of local coordinates");
        equal(copy.getRawX(1), 110f, "last selected screen X");
        equal(copy.getRawY(1), 100f, "last selected screen Y");
        check(copy.getDownTime() == 100L && copy.getEventTime() == 125L, "event times are retained");
        check(copy.getSource() == InputDevice.SOURCE_MOUSE, "source is retained");
        check(copy.getMetaState() == 17, "meta state is retained");
        check(copy.getButtonState() == MotionEvent.BUTTON_PRIMARY, "buttons are retained");
        check(copy.getFlags() == MotionEvent.FLAG_WINDOW_IS_OBSCURED, "event flags are retained");
        check(copy.findPointerIndex(12) == 1 && copy.findPointerIndex(7) == -1,
                "pointer lookup uses the selected identities");

        original.offsetLocation(1000f, 2000f);
        equal(copy.getX(0), 20f, "changing original coordinates does not change a split event");
        copy.offsetLocation(3f, -6f);
        equal(original.getX(0), 1020f, "changing split coordinates does not change its original");
        equal(copy.getRawX(0), 30f, "split-local movement never changes screen coordinates");
        copy.setAction(MotionEvent.ACTION_CANCEL);
        check(original.getAction() == MotionEvent.ACTION_MOVE, "split action is independent");

        int[] ids = { 1, 8 };
        float[] x = { 12f, 24f }, y = { 36f, 48f };
        MotionEvent supplied = MotionEvent.huskObtain(10L, 20L, MotionEvent.ACTION_MOVE, 2, ids, x, y);
        MotionEvent independent = supplied.split(1 << 8);
        ids[1] = 2; x[1] = 99f; y[1] = 101f;
        check(independent.getPointerId(0) == 8, "split owns independent pointer IDs");
        equal(independent.getX(), 24f, "split owns independent X values");
        equal(independent.getY(), 48f, "split owns independent Y values");
    }

    private static void testRepeatedMultitouch() {
        MotionEvent original = event(MotionEvent.ACTION_POINTER_DOWN | (1 << MotionEvent.ACTION_POINTER_INDEX_SHIFT));
        for (int frame = 0; frame < 10000; frame++) {
            MotionEvent left = original.split(1 << 3);
            MotionEvent right = original.split((1 << 7) | (1 << 12));
            check(left.getAction() == MotionEvent.ACTION_MOVE && left.getPointerId(0) == 3,
                    "excluded pointer remains MOVE across repeated delivery");
            check(right.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN && right.getActionIndex() == 0,
                    "selected pointer action stays correct across repeated delivery");
            left.offsetLocation(-2f, 4f);
            equal(right.getX(0), 70f, "two child deliveries do not share coordinate arrays");
            equal(original.getX(0), 30f, "repeated child delivery leaves the source unchanged");
        }
    }

    /** Optional host-only allocation experiment; run with -Xint so dead-store elimination cannot hide work. */
    private static void measureHostAllocation() {
        java.lang.management.ThreadMXBean bean = ManagementFactory.getThreadMXBean();
        if (!(bean instanceof com.sun.management.ThreadMXBean)) {
            System.out.println("SKIP: host per-thread allocation counter unavailable");
            return;
        }
        com.sun.management.ThreadMXBean allocations = (com.sun.management.ThreadMXBean) bean;
        if (!allocations.isThreadAllocatedMemorySupported()) {
            System.out.println("SKIP: host per-thread allocation counter unavailable");
            return;
        }
        allocations.setThreadAllocatedMemoryEnabled(true);
        MotionEvent original = event(MotionEvent.ACTION_MOVE);
        int selection = (1 << 3) | (1 << 12);
        for (int i = 0; i < 1000; i++) allocationSink = original.split(selection);
        long id = Thread.currentThread().getId();
        long before = allocations.getThreadAllocatedBytes(id);
        for (int i = 0; i < 10000; i++) allocationSink = original.split(selection);
        long bytes = allocations.getThreadAllocatedBytes(id) - before;
        System.out.println("HOST ALLOCATION: " + bytes + " bytes / 10000 split calls (host JVM; not iPhone memory/FPS)");
    }

    public static void main(String[] args) {
        testPointerActions();
        testCoordinatesAndMetadata();
        testRepeatedMultitouch();
        System.out.println("PASS: split touch actions, pointer IDs, metadata and independent local/screen coordinates");
        if (args.length > 0 && "--measure-allocation".equals(args[0])) measureHostAllocation();
    }
}
