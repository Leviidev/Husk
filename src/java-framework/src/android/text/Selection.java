package android.text;

/** The cursor and selection of an Editable, kept as two spans on it. */
public class Selection {
    private static final class START implements NoCopySpan {}
    private static final class END implements NoCopySpan {}
    public static final Object SELECTION_START = new START(), SELECTION_END = new END();
    private Selection() {}
    public static final int getSelectionStart(CharSequence t) { return t instanceof Spanned ? ((Spanned) t).getSpanStart(SELECTION_START) : -1; }
    public static final int getSelectionEnd(CharSequence t) { return t instanceof Spanned ? ((Spanned) t).getSpanStart(SELECTION_END) : -1; }
    public static void setSelection(Spannable t, int start, int stop) {
        int len = t.length();
        start = Math.max(0, Math.min(start, len)); stop = Math.max(0, Math.min(stop, len));
        t.setSpan(SELECTION_START, start, start, Spanned.SPAN_POINT_POINT);
        t.setSpan(SELECTION_END, stop, stop, Spanned.SPAN_POINT_POINT);
    }
    public static final void setSelection(Spannable t, int index) { setSelection(t, index, index); }
    public static final void selectAll(Spannable t) { setSelection(t, 0, t.length()); }
    public static final void extendSelection(Spannable t, int index) { if (t.getSpanStart(SELECTION_END) != index) t.setSpan(SELECTION_END, index, index, Spanned.SPAN_POINT_POINT); }
    public static final void removeSelection(Spannable t) { t.removeSpan(SELECTION_START); t.removeSpan(SELECTION_END); }
    public static boolean moveLeft(Spannable t, Layout l) { int s = getSelectionStart(t), e = getSelectionEnd(t); if (s != e) { setSelection(t, Math.min(s, e)); return true; } if (e > 0) { setSelection(t, e - 1); return true; } return false; }
    public static boolean moveRight(Spannable t, Layout l) { int s = getSelectionStart(t), e = getSelectionEnd(t); if (s != e) { setSelection(t, Math.max(s, e)); return true; } if (e < t.length()) { setSelection(t, e + 1); return true; } return false; }
    public static boolean moveUp(Spannable t, Layout l) { return false; }
    public static boolean moveDown(Spannable t, Layout l) { return false; }
    public static boolean moveToLeftEdge(Spannable t, Layout l) { setSelection(t, 0); return true; }
    public static boolean moveToRightEdge(Spannable t, Layout l) { setSelection(t, t.length()); return true; }
    public static boolean extendLeft(Spannable t, Layout l) { int e = getSelectionEnd(t); if (e > 0) { extendSelection(t, e - 1); return true; } return false; }
    public static boolean extendRight(Spannable t, Layout l) { int e = getSelectionEnd(t); if (e < t.length()) { extendSelection(t, e + 1); return true; } return false; }
    public static boolean extendUp(Spannable t, Layout l) { return false; }
    public static boolean extendDown(Spannable t, Layout l) { return false; }
    public static boolean extendToLeftEdge(Spannable t, Layout l) { extendSelection(t, 0); return true; }
    public static boolean extendToRightEdge(Spannable t, Layout l) { extendSelection(t, t.length()); return true; }
}
