package android.widget;

import android.os.Handler;
import android.os.Looper;

/** Filters on a worker thread and publishes on the main one, as the platform's does. */
public abstract class Filter {
    public interface FilterListener { void onFilterComplete(int count); }
    public interface Delayer { long getPostingDelay(CharSequence constraint); }
    protected static class FilterResults { public Object values; public int count; public FilterResults() {} }
    private final Handler mMain = new Handler(Looper.getMainLooper());
    private final Object mLock = new Object();
    private int mGeneration;
    public Filter() {}
    public void setDelayer(Delayer d) {}
    public final void filter(CharSequence c) { filter(c, null); }
    public final void filter(CharSequence constraint, FilterListener listener) {
        final int gen; synchronized (mLock) { gen = ++mGeneration; }
        final CharSequence cs = constraint != null ? constraint.toString() : null;
        Thread t = new Thread(() -> {
            FilterResults r;
            try { r = performFiltering(cs); } catch (Exception e) { android.util.Log.w("Filter", "An exception occured during performFiltering()!", e); r = new FilterResults(); }
            final FilterResults res = r;
            mMain.post(() -> { synchronized (mLock) { if (gen != mGeneration) return; } publishResults(cs, res); if (listener != null) listener.onFilterComplete(res != null ? res.count : -1); });
        }, "Filter");
        t.setDaemon(true);
        t.start();
    }
    protected abstract FilterResults performFiltering(CharSequence constraint);
    protected abstract void publishResults(CharSequence constraint, FilterResults results);
    public CharSequence convertResultToString(Object r) { return r == null ? "" : r.toString(); }
}
