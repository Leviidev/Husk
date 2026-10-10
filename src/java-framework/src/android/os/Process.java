package android.os;

public class Process {
    public static final int THREAD_PRIORITY_DEFAULT = 0, THREAD_PRIORITY_BACKGROUND = 10, THREAD_PRIORITY_DISPLAY = -4,
        THREAD_PRIORITY_URGENT_DISPLAY = -8, THREAD_PRIORITY_AUDIO = -16, THREAD_PRIORITY_URGENT_AUDIO = -19, THREAD_PRIORITY_FOREGROUND = -2;
    public static int myPid() { return 4242; }
    public static int myTid() { return 4242; }
    public static int myUid() { return 10100; }
    public static void setThreadPriority(int p) {}
    public static void setThreadPriority(int tid, int p) {}
    public static int getThreadPriority(int tid) { return 0; }
    public static void killProcess(int pid) { husk.Native.exit(); }
    public static boolean is64Bit() { return true; }
}
