package android.content;
public class ClipData {
    public static class Item { private final CharSequence t; public Item(CharSequence t) { this.t = t; } public CharSequence getText() { return t; } public CharSequence coerceToText(Context c) { return t; } }
    private final Item item;
    private ClipData(Item i) { item = i; }
    public static ClipData newPlainText(CharSequence label, CharSequence text) { return new ClipData(new Item(text)); }
    public int getItemCount() { return 1; }
    public Item getItemAt(int i) { return item; }
}
