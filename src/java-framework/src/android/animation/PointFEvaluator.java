package android.animation;
import android.graphics.PointF;
public class PointFEvaluator implements TypeEvaluator<PointF> {
    private PointF mP;
    public PointFEvaluator() {}
    public PointFEvaluator(PointF reuse) { mP = reuse; }
    public PointF evaluate(float f, PointF a, PointF b) { float x = a.x + f * (b.x - a.x), y = a.y + f * (b.y - a.y); if (mP != null) { mP.set(x, y); return mP; } return new PointF(x, y); }
}
