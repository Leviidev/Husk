package android.animation;
public abstract class AnimatorListenerAdapter implements Animator.AnimatorListener, Animator.AnimatorPauseListener {
    public void onAnimationCancel(Animator a) {} public void onAnimationEnd(Animator a) {} public void onAnimationRepeat(Animator a) {} public void onAnimationStart(Animator a) {}
    public void onAnimationPause(Animator a) {} public void onAnimationResume(Animator a) {}
}
