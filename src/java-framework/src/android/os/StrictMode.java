package android.os;

public final class StrictMode {
    public static final class ThreadPolicy {
        public static final ThreadPolicy LAX = new ThreadPolicy();
        public static final class Builder {
            public Builder() {} public Builder(ThreadPolicy p) {}
            public Builder permitAll() { return this; } public Builder detectAll() { return this; } public Builder penaltyLog() { return this; }
            public Builder permitNetwork() { return this; } public Builder permitDiskReads() { return this; } public Builder permitDiskWrites() { return this; }
            public ThreadPolicy build() { return LAX; }
        }
    }
    public static final class VmPolicy {
        public static final VmPolicy LAX = new VmPolicy();
        public static final class Builder {
            public Builder() {} public Builder(VmPolicy p) {}
            public Builder detectAll() { return this; } public Builder penaltyLog() { return this; } public VmPolicy build() { return LAX; }
        }
    }
    public static void setThreadPolicy(ThreadPolicy p) {}
    public static ThreadPolicy getThreadPolicy() { return ThreadPolicy.LAX; }
    public static ThreadPolicy allowThreadDiskReads() { return ThreadPolicy.LAX; }
    public static ThreadPolicy allowThreadDiskWrites() { return ThreadPolicy.LAX; }
    public static void setVmPolicy(VmPolicy p) {}
    public static VmPolicy getVmPolicy() { return VmPolicy.LAX; }
    public static void disableDeathOnFileUriExposure() {}
    public static void enableDefaults() {}
}
