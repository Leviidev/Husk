package android.app;
public final class RemoteInput implements android.os.Parcelable {
    public static final String RESULTS_CLIP_LABEL = "android.remoteinput.results", EXTRA_RESULTS_DATA = "android.remoteinput.resultsData";
    public static android.os.Bundle getResultsFromIntent(android.content.Intent i) { return null; }
    public static void addResultsToIntent(RemoteInput[] r, android.content.Intent i, android.os.Bundle b) {}
    public String getResultKey() { return null; } public int describeContents() { return 0; }
    public static final class Builder { public Builder(String k) {} public Builder setLabel(CharSequence l) { return this; } public Builder setChoices(CharSequence[] c) { return this; } public Builder setAllowFreeFormInput(boolean b) { return this; } public Builder addExtras(android.os.Bundle e) { return this; } public RemoteInput build() { return new RemoteInput(); } }
}
