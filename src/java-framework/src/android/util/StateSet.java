package android.util;
public class StateSet {
    public static final int[] WILD_CARD = new int[0], NOTHING = new int[] { 0 };
    public static boolean isWildCard(int[] s) { return s.length == 0 || s[0] == 0; }
    public static boolean stateSetMatches(int[] spec, int[] state) {
        if (state == null) return spec == null || isWildCard(spec);
        for (int s : spec) {
            if (s == 0) return true;
            boolean must = s > 0, found = false;
            int v = must ? s : -s;
            for (int t : state) { if (t == 0) break; if (t == v) { found = true; break; } }
            if (must != found) return false;
        }
        return true;
    }
    public static boolean stateSetMatches(int[] spec, int state) { return stateSetMatches(spec, new int[] { state }); }
    public static int[] trimStateSet(int[] s, int len) { return java.util.Arrays.copyOf(s, len); }
    public static String dump(int[] s) { return java.util.Arrays.toString(s); }
}
