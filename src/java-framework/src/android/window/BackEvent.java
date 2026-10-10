package android.window;
public final class BackEvent {
    public static final int EDGE_LEFT = 0, EDGE_RIGHT = 1;
    private final float x, y, p; private final int edge;
    public BackEvent(float x, float y, float progress, int edge) { this.x = x; this.y = y; p = progress; this.edge = edge; }
    public float getTouchX() { return x; } public float getTouchY() { return y; } public float getProgress() { return p; } public int getSwipeEdge() { return edge; }
}
