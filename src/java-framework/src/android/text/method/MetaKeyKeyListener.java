package android.text.method;
import android.text.Editable;
import android.view.KeyEvent;
import android.view.View;
public abstract class MetaKeyKeyListener {
    public static final int META_SHIFT_ON = 1, META_ALT_ON = 2, META_SYM_ON = 4, META_CAP_LOCKED = 256, META_ALT_LOCKED = 512, META_SYM_LOCKED = 1024;
    public boolean onKeyDown(View v, Editable c, int code, KeyEvent e) { return false; }
    public boolean onKeyUp(View v, Editable c, int code, KeyEvent e) { return false; }
    public void clearMetaKeyState(View v, Editable c, int s) {}
    public static void resetMetaState(android.text.Spannable t) {}
    public static int getMetaState(CharSequence t) { return 0; }
    public static int getMetaState(CharSequence t, int m) { return 0; }
    public static long resetLockedMeta(long s) { return s; }
}
