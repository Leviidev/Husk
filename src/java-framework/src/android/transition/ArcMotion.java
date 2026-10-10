package android.transition;
public class ArcMotion extends PathMotion { public ArcMotion() {} public ArcMotion(android.content.Context c, android.util.AttributeSet a) {} public void setMinimumHorizontalAngle(float a) {} public void setMinimumVerticalAngle(float a) {} public void setMaximumAngle(float a) {}
    public android.graphics.Path getPath(float sx, float sy, float ex, float ey) { android.graphics.Path p = new android.graphics.Path(); p.moveTo(sx, sy); p.quadTo(ex, sy, ex, ey); return p; } }
