import android.view.View;
import android.view.ViewGroup;
import android.webkit.WebView;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import sun.misc.Unsafe;

/** Web pages inside Android lists must let those lists take a drag. */
public class WebScrollTest {
    static class Page extends WebView {
        Page() { super(null); }
        @Override public boolean canScrollVertically(int direction) { return true; }
    }
    static class Parent extends ViewGroup {
        boolean left, right, up, down;
        Parent() { super(null); }
        @Override protected void onLayout(boolean c, int l, int t, int r, int b) {}
        @Override public boolean canScrollHorizontally(int d) { return d < 0 ? left : right; }
        @Override public boolean canScrollVertically(int d) { return d < 0 ? up : down; }
    }
    static void parent(View child, ViewGroup parent) throws Exception {
        Field f = View.class.getDeclaredField("mParent"); f.setAccessible(true); f.set(child, parent);
    }
    static void check(Method method, Page page, int expected) throws Exception {
        int actual = (Integer)method.invoke(page);
        if (actual != expected) throw new AssertionError("parent scroll axes " + actual + " expected " + expected);
    }
    public static void main(String[] args) throws Exception {
        Field f = Unsafe.class.getDeclaredField("theUnsafe"); f.setAccessible(true);
        Unsafe unsafe = (Unsafe)f.get(null);
        Page page = (Page)unsafe.allocateInstance(Page.class);
        Parent wrapper = (Parent)unsafe.allocateInstance(Parent.class);
        Parent list = (Parent)unsafe.allocateInstance(Parent.class);
        Method axes = WebView.class.getDeclaredMethod("parentScrollAxes"); axes.setAccessible(true);
        check(axes, page, 0);                         // a standalone page keeps its own scrolling
        parent(page, wrapper); parent(wrapper, list);
        check(axes, page, 0);                         // non-scrolling containers do not take drags
        list.down = true; check(axes, page, 2);       // list at its top
        list.down = false; list.up = true;
        check(axes, page, 2);                         // list at its bottom can still take a drag
        wrapper.left = true; check(axes, page, 3);    // independently scrolling nested ancestors
        list.up = false; check(axes, page, 1);        // horizontal parent, vertical page
        parent(page, null); check(axes, page, 0);      // detached/recycled list item
        System.out.println("PASS: parent list scroll axes, boundaries, nested directions, standalone and detached web pages");
    }
}
