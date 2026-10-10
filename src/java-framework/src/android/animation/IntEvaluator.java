package android.animation;
public class IntEvaluator implements TypeEvaluator<Integer> {
    public Integer evaluate(float f, Integer a, Integer b) { int s = a; return (int) (s + f * (b - s)); }
}
