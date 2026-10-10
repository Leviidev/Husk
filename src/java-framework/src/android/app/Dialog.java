package android.app;
public class Dialog implements android.content.DialogInterface {
    public Dialog(android.content.Context c) {}
    public void show() { android.util.Log.i("Husk", "a dialog was shown (dialogs are not drawn yet)"); }
    public void dismiss() {} public void cancel() {} public void hide() {}
    public boolean isShowing() { return false; }
    public void setCancelable(boolean b) {} public void setCanceledOnTouchOutside(boolean b) {} public void setTitle(CharSequence t) {}
    public void setOnCancelListener(OnCancelListener l) {} public void setOnDismissListener(OnDismissListener l) {}
    public android.view.Window getWindow() { return new android.view.Window(null); }
}
