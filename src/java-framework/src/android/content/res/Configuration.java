package android.content.res;

import android.os.LocaleList;
import android.os.Parcel;
import android.os.Parcelable;
import java.util.Locale;

/** The device configuration: the phone's screen (size, density, orientation), its locale, night mode and input. */
public final class Configuration implements Parcelable, Comparable<Configuration> {
    public static final int ORIENTATION_UNDEFINED = 0, ORIENTATION_PORTRAIT = 1, ORIENTATION_LANDSCAPE = 2, ORIENTATION_SQUARE = 3;
    public static final int KEYBOARD_UNDEFINED = 0, KEYBOARD_NOKEYS = 1, KEYBOARD_QWERTY = 2, KEYBOARD_12KEY = 3;
    public static final int KEYBOARDHIDDEN_UNDEFINED = 0, KEYBOARDHIDDEN_NO = 1, KEYBOARDHIDDEN_YES = 2;
    public static final int HARDKEYBOARDHIDDEN_UNDEFINED = 0, HARDKEYBOARDHIDDEN_NO = 1, HARDKEYBOARDHIDDEN_YES = 2;
    public static final int NAVIGATION_UNDEFINED = 0, NAVIGATION_NONAV = 1, NAVIGATION_DPAD = 2, NAVIGATION_TRACKBALL = 3, NAVIGATION_WHEEL = 4;
    public static final int NAVIGATIONHIDDEN_UNDEFINED = 0, NAVIGATIONHIDDEN_NO = 1, NAVIGATIONHIDDEN_YES = 2;
    public static final int TOUCHSCREEN_UNDEFINED = 0, TOUCHSCREEN_NOTOUCH = 1, TOUCHSCREEN_STYLUS = 2, TOUCHSCREEN_FINGER = 3;
    public static final int SCREENLAYOUT_UNDEFINED = 0, SCREENLAYOUT_SIZE_MASK = 0x0f, SCREENLAYOUT_SIZE_UNDEFINED = 0, SCREENLAYOUT_SIZE_SMALL = 1, SCREENLAYOUT_SIZE_NORMAL = 2, SCREENLAYOUT_SIZE_LARGE = 3, SCREENLAYOUT_SIZE_XLARGE = 4;
    public static final int SCREENLAYOUT_LONG_MASK = 0x30, SCREENLAYOUT_LONG_UNDEFINED = 0, SCREENLAYOUT_LONG_NO = 0x10, SCREENLAYOUT_LONG_YES = 0x20;
    public static final int SCREENLAYOUT_LAYOUTDIR_MASK = 0xC0, SCREENLAYOUT_LAYOUTDIR_SHIFT = 6, SCREENLAYOUT_LAYOUTDIR_UNDEFINED = 0, SCREENLAYOUT_LAYOUTDIR_LTR = 0x40, SCREENLAYOUT_LAYOUTDIR_RTL = 0x80;
    public static final int SCREENLAYOUT_ROUND_MASK = 0x300, SCREENLAYOUT_ROUND_SHIFT = 8, SCREENLAYOUT_ROUND_UNDEFINED = 0, SCREENLAYOUT_ROUND_NO = 0x100, SCREENLAYOUT_ROUND_YES = 0x200;
    public static final int COLOR_MODE_UNDEFINED = 0, COLOR_MODE_WIDE_COLOR_GAMUT_MASK = 0x3, COLOR_MODE_WIDE_COLOR_GAMUT_UNDEFINED = 0, COLOR_MODE_WIDE_COLOR_GAMUT_NO = 1, COLOR_MODE_WIDE_COLOR_GAMUT_YES = 2,
        COLOR_MODE_HDR_MASK = 0xc, COLOR_MODE_HDR_SHIFT = 2, COLOR_MODE_HDR_UNDEFINED = 0, COLOR_MODE_HDR_NO = 4, COLOR_MODE_HDR_YES = 8;
    public static final int UI_MODE_TYPE_MASK = 0x0f, UI_MODE_TYPE_UNDEFINED = 0, UI_MODE_TYPE_NORMAL = 1, UI_MODE_TYPE_DESK = 2, UI_MODE_TYPE_CAR = 3, UI_MODE_TYPE_TELEVISION = 4, UI_MODE_TYPE_APPLIANCE = 5, UI_MODE_TYPE_WATCH = 6, UI_MODE_TYPE_VR_HEADSET = 7;
    public static final int UI_MODE_NIGHT_MASK = 0x30, UI_MODE_NIGHT_UNDEFINED = 0, UI_MODE_NIGHT_NO = 0x10, UI_MODE_NIGHT_YES = 0x20;
    public static final int SCREEN_WIDTH_DP_UNDEFINED = 0, SCREEN_HEIGHT_DP_UNDEFINED = 0, SMALLEST_SCREEN_WIDTH_DP_UNDEFINED = 0, DENSITY_DPI_UNDEFINED = 0, MNC_ZERO = 0xffff;
    public static final int FONT_WEIGHT_ADJUSTMENT_UNDEFINED = Integer.MAX_VALUE;
    public static final int GRAMMATICAL_GENDER_NOT_SPECIFIED = 0, GRAMMATICAL_GENDER_NEUTRAL = 1, GRAMMATICAL_GENDER_FEMININE = 2, GRAMMATICAL_GENDER_MASCULINE = 3;
    public float fontScale;
    public int mcc, mnc;
    @Deprecated public Locale locale;
    public int screenLayout, touchscreen, keyboard, keyboardHidden, hardKeyboardHidden, navigation, navigationHidden, orientation, colorMode, uiMode;
    public int screenWidthDp, screenHeightDp, smallestScreenWidthDp, densityDpi, fontWeightAdjustment;
    private LocaleList mLocaleList = LocaleList.getEmptyLocaleList();
    private int mGrammaticalGender;

    public Configuration() { unset(); }
    public Configuration(Configuration o) { setTo(o); }
    /** Husk: the phone's own configuration. */
    public static Configuration huskDevice() {
        Configuration c = new Configuration();
        c.setToDefaults();
        int w = husk.Native.screenWidth(), h = husk.Native.screenHeight();
        float d = husk.Native.density();
        c.orientation = w > h ? ORIENTATION_LANDSCAPE : ORIENTATION_PORTRAIT;
        c.screenWidthDp = (int) (w / d); c.screenHeightDp = (int) (h / d); c.smallestScreenWidthDp = Math.min(c.screenWidthDp, c.screenHeightDp);
        c.densityDpi = (int) (d * 160);
        c.touchscreen = TOUCHSCREEN_FINGER; c.keyboard = KEYBOARD_NOKEYS; c.keyboardHidden = KEYBOARDHIDDEN_YES; c.hardKeyboardHidden = HARDKEYBOARDHIDDEN_YES;
        c.navigation = NAVIGATION_NONAV; c.navigationHidden = NAVIGATIONHIDDEN_YES;
        int size = c.smallestScreenWidthDp >= 720 ? SCREENLAYOUT_SIZE_XLARGE : c.smallestScreenWidthDp >= 600 ? SCREENLAYOUT_SIZE_LARGE : SCREENLAYOUT_SIZE_NORMAL;
        boolean longScreen = Math.max(w, h) * 3 > Math.min(w, h) * 5;
        c.screenLayout = size | (longScreen ? SCREENLAYOUT_LONG_YES : SCREENLAYOUT_LONG_NO) | SCREENLAYOUT_ROUND_NO;
        c.uiMode = UI_MODE_TYPE_NORMAL | (husk.Native.nightMode() ? UI_MODE_NIGHT_YES : UI_MODE_NIGHT_NO);
        c.colorMode = COLOR_MODE_WIDE_COLOR_GAMUT_YES | COLOR_MODE_HDR_NO;
        c.setLocales(LocaleList.getDefault());
        return c;
    }
    public void unset() { setToDefaults(); fontScale = 0; }
    public void setToDefaults() {
        fontScale = 1; mcc = mnc = 0; mLocaleList = LocaleList.getEmptyLocaleList(); locale = null;
        touchscreen = TOUCHSCREEN_UNDEFINED; keyboard = KEYBOARD_UNDEFINED; keyboardHidden = KEYBOARDHIDDEN_UNDEFINED; hardKeyboardHidden = HARDKEYBOARDHIDDEN_UNDEFINED;
        navigation = NAVIGATION_UNDEFINED; navigationHidden = NAVIGATIONHIDDEN_UNDEFINED; orientation = ORIENTATION_UNDEFINED; screenLayout = SCREENLAYOUT_UNDEFINED;
        colorMode = COLOR_MODE_UNDEFINED; uiMode = UI_MODE_TYPE_UNDEFINED; screenWidthDp = SCREEN_WIDTH_DP_UNDEFINED; screenHeightDp = SCREEN_HEIGHT_DP_UNDEFINED;
        smallestScreenWidthDp = SMALLEST_SCREEN_WIDTH_DP_UNDEFINED; densityDpi = DENSITY_DPI_UNDEFINED; fontWeightAdjustment = FONT_WEIGHT_ADJUSTMENT_UNDEFINED; mGrammaticalGender = 0;
    }
    public void setTo(Configuration o) {
        fontScale = o.fontScale; mcc = o.mcc; mnc = o.mnc; locale = o.locale; mLocaleList = o.mLocaleList;
        touchscreen = o.touchscreen; keyboard = o.keyboard; keyboardHidden = o.keyboardHidden; hardKeyboardHidden = o.hardKeyboardHidden;
        navigation = o.navigation; navigationHidden = o.navigationHidden; orientation = o.orientation; screenLayout = o.screenLayout; colorMode = o.colorMode;
        uiMode = o.uiMode; screenWidthDp = o.screenWidthDp; screenHeightDp = o.screenHeightDp; smallestScreenWidthDp = o.smallestScreenWidthDp;
        densityDpi = o.densityDpi; fontWeightAdjustment = o.fontWeightAdjustment; mGrammaticalGender = o.mGrammaticalGender;
    }
    public LocaleList getLocales() { fixUpLocaleList(); return mLocaleList; }
    private void fixUpLocaleList() { if ((locale == null && !mLocaleList.isEmpty()) || (locale != null && !locale.equals(mLocaleList.get(0)))) mLocaleList = locale == null ? LocaleList.getEmptyLocaleList() : new LocaleList(locale); }
    public void setLocales(LocaleList l) { mLocaleList = l == null ? LocaleList.getEmptyLocaleList() : l; locale = mLocaleList.get(0); setLayoutDirection(locale); }
    public void setLocale(Locale l) { setLocales(l == null ? LocaleList.getEmptyLocaleList() : new LocaleList(l)); }
    public void clearLocales() { mLocaleList = LocaleList.getEmptyLocaleList(); locale = null; }
    public int getLayoutDirection() { return (screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK) == SCREENLAYOUT_LAYOUTDIR_RTL ? android.view.View.LAYOUT_DIRECTION_RTL : android.view.View.LAYOUT_DIRECTION_LTR; }
    public void setLayoutDirection(Locale l) {
        int dir = 1 + (l != null ? android.text.TextUtils.getLayoutDirectionFromLocale(l) : 0);
        screenLayout = (screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK) | (dir << SCREENLAYOUT_LAYOUTDIR_SHIFT);
    }
    public boolean isLayoutSizeAtLeast(int size) { int cur = screenLayout & SCREENLAYOUT_SIZE_MASK; if (cur == SCREENLAYOUT_SIZE_UNDEFINED) return false; return cur >= size; }
    public boolean isScreenRound() { return (screenLayout & SCREENLAYOUT_ROUND_MASK) == SCREENLAYOUT_ROUND_YES; }
    public boolean isScreenWideColorGamut() { return (colorMode & COLOR_MODE_WIDE_COLOR_GAMUT_MASK) == COLOR_MODE_WIDE_COLOR_GAMUT_YES; }
    public boolean isScreenHdr() { return (colorMode & COLOR_MODE_HDR_MASK) == COLOR_MODE_HDR_YES; }
    public boolean isNightModeActive() { return (uiMode & UI_MODE_NIGHT_MASK) == UI_MODE_NIGHT_YES; }
    public int getGrammaticalGender() { return mGrammaticalGender; }
    /** Which of ActivityInfo's CONFIG_* bits differ (only fields delta defines). */
    public int diff(Configuration d) {
        int changed = 0;
        if (d.fontScale > 0 && fontScale != d.fontScale) changed |= 0x40000000;
        if (d.mcc != 0 && mcc != d.mcc) changed |= 1;
        if (d.mnc != 0 && mnc != d.mnc) changed |= 2;
        fixUpLocaleList(); d.fixUpLocaleList();
        if (!d.mLocaleList.isEmpty() && !mLocaleList.equals(d.mLocaleList)) changed |= 4 | 0x2000;
        int dl = d.screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK;
        if (dl != SCREENLAYOUT_LAYOUTDIR_UNDEFINED && dl != (screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK)) changed |= 0x2000;
        if (d.touchscreen != 0 && touchscreen != d.touchscreen) changed |= 8;
        if (d.keyboard != 0 && keyboard != d.keyboard) changed |= 0x10;
        if (d.keyboardHidden != 0 && keyboardHidden != d.keyboardHidden) changed |= 0x20;
        if (d.hardKeyboardHidden != 0 && hardKeyboardHidden != d.hardKeyboardHidden) changed |= 0x20;
        if (d.navigation != 0 && navigation != d.navigation) changed |= 0x40;
        if (d.navigationHidden != 0 && navigationHidden != d.navigationHidden) changed |= 0x20;
        if (d.orientation != 0 && orientation != d.orientation) changed |= 0x80;
        if ((d.screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK) != 0 && (screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK) != (d.screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK)) changed |= 0x100;
        if (d.colorMode != 0 && colorMode != d.colorMode) changed |= 0x4000;
        if (d.uiMode != 0 && uiMode != d.uiMode) changed |= 0x200;
        if (d.screenWidthDp != 0 && screenWidthDp != d.screenWidthDp) changed |= 0x400;
        if (d.screenHeightDp != 0 && screenHeightDp != d.screenHeightDp) changed |= 0x400;
        if (d.smallestScreenWidthDp != 0 && smallestScreenWidthDp != d.smallestScreenWidthDp) changed |= 0x800;
        if (d.densityDpi != 0 && densityDpi != d.densityDpi) changed |= 0x1000;
        if (d.fontWeightAdjustment != FONT_WEIGHT_ADJUSTMENT_UNDEFINED && fontWeightAdjustment != d.fontWeightAdjustment) changed |= 0x10000000;
        return changed;
    }
    /** Take delta's defined fields; returns what changed. */
    public int updateFrom(Configuration d) {
        int changed = diff(d);
        if (d.fontScale > 0) fontScale = d.fontScale;
        if (d.mcc != 0) mcc = d.mcc;
        if (d.mnc != 0) mnc = d.mnc;
        if (!d.getLocales().isEmpty()) { mLocaleList = d.mLocaleList; locale = d.locale; }
        int dl = d.screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK;
        if (dl != 0) screenLayout = (screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK) | dl;
        if (d.touchscreen != 0) touchscreen = d.touchscreen;
        if (d.keyboard != 0) keyboard = d.keyboard;
        if (d.keyboardHidden != 0) keyboardHidden = d.keyboardHidden;
        if (d.hardKeyboardHidden != 0) hardKeyboardHidden = d.hardKeyboardHidden;
        if (d.navigation != 0) navigation = d.navigation;
        if (d.navigationHidden != 0) navigationHidden = d.navigationHidden;
        if (d.orientation != 0) orientation = d.orientation;
        if ((d.screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK) != 0) screenLayout = (screenLayout & SCREENLAYOUT_LAYOUTDIR_MASK) | (d.screenLayout & ~SCREENLAYOUT_LAYOUTDIR_MASK);
        if (d.colorMode != 0) colorMode = d.colorMode;
        if (d.uiMode != 0) uiMode = d.uiMode;
        if (d.screenWidthDp != 0) screenWidthDp = d.screenWidthDp;
        if (d.screenHeightDp != 0) screenHeightDp = d.screenHeightDp;
        if (d.smallestScreenWidthDp != 0) smallestScreenWidthDp = d.smallestScreenWidthDp;
        if (d.densityDpi != 0) densityDpi = d.densityDpi;
        if (d.fontWeightAdjustment != FONT_WEIGHT_ADJUSTMENT_UNDEFINED) fontWeightAdjustment = d.fontWeightAdjustment;
        return changed;
    }
    public static boolean needNewResources(int configChanges, int interestingChanges) { return (configChanges & (interestingChanges | 0x40000000)) != 0; }
    public int compareTo(Configuration that) {
        int n;
        float a = fontScale, b = that.fontScale;
        if (a < b) return -1; if (a > b) return 1;
        n = mcc - that.mcc; if (n != 0) return n;
        n = mnc - that.mnc; if (n != 0) return n;
        fixUpLocaleList(); that.fixUpLocaleList();
        n = mLocaleList.toLanguageTags().compareTo(that.mLocaleList.toLanguageTags()); if (n != 0) return n;
        n = touchscreen - that.touchscreen; if (n != 0) return n;
        n = keyboard - that.keyboard; if (n != 0) return n;
        n = keyboardHidden - that.keyboardHidden; if (n != 0) return n;
        n = hardKeyboardHidden - that.hardKeyboardHidden; if (n != 0) return n;
        n = navigation - that.navigation; if (n != 0) return n;
        n = navigationHidden - that.navigationHidden; if (n != 0) return n;
        n = orientation - that.orientation; if (n != 0) return n;
        n = colorMode - that.colorMode; if (n != 0) return n;
        n = screenLayout - that.screenLayout; if (n != 0) return n;
        n = uiMode - that.uiMode; if (n != 0) return n;
        n = screenWidthDp - that.screenWidthDp; if (n != 0) return n;
        n = screenHeightDp - that.screenHeightDp; if (n != 0) return n;
        n = smallestScreenWidthDp - that.smallestScreenWidthDp; if (n != 0) return n;
        n = densityDpi - that.densityDpi; if (n != 0) return n;
        n = fontWeightAdjustment - that.fontWeightAdjustment; if (n != 0) return n;
        return mGrammaticalGender - that.mGrammaticalGender;
    }
    public boolean equals(Configuration that) { if (that == null) return false; if (that == this) return true; return compareTo(that) == 0; }
    @Override public boolean equals(Object that) { return that instanceof Configuration && equals((Configuration) that); }
    @Override public int hashCode() {
        int result = 17;
        result = 31 * result + Float.floatToIntBits(fontScale); result = 31 * result + mcc; result = 31 * result + mnc; result = 31 * result + getLocales().hashCode();
        result = 31 * result + touchscreen; result = 31 * result + keyboard; result = 31 * result + keyboardHidden; result = 31 * result + hardKeyboardHidden;
        result = 31 * result + navigation; result = 31 * result + navigationHidden; result = 31 * result + orientation; result = 31 * result + screenLayout;
        result = 31 * result + colorMode; result = 31 * result + uiMode; result = 31 * result + screenWidthDp; result = 31 * result + screenHeightDp;
        result = 31 * result + smallestScreenWidthDp; result = 31 * result + densityDpi; result = 31 * result + fontWeightAdjustment;
        return result;
    }
    @Override public String toString() {
        return "{" + fontScale + " " + mcc + "mcc" + mnc + "mnc " + getLocales() + " sw" + smallestScreenWidthDp + "dp w" + screenWidthDp + "dp h" + screenHeightDp + "dp " + densityDpi + "dpi"
            + (orientation == ORIENTATION_LANDSCAPE ? " land" : orientation == ORIENTATION_PORTRAIT ? " port" : "") + ((uiMode & UI_MODE_NIGHT_MASK) == UI_MODE_NIGHT_YES ? " night" : "") + "}";
    }
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel p, int flags) {
        p.writeFloat(fontScale); p.writeInt(mcc); p.writeInt(mnc); p.writeString(getLocales().toLanguageTags());
        p.writeInt(touchscreen); p.writeInt(keyboard); p.writeInt(keyboardHidden); p.writeInt(hardKeyboardHidden); p.writeInt(navigation); p.writeInt(navigationHidden);
        p.writeInt(orientation); p.writeInt(screenLayout); p.writeInt(colorMode); p.writeInt(uiMode); p.writeInt(screenWidthDp); p.writeInt(screenHeightDp);
        p.writeInt(smallestScreenWidthDp); p.writeInt(densityDpi); p.writeInt(fontWeightAdjustment); p.writeInt(mGrammaticalGender);
    }
    public void readFromParcel(Parcel p) {
        fontScale = p.readFloat(); mcc = p.readInt(); mnc = p.readInt(); setLocales(LocaleList.forLanguageTags(p.readString()));
        touchscreen = p.readInt(); keyboard = p.readInt(); keyboardHidden = p.readInt(); hardKeyboardHidden = p.readInt(); navigation = p.readInt(); navigationHidden = p.readInt();
        orientation = p.readInt(); screenLayout = p.readInt(); colorMode = p.readInt(); uiMode = p.readInt(); screenWidthDp = p.readInt(); screenHeightDp = p.readInt();
        smallestScreenWidthDp = p.readInt(); densityDpi = p.readInt(); fontWeightAdjustment = p.readInt(); mGrammaticalGender = p.readInt();
    }
    public static final Parcelable.Creator<Configuration> CREATOR = new Parcelable.Creator<Configuration>() {
        public Configuration createFromParcel(Parcel p) { Configuration c = new Configuration(); c.readFromParcel(p); return c; }
        public Configuration[] newArray(int n) { return new Configuration[n]; }
    };
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    public static final int ASSETS_SEQ_UNDEFINED = 0;
    public static final int DENSITY_DPI_ANY = 65534;
    public static final int DENSITY_DPI_NONE = 65535;
    public static android.content.res.Configuration EMPTY = new android.content.res.Configuration();
    public static final int GRAMMATICAL_GENDER_UNDEFINED = -1;
    public static final int KEYBOARDHIDDEN_SOFT = 3;
    public static final int NATIVE_CONFIG_COLOR_MODE = 65536;
    public static final int NATIVE_CONFIG_DENSITY = 256;
    public static final int NATIVE_CONFIG_GRAMMATICAL_GENDER = 131072;
    public static final int NATIVE_CONFIG_KEYBOARD = 16;
    public static final int NATIVE_CONFIG_KEYBOARD_HIDDEN = 32;
    public static final int NATIVE_CONFIG_LAYOUTDIR = 16384;
    public static final int NATIVE_CONFIG_LOCALE = 4;
    public static final int NATIVE_CONFIG_MCC = 1;
    public static final int NATIVE_CONFIG_MNC = 2;
    public static final int NATIVE_CONFIG_NAVIGATION = 64;
    public static final int NATIVE_CONFIG_ORIENTATION = 128;
    public static final int NATIVE_CONFIG_SCREEN_LAYOUT = 2048;
    public static final int NATIVE_CONFIG_SCREEN_SIZE = 512;
    public static final int NATIVE_CONFIG_SMALLEST_SCREEN_SIZE = 8192;
    public static final int NATIVE_CONFIG_TOUCHSCREEN = 8;
    public static final int NATIVE_CONFIG_UI_MODE = 4096;
    public static final int NATIVE_CONFIG_VERSION = 1024;
    public static final int SCREENLAYOUT_COMPAT_NEEDED = 268435456;
    public int assetsSeq;
    public int compatScreenHeightDp;
    public int compatScreenWidthDp;
    public int compatSmallestScreenWidthDp;
    public int seq;
    public boolean userSetLocale;
    public static java.lang.String configurationDiffToString(int p0) { return null; }
    public static android.content.res.Configuration generateDelta(android.content.res.Configuration p0, android.content.res.Configuration p1) { return null; }
    public static java.lang.String getUiModeTypeString(int p0) { return null; }
    public static java.lang.String localesToResourceQualifier(android.os.LocaleList p0) { return null; }
    public static void readXmlAttrs(org.xmlpull.v1.XmlPullParser p0, android.content.res.Configuration p1) {}
    public static int reduceScreenLayout(int p0, int p1, int p2) { return 0; }
    public static int resetScreenLayout(int p0) { return 0; }
    public static java.lang.String resourceQualifierString(android.content.res.Configuration p0) { return null; }
    public static java.lang.String resourceQualifierString(android.content.res.Configuration p0, android.util.DisplayMetrics p1) { return null; }
    public static java.lang.String uiModeToString(int p0) { return null; }
    public int diff(android.content.res.Configuration p0, boolean p1, boolean p2) { return 0; }
    public int diffPublicOnly(android.content.res.Configuration p0) { return 0; }
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1) {}
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1, boolean p2) {}
    public void dumpDebug(android.util.proto.ProtoOutputStream p0, long p1, boolean p2, boolean p3) {}
    public int getGrammaticalGenderRaw() { return 0; }
    public boolean isOtherSeqNewer(android.content.res.Configuration p0) { return false; }
    public void makeDefault() {}
    public void readFromProto(android.util.proto.ProtoInputStream p0, long p1) {}
    public void setGrammaticalGender(int p0) {}
    public void setTo(android.content.res.Configuration p0, int p1, int p2) {}
    public void writeResConfigToProto(android.util.proto.ProtoOutputStream p0, long p1, android.util.DisplayMetrics p2) {}
    // ---- end of generated members
}
