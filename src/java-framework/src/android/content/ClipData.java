package android.content;

import java.util.ArrayList;

public class ClipData implements android.os.Parcelable {
    public static class Item {
        private final CharSequence mText; private final String mHtml; private final Intent mIntent; private final android.net.Uri mUri;
        public Item(CharSequence t) { this(t, null, null, null); }
        public Item(CharSequence t, String html) { this(t, html, null, null); }
        public Item(Intent i) { this(null, null, i, null); }
        public Item(android.net.Uri u) { this(null, null, null, u); }
        public Item(CharSequence t, Intent i, android.net.Uri u) { this(t, null, i, u); }
        public Item(CharSequence t, String html, Intent i, android.net.Uri u) { mText = t; mHtml = html; mIntent = i; mUri = u; }
        public CharSequence getText() { return mText; } public String getHtmlText() { return mHtml; } public Intent getIntent() { return mIntent; } public android.net.Uri getUri() { return mUri; }
        public CharSequence coerceToText(Context c) { if (mText != null) return mText; if (mUri != null) return mUri.toString(); if (mIntent != null) return mIntent.toUri(0); return ""; }
        public CharSequence coerceToStyledText(Context c) { return coerceToText(c); }
        public String coerceToHtmlText(Context c) { return mHtml != null ? mHtml : String.valueOf(coerceToText(c)); }
    }
    private final ClipDescription mDesc;
    private final ArrayList<Item> mItems = new ArrayList<>();
    public ClipData(CharSequence label, String[] types, Item item) { mDesc = new ClipDescription(label, types); mItems.add(item); }
    public ClipData(ClipDescription d, Item item) { mDesc = d; mItems.add(item); }
    public ClipData(ClipData o) { mDesc = o.mDesc; mItems.addAll(o.mItems); }
    public static ClipData newPlainText(CharSequence label, CharSequence text) { return new ClipData(label, new String[] { ClipDescription.MIMETYPE_TEXT_PLAIN }, new Item(text)); }
    public static ClipData newHtmlText(CharSequence label, CharSequence text, String html) { return new ClipData(label, new String[] { ClipDescription.MIMETYPE_TEXT_HTML }, new Item(text, html)); }
    public static ClipData newIntent(CharSequence label, Intent i) { return new ClipData(label, new String[] { ClipDescription.MIMETYPE_TEXT_INTENT }, new Item(i)); }
    public static ClipData newUri(ContentResolver r, CharSequence label, android.net.Uri u) { return newRawUri(label, u); }
    public static ClipData newRawUri(CharSequence label, android.net.Uri u) { return new ClipData(label, new String[] { ClipDescription.MIMETYPE_TEXT_URILIST }, new Item(u)); }
    public ClipDescription getDescription() { return mDesc; }
    public void addItem(Item i) { mItems.add(i); }
    public void addItem(ContentResolver r, Item i) { mItems.add(i); }
    public int getItemCount() { return mItems.size(); }
    public Item getItemAt(int i) { return mItems.get(i); }
    public int describeContents() { return 0; }
}
