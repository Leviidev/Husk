package android.widget;

import android.os.Parcel;
import android.os.Parcelable;

/** Views for another process (notifications, widgets): recorded and accepted; Husk has no home screen or shade to show them in. */
public class RemoteViews implements Parcelable, android.view.LayoutInflater.Filter {
    public static class ActionException extends RuntimeException { public ActionException(Exception e) { super(e); } public ActionException(String s) { super(s); } }
    public static final class RemoteResponse { public static RemoteResponse fromPendingIntent(android.app.PendingIntent p) { return new RemoteResponse(); } public static RemoteResponse fromFillInIntent(android.content.Intent i) { return new RemoteResponse(); } public RemoteResponse addSharedElement(int id, String n) { return this; } }
    public static final class RemoteCollectionItems implements Parcelable { public static final class Builder { public Builder addItem(long id, RemoteViews v) { return this; } public Builder setHasStableIds(boolean b) { return this; } public Builder setViewTypeCount(int n) { return this; } public RemoteCollectionItems build() { return new RemoteCollectionItems(); } } public int describeContents() { return 0; } public void writeToParcel(Parcel p, int f) {} }
    public static final String EXTRA_SHARED_ELEMENT_BOUNDS = "android.widget.extra.SHARED_ELEMENT_BOUNDS";
    private final String mPackage;
    private final int mLayoutId;
    public RemoteViews(String packageName, int layoutId) { mPackage = packageName; mLayoutId = layoutId; }
    public RemoteViews(String packageName, int layoutId, int viewId) { this(packageName, layoutId); }
    public RemoteViews(RemoteViews landscape, RemoteViews portrait) { this(portrait.mPackage, portrait.mLayoutId); }
    public RemoteViews(java.util.Map<android.util.SizeF, RemoteViews> m) { this("", 0); }
    public RemoteViews(RemoteViews src) { this(src.mPackage, src.mLayoutId); }
    public RemoteViews(Parcel p) { this(p.readString(), p.readInt()); }
    public String getPackage() { return mPackage; }
    public int getLayoutId() { return mLayoutId; }
    public int getViewId() { return 0; }
    public RemoteViews clone() { return new RemoteViews(this); }
    public boolean onLoadClass(Class c) { return true; }
    public android.view.View apply(android.content.Context c, android.view.ViewGroup parent) { return android.view.LayoutInflater.from(c).inflate(mLayoutId, parent, false); }
    public void reapply(android.content.Context c, android.view.View v) {}
    public void addView(int viewId, RemoteViews nested) {} public void addStableView(int viewId, RemoteViews nested, int stableId) {}
    public void removeAllViews(int viewId) {} public void showNext(int viewId) {} public void showPrevious(int viewId) {} public void setDisplayedChild(int viewId, int i) {}
    public void setViewVisibility(int viewId, int v) {} public void setTextViewText(int viewId, CharSequence t) {} public void setTextViewTextSize(int viewId, int u, float s) {}
    public void setTextViewCompoundDrawables(int viewId, int l, int t, int r, int b) {} public void setTextViewCompoundDrawablesRelative(int viewId, int s, int t, int e, int b) {}
    public void setImageViewResource(int viewId, int r) {} public void setImageViewUri(int viewId, android.net.Uri u) {} public void setImageViewBitmap(int viewId, android.graphics.Bitmap b) {} public void setImageViewIcon(int viewId, android.graphics.drawable.Icon i) {}
    public void setEmptyView(int viewId, int emptyViewId) {} public void setChronometer(int viewId, long base, String format, boolean started) {} public void setChronometerCountDown(int viewId, boolean d) {}
    public void setProgressBar(int viewId, int max, int progress, boolean indeterminate) {}
    public void setOnClickPendingIntent(int viewId, android.app.PendingIntent p) {} public void setOnClickResponse(int viewId, RemoteResponse r) {} public void setPendingIntentTemplate(int viewId, android.app.PendingIntent p) {} public void setOnClickFillInIntent(int viewId, android.content.Intent i) {} public void setOnCheckedChangeResponse(int viewId, RemoteResponse r) {}
    public void setTextColor(int viewId, int c) {} public void setTextColor(int viewId, android.content.res.ColorStateList c) {} public void setRemoteAdapter(int viewId, android.content.Intent i) {} public void setRemoteAdapter(int viewId, RemoteCollectionItems i) {} public void setScrollPosition(int viewId, int p) {} public void setRelativeScrollPosition(int viewId, int o) {}
    public void setViewPadding(int viewId, int l, int t, int r, int b) {} public void setViewLayoutMargin(int viewId, int t, float v, int u) {} public void setViewLayoutWidth(int viewId, float v, int u) {} public void setViewLayoutHeight(int viewId, float v, int u) {}
    public void setBoolean(int viewId, String m, boolean v) {} public void setByte(int viewId, String m, byte v) {} public void setShort(int viewId, String m, short v) {} public void setInt(int viewId, String m, int v) {} public void setLong(int viewId, String m, long v) {} public void setFloat(int viewId, String m, float v) {} public void setDouble(int viewId, String m, double v) {} public void setChar(int viewId, String m, char v) {} public void setString(int viewId, String m, String v) {} public void setCharSequence(int viewId, String m, CharSequence v) {} public void setUri(int viewId, String m, android.net.Uri v) {} public void setBitmap(int viewId, String m, android.graphics.Bitmap v) {} public void setBundle(int viewId, String m, android.os.Bundle v) {} public void setIntent(int viewId, String m, android.content.Intent v) {} public void setIcon(int viewId, String m, android.graphics.drawable.Icon v) {} public void setColorStateList(int viewId, String m, android.content.res.ColorStateList v) {} public void setColor(int viewId, String m, int c) {} public void setColorInt(int viewId, String m, int a, int b) {}
    public void setContentDescription(int viewId, CharSequence d) {} public void setAccessibilityTraversalBefore(int viewId, int t) {} public void setAccessibilityTraversalAfter(int viewId, int t) {} public void setLabelFor(int viewId, int l) {} public void setCompoundButtonChecked(int viewId, boolean c) {} public void setRadioGroupChecked(int viewId, int c) {}
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel p, int f) { p.writeString(mPackage); p.writeInt(mLayoutId); }
    public static final Parcelable.Creator<RemoteViews> CREATOR = new Parcelable.Creator<RemoteViews>() { public RemoteViews createFromParcel(Parcel p) { return new RemoteViews(p); } public RemoteViews[] newArray(int n) { return new RemoteViews[n]; } };
}
