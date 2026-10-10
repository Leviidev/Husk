package husk;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.text.Spanned;
import android.text.TextPaint;
import android.text.style.*;

/** Text with spans, measured and drawn in runs: each run with the paint its spans make (colour, size, style, underline...). */
public final class TextRuns {
    private TextRuns() {}
    private static final ThreadLocal<TextPaint> sWork = new ThreadLocal<TextPaint>() { protected TextPaint initialValue() { return new TextPaint(); } };
    public static float measure(CharSequence text, int start, int end, TextPaint paint) {
        if (end <= start) return 0;
        if (!(text instanceof Spanned)) return paint.measureText(text.toString(), start, end);
        Spanned sp = (Spanned) text;
        if (sp.nextSpanTransition(start, end, MetricAffectingSpan.class) >= end && sp.getSpans(start, end, MetricAffectingSpan.class).length == 0) return paint.measureText(sp.toString(), start, end);
        float w = 0;
        TextPaint wp = sWork.get();
        String s = sp.toString();
        for (int i = start; i < end; ) {
            int next = sp.nextSpanTransition(i, end, MetricAffectingSpan.class);
            wp.set(paint);
            ReplacementSpan repl = null;
            for (MetricAffectingSpan m : sp.getSpans(i, next, MetricAffectingSpan.class)) { if (m instanceof ReplacementSpan) repl = (ReplacementSpan) m; else m.updateMeasureState(wp); }
            if (repl != null) { if (i == sp.getSpanStart(repl) || i == start) w += repl.getSize(wp, text, sp.getSpanStart(repl), sp.getSpanEnd(repl), null); }
            else w += wp.measureText(s, i, next);
            i = next;
        }
        return w;
    }
    public static void draw(Canvas c, CharSequence text, int start, int end, float x, float baseline, int top, int bottom, TextPaint paint) {
        if (end <= start) return;
        String s = text.toString();
        if (!(text instanceof Spanned)) { c.drawText(s, start, end, x, baseline, paint); return; }
        Spanned sp = (Spanned) text;
        TextPaint wp = new TextPaint();
        for (int i = start; i < end; ) {
            int next = sp.nextSpanTransition(i, end, CharacterStyle.class);
            wp.set(paint);
            wp.bgColor = 0;
            ReplacementSpan repl = null;
            for (CharacterStyle cs : sp.getSpans(i, next, CharacterStyle.class)) {
                CharacterStyle u = cs.getUnderlying();
                if (u instanceof ReplacementSpan) { repl = (ReplacementSpan) u; continue; }
                if (u instanceof MetricAffectingSpan) ((MetricAffectingSpan) u).updateMeasureState(wp);
                u.updateDrawState(wp);
            }
            float w;
            if (repl != null) {
                w = repl.getSize(wp, text, sp.getSpanStart(repl), sp.getSpanEnd(repl), null);
                if (i == sp.getSpanStart(repl) || i == start) repl.draw(c, text, sp.getSpanStart(repl), sp.getSpanEnd(repl), x, top, (int) baseline, bottom, wp);
                x += w; i = next; continue;
            }
            w = wp.measureText(s, i, next);
            if (wp.bgColor != 0) { Paint bg = new Paint(); bg.setColor(wp.bgColor); c.drawRect(x, top, x + w, bottom, bg); }
            c.drawText(s, i, next, x, baseline + wp.baselineShift, wp);
            x += w;
            i = next;
        }
    }
}
