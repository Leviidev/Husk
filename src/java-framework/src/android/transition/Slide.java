package android.transition;
public class Slide extends Visibility { private int mSide = android.view.Gravity.BOTTOM; public Slide() {} public Slide(int s) { mSide = s; } public Slide(android.content.Context c, android.util.AttributeSet a) { super(c, a); } public void setSlideEdge(int s) { mSide = s; } public int getSlideEdge() { return mSide; } }
