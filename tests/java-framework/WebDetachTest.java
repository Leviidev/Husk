import android.view.View;
import android.view.WindowManager;
import android.webkit.WebView;
import husk.ViewRoot;
import java.lang.reflect.Field;
import java.util.ArrayList;
import sun.misc.Unsafe;

/** A removed WebView must release its native page and touch region. */
public class WebDetachTest {
    static class Page extends WebView {
        boolean attachedDuringDetach;
        Page() { super(null); }
        @Override public void getLocationOnScreen(int[] out) { out[0] = 0; out[1] = 161; }
        @Override public boolean post(Runnable action) { bridge(action); return true; }
        @Override protected void onAttachedToWindow() { bridge(() -> super.onAttachedToWindow()); }
        @Override protected void onWindowVisibilityChanged(int visibility) {
            bridge(() -> super.onWindowVisibilityChanged(visibility));
        }
        @Override protected void onDetachedFromWindow() {
            attachedDuringDetach = isAttachedToWindow();
            bridge(() -> super.onDetachedFromWindow());
        }
        void attachCallback() { onAttachedToWindow(); }
        void windowVisibility(int visibility) { onWindowVisibilityChanged(visibility); }
    }

    // The host JDK has no WebKit bridge. WebView records its frame before this
    // native boundary; allow the lifecycle to finish so its final state is tested.
    static void bridge(Runnable action) {
        try { action.run(); }
        catch (UnsatisfiedLinkError error) {
            if (!error.getMessage().contains("husk.Web.frame(")) throw error;
        }
    }
    static Field field(Class<?> owner, String name) throws Exception {
        Field f = owner.getDeclaredField(name); f.setAccessible(true); return f;
    }
    static void check(Page page, boolean expected, String stage) throws Exception {
        boolean visible = field(WebView.class, "mFrameVisible").getBoolean(page);
        boolean touchable = field(WebView.class, "mFrameTouchable").getBoolean(page);
        if (visible != expected || touchable != expected)
            throw new AssertionError(stage + ": visible=" + visible + " touchable=" + touchable);
    }

    @SuppressWarnings("unchecked")
    public static void main(String[] args) throws Exception {
        Unsafe unsafe = (Unsafe)field(Unsafe.class, "theUnsafe").get(null);
        Page page = (Page)unsafe.allocateInstance(Page.class);
        ViewRoot root = (ViewRoot)unsafe.allocateInstance(ViewRoot.class);
        field(ViewRoot.class, "mView").set(root, page);
        field(ViewRoot.class, "mLp").set(root, new WindowManager.LayoutParams());
        ArrayList<ViewRoot> roots = (ArrayList<ViewRoot>)field(ViewRoot.class, "sRoots").get(null);
        roots.add(root);
        field(View.class, "mRoot").set(page, root);
        field(View.class, "mRight").setInt(page, 1206);
        field(View.class, "mBottom").setInt(page, 2205);
        try {
            page.attachCallback();
            check(page, true, "attached");
            page.windowVisibility(View.GONE);
            check(page, false, "window hidden before attachment is cleared");
            page.windowVisibility(View.VISIBLE);
            check(page, true, "window visible again");
            page.huskDetach();
            if (!page.attachedDuringDetach || page.isAttachedToWindow())
                throw new AssertionError("test must exercise attachment clearing after the detach callback");
            check(page, false, "removed page");
            field(View.class, "mRoot").set(page, root);
            page.attachCallback();
            check(page, true, "reused page attached again");
        } finally { roots.clear(); }
        System.out.println("PASS: hidden/detached web pages release native visibility and touches; reattachment restores them");
    }
}
