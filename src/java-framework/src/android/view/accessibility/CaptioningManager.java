package android.view.accessibility;

/** Captions are off, with Android's default style (white on black) and its presets. */
public class CaptioningManager {
    public CaptioningManager(android.content.Context c) {}
    CaptioningManager() {}
    private boolean mAudioCaptioning, mAudioCaptioningUi;
    public void addCaptioningChangeListener(CaptioningChangeListener l) {}
    public void removeCaptioningChangeListener(CaptioningChangeListener l) {}
    public float getFontScale() { return 1f; }
    public java.util.Locale getLocale() { return null; }
    public String getRawLocale() { return null; }
    public int getRawUserStyle() { return 0; }
    public CaptionStyle getUserStyle() { return CaptionStyle.DEFAULT; }
    public boolean isCallCaptioningEnabled() { return false; }
    public boolean isEnabled() { return false; }
    public boolean isSystemAudioCaptioningEnabled() { return mAudioCaptioning; }
    public boolean isSystemAudioCaptioningUiEnabled() { return mAudioCaptioningUi; }
    public void setSystemAudioCaptioningEnabled(boolean v) { mAudioCaptioning = v; }
    public void setSystemAudioCaptioningUiEnabled(boolean v) { mAudioCaptioningUi = v; }

    public static final class CaptionStyle {
        public static final int COLOR_UNSPECIFIED = 0x00FFFFFF;
        private static final int COLOR_NONE_OPAQUE = 0x000000FF;
        public static final int EDGE_TYPE_UNSPECIFIED = -1, EDGE_TYPE_NONE = 0, EDGE_TYPE_OUTLINE = 1, EDGE_TYPE_DROP_SHADOW = 2, EDGE_TYPE_RAISED = 3, EDGE_TYPE_DEPRESSED = 4;
        public static final int PRESET_CUSTOM = -1;
        private static final int WHITE = 0xFFFFFFFF, BLACK = 0xFF000000, YELLOW = 0xFFFFFF00, BLUE = 0xFF0000FF;
        private static final CaptionStyle WHITE_ON_BLACK = new CaptionStyle(WHITE, BLACK, EDGE_TYPE_NONE, BLACK, COLOR_NONE_OPAQUE, null);
        private static final CaptionStyle BLACK_ON_WHITE = new CaptionStyle(BLACK, WHITE, EDGE_TYPE_NONE, BLACK, COLOR_NONE_OPAQUE, null);
        private static final CaptionStyle YELLOW_ON_BLACK = new CaptionStyle(YELLOW, BLACK, EDGE_TYPE_NONE, BLACK, COLOR_NONE_OPAQUE, null);
        private static final CaptionStyle YELLOW_ON_BLUE = new CaptionStyle(YELLOW, BLUE, EDGE_TYPE_NONE, BLACK, COLOR_NONE_OPAQUE, null);
        private static final CaptionStyle UNSPECIFIED = new CaptionStyle(COLOR_UNSPECIFIED, COLOR_UNSPECIFIED, EDGE_TYPE_UNSPECIFIED, COLOR_UNSPECIFIED, COLOR_UNSPECIFIED, null);
        public static final CaptionStyle DEFAULT = WHITE_ON_BLACK;
        public static final CaptionStyle[] PRESETS = { WHITE_ON_BLACK, BLACK_ON_WHITE, YELLOW_ON_BLACK, YELLOW_ON_BLUE, UNSPECIFIED };
        public final int foregroundColor, backgroundColor, edgeType, edgeColor, windowColor;
        public final String mRawTypeface;
        private CaptionStyle(int fg, int bg, int et, int ec, int wc, String tf) { foregroundColor = fg; backgroundColor = bg; edgeType = et; edgeColor = ec; windowColor = wc; mRawTypeface = tf; }
        protected CaptionStyle() { this(WHITE, BLACK, EDGE_TYPE_NONE, BLACK, COLOR_NONE_OPAQUE, null); }
        public static CaptionStyle getCustomStyle(android.content.ContentResolver cr) { return DEFAULT; }
        public static boolean hasColor(int c) { return (c >>> 24) != 0 || (c & 0xFFFF00) == 0; }
        public CaptionStyle applyStyle(CaptionStyle o) {
            return new CaptionStyle(o.hasForegroundColor() ? o.foregroundColor : foregroundColor, o.hasBackgroundColor() ? o.backgroundColor : backgroundColor,
                o.hasEdgeType() ? o.edgeType : edgeType, o.hasEdgeColor() ? o.edgeColor : edgeColor, o.hasWindowColor() ? o.windowColor : windowColor,
                o.mRawTypeface != null ? o.mRawTypeface : mRawTypeface);
        }
        public android.graphics.Typeface getTypeface() { return mRawTypeface == null ? null : android.graphics.Typeface.create(mRawTypeface, android.graphics.Typeface.NORMAL); }
        public boolean hasBackgroundColor() { return hasColor(backgroundColor); }
        public boolean hasEdgeColor() { return hasColor(edgeColor); }
        public boolean hasEdgeType() { return edgeType != EDGE_TYPE_UNSPECIFIED; }
        public boolean hasForegroundColor() { return hasColor(foregroundColor); }
        public boolean hasWindowColor() { return hasColor(windowColor); }
    }
    public static abstract class CaptioningChangeListener {
        public CaptioningChangeListener() {}
        public void onEnabledChanged(boolean e) {}
        public void onFontScaleChanged(float s) {}
        public void onLocaleChanged(java.util.Locale l) {}
        public void onSystemAudioCaptioningChanged(boolean e) {}
        public void onSystemAudioCaptioningUiChanged(boolean e) {}
        public void onUserStyleChanged(CaptionStyle s) {}
    }
    public interface SystemAudioCaptioningAccessing {
        boolean isSystemAudioCaptioningUiEnabled(int u);
        void setSystemAudioCaptioningEnabled(boolean e, int u);
        void setSystemAudioCaptioningUiEnabled(boolean e, int u);
    }
}
