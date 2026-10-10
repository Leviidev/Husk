package android.text.method;
import android.text.Spannable;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.TextView;
public class BaseMovementMethod implements MovementMethod {
    public boolean canSelectArbitrarily() { return false; }
    public void initialize(TextView w, Spannable t) {}
    public boolean onKeyDown(TextView w, Spannable t, int c, KeyEvent e) { return false; }
    public boolean onKeyUp(TextView w, Spannable t, int c, KeyEvent e) { return false; }
    public boolean onKeyOther(TextView w, Spannable t, KeyEvent e) { return false; }
    public void onTakeFocus(TextView w, Spannable t, int d) {}
    public boolean onTouchEvent(TextView w, Spannable t, MotionEvent e) { return false; }
    public boolean onTrackballEvent(TextView w, Spannable t, MotionEvent e) { return false; }
    public boolean onGenericMotionEvent(TextView w, Spannable t, MotionEvent e) { return false; }
}
