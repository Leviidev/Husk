package android.text;
public interface Spanned extends CharSequence {
    int SPAN_POINT_MARK_MASK = 0x33, SPAN_MARK_MARK = 0x11, SPAN_MARK_POINT = 0x12, SPAN_POINT_MARK = 0x21, SPAN_POINT_POINT = 0x22, SPAN_PARAGRAPH = 0x33,
        SPAN_INCLUSIVE_EXCLUSIVE = SPAN_MARK_MARK, SPAN_INCLUSIVE_INCLUSIVE = SPAN_MARK_POINT, SPAN_EXCLUSIVE_EXCLUSIVE = SPAN_POINT_MARK, SPAN_EXCLUSIVE_INCLUSIVE = SPAN_POINT_POINT,
        SPAN_COMPOSING = 0x100, SPAN_INTERMEDIATE = 0x200, SPAN_USER_SHIFT = 24, SPAN_USER = 0xff000000, SPAN_PRIORITY_SHIFT = 16, SPAN_PRIORITY = 0xff0000;
    <T> T[] getSpans(int start, int end, Class<T> type);
    int getSpanStart(Object tag);
    int getSpanEnd(Object tag);
    int getSpanFlags(Object tag);
    int nextSpanTransition(int start, int limit, Class type);
}
