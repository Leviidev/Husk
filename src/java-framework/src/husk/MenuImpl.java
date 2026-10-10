package husk;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.*;
import java.util.ArrayList;

/** A menu's items, kept in order; nothing is drawn from it except what an action bar chooses to show. */
public class MenuImpl implements Menu, SubMenu, ContextMenu {
    private final Context mContext;
    final ArrayList<Item> mItems = new ArrayList<>();
    private Item mParent;
    private CharSequence mHeader;
    public MenuImpl(Context c) { mContext = c; }
    /** A menu showing the given items (they stay the originals: clicks reach their listeners). */
    public static MenuImpl viewOf(Context c, java.util.List<MenuItem> items) { MenuImpl m = new MenuImpl(c); for (MenuItem i : items) m.mItems.add((Item) i); return m; }
    public class Item implements MenuItem {
        int id, group, order; CharSequence title, condensed; Drawable icon; Intent intent; boolean checkable, checked, visible = true, enabled = true;
        char numeric, alpha; OnMenuItemClickListener click; View actionView; ActionProvider provider; MenuImpl sub; int showAs; OnActionExpandListener expand;
        public SubMenu huskSubMenu() { if (sub == null) { sub = new MenuImpl(mContext); sub.mParent = this; } return sub; }
        public int getItemId() { return id; } public int getGroupId() { return group; } public int getOrder() { return order; }
        public MenuItem setTitle(CharSequence t) { title = t; return this; } public MenuItem setTitle(int r) { title = mContext.getText(r); return this; } public CharSequence getTitle() { return title; }
        public MenuItem setTitleCondensed(CharSequence t) { condensed = t; return this; } public CharSequence getTitleCondensed() { return condensed != null ? condensed : title; }
        public MenuItem setIcon(Drawable d) { icon = d; return this; } public MenuItem setIcon(int r) { icon = r == 0 ? null : mContext.getDrawable(r); return this; } public Drawable getIcon() { return icon; }
        public MenuItem setIntent(Intent i) { intent = i; return this; } public Intent getIntent() { return intent; }
        public MenuItem setShortcut(char n, char a) { numeric = n; alpha = a; return this; } public MenuItem setNumericShortcut(char c) { numeric = c; return this; } public char getNumericShortcut() { return numeric; }
        public MenuItem setAlphabeticShortcut(char c) { alpha = c; return this; } public char getAlphabeticShortcut() { return alpha; }
        public MenuItem setCheckable(boolean c) { checkable = c; return this; } public boolean isCheckable() { return checkable; }
        public MenuItem setChecked(boolean c) { checked = c; return this; } public boolean isChecked() { return checked; }
        public MenuItem setVisible(boolean v) { visible = v; return this; } public boolean isVisible() { return visible; }
        public MenuItem setEnabled(boolean e) { enabled = e; return this; } public boolean isEnabled() { return enabled; }
        public boolean hasSubMenu() { return sub != null; } public SubMenu getSubMenu() { return sub; }
        public MenuItem setOnMenuItemClickListener(OnMenuItemClickListener l) { click = l; return this; }
        public ContextMenu.ContextMenuInfo getMenuInfo() { return null; }
        public void setShowAsAction(int a) { showAs = a; } public MenuItem setShowAsActionFlags(int a) { showAs = a; return this; } public int huskShowAs() { return showAs; }
        public MenuItem setActionView(View v) { actionView = v; return this; } public MenuItem setActionView(int r) { actionView = LayoutInflater.from(mContext).inflate(r, null); return this; } public View getActionView() { return actionView; }
        public MenuItem setActionProvider(ActionProvider p) { provider = p; return this; } public ActionProvider getActionProvider() { return provider; }
        public boolean expandActionView() { return expand == null || expand.onMenuItemActionExpand(this); } public boolean collapseActionView() { return expand == null || expand.onMenuItemActionCollapse(this); }
        public boolean isActionViewExpanded() { return false; } public MenuItem setOnActionExpandListener(OnActionExpandListener l) { expand = l; return this; }
        /** Clicked: its listener, else the activity's onOptionsItemSelected (through the callback the action bar hands it to). */
        public boolean huskInvoke() { if (click != null && click.onMenuItemClick(this)) return true; if (intent != null) { mContext.startActivity(intent); return true; } return false; }
    }
    private Item addInternal(int group, int id, int order, CharSequence title) {
        Item it = new Item();
        it.group = group; it.id = id; it.order = order; it.title = title;
        int i = 0;
        while (i < mItems.size() && mItems.get(i).order <= order) i++;
        mItems.add(i, it);
        return it;
    }
    public MenuItem add(CharSequence t) { return addInternal(0, 0, 0, t); }
    public MenuItem add(int r) { return addInternal(0, 0, 0, mContext.getText(r)); }
    public MenuItem add(int g, int id, int o, CharSequence t) { return addInternal(g, id, o, t); }
    public MenuItem add(int g, int id, int o, int r) { return addInternal(g, id, o, mContext.getText(r)); }
    public SubMenu addSubMenu(CharSequence t) { return addInternal(0, 0, 0, t).huskSubMenu(); }
    public SubMenu addSubMenu(int r) { return addInternal(0, 0, 0, mContext.getText(r)).huskSubMenu(); }
    public SubMenu addSubMenu(int g, int id, int o, CharSequence t) { return addInternal(g, id, o, t).huskSubMenu(); }
    public SubMenu addSubMenu(int g, int id, int o, int r) { return addInternal(g, id, o, mContext.getText(r)).huskSubMenu(); }
    public int addIntentOptions(int g, int id, int o, android.content.ComponentName c, Intent[] s, Intent i, int f, MenuItem[] out) { return 0; }
    public void removeItem(int id) { for (int i = mItems.size() - 1; i >= 0; i--) if (mItems.get(i).id == id) mItems.remove(i); }
    public void removeGroup(int g) { for (int i = mItems.size() - 1; i >= 0; i--) if (mItems.get(i).group == g) mItems.remove(i); }
    public void clear() { mItems.clear(); }
    public void setGroupCheckable(int g, boolean c, boolean e) { for (Item i : mItems) if (i.group == g) i.checkable = c; }
    public void setGroupVisible(int g, boolean v) { for (Item i : mItems) if (i.group == g) i.visible = v; }
    public void setGroupEnabled(int g, boolean e) { for (Item i : mItems) if (i.group == g) i.enabled = e; }
    public boolean hasVisibleItems() { for (Item i : mItems) if (i.visible) return true; return false; }
    public MenuItem findItem(int id) { for (Item i : mItems) { if (i.id == id) return i; if (i.sub != null) { MenuItem f = i.sub.findItem(id); if (f != null) return f; } } return null; }
    public int size() { return mItems.size(); }
    public MenuItem getItem(int i) { return mItems.get(i); }
    public void close() {}
    public boolean performShortcut(int k, KeyEvent e, int f) { return false; }
    public boolean isShortcutKey(int k, KeyEvent e) { return false; }
    public boolean performIdentifierAction(int id, int f) { MenuItem i = findItem(id); return i != null && ((Item) i).huskInvoke(); }
    public void setQwertyMode(boolean q) {}
    public MenuImpl setHeaderTitle(int r) { mHeader = mContext.getText(r); return this; }
    public MenuImpl setHeaderTitle(CharSequence t) { mHeader = t; return this; }
    public MenuImpl setHeaderIcon(int r) { return this; }
    public MenuImpl setHeaderIcon(Drawable d) { return this; }
    public MenuImpl setHeaderView(View v) { return this; }
    public void clearHeader() { mHeader = null; }
    public MenuImpl setIcon(int r) { if (mParent != null) mParent.setIcon(r); return this; }
    public MenuImpl setIcon(Drawable d) { if (mParent != null) mParent.setIcon(d); return this; }
    public MenuItem getItem() { return mParent; }
}
