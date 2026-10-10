package android.view;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import android.util.AttributeSet;
import android.util.Xml;
import java.lang.reflect.Constructor;
import java.util.HashMap;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

/** Layouts (compiled XML) into views, as Android's LayoutInflater: factories first, then the class by name with (Context, AttributeSet). */
public abstract class LayoutInflater {
    public interface Factory { View onCreateView(String name, Context context, AttributeSet attrs); }
    public interface Factory2 extends Factory { View onCreateView(View parent, String name, Context context, AttributeSet attrs); }
    public interface Filter { boolean onLoadClass(Class clazz); }
    private static final String TAG_MERGE = "merge", TAG_INCLUDE = "include", TAG_1995 = "blink", TAG_REQUEST_FOCUS = "requestFocus", TAG_TAG = "tag";
    private static final int[] ATTRS_THEME = { android.R.attr.theme };
    private static final HashMap<String, Constructor<? extends View>> sConstructors = new HashMap<>();
    static final Class<?>[] mConstructorSignature = { Context.class, AttributeSet.class };

    protected final Context mContext;
    private Factory mFactory;
    private Factory2 mFactory2, mPrivateFactory;
    private boolean mFactorySet;
    private Filter mFilter;
    final Object[] mConstructorArgs = new Object[2];

    protected LayoutInflater(Context c) { mContext = c; }
    protected LayoutInflater(LayoutInflater original, Context newContext) {
        mContext = newContext;
        mFactory = original.mFactory; mFactory2 = original.mFactory2; mPrivateFactory = original.mPrivateFactory; mFilter = original.mFilter;
    }
    public static LayoutInflater from(Context c) {
        LayoutInflater li = (LayoutInflater) c.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
        if (li == null) throw new AssertionError("LayoutInflater not found.");
        return li;
    }
    public abstract LayoutInflater cloneInContext(Context c);
    public Context getContext() { return mContext; }
    public final Factory getFactory() { return mFactory; }
    public final Factory2 getFactory2() { return mFactory2; }
    public void setFactory(Factory f) {
        if (mFactorySet) throw new IllegalStateException("A factory has already been set on this LayoutInflater");
        if (f == null) throw new NullPointerException("Given factory can not be null");
        mFactorySet = true;
        mFactory = mFactory == null ? f : new FactoryMerger(f, null, mFactory, mFactory2);
    }
    public void setFactory2(Factory2 f) {
        if (mFactorySet) throw new IllegalStateException("A factory has already been set on this LayoutInflater");
        if (f == null) throw new NullPointerException("Given factory can not be null");
        mFactorySet = true;
        if (mFactory == null) { mFactory = mFactory2 = f; }
        else { mFactory = mFactory2 = new FactoryMerger(f, f, mFactory, mFactory2); }
    }
    public void setPrivateFactory(Factory2 f) { mPrivateFactory = mPrivateFactory == null ? f : new FactoryMerger(f, f, mPrivateFactory, mPrivateFactory); }
    public Filter getFilter() { return mFilter; }
    public void setFilter(Filter f) { mFilter = f; }
    private static class FactoryMerger implements Factory2 {
        private final Factory f1, f2; private final Factory2 f12, f22;
        FactoryMerger(Factory f1, Factory2 f12, Factory f2, Factory2 f22) { this.f1 = f1; this.f2 = f2; this.f12 = f12; this.f22 = f22; }
        public View onCreateView(String name, Context c, AttributeSet a) { View v = f1.onCreateView(name, c, a); return v != null ? v : f2.onCreateView(name, c, a); }
        public View onCreateView(View parent, String name, Context c, AttributeSet a) {
            View v = f12 != null ? f12.onCreateView(parent, name, c, a) : f1.onCreateView(name, c, a);
            if (v != null) return v;
            return f22 != null ? f22.onCreateView(parent, name, c, a) : f2.onCreateView(name, c, a);
        }
    }

    public View inflate(int resource, ViewGroup root) { return inflate(resource, root, root != null); }
    public View inflate(XmlPullParser parser, ViewGroup root) { return inflate(parser, root, root != null); }
    public View inflate(int resource, ViewGroup root, boolean attachToRoot) {
        Resources res = getContext().getResources();
        XmlResourceParser parser = res.getLayout(resource);
        try { return inflate(parser, root, attachToRoot); } finally { parser.close(); }
    }
    public View inflate(XmlPullParser parser, ViewGroup root, boolean attachToRoot) {
        synchronized (mConstructorArgs) {
            Context inflaterContext = mContext;
            AttributeSet attrs = Xml.asAttributeSet(parser);
            Context last = (Context) mConstructorArgs[0];
            mConstructorArgs[0] = inflaterContext;
            View result = root;
            try {
                int type;
                while ((type = parser.next()) != XmlPullParser.START_TAG && type != XmlPullParser.END_DOCUMENT) {}
                if (type != XmlPullParser.START_TAG) throw new InflateException(parser.getPositionDescription() + ": No start tag found!");
                String name = parser.getName();
                if (TAG_MERGE.equals(name)) {
                    if (root == null || !attachToRoot) throw new InflateException("<merge /> can be used only with a valid ViewGroup root and attachToRoot=true");
                    rInflate(parser, root, inflaterContext, attrs, false);
                } else {
                    View temp = createViewFromTag(root, name, inflaterContext, attrs);
                    ViewGroup.LayoutParams params = null;
                    if (root != null) {
                        params = root.generateLayoutParams(attrs);
                        if (!attachToRoot) temp.setLayoutParams(params);
                    }
                    rInflateChildren(parser, temp, attrs, true);
                    if (root != null && attachToRoot) root.addView(temp, params);
                    if (root == null || !attachToRoot) result = temp;
                }
            } catch (XmlPullParserException e) {
                InflateException ie = new InflateException(e.getMessage()); ie.initCause(e); throw ie;
            } catch (InflateException e) { throw e;
            } catch (Exception e) {
                InflateException ie = new InflateException(parser.getPositionDescription() + ": " + e.getMessage()); ie.initCause(e); throw ie;
            } finally {
                mConstructorArgs[0] = last;
                mConstructorArgs[1] = null;
            }
            return result;
        }
    }

    public final View createView(String name, String prefix, AttributeSet attrs) throws ClassNotFoundException, InflateException {
        Context c = (Context) mConstructorArgs[0];
        if (c == null) c = mContext;
        return createView(c, name, prefix, attrs);
    }
    public final View createView(Context viewContext, String name, String prefix, AttributeSet attrs) throws ClassNotFoundException, InflateException {
        String full = prefix != null ? prefix + name : name;
        Constructor<? extends View> ctor;
        synchronized (sConstructors) { ctor = sConstructors.get(full); }
        try {
            if (ctor == null) {
                Class<? extends View> clazz = Class.forName(full, false, mContext.getClassLoader()).asSubclass(View.class);
                if (mFilter != null && !mFilter.onLoadClass(clazz)) throw new InflateException(attrs.getPositionDescription() + ": Class not allowed to be inflated " + full);
                ctor = clazz.getConstructor(mConstructorSignature);
                ctor.setAccessible(true);
                synchronized (sConstructors) { sConstructors.put(full, ctor); }
            }
            Object last = mConstructorArgs[0];
            mConstructorArgs[0] = viewContext;
            mConstructorArgs[1] = attrs;
            try {
                View v = ctor.newInstance(viewContext, attrs);
                if (v instanceof ViewStub) ((ViewStub) v).setLayoutInflater(cloneInContext(viewContext));
                return v;
            } finally { mConstructorArgs[0] = last; }
        } catch (NoSuchMethodException e) {
            InflateException ie = new InflateException(attrs.getPositionDescription() + ": Error inflating class " + full); ie.initCause(e); throw ie;
        } catch (ClassCastException e) {
            InflateException ie = new InflateException(attrs.getPositionDescription() + ": Class is not a View " + full); ie.initCause(e); throw ie;
        } catch (ClassNotFoundException e) { throw e;
        } catch (java.lang.reflect.InvocationTargetException e) {
            Throwable t = e.getCause();
            InflateException ie = new InflateException(attrs.getPositionDescription() + ": Error inflating class " + full + (t != null ? ": " + t : "")); ie.initCause(t != null ? t : e); throw ie;
        } catch (Exception e) {
            InflateException ie = new InflateException(attrs.getPositionDescription() + ": Error inflating class " + full + ": " + e); ie.initCause(e); throw ie;
        }
    }
    protected View onCreateView(String name, AttributeSet attrs) throws ClassNotFoundException { return createView(name, "android.view.", attrs); }
    protected View onCreateView(View parent, String name, AttributeSet attrs) throws ClassNotFoundException { return onCreateView(name, attrs); }
    public View onCreateView(Context viewContext, View parent, String name, AttributeSet attrs) throws ClassNotFoundException { return onCreateView(parent, name, attrs); }

    private View createViewFromTag(View parent, String name, Context context, AttributeSet attrs) { return createViewFromTag(parent, name, context, attrs, false); }
    View createViewFromTag(View parent, String name, Context context, AttributeSet attrs, boolean ignoreThemeAttr) {
        if ("view".equals(name)) name = attrs.getAttributeValue(null, "class");
        if (!ignoreThemeAttr) {
            TypedArray ta = context.obtainStyledAttributes(attrs, ATTRS_THEME);
            int themeResId = ta.getResourceId(0, 0);
            ta.recycle();
            if (themeResId != 0) context = new ContextThemeWrapper(context, themeResId);
        }
        try {
            View view = tryCreateView(parent, name, context, attrs);
            if (view == null) {
                Object last = mConstructorArgs[0];
                mConstructorArgs[0] = context;
                try {
                    if (name.indexOf('.') == -1) view = onCreateView(context, parent, name, attrs);
                    else view = createView(context, name, null, attrs);
                } finally { mConstructorArgs[0] = last; }
            }
            return view;
        } catch (InflateException e) { throw e;
        } catch (ClassNotFoundException e) {
            InflateException ie = new InflateException(attrs.getPositionDescription() + ": Error inflating class " + name); ie.initCause(e); throw ie;
        } catch (Exception e) {
            InflateException ie = new InflateException(attrs.getPositionDescription() + ": Error inflating class " + name + ": " + e); ie.initCause(e); throw ie;
        }
    }
    public final View tryCreateView(View parent, String name, Context context, AttributeSet attrs) {
        if (TAG_1995.equals(name)) return new BlinkLayout(context, attrs);
        View view;
        if (mFactory2 != null) view = mFactory2.onCreateView(parent, name, context, attrs);
        else if (mFactory != null) view = mFactory.onCreateView(name, context, attrs);
        else view = null;
        if (view == null && mPrivateFactory != null) view = mPrivateFactory.onCreateView(parent, name, context, attrs);
        return view;
    }
    final void rInflateChildren(XmlPullParser parser, View parent, AttributeSet attrs, boolean finishInflate) throws Exception { rInflate(parser, parent, parent.getContext(), attrs, finishInflate); }
    void rInflate(XmlPullParser parser, View parent, Context context, AttributeSet attrs, boolean finishInflate) throws Exception {
        int depth = parser.getDepth(), type;
        boolean pendingRequestFocus = false;
        while (((type = parser.next()) != XmlPullParser.END_TAG || parser.getDepth() > depth) && type != XmlPullParser.END_DOCUMENT) {
            if (type != XmlPullParser.START_TAG) continue;
            String name = parser.getName();
            if (TAG_REQUEST_FOCUS.equals(name)) { pendingRequestFocus = true; consumeChildElements(parser); }
            else if (TAG_TAG.equals(name)) { parseViewTag(parser, parent, attrs); }
            else if (TAG_INCLUDE.equals(name)) {
                if (parser.getDepth() == 0) throw new InflateException("<include /> cannot be the root element");
                parseInclude(parser, context, parent, attrs);
            } else if (TAG_MERGE.equals(name)) throw new InflateException("<merge /> must be the root element");
            else {
                View view = createViewFromTag(parent, name, context, attrs);
                ViewGroup viewGroup = (ViewGroup) parent;
                ViewGroup.LayoutParams params = viewGroup.generateLayoutParams(attrs);
                rInflateChildren(parser, view, attrs, true);
                viewGroup.addView(view, params);
            }
        }
        if (pendingRequestFocus) parent.restoreDefaultFocus();
        if (finishInflate) parent.onFinishInflate();
    }
    private void parseViewTag(XmlPullParser parser, View view, AttributeSet attrs) throws Exception {
        int[] a = { android.R.attr.id, android.R.attr.value };
        TypedArray ta = view.getContext().obtainStyledAttributes(attrs, a);
        int key = ta.getResourceId(0, 0);
        CharSequence value = ta.getText(1);
        view.setTag(key, value);
        ta.recycle();
        consumeChildElements(parser);
    }
    private void parseInclude(XmlPullParser parser, Context context, View parent, AttributeSet attrs) throws Exception {
        if (!(parent instanceof ViewGroup)) throw new InflateException("<include /> can only be used inside of a ViewGroup");
        TypedArray ta = context.obtainStyledAttributes(attrs, ATTRS_THEME);
        int themeResId = ta.getResourceId(0, 0);
        ta.recycle();
        if (themeResId != 0) context = new ContextThemeWrapper(context, themeResId);
        int layout = attrs.getAttributeResourceValue(null, "layout", 0);
        if (layout == 0) {
            String value = attrs.getAttributeValue(null, "layout");
            if (value == null || value.length() <= 0) throw new InflateException("You must specify a layout in the include tag: <include layout=\"@layout/layoutID\" />");
            layout = context.getResources().getIdentifier(value.substring(1), "attr", context.getPackageName());
            if (layout != 0) {
                android.util.TypedValue v = new android.util.TypedValue();
                if (context.getTheme().resolveAttribute(layout, v, true)) layout = v.resourceId != 0 ? v.resourceId : v.data;
            }
        }
        if (layout == 0) throw new InflateException("You must specify a valid layout reference. The layout ID " + attrs.getAttributeValue(null, "layout") + " is not valid.");
        XmlResourceParser childParser = context.getResources().getLayout(layout);
        try {
            AttributeSet childAttrs = Xml.asAttributeSet(childParser);
            int type;
            while ((type = childParser.next()) != XmlPullParser.START_TAG && type != XmlPullParser.END_DOCUMENT) {}
            if (type != XmlPullParser.START_TAG) throw new InflateException(childParser.getPositionDescription() + ": No start tag found!");
            String childName = childParser.getName();
            if (TAG_MERGE.equals(childName)) rInflate(childParser, parent, context, childAttrs, false);
            else {
                View view = createViewFromTag(parent, childName, context, childAttrs);
                ViewGroup group = (ViewGroup) parent;
                int[] ia = { android.R.attr.id, android.R.attr.visibility };
                TypedArray a = context.obtainStyledAttributes(attrs, ia);
                int id = a.getResourceId(0, View.NO_ID);
                int visibility = a.getInt(1, -1);
                a.recycle();
                ViewGroup.LayoutParams params = null;
                try { params = group.generateLayoutParams(attrs); } catch (RuntimeException e) {}
                if (params == null) params = group.generateLayoutParams(childAttrs);
                view.setLayoutParams(params);
                rInflateChildren(childParser, view, childAttrs, true);
                if (id != View.NO_ID) view.setId(id);
                switch (visibility) { case 0: view.setVisibility(View.VISIBLE); break; case 1: view.setVisibility(View.INVISIBLE); break; case 2: view.setVisibility(View.GONE); break; }
                group.addView(view);
            }
        } finally { childParser.close(); }
        consumeChildElements(parser);
    }
    static void consumeChildElements(XmlPullParser parser) throws Exception {
        int depth = parser.getDepth(), type;
        while (((type = parser.next()) != XmlPullParser.END_TAG || parser.getDepth() > depth) && type != XmlPullParser.END_DOCUMENT) {}
    }
    private static class BlinkLayout extends android.widget.FrameLayout { BlinkLayout(Context c, AttributeSet a) { super(c, a); } }
}
