package android.transition;
public class PatternPathMotion extends PathMotion { public PatternPathMotion() {} public PatternPathMotion(android.graphics.Path p) {} public void setPatternPath(android.graphics.Path p) {} public android.graphics.Path getPatternPath() { return null; }
    public android.graphics.Path getPath(float sx, float sy, float ex, float ey) { android.graphics.Path p = new android.graphics.Path(); p.moveTo(sx, sy); p.lineTo(ex, ey); return p; } }
