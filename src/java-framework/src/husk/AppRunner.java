package husk;

import android.app.Activity;
import android.os.Looper;
import android.os.SystemClock;
import android.view.MotionEvent;

/** Runs an app as ActivityThread would: the launcher activity created, its lifecycle, then the main looper with touches delivered. */
public final class AppRunner {
    private static Activity sActivity;
    private static final int[] ids = new int[16];
    private static final float[] xs = new float[16], ys = new float[16];
    private static int down;
    private static long downTime;

    public static void run(String activityClass) throws Exception {
        Looper.prepareMainLooper();
        Class<?> c = Class.forName(activityClass);
        sActivity = (Activity) c.newInstance();
        Looper.setInputSink(AppRunner::deliver);
        sActivity.huskCreate();
        sActivity.huskStart();
        Looper.loop();
    }

    /** events: [n, then n x (phase, id, xbits, ybits)] -- phase 0 down, 1 move, 2 up, 3 cancel; 4 is a key (id = code, x = down/up), 5 is back */
    private static void deliver(int[] ev) {
        int n = ev[0];
        for (int k = 0; k < n; k++) {
            int phase = ev[1 + 4 * k], id = ev[2 + 4 * k];
            float x = Float.intBitsToFloat(ev[3 + 4 * k]), y = Float.intBitsToFloat(ev[4 + 4 * k]);
            if (phase == 5) { sActivity.huskBack(); continue; }
            if (phase == 4) {
                android.view.KeyEvent ke = new android.view.KeyEvent(SystemClock.uptimeMillis(), SystemClock.uptimeMillis(), ev[3 + 4 * k] != 0 ? 0 : 1, id, 0);
                sActivity.dispatchKeyEvent(ke);
                continue;
            }
            long now = SystemClock.uptimeMillis();
            int idx = -1;
            for (int i = 0; i < down; i++) if (ids[i] == id) idx = i;
            int action;
            if (phase == 0) {
                if (idx >= 0 || down >= 16) continue;
                idx = down++; ids[idx] = id; xs[idx] = x; ys[idx] = y;
                if (down == 1) downTime = now;
                action = down == 1 ? MotionEvent.ACTION_DOWN : MotionEvent.ACTION_POINTER_DOWN | (idx << 8);
            } else if (phase == 1) {
                if (idx < 0) continue;
                xs[idx] = x; ys[idx] = y;
                action = MotionEvent.ACTION_MOVE;
            } else {
                if (idx < 0) continue;
                xs[idx] = x; ys[idx] = y;
                action = phase == 3 ? MotionEvent.ACTION_CANCEL : down == 1 ? MotionEvent.ACTION_UP : MotionEvent.ACTION_POINTER_UP | (idx << 8);
            }
            MotionEvent e = MotionEvent.huskObtain(downTime, now, action, down, java.util.Arrays.copyOf(ids, down), java.util.Arrays.copyOf(xs, down), java.util.Arrays.copyOf(ys, down));
            sActivity.dispatchTouchEvent(e);
            if (phase >= 2) {
                for (int i = idx; i + 1 < down; i++) { ids[i] = ids[i + 1]; xs[i] = xs[i + 1]; ys[i] = ys[i + 1]; }
                down--;
                if (phase == 3) down = 0;
            }
        }
    }
}
