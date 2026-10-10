package android.text.method;
public class ScrollingMovementMethod extends BaseMovementMethod {
    private static ScrollingMovementMethod sInstance;
    public static MovementMethod getInstance() { if (sInstance == null) sInstance = new ScrollingMovementMethod(); return sInstance; }
    private float mLastY;
    @Override public boolean onTouchEvent(android.widget.TextView w, android.text.Spannable t, android.view.MotionEvent e) {
        if (e.getActionMasked() == android.view.MotionEvent.ACTION_DOWN) { mLastY = e.getY(); return true; }
        if (e.getActionMasked() == android.view.MotionEvent.ACTION_MOVE) {
            int dy = (int) (mLastY - e.getY()); mLastY = e.getY();
            int max = Math.max(0, w.getLayout() == null ? 0 : w.getLayout().getHeight() - (w.getHeight() - w.getTotalPaddingTop() - w.getTotalPaddingBottom()));
            w.scrollTo(w.getScrollX(), Math.max(0, Math.min(max, w.getScrollY() + dy)));
            return true;
        }
        return false;
    }
}
