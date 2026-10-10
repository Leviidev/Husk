package android.text.method;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
public interface KeyListener {
    int getInputType();
    boolean onKeyDown(View v, Editable text, int code, KeyEvent e);
    boolean onKeyUp(View v, Editable text, int code, KeyEvent e);
    boolean onKeyOther(View v, Editable text, KeyEvent e);
    void clearMetaKeyState(View v, Editable content, int states);
}
