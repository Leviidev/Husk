package android.view;
public class LayoutInflater {
    private final android.content.Context c;
    public LayoutInflater(android.content.Context c) { this.c = c; }
    public static LayoutInflater from(android.content.Context c) { return new LayoutInflater(c); }
    public View inflate(int res, ViewGroup root) { android.util.Log.w("Husk", "inflate(" + res + ") is not supported yet"); return new View(c); }
    public View inflate(int res, ViewGroup root, boolean attach) { return inflate(res, root); }
}
