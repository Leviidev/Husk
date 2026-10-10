package android.view;
public abstract class ActionMode {
    public static final int TYPE_PRIMARY = 0, TYPE_FLOATING = 1, DEFAULT_HIDE_DURATION = -1;
    public interface Callback { boolean onCreateActionMode(ActionMode m, Menu menu); boolean onPrepareActionMode(ActionMode m, Menu menu); boolean onActionItemClicked(ActionMode m, MenuItem item); void onDestroyActionMode(ActionMode m); }
    public static abstract class Callback2 implements Callback { public void onGetContentRect(ActionMode m, View v, android.graphics.Rect out) { out.set(0, 0, v.getWidth(), v.getHeight()); } }
    private Object mTag; private boolean mTitleOptional;
    public void setTag(Object t) { mTag = t; } public Object getTag() { return mTag; }
    public abstract void setTitle(CharSequence t); public abstract void setTitle(int r); public abstract void setSubtitle(CharSequence s); public abstract void setSubtitle(int r);
    public void setTitleOptionalHint(boolean o) { mTitleOptional = o; } public boolean getTitleOptionalHint() { return mTitleOptional; } public boolean isTitleOptional() { return false; }
    public abstract void setCustomView(View v); public abstract void invalidate(); public void invalidateContentRect() {} public abstract void finish();
    public abstract Menu getMenu(); public abstract CharSequence getTitle(); public abstract CharSequence getSubtitle(); public abstract View getCustomView();
    public abstract MenuInflater getMenuInflater(); public int getType() { return TYPE_PRIMARY; } public void setType(int t) {} public void hide(long d) {}
    public void onWindowFocusChanged(boolean f) {} public boolean isUiFocusable() { return true; }
}
