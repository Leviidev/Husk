package android.app;
public class UiModeManager {
    public static final int MODE_NIGHT_AUTO = 0, MODE_NIGHT_NO = 1, MODE_NIGHT_YES = 2, MODE_NIGHT_CUSTOM = 3;
    private int mNight = MODE_NIGHT_NO;
    public int getCurrentModeType() { return android.content.res.Configuration.UI_MODE_TYPE_NORMAL; }
    public int getNightMode() { return mNight; } public void setNightMode(int m) { mNight = m; } public void setApplicationNightMode(int m) { mNight = m; }
    public void enableCarMode(int f) {} public void disableCarMode(int f) {} public float getContrast() { return 0; }
}
