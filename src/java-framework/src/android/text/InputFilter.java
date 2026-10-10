package android.text;
public interface InputFilter {
    CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend);
    class AllCaps implements InputFilter {
        public AllCaps() {} public AllCaps(java.util.Locale l) {}
        public CharSequence filter(CharSequence s, int st, int en, Spanned d, int ds, int de) { String up = s.subSequence(st, en).toString().toUpperCase(); return up.contentEquals(s.subSequence(st, en)) ? null : up; }
    }
    class LengthFilter implements InputFilter {
        private final int mMax;
        public LengthFilter(int max) { mMax = max; }
        public CharSequence filter(CharSequence s, int st, int en, Spanned d, int ds, int de) {
            int keep = mMax - (d.length() - (de - ds));
            if (keep <= 0) return "";
            if (keep >= en - st) return null;
            keep += st;
            if (Character.isHighSurrogate(s.charAt(keep - 1))) { --keep; if (keep == st) return ""; }
            return s.subSequence(st, keep);
        }
        public int getMax() { return mMax; }
    }
}
