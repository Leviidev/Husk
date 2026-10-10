package android.app;
public abstract class FragmentTransaction {
    public static final int TRANSIT_ENTER_MASK = 0x1000, TRANSIT_EXIT_MASK = 0x2000, TRANSIT_UNSET = -1, TRANSIT_NONE = 0, TRANSIT_FRAGMENT_OPEN = 0x1001,
        TRANSIT_FRAGMENT_CLOSE = 0x2002, TRANSIT_FRAGMENT_FADE = 0x1003;
    public abstract FragmentTransaction add(Fragment f, String tag); public abstract FragmentTransaction add(int c, Fragment f); public abstract FragmentTransaction add(int c, Fragment f, String tag);
    public abstract FragmentTransaction replace(int c, Fragment f); public abstract FragmentTransaction replace(int c, Fragment f, String tag); public abstract FragmentTransaction remove(Fragment f);
    public abstract FragmentTransaction hide(Fragment f); public abstract FragmentTransaction show(Fragment f); public abstract FragmentTransaction detach(Fragment f); public abstract FragmentTransaction attach(Fragment f);
    public abstract FragmentTransaction setPrimaryNavigationFragment(Fragment f); public abstract boolean isEmpty(); public abstract FragmentTransaction setCustomAnimations(int a, int b);
    public abstract FragmentTransaction setCustomAnimations(int a, int b, int c, int d); public abstract FragmentTransaction addSharedElement(android.view.View v, String n);
    public abstract FragmentTransaction setTransition(int t); public abstract FragmentTransaction setTransitionStyle(int s); public abstract FragmentTransaction addToBackStack(String name);
    public abstract boolean isAddToBackStackAllowed(); public abstract FragmentTransaction disallowAddToBackStack(); public abstract FragmentTransaction setBreadCrumbTitle(int r);
    public abstract FragmentTransaction setBreadCrumbTitle(CharSequence t); public abstract FragmentTransaction setBreadCrumbShortTitle(int r); public abstract FragmentTransaction setBreadCrumbShortTitle(CharSequence t);
    public abstract FragmentTransaction setReorderingAllowed(boolean r); public abstract FragmentTransaction runOnCommit(Runnable r); public abstract int commit(); public abstract int commitAllowingStateLoss();
    public abstract void commitNow(); public abstract void commitNowAllowingStateLoss();
}
