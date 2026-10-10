package android.content.pm;
public class ServiceInfo extends ComponentInfo implements android.os.Parcelable {
    public static final int FLAG_STOP_WITH_TASK = 1, FLAG_ISOLATED_PROCESS = 2, FOREGROUND_SERVICE_TYPE_NONE = 0, FOREGROUND_SERVICE_TYPE_DATA_SYNC = 1, FOREGROUND_SERVICE_TYPE_MEDIA_PLAYBACK = 2,
        FOREGROUND_SERVICE_TYPE_PHONE_CALL = 4, FOREGROUND_SERVICE_TYPE_LOCATION = 8, FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE = 16, FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION = 32,
        FOREGROUND_SERVICE_TYPE_CAMERA = 64, FOREGROUND_SERVICE_TYPE_MICROPHONE = 128, FOREGROUND_SERVICE_TYPE_HEALTH = 256, FOREGROUND_SERVICE_TYPE_REMOTE_MESSAGING = 512,
        FOREGROUND_SERVICE_TYPE_SYSTEM_EXEMPTED = 1024, FOREGROUND_SERVICE_TYPE_SHORT_SERVICE = 2048, FOREGROUND_SERVICE_TYPE_SPECIAL_USE = 0x40000000, FOREGROUND_SERVICE_TYPE_MANIFEST = -1;
    public String permission; public int flags, foregroundServiceType;
    public ServiceInfo() {} public ServiceInfo(ServiceInfo o) { super(o); permission = o.permission; flags = o.flags; }
    public int getForegroundServiceType() { return foregroundServiceType; }
    public int describeContents() { return 0; }
}
