package android.view;
public final class PointerIcon {
    public static final int TYPE_NULL = 0, TYPE_ARROW = 1000, TYPE_HAND = 1002, TYPE_TEXT = 1008, TYPE_DEFAULT = 1000, TYPE_CROSSHAIR = 1007, TYPE_GRAB = 1020, TYPE_GRABBING = 1021;
    private final int mType;
    private PointerIcon(int t) { mType = t; }
    public static PointerIcon getSystemIcon(android.content.Context c, int type) { return new PointerIcon(type); }
    public static PointerIcon create(android.graphics.Bitmap b, float x, float y) { return new PointerIcon(TYPE_ARROW); }
    public static PointerIcon load(android.content.res.Resources r, int id) { return new PointerIcon(TYPE_ARROW); }
    public int getType() { return mType; }
}
