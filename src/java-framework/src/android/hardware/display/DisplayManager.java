package android.hardware.display;
public final class DisplayManager {
    public static final String DISPLAY_CATEGORY_PRESENTATION = "android.hardware.display.category.PRESENTATION";
    public interface DisplayListener { void onDisplayAdded(int id); void onDisplayRemoved(int id); void onDisplayChanged(int id); }
    private static final android.view.Display sD = new android.view.Display();
    public android.view.Display getDisplay(int id) { return id == 0 ? sD : null; }
    public android.view.Display[] getDisplays() { return new android.view.Display[] { sD }; }
    public android.view.Display[] getDisplays(String cat) { return cat == null ? getDisplays() : new android.view.Display[0]; }
    public void registerDisplayListener(DisplayListener l, android.os.Handler h) {}
    public void unregisterDisplayListener(DisplayListener l) {}
}
