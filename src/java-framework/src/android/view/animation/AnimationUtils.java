package android.view.animation;
public class AnimationUtils {
    public static long currentAnimationTimeMillis() { return android.os.SystemClock.uptimeMillis(); }
    public static Animation loadAnimation(android.content.Context c, int id) { return c.getResources().huskLoadAnimation(id); }
    public static Interpolator loadInterpolator(android.content.Context c, int id) { return c.getResources().huskLoadInterpolator(id); }
    public static LayoutAnimationController loadLayoutAnimation(android.content.Context c, int id) { return new LayoutAnimationController(loadAnimation(c, id)); }
    public static Animation makeInAnimation(android.content.Context c, boolean fromLeft) { Animation a = new TranslateAnimation(Animation.RELATIVE_TO_SELF, fromLeft ? -1 : 1, Animation.RELATIVE_TO_SELF, 0, 0, 0, 0, 0); a.setDuration(300); return a; }
    public static Animation makeOutAnimation(android.content.Context c, boolean toRight) { Animation a = new TranslateAnimation(Animation.RELATIVE_TO_SELF, 0, Animation.RELATIVE_TO_SELF, toRight ? 1 : -1, 0, 0, 0, 0); a.setDuration(300); return a; }
}
