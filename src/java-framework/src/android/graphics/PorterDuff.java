package android.graphics;
public class PorterDuff {
    public enum Mode {
        CLEAR(0), SRC(1), DST(2), SRC_OVER(3), DST_OVER(4), SRC_IN(5), DST_IN(6), SRC_OUT(7), DST_OUT(8), SRC_ATOP(9), DST_ATOP(10), XOR(11),
        DARKEN(16), LIGHTEN(17), MULTIPLY(13), SCREEN(14), ADD(12), OVERLAY(15);
        public final int nativeInt;
        Mode(int n) { nativeInt = n; }
    }
}
