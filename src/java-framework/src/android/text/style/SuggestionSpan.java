package android.text.style;
public class SuggestionSpan extends CharacterStyle implements android.text.ParcelableSpan {
    public static final int FLAG_EASY_CORRECT = 1, FLAG_MISSPELLED = 2, FLAG_AUTO_CORRECTION = 4, FLAG_GRAMMAR_ERROR = 8, SUGGESTIONS_MAX_SIZE = 5;
    public static final String ACTION_SUGGESTION_PICKED = "android.text.style.SUGGESTION_PICKED";
    private final String[] mSuggestions; private final int mFlags;
    public SuggestionSpan(android.content.Context c, String[] s, int f) { mSuggestions = s; mFlags = f; }
    public SuggestionSpan(java.util.Locale l, String[] s, int f) { mSuggestions = s; mFlags = f; }
    public SuggestionSpan(android.content.Context c, java.util.Locale l, String[] s, int f, Class<?> n) { mSuggestions = s; mFlags = f; }
    public String[] getSuggestions() { return mSuggestions; } public int getFlags() { return mFlags; } public void setFlags(int f) {}
    @Override public void updateDrawState(android.text.TextPaint tp) {}
    public int getSpanTypeId() { return 19; } public int describeContents() { return 0; }
}
