package android.app;

import android.content.Context;
import android.content.DialogInterface;
import android.os.Bundle;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import java.lang.reflect.Field;
import sun.misc.Unsafe;

/** Real framework lifecycle/transactions with a host window double, no game credentials. */
public final class DialogFragmentTest {
    static void check(boolean condition, String message) { if (!condition) throw new AssertionError(message); }
    static final class Inflater extends LayoutInflater {
        Inflater(Context c) { super(c); }
        public LayoutInflater cloneInContext(Context c) { return new Inflater(c); }
    }
    static final class Host extends Activity {
        Group root;
        public LayoutInflater getLayoutInflater() { return new Inflater(this); }
        public <T extends View> T findViewById(int id) { return root == null ? null : root.findViewById(id); }
    }
    static final class Group extends ViewGroup {
        Group(Context c) { super(c); }
        protected void onLayout(boolean changed, int l, int t, int r, int b) {}
        public void requestLayout() {}
        public void invalidate() {}
    }
    static final class TestDialog extends Dialog {
        View content; boolean showing, cancelable; int shows, dismisses;
        DialogInterface.OnDismissListener listener;
        Host host;
        private TestDialog(Host h) { super(h); }
        public LayoutInflater getLayoutInflater() { return host.getLayoutInflater(); }
        public void setContentView(View v) { content = v; }
        public void setCancelable(boolean c) { cancelable = c; }
        public void setOnCancelListener(DialogInterface.OnCancelListener l) {}
        public void setOnDismissListener(DialogInterface.OnDismissListener l) { listener = l; }
        public void show() { showing = true; shows++; }
        public void hide() { showing = false; }
        public void dismiss() { showing = false; dismisses++; if (listener != null) listener.onDismiss(this); }
        public Bundle onSaveInstanceState() { return new Bundle(); }
    }
    static TestDialog window(Host host) {
        try {
            Field field = Unsafe.class.getDeclaredField("theUnsafe"); field.setAccessible(true);
            TestDialog d = (TestDialog) ((Unsafe) field.get(null)).allocateInstance(TestDialog.class);
            d.host = host;
            return d;
        } catch (Exception e) { throw new AssertionError(e); }
    }
    static final class Auth extends DialogFragment {
        TestDialog dialog; int creates, starts, destroys, dismissed; Group content;
        public Dialog onCreateDialog(Bundle saved) { creates++; return dialog = window((Host) getActivity()); }
        public View onCreateView(LayoutInflater i, ViewGroup container, Bundle saved) {
            check(getDialog() != null, "dialog must exist before inflating auth view");
            content = new Group(getActivity()); content.setId(40); return content;
        }
        public void onStart() { super.onStart(); starts++; }
        public void onDestroy() { super.onDestroy(); destroys++; }
        public void onDismiss(DialogInterface d) { super.onDismiss(d); dismissed++; }
    }
    static final class Login extends Fragment {
        int starts, destroys;
        public View onCreateView(LayoutInflater i, ViewGroup parent, Bundle b) { return new View(getActivity()); }
        public void onStart() { super.onStart(); starts++; }
        public void onDestroy() { super.onDestroy(); destroys++; }
    }
    static final class Inline extends DialogFragment {
        public Dialog onCreateDialog(Bundle saved) { throw new AssertionError("embedded fragment must not create a dialog"); }
        public View onCreateView(LayoutInflater i, ViewGroup parent, Bundle saved) { return new View(getActivity()); }
    }
    public static void main(String[] args) {
        Host host = new Host(); FragmentManager manager = new FragmentManager(host);
        manager.huskHostState(FragmentManager.HOST_RESUMED);
        Auth auth = new Auth();
        check(auth.isCancelable() && auth.getShowsDialog(), "Android dialog defaults");
        auth.show(manager, "AUTH_DIALOG");
        check(manager.findFragmentByTag("AUTH_DIALOG") == null, "show queues a transaction");
        Handler.drain();
        check(manager.findFragmentByTag("AUTH_DIALOG") == auth, "show must add fragment");
        check(auth.creates == 1 && auth.starts == 1 && auth.dialog.showing, "auth dialog lifecycle runs");
        check(auth.dialog.content == auth.content && auth.dialog.getOwnerActivity() == host, "content and owner attached");
        auth.setCancelable(false); check(!auth.dialog.cancelable, "cancelable propagates");
        Login login = new Login(); FragmentManager children = auth.getChildFragmentManager();
        check(children != manager, "login child manager must not replace the activity fragments");
        children.beginTransaction().replace(40, login, "LOGIN").commit(); Handler.drain();
        check(login.getParentFragment() == auth && login.getView().getParent() == auth.content, "child attaches inside dialog");
        check(login.starts == 1 && manager.findFragmentByTag("AUTH_DIALOG") == auth, "auth parent survives child transaction");
        manager.huskHostState(FragmentManager.HOST_CREATED);
        check(!auth.dialog.showing && !login.mStarted, "parent stop stops login and hides dialog");
        manager.huskHostState(FragmentManager.HOST_RESUMED);
        check(auth.dialog.showing && login.starts == 2, "resume restores dialog and child");
        TestDialog first = auth.dialog;
        auth.dismiss(); Handler.drain();
        check(manager.findFragmentByTag("AUTH_DIALOG") == null && !first.showing, "dismiss removes auth window and fragment");
        check(login.destroys == 1 && auth.destroys == 1 && auth.dismissed == 1, "dismiss destroys child once, callback once");
        auth.dismiss(); Handler.drain(); check(auth.destroys == 1, "repeated dismiss is idempotent");
        Auth cancelled = new Auth(); cancelled.show(manager, "CANCEL"); Handler.drain();
        cancelled.dialog.dismiss(); Handler.drain();
        check(!cancelled.isAdded() && cancelled.dismissed == 1, "window dismissal removes fragment once");
        Auth stopped = new Auth(); stopped.show(manager, "STOP"); Handler.drain();
        host.root = new Group(host); host.root.setId(50);
        Inline inline = new Inline(); inline.setShowsDialog(false);
        manager.beginTransaction().add(50, inline, "INLINE").commit(); Handler.drain();
        check(inline.getDialog() == null && inline.getView().getParent() == host.root, "embedded DialogFragment keeps its container");
        manager.beginTransaction().remove(inline).commit(); Handler.drain();
        manager.huskHostState(FragmentManager.HOST_NONE);
        check(!stopped.isAdded() && stopped.getDialog() == null && stopped.destroys == 1, "host teardown removes dialogs");
        System.out.println("PASS: production dialog show/dismiss, auth child containers, pause/resume and host teardown");
    }
}
