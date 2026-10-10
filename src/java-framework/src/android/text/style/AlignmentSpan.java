package android.text.style;
public interface AlignmentSpan extends ParagraphStyle {
    android.text.Layout.Alignment getAlignment();
    class Standard implements AlignmentSpan, android.text.ParcelableSpan {
        private final android.text.Layout.Alignment mAlignment;
        public Standard(android.text.Layout.Alignment a) { mAlignment = a; }
        public android.text.Layout.Alignment getAlignment() { return mAlignment; }
        public int getSpanTypeId() { return 1; } public int describeContents() { return 0; }
    }
}
