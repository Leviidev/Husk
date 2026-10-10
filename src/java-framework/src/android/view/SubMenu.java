package android.view;
public interface SubMenu extends Menu {
    SubMenu setHeaderTitle(int r); SubMenu setHeaderTitle(CharSequence t); SubMenu setHeaderIcon(int r); SubMenu setHeaderIcon(android.graphics.drawable.Drawable d);
    SubMenu setHeaderView(View v); void clearHeader(); SubMenu setIcon(int r); SubMenu setIcon(android.graphics.drawable.Drawable d); MenuItem getItem();
}
