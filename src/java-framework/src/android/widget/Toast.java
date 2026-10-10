package android.widget;
public class Toast {
    public static final int LENGTH_SHORT = 0, LENGTH_LONG = 1;
    private final CharSequence text;
    private Toast(CharSequence t) { text = t; }
    public static Toast makeText(android.content.Context c, CharSequence t, int d) { return new Toast(t); }
    public static Toast makeText(android.content.Context c, int res, int d) { return new Toast(""); }
    public void show() { android.util.Log.i("Toast", String.valueOf(text)); }
    public void cancel() {} public void setGravity(int g, int x, int y) {}
}
