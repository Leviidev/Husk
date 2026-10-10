package android.media;

import java.util.ArrayList;
import java.util.List;

/** The platform's media router: one system route, the phone's own speaker ("Phone"), always selected; apps may add user routes.
 *  androidx.mediarouter (YouTube, Cast-capable players) mirrors this and refuses to start without a selected route. */
public class MediaRouter {
    public static final int AVAILABILITY_FLAG_IGNORE_DEFAULT_ROUTE = 1;
    public static final int CALLBACK_FLAG_PASSIVE_DISCOVERY = 8, CALLBACK_FLAG_PERFORM_ACTIVE_SCAN = 1, CALLBACK_FLAG_REQUEST_DISCOVERY = 4, CALLBACK_FLAG_UNFILTERED_EVENTS = 2;
    public static final String MIRRORING_GROUP_ID = "android.media.mirroring_group";
    public static final int ROUTE_TYPE_LIVE_AUDIO = 1, ROUTE_TYPE_LIVE_VIDEO = 2, ROUTE_TYPE_REMOTE_DISPLAY = 4, ROUTE_TYPE_USER = 8388608;

    private static final RouteCategory sSystem = new RouteCategory("System", ROUTE_TYPE_LIVE_AUDIO | ROUTE_TYPE_LIVE_VIDEO, false, true);
    private static final RouteInfo sDefault = new RouteInfo(sSystem);
    static {
        sDefault.mName = "Phone"; sDefault.mSupportedTypes = ROUTE_TYPE_LIVE_AUDIO | ROUTE_TYPE_LIVE_VIDEO; sDefault.mDefault = true;
        sDefault.mDeviceType = RouteInfo.DEVICE_TYPE_SPEAKER; sDefault.mPlaybackType = RouteInfo.PLAYBACK_TYPE_LOCAL;
        sDefault.mVolumeHandling = RouteInfo.PLAYBACK_VOLUME_VARIABLE; sDefault.mVolumeMax = 15; sDefault.mVolume = 10; sDefault.mPlaybackStream = AudioManager.STREAM_MUSIC;
    }
    private static final ArrayList<RouteInfo> sRoutes = new ArrayList<>();
    private static final ArrayList<RouteCategory> sCategories = new ArrayList<>();
    static { sRoutes.add(sDefault); sCategories.add(sSystem); }
    private static RouteInfo sSelected = sDefault;
    private final ArrayList<Callback> mCallbacks = new ArrayList<>();

    public MediaRouter(android.content.Context c) {}
    public void addCallback(int types, Callback cb) { addCallback(types, cb, 0); }
    public void addCallback(int types, Callback cb, int flags) { synchronized (mCallbacks) { if (!mCallbacks.contains(cb)) mCallbacks.add(cb); } }
    public void removeCallback(Callback cb) { synchronized (mCallbacks) { mCallbacks.remove(cb); } }
    public RouteInfo getDefaultRoute() { return sDefault; }
    public RouteInfo getFallbackRoute() { return sDefault; }
    public RouteInfo getSelectedRoute() { return sSelected; }
    public RouteInfo getSelectedRoute(int type) { return sSelected; }
    public RouteCategory getSystemCategory() { return sSystem; }
    public int getRouteCount() { synchronized (sRoutes) { return sRoutes.size(); } }
    public RouteInfo getRouteAt(int i) { synchronized (sRoutes) { return sRoutes.get(i); } }
    public int getCategoryCount() { synchronized (sRoutes) { return sCategories.size(); } }
    public RouteCategory getCategoryAt(int i) { synchronized (sRoutes) { return sCategories.get(i); } }
    public boolean isRouteAvailable(int types, int flags) { return (flags & AVAILABILITY_FLAG_IGNORE_DEFAULT_ROUTE) == 0 || getRouteCount() > 1; }
    public void selectRoute(int types, RouteInfo r) {
        if (r == null || r == sSelected) return;
        RouteInfo old = sSelected;
        sSelected = r;
        for (Callback cb : callbacks()) { cb.onRouteUnselected(this, types, old); cb.onRouteSelected(this, types, r); }
    }
    public RouteCategory createRouteCategory(CharSequence name, boolean groupable) { RouteCategory c = new RouteCategory(name, ROUTE_TYPE_USER, groupable, false); synchronized (sRoutes) { sCategories.add(c); } return c; }
    public RouteCategory createRouteCategory(int nameRes, boolean groupable) { return createRouteCategory("Routes", groupable); }
    public UserRouteInfo createUserRoute(RouteCategory c) { return new UserRouteInfo(c); }
    public void addUserRoute(UserRouteInfo r) { synchronized (sRoutes) { if (!sRoutes.contains(r)) sRoutes.add(r); } for (Callback cb : callbacks()) cb.onRouteAdded(this, r); }
    public void removeUserRoute(UserRouteInfo r) { synchronized (sRoutes) { sRoutes.remove(r); } if (sSelected == r) sSelected = sDefault; for (Callback cb : callbacks()) cb.onRouteRemoved(this, r); }
    public void clearUserRoutes() { synchronized (sRoutes) { sRoutes.removeIf(x -> x instanceof UserRouteInfo); } sSelected = sDefault; }
    private ArrayList<Callback> callbacks() { synchronized (mCallbacks) { return new ArrayList<>(mCallbacks); } }

    public static abstract class Callback {
        public Callback() {}
        public abstract void onRouteSelected(MediaRouter router, int type, RouteInfo info);
        public abstract void onRouteUnselected(MediaRouter router, int type, RouteInfo info);
        public abstract void onRouteAdded(MediaRouter router, RouteInfo info);
        public abstract void onRouteRemoved(MediaRouter router, RouteInfo info);
        public abstract void onRouteChanged(MediaRouter router, RouteInfo info);
        public abstract void onRouteGrouped(MediaRouter router, RouteInfo info, RouteGroup group, int index);
        public abstract void onRouteUngrouped(MediaRouter router, RouteInfo info, RouteGroup group);
        public abstract void onRouteVolumeChanged(MediaRouter router, RouteInfo info);
        public void onRoutePresentationDisplayChanged(MediaRouter router, RouteInfo info) {}
    }
    public static class SimpleCallback extends Callback {
        public SimpleCallback() {}
        public void onRouteSelected(MediaRouter r, int t, RouteInfo i) {} public void onRouteUnselected(MediaRouter r, int t, RouteInfo i) {}
        public void onRouteAdded(MediaRouter r, RouteInfo i) {} public void onRouteRemoved(MediaRouter r, RouteInfo i) {} public void onRouteChanged(MediaRouter r, RouteInfo i) {}
        public void onRouteGrouped(MediaRouter r, RouteInfo i, RouteGroup g, int x) {} public void onRouteUngrouped(MediaRouter r, RouteInfo i, RouteGroup g) {}
        public void onRouteVolumeChanged(MediaRouter r, RouteInfo i) {}
    }
    public static abstract class VolumeCallback {
        public VolumeCallback() {}
        public abstract void onVolumeSetRequest(RouteInfo info, int volume);
        public abstract void onVolumeUpdateRequest(RouteInfo info, int direction);
    }

    public static class RouteCategory {
        CharSequence mName; int mTypes; boolean mGroupable, mSystem;
        RouteCategory(CharSequence name, int types, boolean groupable, boolean system) { mName = name; mTypes = types; mGroupable = groupable; mSystem = system; }
        protected RouteCategory() {}
        public CharSequence getName() { return mName; }
        public CharSequence getName(android.content.Context c) { return mName; }
        public List getRoutes(List out) { if (out == null) out = new ArrayList(); else out.clear(); synchronized (sRoutes) { for (RouteInfo r : sRoutes) if (r.mCategory == this) out.add(r); } return out; }
        public int getSupportedTypes() { return mTypes; }
        public boolean isGroupable() { return mGroupable; }
        public boolean isSystem() { return mSystem; }
    }

    public static class RouteInfo {
        public static final int DEVICE_TYPE_BLUETOOTH = 3, DEVICE_TYPE_SPEAKER = 2, DEVICE_TYPE_TV = 1, DEVICE_TYPE_UNKNOWN = 0;
        public static final int PLAYBACK_TYPE_LOCAL = 0, PLAYBACK_TYPE_REMOTE = 1, PLAYBACK_VOLUME_FIXED = 0, PLAYBACK_VOLUME_VARIABLE = 1;
        public static final int STATUS_AVAILABLE = 3, STATUS_CONNECTED = 6, STATUS_CONNECTING = 2, STATUS_IN_USE = 5, STATUS_NONE = 0, STATUS_NOT_AVAILABLE = 4, STATUS_SCANNING = 1;
        RouteCategory mCategory; CharSequence mName, mDescription, mStatus; Object mTag; RouteGroup mGroup;
        int mSupportedTypes = ROUTE_TYPE_USER, mDeviceType, mPlaybackType, mPlaybackStream = AudioManager.STREAM_MUSIC, mVolume, mVolumeMax, mVolumeHandling, mStatusCode;
        boolean mDefault, mEnabled = true;
        android.graphics.drawable.Drawable mIcon;
        public RouteInfo(RouteCategory c) { mCategory = c; }
        RouteInfo() { this(null); }
        public RouteCategory getCategory() { return mCategory; }
        public CharSequence getName() { return mName; }
        public CharSequence getName(android.content.Context c) { return mName; }
        public CharSequence getDescription() { return mDescription; }
        public CharSequence getStatus() { return mStatus; }
        public int getStatusCode() { return mStatusCode; }
        public int getSupportedTypes() { return mSupportedTypes; }
        public int getDeviceType() { return mDeviceType; }
        public String getDeviceAddress() { return null; }
        public RouteGroup getGroup() { return mGroup; }
        public android.graphics.drawable.Drawable getIconDrawable() { return mIcon; }
        public Object getTag() { return mTag; }
        public void setTag(Object t) { mTag = t; }
        public int getPlaybackType() { return mPlaybackType; }
        public int getPlaybackStream() { return mPlaybackStream; }
        public int getVolume() { return mVolume; }
        public int getVolumeMax() { return mVolumeMax; }
        public int getVolumeHandling() { return mVolumeHandling; }
        public android.view.Display getPresentationDisplay() { return null; }
        public android.view.Display[] getAllPresentationDisplays() { return new android.view.Display[0]; }
        public RouteInfo getDefaultAudioVideo() { return sDefault; }
        public boolean isEnabled() { return mEnabled; }
        public boolean isConnecting() { return false; }
        public boolean isDefault() { return mDefault; }
        public boolean isBluetooth() { return false; }
        public boolean isSelected() { return sSelected == this; }
        public boolean matchesTypes(int types) { return (mSupportedTypes & types) != 0; }
        public void requestSetVolume(int v) { mVolume = Math.max(0, Math.min(mVolumeMax, v)); }
        public void requestUpdateVolume(int d) { requestSetVolume(mVolume + d); }
        public void select() { sSelected = this; }
        public boolean updatePresentationDisplay() { return false; }
        @Override public String toString() { return "RouteInfo{ name=" + mName + " }"; }
    }
    public static class RouteGroup extends RouteInfo {
        private final ArrayList<RouteInfo> mRoutes = new ArrayList<>();
        RouteGroup(RouteCategory c) { super(c); }
        public void addRoute(RouteInfo r) { mRoutes.add(r); r.mGroup = this; }
        public void addRoute(RouteInfo r, int i) { mRoutes.add(i, r); r.mGroup = this; }
        public void removeRoute(RouteInfo r) { mRoutes.remove(r); }
        public void removeRoute(int i) { mRoutes.remove(i); }
        public int getRouteCount() { return mRoutes.size(); }
        public RouteInfo getRouteAt(int i) { return mRoutes.get(i); }
        public void setIconDrawable(android.graphics.drawable.Drawable d) { mIcon = d; }
        public void setIconResource(int r) {}
    }
    public static class UserRouteInfo extends RouteInfo {
        private Object mRcc; private VolumeCallback mVolumeCb;
        UserRouteInfo(RouteCategory c) { super(c); }
        public void setName(CharSequence n) { mName = n; }
        public void setName(int r) {}
        public void setDescription(CharSequence d) { mDescription = d; }
        public void setStatus(CharSequence s) { mStatus = s; }
        public void setIconDrawable(android.graphics.drawable.Drawable d) { mIcon = d; }
        public void setIconResource(int r) {}
        public void setPlaybackType(int t) { mPlaybackType = t; }
        public void setPlaybackStream(int s) { mPlaybackStream = s; }
        public void setVolume(int v) { mVolume = v; }
        public void setVolumeMax(int v) { mVolumeMax = v; }
        public void setVolumeHandling(int h) { mVolumeHandling = h; }
        public void setVolumeCallback(VolumeCallback cb) { mVolumeCb = cb; }
        public VolumeCallback getVolumeCallback() { return mVolumeCb; }
        public void setRemoteControlClient(RemoteControlClient c) { mRcc = c; }
        public RemoteControlClient getRemoteControlClient() { return (RemoteControlClient) mRcc; }
        public void requestSetVolume(int v) { if (mVolumeCb != null) mVolumeCb.onVolumeSetRequest(this, v); }
        public void requestUpdateVolume(int d) { if (mVolumeCb != null) mVolumeCb.onVolumeUpdateRequest(this, d); }
    }
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public void addRouteInt(android.media.MediaRouter.RouteInfo p0) {}
    public void rebindAsUser(int p0) {}
    public void removeRouteInt(android.media.MediaRouter.RouteInfo p0) {}
    public void selectRouteInt(int p0, android.media.MediaRouter.RouteInfo p1, boolean p2) {}
    public void setRouterGroupId(java.lang.String p0) {}
    // ---- end of generated members
}
