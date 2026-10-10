package android.text.method;
import android.text.Spannable;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.widget.TextView;
public interface MovementMethod {
    void initialize(TextView w, Spannable text);
    boolean onKeyDown(TextView w, Spannable text, int code, KeyEvent e);
    boolean onKeyUp(TextView w, Spannable text, int code, KeyEvent e);
    boolean onKeyOther(TextView w, Spannable text, KeyEvent e);
    void onTakeFocus(TextView w, Spannable text, int dir);
    boolean onTrackballEvent(TextView w, Spannable text, MotionEvent e);
    boolean onTouchEvent(TextView w, Spannable text, MotionEvent e);
    boolean onGenericMotionEvent(TextView w, Spannable text, MotionEvent e);
    boolean canSelectArbitrarily();
}
