package android.view;

/** Velocity from the last 100 ms of each pointer's moves, by least squares on a line. */
public final class VelocityTracker {
    private static final int N = 20;
    private final android.util.SparseArray<float[]> mHist = new android.util.SparseArray<>();   /* per pointer: ring of (t, x, y) */
    private final android.util.SparseIntArray mCount = new android.util.SparseIntArray();
    private final android.util.SparseArray<float[]> mVel = new android.util.SparseArray<>();
    private int mActive = -1;
    private VelocityTracker() {}
    public static VelocityTracker obtain() { return new VelocityTracker(); }
    public void recycle() { clear(); }
    public void clear() { mHist.clear(); mCount.clear(); mVel.clear(); }
    public void addMovement(MotionEvent e) {
        int am = e.getActionMasked();
        if (am == MotionEvent.ACTION_DOWN) clear();
        float t = e.getEventTime();
        for (int i = 0; i < e.getPointerCount(); i++) {
            int id = e.getPointerId(i);
            float[] h = mHist.get(id);
            if (h == null) { h = new float[N * 3]; mHist.put(id, h); }
            int c = mCount.get(id);
            int k = (c % N) * 3;
            h[k] = t; h[k + 1] = e.getRawX(i); h[k + 2] = e.getRawY(i);
            mCount.put(id, c + 1);
        }
        mActive = e.getPointerId(0);
    }
    public void computeCurrentVelocity(int units) { computeCurrentVelocity(units, Float.MAX_VALUE); }
    public void computeCurrentVelocity(int units, float max) {
        mVel.clear();
        for (int p = 0; p < mHist.size(); p++) {
            int id = mHist.keyAt(p); float[] h = mHist.valueAt(p); int c = mCount.get(id), n = Math.min(c, N);
            if (n < 2) { mVel.put(id, new float[] { 0, 0 }); continue; }
            float tl = h[((c - 1) % N) * 3];
            double st = 0, sx = 0, sy = 0, stt = 0, stx = 0, sty = 0; int m = 0;
            for (int j = 0; j < n; j++) {
                int k = ((c - 1 - j) % N) * 3;
                float dt = h[k] - tl;
                if (dt < -100) break;
                st += dt; sx += h[k + 1]; sy += h[k + 2]; stt += dt * dt; stx += dt * h[k + 1]; sty += dt * h[k + 2]; m++;
            }
            double den = m * stt - st * st;
            float vx = 0, vy = 0;
            if (m >= 2 && den != 0) { vx = (float) ((m * stx - st * sx) / den * units); vy = (float) ((m * sty - st * sy) / den * units); }
            vx = Math.max(-max, Math.min(max, vx)); vy = Math.max(-max, Math.min(max, vy));
            mVel.put(id, new float[] { vx, vy });
        }
    }
    public float getXVelocity() { return getXVelocity(mActive); }
    public float getYVelocity() { return getYVelocity(mActive); }
    public float getXVelocity(int id) { float[] v = mVel.get(id); return v == null ? 0 : v[0]; }
    public float getYVelocity(int id) { float[] v = mVel.get(id); return v == null ? 0 : v[1]; }
    public float getAxisVelocity(int axis) { return axis == MotionEvent.AXIS_X ? getXVelocity() : axis == MotionEvent.AXIS_Y ? getYVelocity() : 0; }
    public float getAxisVelocity(int axis, int id) { return axis == MotionEvent.AXIS_X ? getXVelocity(id) : axis == MotionEvent.AXIS_Y ? getYVelocity(id) : 0; }
    public boolean isAxisSupported(int axis) { return axis == MotionEvent.AXIS_X || axis == MotionEvent.AXIS_Y; }
}
