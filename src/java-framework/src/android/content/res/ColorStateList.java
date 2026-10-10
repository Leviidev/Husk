package android.content.res;

import android.graphics.Color;

public class ColorStateList implements android.os.Parcelable {
    private static final int[][] EMPTY = { new int[0] };
    private final int[][] mStateSpecs;
    private final int[] mColors;
    private final int mDefaultColor;
    private final boolean mStateful;
    public ColorStateList(int[][] states, int[] colors) {
        mStateSpecs = states; mColors = colors;
        int def = colors.length > 0 ? colors[0] : Color.RED;
        for (int i = 0; i < states.length; i++) if (states[i].length == 0) { def = colors[i]; break; }
        mDefaultColor = def;
        boolean sf = false;
        for (int[] s : states) if (s.length > 0) sf = true;
        mStateful = sf;
    }
    public static ColorStateList valueOf(int color) { return new ColorStateList(EMPTY, new int[] { color }); }
    public static ColorStateList createFromXml(Resources r, org.xmlpull.v1.XmlPullParser p) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException { return husk.ResInflate.colorStateList(r, p, null); }
    public static ColorStateList createFromXml(Resources r, org.xmlpull.v1.XmlPullParser p, Resources.Theme t) throws org.xmlpull.v1.XmlPullParserException, java.io.IOException { return husk.ResInflate.colorStateList(r, p, t); }
    public ColorStateList withAlpha(int a) { int[] c = mColors.clone(); for (int i = 0; i < c.length; i++) c[i] = (c[i] & 0xFFFFFF) | (a << 24); return new ColorStateList(mStateSpecs, c); }
    public ColorStateList withLStar(float l) { return this; }
    public boolean isStateful() { return mStateful; }
    public boolean hasFocusStateSpecified() { return false; }
    public boolean isOpaque() { for (int c : mColors) if ((c >>> 24) != 0xFF) return false; return true; }
    public int getColorForState(int[] state, int def) {
        for (int i = 0; i < mStateSpecs.length; i++) if (android.util.StateSet.stateSetMatches(mStateSpecs[i], state)) return mColors[i];
        return def;
    }
    public int getDefaultColor() { return mDefaultColor; }
    public int[][] getStates() { return mStateSpecs; }
    public int[] getColors() { return mColors; }
    public int getChangingConfigurations() { return 0; }
    public boolean canApplyTheme() { return false; }
    public int describeContents() { return 0; }
    @Override public String toString() { return "ColorStateList{mStateSpecs=" + java.util.Arrays.deepToString(mStateSpecs) + "mColors=" + java.util.Arrays.toString(mColors) + "mDefaultColor=" + mDefaultColor + '}'; }
}
