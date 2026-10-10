package android.text.method;
import android.text.Selection;
import android.text.Spannable;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.TextView;
/** Editable text: a tap puts the cursor where it lands, arrow keys move it. */
public class ArrowKeyMovementMethod extends BaseMovementMethod {
    private static ArrowKeyMovementMethod sInstance;
    public static MovementMethod getInstance() { if (sInstance == null) sInstance = new ArrowKeyMovementMethod(); return sInstance; }
    @Override public boolean canSelectArbitrarily() { return true; }
    @Override public void initialize(TextView w, Spannable t) { Selection.setSelection(t, 0); }
    @Override public void onTakeFocus(TextView w, Spannable t, int d) { if (Selection.getSelectionStart(t) < 0) Selection.setSelection(t, t.length()); }
    @Override public boolean onKeyDown(TextView w, Spannable t, int c, KeyEvent e) {
        if (c == KeyEvent.KEYCODE_DPAD_LEFT) return Selection.moveLeft(t, w.getLayout());
        if (c == KeyEvent.KEYCODE_DPAD_RIGHT) return Selection.moveRight(t, w.getLayout());
        if (c == KeyEvent.KEYCODE_MOVE_HOME) return Selection.moveToLeftEdge(t, w.getLayout());
        if (c == KeyEvent.KEYCODE_MOVE_END) return Selection.moveToRightEdge(t, w.getLayout());
        return false;
    }
    @Override public boolean onTouchEvent(TextView w, Spannable t, MotionEvent e) {
        if (e.getActionMasked() == MotionEvent.ACTION_UP) {
            int off = w.getOffsetForPosition(e.getX(), e.getY());
            if (off >= 0) Selection.setSelection(t, off);
        }
        return false;
    }
}
