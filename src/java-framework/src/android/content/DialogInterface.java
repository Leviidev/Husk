package android.content;
public interface DialogInterface {
    int BUTTON_POSITIVE = -1, BUTTON_NEGATIVE = -2, BUTTON_NEUTRAL = -3;
    @Deprecated int BUTTON1 = BUTTON_POSITIVE, BUTTON2 = BUTTON_NEGATIVE, BUTTON3 = BUTTON_NEUTRAL;
    void cancel();
    void dismiss();
    interface OnCancelListener { void onCancel(DialogInterface d); }
    interface OnDismissListener { void onDismiss(DialogInterface d); }
    interface OnShowListener { void onShow(DialogInterface d); }
    interface OnClickListener { void onClick(DialogInterface d, int which); }
    interface OnMultiChoiceClickListener { void onClick(DialogInterface d, int which, boolean isChecked); }
    interface OnKeyListener { boolean onKey(DialogInterface d, int keyCode, android.view.KeyEvent e); }
}
