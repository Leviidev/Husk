package android.text;
public class TextDirectionHeuristics {
    private static final TextDirectionHeuristic LTR_ = new TextDirectionHeuristic() { public boolean isRtl(char[] a, int s, int c) { return false; } public boolean isRtl(CharSequence cs, int s, int c) { return false; } };
    private static final TextDirectionHeuristic RTL_ = new TextDirectionHeuristic() { public boolean isRtl(char[] a, int s, int c) { return true; } public boolean isRtl(CharSequence cs, int s, int c) { return true; } };
    public static final TextDirectionHeuristic LTR = LTR_, RTL = RTL_, FIRSTSTRONG_LTR = LTR_, FIRSTSTRONG_RTL = LTR_, ANYRTL_LTR = LTR_, LOCALE = LTR_;
}
