package android.content.res;

import java.util.Locale;

public final class Configuration {
    public static final int SCREENLAYOUT_LAYOUTDIR_MASK = 0xC0, SCREENLAYOUT_LAYOUTDIR_SHIFT = 6, SCREENLAYOUT_LAYOUTDIR_UNDEFINED = 0, SCREENLAYOUT_LAYOUTDIR_LTR = 0x40, SCREENLAYOUT_LAYOUTDIR_RTL = 0x80;
    public static final int ORIENTATION_UNDEFINED = 0, ORIENTATION_PORTRAIT = 1, ORIENTATION_LANDSCAPE = 2;
    public static final int KEYBOARD_NOKEYS = 1, KEYBOARD_QWERTY = 2, KEYBOARDHIDDEN_NO = 1, KEYBOARDHIDDEN_YES = 2, HARDKEYBOARDHIDDEN_YES = 2;
    public static final int NAVIGATION_NONAV = 1, TOUCHSCREEN_FINGER = 3, SCREENLAYOUT_SIZE_MASK = 15, SCREENLAYOUT_SIZE_NORMAL = 2, SCREENLAYOUT_SIZE_LARGE = 3;
    public static final int UI_MODE_NIGHT_MASK = 48, UI_MODE_NIGHT_NO = 16, UI_MODE_NIGHT_YES = 32, UI_MODE_TYPE_NORMAL = 1;
    public int orientation, keyboard = KEYBOARD_NOKEYS, keyboardHidden = KEYBOARDHIDDEN_YES, hardKeyboardHidden = HARDKEYBOARDHIDDEN_YES;
    public int navigation = NAVIGATION_NONAV, touchscreen = TOUCHSCREEN_FINGER, screenLayout = SCREENLAYOUT_SIZE_NORMAL, uiMode = UI_MODE_TYPE_NORMAL | UI_MODE_NIGHT_NO;
    public int screenWidthDp, screenHeightDp, smallestScreenWidthDp, densityDpi;
    public float fontScale = 1f;
    public Locale locale = Locale.getDefault();
    public Configuration() {
        int w = husk.Native.screenWidth(), h = husk.Native.screenHeight();
        float d = husk.Native.density();
        orientation = w > h ? ORIENTATION_LANDSCAPE : ORIENTATION_PORTRAIT;
        screenWidthDp = (int) (w / d); screenHeightDp = (int) (h / d); smallestScreenWidthDp = Math.min(screenWidthDp, screenHeightDp);
        densityDpi = (int) (d * 160);
    }
    public Configuration(Configuration o) { setTo(o); }
    public void setTo(Configuration o) {
        orientation = o.orientation; keyboard = o.keyboard; keyboardHidden = o.keyboardHidden; navigation = o.navigation;
        touchscreen = o.touchscreen; screenLayout = o.screenLayout; uiMode = o.uiMode; screenWidthDp = o.screenWidthDp;
        screenHeightDp = o.screenHeightDp; smallestScreenWidthDp = o.smallestScreenWidthDp; densityDpi = o.densityDpi; fontScale = o.fontScale; locale = o.locale;
    }
    public android.os.LocaleList getLocales() { return new android.os.LocaleList(locale); }
    public void setLocale(Locale l) { locale = l; setLayoutDirection(l); }
    public void setLocales(android.os.LocaleList l) { locale = l == null || l.isEmpty() ? Locale.getDefault() : l.get(0); setLayoutDirection(locale); }
    public int getLayoutDirection() { return (screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK) == SCREENLAYOUT_LAYOUTDIR_RTL ? android.view.View.LAYOUT_DIRECTION_RTL : android.view.View.LAYOUT_DIRECTION_LTR; }
    public void setLayoutDirection(Locale l) { int d = l != null && android.text.TextUtils.getLayoutDirectionFromLocale(l) == android.view.View.LAYOUT_DIRECTION_RTL ? SCREENLAYOUT_LAYOUTDIR_RTL : SCREENLAYOUT_LAYOUTDIR_LTR; screenLayout = (screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK) | d; }
}
