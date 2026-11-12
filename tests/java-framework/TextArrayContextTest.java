// SPDX-License-Identifier: GPL-2.0-or-later
package android.graphics;

/** Character-array APIs must retain full control-token context for the native
 * rendering filter, even when a styled/positioned run draws a single unit. */
public final class TextArrayContextTest {
    static final String SOURCE = "x^FFFF\u0110\u0103ng nh\u1eadp\uD83D\uDE42";
    static void check(boolean value) { if (!value) throw new AssertionError(); }
    static final class Recorder extends Canvas {
        int calls, start, end;
        @Override public void drawText(String s, int start, int end, float x, float y, Paint p) {
            check(SOURCE.equals(s)); this.start = start; this.end = end; calls++;
        }
    }
    static final class Measuring extends Paint {
        int calls;
        void input(String s, int start, int end) { check(SOURCE.equals(s)); check(start == 3 && end == 8); calls++; }
        @Override public float measureText(String s, int start, int end) { input(s, start, end); return 7; }
        @Override public int breakText(CharSequence s, int start, int end, boolean forwards, float width, float[] measured) {
            input(s.toString(), start, end); return 4;
        }
        @Override public int getTextWidths(String s, int start, int end, float[] widths) { input(s, start, end); return end - start; }
        @Override public void getTextBounds(String s, int start, int end, Rect bounds) { input(s, start, end); bounds.set(0, 0, 7, 2); }
    }
    public static void main(String[] args) {
        char[] text = SOURCE.toCharArray(), original = text.clone();
        Recorder canvas = new Recorder();
        canvas.drawText(text, 3, 5, 0, 0, null);
        check(canvas.start == 3 && canvas.end == 8);
        canvas.drawTextRun(text, 3, 5, 0, text.length, 0, 0, false, null);
        canvas.drawTextOnPath(text, 3, 5, null, 0, 0, null);
        check(canvas.calls == 3);
        canvas.drawPosText(text, 3, 5, new float[10], null);
        check(canvas.calls == 8 && canvas.start == 7 && canvas.end == 8);
        Measuring paint = new Measuring();
        check(paint.measureText(text, 3, 5) == 7);
        check(paint.breakText(text, 3, 5, 10, null) == 4);
        check(paint.getTextWidths(text, 3, 5, new float[5]) == 5);
        Rect bounds = new Rect(); paint.getTextBounds(text, 3, 5, bounds);
        check(bounds.right == 7 && paint.calls == 4);
        check(java.util.Arrays.equals(original, text));
        try { canvas.drawText(text, -1, 1, 0, 0, null); throw new AssertionError(); }
        catch (IndexOutOfBoundsException expected) {}
        try { paint.measureText(text, 3, Integer.MAX_VALUE); throw new AssertionError(); }
        catch (IndexOutOfBoundsException expected) {}
        System.out.println("PASS: Canvas/Paint array, run and positioned text retain full Unicode source plus original ranges without mutation");
    }
}
