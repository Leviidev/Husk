package husk;

/** What Husk's runtime does for the framework: logging, the app's files and assets, the screen, input, sound. */
public final class Native {
    private Native() {}
    public static native void log(int priority, String tag, String msg);
    public static native byte[] readAsset(String path);
    public static native String[] listAssets(String dir);
    public static native long assetFd(String path);          // fd << 32 | offset bits; -1 when compressed or missing
    public static native long assetLength(String path);
    public static native String dataDir();
    public static native String externalDir();
    public static native String packageName();
    public static native int screenWidth();
    public static native int screenHeight();
    public static native float density();
    public static native void startGL(Object view, Object renderer, int esVersion);
    public static native void requestRender();
    public static native void setContentView(Object view);
    public static native int[] pollInput();                  // packed touch events, or null
    public static native void vibrate(long ms);
    public static native void openUrl(String url);
    public static native void exit();
    public static native long uptimeNanos();
}
