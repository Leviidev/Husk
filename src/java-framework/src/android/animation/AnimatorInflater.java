package android.animation;
public class AnimatorInflater {
    public static Animator loadAnimator(android.content.Context c, int id) { return c.getResources().huskLoadAnimator(id); }
    public static StateListAnimator loadStateListAnimator(android.content.Context c, int id) { return new StateListAnimator(); }
}
