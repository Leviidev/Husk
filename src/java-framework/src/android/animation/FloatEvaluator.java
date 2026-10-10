package android.animation;
public class FloatEvaluator implements TypeEvaluator<Number> {
    public Float evaluate(float f, Number a, Number b) { float s = a.floatValue(); return s + f * (b.floatValue() - s); }
}
