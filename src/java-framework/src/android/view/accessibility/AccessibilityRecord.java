package android.view.accessibility;
public class AccessibilityRecord {
    private final java.util.ArrayList<CharSequence> mText = new java.util.ArrayList<>();
    public void setSource(android.view.View v) {} public void setSource(android.view.View v, int id) {} public AccessibilityNodeInfo getSource() { return null; }
    public java.util.List<CharSequence> getText() { return mText; } public void setClassName(CharSequence c) {} public CharSequence getClassName() { return null; }
    public void setContentDescription(CharSequence c) {} public void setChecked(boolean b) {} public void setEnabled(boolean b) {} public void setPassword(boolean b) {}
    public void setScrollable(boolean b) {} public void setItemCount(int c) {} public void setCurrentItemIndex(int i) {} public void setFromIndex(int i) {} public void setToIndex(int i) {}
    public void setScrollX(int x) {} public void setScrollY(int y) {} public void setMaxScrollX(int x) {} public void setMaxScrollY(int y) {} public void setScrollDeltaX(int d) {} public void setScrollDeltaY(int d) {}
    public void setBeforeText(CharSequence t) {} public void setAddedCount(int c) {} public void setRemovedCount(int c) {} public int getItemCount() { return 0; } public void recycle() {}
}
