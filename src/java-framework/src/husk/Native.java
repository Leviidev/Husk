package husk;

/** What Husk's runtime does for the framework: logging, the app's files and assets, the screen, input, the host's keyboard and clipboard. */
public final class Native {
    private Native() {}
    public static native void log(int priority, String tag, String msg);
    public static native byte[] readAsset(String path);
    public static native String[] listAssets(String dir);
    public static native long assetFd(String path);          // fd << 40 | offset; -1 when compressed or missing
    public static native long assetLength(String path);
    /** A file in one of the APKs: 0 the app's, 1 the platform's resources (framework-res), 2.. the app's split APKs. */
    public static native byte[] readApkFile(int apk, String path);
    public static native long apkFileFd(int apk, String path);
    public static native long apkFileLength(int apk, String path);
    public static native String apkPath();
    /** The app's split APKs (a Google Play install: libraries, resources, asset packs), in the order they were given. */
    public static native String[] splitPaths();
    /** The NDK looper of this thread: made with the Java one, and its callbacks run between messages. */
    public static native void nativeLooperPrepare();
    public static native boolean nativeLooperPoll();
    /** Class.getClassLoader for the app's own classes. */
    public static native void setAppClassLoader(ClassLoader loader);
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
    public static native String[] pollText();                // typed text, deletes and actions from the keyboard
    public static native void showKeyboard(boolean show, int inputType, int imeOptions);
    public static native void setClipboard(String text);
    public static native String getClipboard();
    public static native void share(String text);
    public static native void setOrientation(int androidOrientation);
    public static native int[] insets();
    public static native boolean nightMode();                // the phone is in dark mode                     // left, top, right, bottom of the screen the app must keep clear
    public static native void vibrate(long ms);
    public static native void openUrl(String url);
    public static native void exit();
    public static native long uptimeNanos();
}
