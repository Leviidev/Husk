package husk;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;

/** The inflater apps get: bare class names are looked for in android.widget, android.webkit and android.app, then android.view. */
public class PhoneLayoutInflater extends LayoutInflater {
    private static final String[] sClassPrefixList = { "android.widget.", "android.webkit.", "android.app." };
    public PhoneLayoutInflater(Context c) { super(c); }
    protected PhoneLayoutInflater(LayoutInflater original, Context c) { super(original, c); }
    @Override protected View onCreateView(String name, AttributeSet attrs) throws ClassNotFoundException {
        for (String prefix : sClassPrefixList) {
            try { View v = createView(name, prefix, attrs); if (v != null) return v; } catch (ClassNotFoundException e) {}
        }
        return super.onCreateView(name, attrs);
    }
    @Override public LayoutInflater cloneInContext(Context c) { return new PhoneLayoutInflater(this, c); }
}
