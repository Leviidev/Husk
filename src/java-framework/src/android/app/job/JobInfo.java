package android.app.job;
public class JobInfo implements android.os.Parcelable {
    public static final int NETWORK_TYPE_NONE = 0, NETWORK_TYPE_ANY = 1, NETWORK_TYPE_UNMETERED = 2, NETWORK_TYPE_NOT_ROAMING = 3, NETWORK_TYPE_CELLULAR = 4, BACKOFF_POLICY_LINEAR = 0, BACKOFF_POLICY_EXPONENTIAL = 1, PRIORITY_DEFAULT = 300;
    public static final long DEFAULT_INITIAL_BACKOFF_MILLIS = 30000, MAX_BACKOFF_DELAY_MILLIS = 18000000;
    private final int mId; private final android.os.PersistableBundle mExtras;
    JobInfo(int id, android.os.PersistableBundle e) { mId = id; mExtras = e; }
    public int getId() { return mId; } public android.os.PersistableBundle getExtras() { return mExtras; } public android.content.ComponentName getService() { return null; }
    public static long getMinPeriodMillis() { return 900000; } public static long getMinFlexMillis() { return 300000; }
    public int describeContents() { return 0; }
    public static final class Builder {
        private final int id; private android.os.PersistableBundle extras = new android.os.PersistableBundle();
        public Builder(int id, android.content.ComponentName service) { this.id = id; }
        public Builder setExtras(android.os.PersistableBundle e) { extras = e; return this; } public Builder setTransientExtras(android.os.Bundle e) { return this; } public Builder setRequiredNetworkType(int t) { return this; }
        public Builder setRequiredNetwork(android.net.NetworkRequest r) { return this; } public Builder setRequiresCharging(boolean b) { return this; } public Builder setRequiresDeviceIdle(boolean b) { return this; }
        public Builder setRequiresBatteryNotLow(boolean b) { return this; } public Builder setRequiresStorageNotLow(boolean b) { return this; } public Builder addTriggerContentUri(Object u) { return this; }
        public Builder setTriggerContentUpdateDelay(long d) { return this; } public Builder setTriggerContentMaxDelay(long d) { return this; } public Builder setPeriodic(long i) { return this; } public Builder setPeriodic(long i, long f) { return this; }
        public Builder setMinimumLatency(long l) { return this; } public Builder setOverrideDeadline(long d) { return this; } public Builder setBackoffCriteria(long b, int p) { return this; } public Builder setImportantWhileForeground(boolean b) { return this; }
        public Builder setPrefetch(boolean b) { return this; } public Builder setPersisted(boolean b) { return this; } public Builder setExpedited(boolean b) { return this; } public Builder setEstimatedNetworkBytes(long d, long u) { return this; }
        public Builder setUserInitiated(boolean b) { return this; } public Builder setPriority(int p) { return this; }
        public JobInfo build() { return new JobInfo(id, extras); }
    }
}
