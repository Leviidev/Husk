package android.view;

import android.content.Context;
import android.content.res.TypedArray;
import android.content.res.XmlResourceParser;
import org.xmlpull.v1.XmlPullParser;

/** Menus from XML (res/menu) into a Menu: items, groups and submenus with their titles, icons, ids and flags. */
public class MenuInflater {
    private final Context mContext;
    public MenuInflater(Context c) { mContext = c; }
    public void inflate(int res, Menu menu) {
        XmlResourceParser p = mContext.getResources().getLayout(res);
        try { parse(p, menu); } catch (Exception e) { throw new InflateException("Error inflating menu XML", e); } finally { p.close(); }
    }
    private void parse(XmlPullParser p, Menu menu) throws Exception {
        int[] itemAttrs = sorted(android.R.attr.id, android.R.attr.title, android.R.attr.icon, android.R.attr.orderInCategory, android.R.attr.checkable, android.R.attr.checked,
                                 android.R.attr.visible, android.R.attr.enabled, android.R.attr.showAsAction, android.R.attr.titleCondensed, android.R.attr.menuCategory);
        int[] groupAttrs = sorted(android.R.attr.id, android.R.attr.checkableBehavior, android.R.attr.visible, android.R.attr.enabled, android.R.attr.orderInCategory);
        java.util.ArrayDeque<Menu> menus = new java.util.ArrayDeque<>();
        menus.push(menu);
        int group = 0, groupCheck = 0;
        boolean groupVisible = true, groupEnabled = true;
        MenuItem last = null;
        int t;
        while ((t = p.next()) != XmlPullParser.END_DOCUMENT) {
            if (t == XmlPullParser.START_TAG) {
                String n = p.getName();
                if ("group".equals(n)) {
                    TypedArray a = mContext.obtainStyledAttributes(android.util.Xml.asAttributeSet(p), groupAttrs);
                    group = a.getResourceId(idx(groupAttrs, android.R.attr.id), 0);
                    groupCheck = a.getInt(idx(groupAttrs, android.R.attr.checkableBehavior), 0);
                    groupVisible = a.getBoolean(idx(groupAttrs, android.R.attr.visible), true);
                    groupEnabled = a.getBoolean(idx(groupAttrs, android.R.attr.enabled), true);
                    a.recycle();
                } else if ("item".equals(n)) {
                    android.util.AttributeSet set = android.util.Xml.asAttributeSet(p);
                    TypedArray a = mContext.obtainStyledAttributes(set, itemAttrs);
                    int id = a.getResourceId(idx(itemAttrs, android.R.attr.id), 0);
                    CharSequence title = a.getText(idx(itemAttrs, android.R.attr.title));
                    int order = a.getInt(idx(itemAttrs, android.R.attr.orderInCategory), 0);
                    MenuItem item = menus.peek().add(group, id, order, title != null ? title : "");
                    item.setIcon(a.getDrawable(idx(itemAttrs, android.R.attr.icon)));
                    item.setCheckable(a.getBoolean(idx(itemAttrs, android.R.attr.checkable), groupCheck != 0));
                    item.setChecked(a.getBoolean(idx(itemAttrs, android.R.attr.checked), false));
                    item.setVisible(a.getBoolean(idx(itemAttrs, android.R.attr.visible), groupVisible));
                    item.setEnabled(a.getBoolean(idx(itemAttrs, android.R.attr.enabled), groupEnabled));
                    int show = a.getInt(idx(itemAttrs, android.R.attr.showAsAction), -1);
                    if (show < 0) { String v = set.getAttributeValue("http://schemas.android.com/apk/res-auto", "showAsAction"); if (v != null) try { show = Integer.decode(v); } catch (NumberFormatException e) {} }
                    if (show >= 0) item.setShowAsAction(show);
                    a.recycle();
                    last = item;
                } else if ("menu".equals(n) && last != null) {
                    SubMenu sm = ((husk.MenuImpl.Item) last).huskSubMenu();
                    menus.push(sm);
                }
            } else if (t == XmlPullParser.END_TAG) {
                String n = p.getName();
                if ("group".equals(n)) { group = 0; groupCheck = 0; groupVisible = true; groupEnabled = true; }
                else if ("menu".equals(n) && menus.size() > 1) menus.pop();
            }
        }
    }
    private static int[] sorted(int... a) { int[] s = a.clone(); java.util.Arrays.sort(s); return s; }
    private static int idx(int[] a, int v) { return java.util.Arrays.binarySearch(a, v); }
}
