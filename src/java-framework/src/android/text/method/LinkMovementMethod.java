package android.text.method;
import android.text.Layout;
import android.text.Selection;
import android.text.Spannable;
import android.text.style.ClickableSpan;
import android.view.MotionEvent;
import android.widget.TextView;
/** Text with links: a tap on a ClickableSpan clicks it. */
public class LinkMovementMethod extends ScrollingMovementMethod {
    private static LinkMovementMethod sInstance;
    public static MovementMethod getInstance() { if (sInstance == null) sInstance = new LinkMovementMethod(); return sInstance; }
    @Override public boolean canSelectArbitrarily() { return true; }
    @Override public boolean onTouchEvent(TextView w, Spannable buffer, MotionEvent e) {
        int a = e.getActionMasked();
        if (a == MotionEvent.ACTION_UP || a == MotionEvent.ACTION_DOWN) {
            int x = (int) e.getX() - w.getTotalPaddingLeft() + w.getScrollX(), y = (int) e.getY() - w.getTotalPaddingTop() + w.getScrollY();
            Layout l = w.getLayout();
            if (l != null) {
                int line = l.getLineForVertical(y), off = l.getOffsetForHorizontal(line, x);
                ClickableSpan[] links = buffer.getSpans(off, off, ClickableSpan.class);
                if (links.length != 0) {
                    if (a == MotionEvent.ACTION_UP) links[0].onClick(w);
                    else Selection.setSelection(buffer, buffer.getSpanStart(links[0]), buffer.getSpanEnd(links[0]));
                    return true;
                }
                Selection.removeSelection(buffer);
            }
        }
        return super.onTouchEvent(w, buffer, e);
    }
}
