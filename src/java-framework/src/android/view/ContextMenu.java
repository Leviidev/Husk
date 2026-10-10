package android.view;
public interface ContextMenu extends Menu {
    interface ContextMenuInfo {}
    ContextMenu setHeaderTitle(int r); ContextMenu setHeaderTitle(CharSequence t); ContextMenu setHeaderIcon(int r); ContextMenu setHeaderIcon(android.graphics.drawable.Drawable d);
    ContextMenu setHeaderView(View v); void clearHeader();
}
