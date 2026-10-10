package android.app;
public class ActivityManager {
    public static class MemoryInfo implements android.os.Parcelable { public long availMem = 2L << 30, totalMem = 6L << 30, threshold = 256L << 20, advertisedMem = 6L << 30; public boolean lowMemory; public int describeContents() { return 0; } }
    public static class TaskDescription implements android.os.Parcelable {
        public TaskDescription() {} public TaskDescription(String label) {} public TaskDescription(String label, android.graphics.Bitmap icon) {} public TaskDescription(String label, int icon) {}
        public TaskDescription(String label, android.graphics.Bitmap icon, int color) {} public TaskDescription(String label, int icon, int color) {}
        public String getLabel() { return null; } public int describeContents() { return 0; }
        public static final class Builder { public Builder setLabel(String l) { return this; } public Builder setIcon(int i) { return this; } public Builder setPrimaryColor(int c) { return this; } public Builder setBackgroundColor(int c) { return this; } public TaskDescription build() { return new TaskDescription(); } }
    }
    public static class RunningAppProcessInfo implements android.os.Parcelable {
        public static final int IMPORTANCE_FOREGROUND = 100, IMPORTANCE_VISIBLE = 200, IMPORTANCE_SERVICE = 300, IMPORTANCE_CACHED = 400, IMPORTANCE_GONE = 1000;
        public String processName; public int pid, uid, importance = IMPORTANCE_FOREGROUND, lastTrimLevel; public String[] pkgList;
        public RunningAppProcessInfo() {} public RunningAppProcessInfo(String n, int p, String[] l) { processName = n; pid = p; pkgList = l; }
        public int describeContents() { return 0; }
    }
    public static class RunningTaskInfo { public android.content.ComponentName topActivity, baseActivity; public int numActivities = 1, id = 1; }
    public static class AppTask { public void finishAndRemoveTask() {} public void moveToFront() {} public RecentTaskInfo getTaskInfo() { return new RecentTaskInfo(); } }
    public static class RecentTaskInfo { public int id = 1; }
    public static class RunningServiceInfo {}
    public void getMemoryInfo(MemoryInfo m) { Runtime r = Runtime.getRuntime(); m.totalMem = 6L << 30; m.availMem = 2L << 30; m.threshold = 256L << 20; m.lowMemory = false; }
    public int getMemoryClass() { return 512; }
    public int getLargeMemoryClass() { return 512; }
    public boolean isLowRamDevice() { return false; }
    public static boolean isRunningInTestHarness() { return false; }
    public static boolean isUserAMonkey() { return false; }
    public static void getMyMemoryState(RunningAppProcessInfo out) { out.importance = RunningAppProcessInfo.IMPORTANCE_FOREGROUND; out.processName = husk.Native.packageName(); out.pid = android.os.Process.myPid(); }
    public java.util.List<RunningAppProcessInfo> getRunningAppProcesses() {
        java.util.ArrayList<RunningAppProcessInfo> l = new java.util.ArrayList<>();
        l.add(new RunningAppProcessInfo(husk.Native.packageName(), android.os.Process.myPid(), new String[] { husk.Native.packageName() }));
        return l;
    }
    public java.util.List<RunningTaskInfo> getRunningTasks(int max) { return new java.util.ArrayList<>(); }
    public java.util.List<AppTask> getAppTasks() { java.util.ArrayList<AppTask> l = new java.util.ArrayList<>(); l.add(new AppTask()); return l; }
    public java.util.List<RunningServiceInfo> getRunningServices(int max) { return new java.util.ArrayList<>(); }
    public android.os.Debug.MemoryInfo[] getProcessMemoryInfo(int[] pids) { android.os.Debug.MemoryInfo[] r = new android.os.Debug.MemoryInfo[pids.length]; for (int i = 0; i < r.length; i++) r[i] = new android.os.Debug.MemoryInfo(); return r; }
    public int getLauncherLargeIconSize() { return (int) (48 * husk.Native.density()); }
    public int getLauncherLargeIconDensity() { return (int) (husk.Native.density() * 160); }
    public void killBackgroundProcesses(String p) {}
    public boolean clearApplicationUserData() { return false; }
    public java.util.List<android.app.ApplicationExitInfo> getHistoricalProcessExitReasons(String p, int pid, int max) { return new java.util.ArrayList<>(); }
    public boolean isBackgroundRestricted() { return false; }
    public void setProcessStateSummary(byte[] s) {}
    public int getLockTaskModeState() { return 0; }
    public void moveTaskToFront(int id, int flags) {}
}
