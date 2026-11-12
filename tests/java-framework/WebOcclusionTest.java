import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import java.lang.reflect.Field;
import java.util.ArrayList;
import sun.misc.Unsafe;

/** Host regression: native web pages yield only the regions covered by Android controls. */
public class WebOcclusionTest {
    static Unsafe unsafe;
    static class Button extends View {
        Rect bounds; boolean visible = true; float z;
        Button() { super(null); }
        @Override public int getVisibility() { return visible ? VISIBLE : GONE; }
        @Override public boolean isClickable() { return true; }
        @Override public float getZ() { return z; }
        @Override public boolean getGlobalVisibleRect(Rect out, android.graphics.Point offset) { out.set(bounds); return true; }
        @Override public void getLocationOnScreen(int[] out) { out[0] = 0; out[1] = 60; }
        @Override public void getLocationInWindow(int[] out) { out[0] = out[1] = 0; }
    }
    static class Group extends ViewGroup {
        Group() { super(null); }
        @Override protected void onLayout(boolean c, int l, int t, int r, int b) {}
    }
    static void set(Object o, Class<?> cls, String name, Object value) throws Exception {
        Field f = cls.getDeclaredField(name); f.setAccessible(true); f.set(o, value);
    }
    static Group group(View... children) throws Exception {
        Group g = (Group)unsafe.allocateInstance(Group.class);
        set(g, ViewGroup.class, "mChildren", children);
        set(g, ViewGroup.class, "mChildrenCount", children.length);
        return g;
    }
    static Button button() throws Exception {
        Button v = (Button)unsafe.allocateInstance(Button.class);
        v.bounds = new Rect(0, 0, 48, 48); v.visible = true;
        return v;
    }
    static void check(Group group, View web, int count) {
        ArrayList<Rect> rects = new ArrayList<>();
        group.huskTouchOccluders(web, rects);
        if (rects.size() != count) throw new AssertionError("regions " + rects.size() + " expected " + count);
        if (count > 0 && !rects.get(0).equals(new Rect(0, 60, 48, 108))) throw new AssertionError("screen coordinates " + rects);
    }
    public static void main(String[] args) throws Exception {
        Field f = Unsafe.class.getDeclaredField("theUnsafe"); f.setAccessible(true); unsafe = (Unsafe)f.get(null);
        View web = (View)unsafe.allocateInstance(View.class);
        Button close = button();
        Group popup = group(web, group(), close);
        check(popup, web, 1);                 // transparent overlay is not an input blocker
        close.visible = false; check(popup, web, 0);
        close.visible = true;
        check(group(close, web), web, 0);    // controls behind the page do not block it
        check(group(web, group(close)), web, 1);
        close.z = -1; check(popup, web, 0);   // Z order agrees with Android touch dispatch
        close.z = 1; check(group(close, web), web, 1);
        System.out.println("PASS: visible/nested/hidden/behind/elevated controls and window offset");
    }
}
