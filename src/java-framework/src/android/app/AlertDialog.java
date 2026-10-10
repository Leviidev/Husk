package android.app;
import android.content.DialogInterface;
public class AlertDialog extends Dialog {
    public AlertDialog(android.content.Context c) { super(c); }
    public static class Builder {
        private final android.content.Context c;
        public Builder(android.content.Context c) { this.c = c; }
        public Builder(android.content.Context c, int theme) { this.c = c; }
        public Builder setTitle(CharSequence t) { return this; } public Builder setTitle(int t) { return this; }
        public Builder setMessage(CharSequence m) { return this; } public Builder setMessage(int m) { return this; }
        public Builder setView(android.view.View v) { return this; } public Builder setCancelable(boolean b) { return this; }
        public Builder setPositiveButton(CharSequence t, DialogInterface.OnClickListener l) { return this; }
        public Builder setNegativeButton(CharSequence t, DialogInterface.OnClickListener l) { return this; }
        public Builder setNeutralButton(CharSequence t, DialogInterface.OnClickListener l) { return this; }
        public Builder setPositiveButton(int t, DialogInterface.OnClickListener l) { return this; }
        public Builder setNegativeButton(int t, DialogInterface.OnClickListener l) { return this; }
        public Builder setOnCancelListener(DialogInterface.OnCancelListener l) { return this; }
        public Builder setItems(CharSequence[] items, DialogInterface.OnClickListener l) { return this; }
        public AlertDialog create() { return new AlertDialog(c); }
        public AlertDialog show() { AlertDialog d = create(); d.show(); return d; }
    }
    public void setButton(int which, CharSequence t, DialogInterface.OnClickListener l) {}
    public void setMessage(CharSequence m) {}
    public void setView(android.view.View v) {}
}
