package android.window;
public final class SplashScreenView extends android.widget.FrameLayout {
    public SplashScreenView(android.content.Context c) { super(c); }
    public android.view.View getIconView() { return null; }
    public java.time.Duration getIconAnimationDuration() { return null; }
    public java.time.Instant getIconAnimationStart() { return null; }
    public void remove() { if (getParent() instanceof android.view.ViewGroup) ((android.view.ViewGroup) getParent()).removeView(this); }
}
