package android.text.style;
public class URLSpan extends ClickableSpan implements android.text.ParcelableSpan {
    private final String mURL;
    public URLSpan(String url) { mURL = url; }
    public String getURL() { return mURL; }
    @Override public void onClick(android.view.View widget) {
        android.content.Intent i = new android.content.Intent(android.content.Intent.ACTION_VIEW, android.net.Uri.parse(mURL));
        try { widget.getContext().startActivity(i); } catch (android.content.ActivityNotFoundException e) { android.util.Log.w("URLSpan", "Actvity was not found for intent, " + i); }
    }
    public int getSpanTypeId() { return 11; } public int describeContents() { return 0; }
}
