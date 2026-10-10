package android.os;
public abstract class CombinedVibration {
    public static CombinedVibration createParallel(VibrationEffect e) { return new CombinedVibration() {}; }
    public static ParallelCombination startParallel() { return new ParallelCombination(); }
    public static final class ParallelCombination { public ParallelCombination addVibrator(int id, VibrationEffect e) { return this; } public CombinedVibration combine() { return new CombinedVibration() {}; } }
}
