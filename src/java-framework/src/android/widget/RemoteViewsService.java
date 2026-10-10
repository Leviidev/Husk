package android.widget;
public abstract class RemoteViewsService extends android.app.Service {
    public interface RemoteViewsFactory { void onCreate(); void onDataSetChanged(); void onDestroy(); int getCount(); RemoteViews getViewAt(int p); RemoteViews getLoadingView(); int getViewTypeCount(); long getItemId(int p); boolean hasStableIds(); }
    public android.os.IBinder onBind(android.content.Intent i) { return null; }
    public abstract RemoteViewsFactory onGetViewFactory(android.content.Intent i);
}
