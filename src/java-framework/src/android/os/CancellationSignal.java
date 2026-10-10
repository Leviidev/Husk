package android.os;
public final class CancellationSignal {
    public interface OnCancelListener { void onCancel(); }
    private boolean mCanceled; private OnCancelListener mListener;
    public boolean isCanceled() { synchronized (this) { return mCanceled; } }
    public void throwIfCanceled() { if (isCanceled()) throw new OperationCanceledException(); }
    public void cancel() { OnCancelListener l; synchronized (this) { if (mCanceled) return; mCanceled = true; l = mListener; } if (l != null) l.onCancel(); }
    public void setOnCancelListener(OnCancelListener l) { synchronized (this) { mListener = l; if (!mCanceled || l == null) return; } l.onCancel(); }
}
