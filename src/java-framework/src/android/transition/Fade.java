package android.transition;
public class Fade extends Visibility { public static final int IN = 1, OUT = 2; public Fade() {} public Fade(int m) { setMode(m); } public Fade(android.content.Context c, android.util.AttributeSet a) { super(c, a); } }
