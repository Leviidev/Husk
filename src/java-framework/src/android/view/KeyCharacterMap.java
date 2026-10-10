package android.view;
public class KeyCharacterMap {
    public static final int VIRTUAL_KEYBOARD = -1, FULL = 4;
    public static KeyCharacterMap load(int id) { return new KeyCharacterMap(); }
    public static boolean deviceHasKey(int code) { return code == KeyEvent.KEYCODE_BACK; }
    public int get(int code, int meta) { return new KeyEvent(0, code).getUnicodeChar(meta); }
    public KeyEvent[] getEvents(char[] chars) { return new KeyEvent[0]; }
    public int getKeyboardType() { return FULL; }
}
