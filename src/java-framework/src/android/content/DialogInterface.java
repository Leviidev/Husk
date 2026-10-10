package android.content;
public interface DialogInterface {
    int BUTTON_POSITIVE = -1, BUTTON_NEGATIVE = -2, BUTTON_NEUTRAL = -3;
    void cancel(); void dismiss();
    interface OnClickListener { void onClick(DialogInterface d, int which); }
    interface OnCancelListener { void onCancel(DialogInterface d); }
    interface OnDismissListener { void onDismiss(DialogInterface d); }
    interface OnKeyListener { boolean onKey(DialogInterface d, int code, android.view.KeyEvent e); }
}
