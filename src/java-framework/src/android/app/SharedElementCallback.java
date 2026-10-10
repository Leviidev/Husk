package android.app;
public abstract class SharedElementCallback {
    public interface OnSharedElementsReadyListener { void onSharedElementsReady(); }
    public void onSharedElementStart(java.util.List<String> n, java.util.List<android.view.View> e, java.util.List<android.view.View> s) {}
    public void onSharedElementEnd(java.util.List<String> n, java.util.List<android.view.View> e, java.util.List<android.view.View> s) {}
    public void onRejectSharedElements(java.util.List<android.view.View> r) {}
    public void onMapSharedElements(java.util.List<String> n, java.util.Map<String, android.view.View> m) {}
    public void onSharedElementsArrived(java.util.List<String> n, java.util.List<android.view.View> e, OnSharedElementsReadyListener l) { l.onSharedElementsReady(); }
}
