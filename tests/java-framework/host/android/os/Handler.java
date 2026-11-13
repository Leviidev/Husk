package android.os;
/** Deterministic host queue; production Fragment/FragmentManager code is tested unchanged. */
public class Handler {
    private static final java.util.ArrayList<Runnable> queue = new java.util.ArrayList<>();
    public Handler() {}
    public Handler(Looper looper) {}
    public boolean post(Runnable r) { queue.add(r); return true; }
    public boolean postDelayed(Runnable r, long delayMillis) { queue.add(r); return true; }
    public void removeCallbacks(Runnable r) { queue.remove(r); }
    public static int pendingCount() { return queue.size(); }
    public static void drain() { while (!queue.isEmpty()) queue.remove(0).run(); }
}
