package android.graphics.drawable;
public interface Animatable2 extends Animatable {
    abstract class AnimationCallback { public void onAnimationStart(Drawable d) {} public void onAnimationEnd(Drawable d) {} }
    void registerAnimationCallback(AnimationCallback c);
    boolean unregisterAnimationCallback(AnimationCallback c);
    void clearAnimationCallbacks();
}
