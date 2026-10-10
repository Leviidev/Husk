package android.view;
public abstract class ActionProvider {
    public interface VisibilityListener { void onActionProviderVisibilityChanged(boolean v); }
    public ActionProvider(android.content.Context c) {}
    @Deprecated public abstract View onCreateActionView();
    public View onCreateActionView(MenuItem item) { return onCreateActionView(); }
    public boolean overridesItemVisibility() { return false; } public boolean isVisible() { return true; } public void refreshVisibility() {}
    public boolean onPerformDefaultAction() { return false; } public boolean hasSubMenu() { return false; } public void onPrepareSubMenu(SubMenu s) {}
    public void setVisibilityListener(VisibilityListener l) {}
}
