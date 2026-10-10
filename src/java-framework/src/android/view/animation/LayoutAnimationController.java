package android.view.animation;
public class LayoutAnimationController {
    public static class AnimationParameters { public int count, index; }
    private Animation mAnim;
    public LayoutAnimationController(Animation a) { mAnim = a; }
    public LayoutAnimationController(Animation a, float delay) { mAnim = a; }
    public LayoutAnimationController(android.content.Context c, android.util.AttributeSet a) {}
    public Animation getAnimation() { return mAnim; }
    public void setAnimation(Animation a) { mAnim = a; }
    public void setDelay(float d) {}
    public void setOrder(int o) {}
    public void start() {}
    public boolean isDone() { return true; }
}
