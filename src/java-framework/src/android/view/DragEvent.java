package android.view;
public class DragEvent {
    public static final int ACTION_DRAG_STARTED = 1, ACTION_DRAG_LOCATION = 2, ACTION_DROP = 3, ACTION_DRAG_ENDED = 4, ACTION_DRAG_ENTERED = 5, ACTION_DRAG_EXITED = 6;
    public int getAction() { return 0; } public float getX() { return 0; } public float getY() { return 0; }
    public android.content.ClipData getClipData() { return null; } public android.content.ClipDescription getClipDescription() { return null; }
    public Object getLocalState() { return null; } public boolean getResult() { return false; }
}
