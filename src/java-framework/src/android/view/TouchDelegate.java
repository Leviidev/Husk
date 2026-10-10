package android.view;
import android.graphics.Rect;
public class TouchDelegate {
    private final Rect mBounds; private final View mDelegate; private boolean mTargeted;
    public TouchDelegate(Rect bounds, View delegate) { mBounds = bounds; mDelegate = delegate; }
    public boolean onTouchEvent(MotionEvent e) {
        int x = (int) e.getX(), y = (int) e.getY();
        switch (e.getActionMasked()) {
        case MotionEvent.ACTION_DOWN: mTargeted = mBounds.contains(x, y); break;
        case MotionEvent.ACTION_CANCEL: case MotionEvent.ACTION_UP: case MotionEvent.ACTION_MOVE: break;
        }
        if (!mTargeted) return false;
        boolean last = e.getActionMasked() == MotionEvent.ACTION_UP || e.getActionMasked() == MotionEvent.ACTION_CANCEL;
        MotionEvent c = MotionEvent.obtain(e);
        c.setLocation(mDelegate.getWidth() / 2f, mDelegate.getHeight() / 2f);
        boolean h = mDelegate.dispatchTouchEvent(c);
        if (last) mTargeted = false;
        return h;
    }
}
