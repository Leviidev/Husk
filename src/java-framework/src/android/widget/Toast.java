package android.widget;

import android.content.Context;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.view.*;
import java.util.ArrayDeque;

/** A short message in a window near the bottom of the screen, one at a time, for 2 or 3.5 seconds. */
public class Toast {
    public static final int LENGTH_SHORT = 0, LENGTH_LONG = 1;
    public static abstract class Callback { public void onToastShown() {} public void onToastHidden() {} }
    private static final ArrayDeque<Toast> sQueue = new ArrayDeque<>();
    private static Toast sShowing;
    private static Handler sHandler;
    private final Context mContext;
    private View mNextView;
    private CharSequence mText;
    private int mDuration, mGravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM, mX, mY;
    private float mHorizontalMargin, mVerticalMargin;
    private View mShown;
    private final java.util.ArrayList<Callback> mCallbacks = new java.util.ArrayList<>();
    public Toast(Context c) { mContext = c; mY = (int) (64 * c.getResources().getDisplayMetrics().density); }
    public static Toast makeText(Context c, CharSequence text, int duration) { Toast t = new Toast(c); t.mText = text; t.mDuration = duration; return t; }
    public static Toast makeText(Context c, int resId, int duration) { return makeText(c, c.getResources().getText(resId), duration); }
    public void show() {
        android.util.Log.i("Toast", String.valueOf(mText != null ? mText : "(custom view)"));
        if (sHandler == null) sHandler = new Handler(Looper.getMainLooper());
        sHandler.post(() -> { if (sShowing == null) display(this); else if (!sQueue.contains(this)) sQueue.add(this); });
    }
    public void cancel() { if (sHandler == null) return; sHandler.post(() -> { sQueue.remove(this); if (sShowing == this) hide(this); }); }
    private static void display(Toast t) {
        sShowing = t;
        View v = t.mNextView != null ? t.mNextView : t.makeView();
        if (v.getParent() instanceof ViewGroup) ((ViewGroup) v.getParent()).removeView(v);
        WindowManager.LayoutParams p = new WindowManager.LayoutParams();
        p.width = ViewGroup.LayoutParams.WRAP_CONTENT; p.height = ViewGroup.LayoutParams.WRAP_CONTENT;
        p.type = WindowManager.LayoutParams.TYPE_TOAST;
        p.flags = WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE | WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL;
        p.gravity = t.mGravity; p.x = t.mX; p.y = t.mY;
        p.horizontalMargin = t.mHorizontalMargin; p.verticalMargin = t.mVerticalMargin;
        p.format = android.graphics.PixelFormat.TRANSLUCENT;
        try { ((WindowManager) t.mContext.getApplicationContext().getSystemService(Context.WINDOW_SERVICE)).addView(v, p); } catch (RuntimeException e) { android.util.Log.w("Toast", "could not show", e); sShowing = null; return; }
        t.mShown = v;
        for (Callback c : t.mCallbacks) c.onToastShown();
        sHandler.postDelayed(() -> { if (sShowing == t) hide(t); }, t.mDuration == LENGTH_LONG ? 3500 : 2000);
    }
    private static void hide(Toast t) {
        if (t.mShown != null) { try { ((WindowManager) t.mContext.getApplicationContext().getSystemService(Context.WINDOW_SERVICE)).removeViewImmediate(t.mShown); } catch (RuntimeException e) {} t.mShown = null; }
        for (Callback c : t.mCallbacks) c.onToastHidden();
        sShowing = null;
        Toast next = sQueue.poll();
        if (next != null) display(next);
    }
    private View makeView() {
        float d = mContext.getResources().getDisplayMetrics().density;
        TextView tv = new TextView(mContext);
        tv.setText(mText);
        tv.setTextColor(0xFFFFFFFF);
        tv.setTextSize(android.util.TypedValue.COMPLEX_UNIT_SP, 14);
        tv.setMaxWidth((int) (husk.Native.screenWidth() * 0.85f));
        tv.setPadding((int) (16 * d), (int) (12 * d), (int) (16 * d), (int) (12 * d));
        GradientDrawable bg = new GradientDrawable();
        bg.setColor(0xE6323232);
        bg.setCornerRadius(24 * d);
        tv.setBackground(bg);
        return tv;
    }
    public void setDuration(int d) { mDuration = d; } public int getDuration() { return mDuration; }
    public void setMargin(float h, float v) { mHorizontalMargin = h; mVerticalMargin = v; }
    public float getHorizontalMargin() { return mHorizontalMargin; } public float getVerticalMargin() { return mVerticalMargin; }
    public void setGravity(int g, int x, int y) { mGravity = g; mX = x; mY = y; }
    public int getGravity() { return mGravity; } public int getXOffset() { return mX; } public int getYOffset() { return mY; }
    @Deprecated public void setView(View v) { mNextView = v; }
    @Deprecated public View getView() { return mNextView; }
    public void setText(int r) { mText = mContext.getText(r); }
    public void setText(CharSequence s) { mText = s; }
    public void addCallback(Callback c) { mCallbacks.add(c); }
    public void removeCallback(Callback c) { mCallbacks.remove(c); }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static android.widget.Toast makeCustomToastWithIcon(android.content.Context p0, android.os.Looper p1, java.lang.CharSequence p2, int p3, android.graphics.drawable.Drawable p4) { return null; }
    public static android.widget.Toast makeText(android.content.Context p0, android.os.Looper p1, java.lang.CharSequence p2, int p3) { return null; }
    public android.view.WindowManager.LayoutParams getWindowParams() { return null; }
    // ---- end of generated members
}
