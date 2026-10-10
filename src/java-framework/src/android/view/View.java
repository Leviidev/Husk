package android.view;

import android.animation.StateListAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.*;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.animation.Animation;
import android.view.animation.Transformation;
import java.util.ArrayList;

/**
 * A view, as Android has it: measured, laid out and drawn by its window's root (husk.ViewRoot), which draws the whole tree into one
 * bitmap with a Canvas each frame something changed. Transforms, alpha and clipping are applied by the parent as it draws its children.
 */
public class View implements Drawable.Callback, KeyEvent.Callback {
    public static final int VISIBLE = 0, INVISIBLE = 4, GONE = 8, NO_ID = -1, FOCUSABLE = 1, NOT_FOCUSABLE = 0, FOCUSABLE_AUTO = 16;
    public static final int SYSTEM_UI_FLAG_VISIBLE = 0, SYSTEM_UI_FLAG_LOW_PROFILE = 1, SYSTEM_UI_FLAG_HIDE_NAVIGATION = 2, SYSTEM_UI_FLAG_FULLSCREEN = 4,
        SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR = 16, SYSTEM_UI_FLAG_LAYOUT_STABLE = 256, SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 512,
        SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 1024, SYSTEM_UI_FLAG_IMMERSIVE = 2048, SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 4096,
        SYSTEM_UI_FLAG_LIGHT_STATUS_BAR = 8192, STATUS_BAR_HIDDEN = 1, STATUS_BAR_VISIBLE = 0, SYSTEM_UI_LAYOUT_FLAGS = 1536;
    public static final int LAYER_TYPE_NONE = 0, LAYER_TYPE_SOFTWARE = 1, LAYER_TYPE_HARDWARE = 2, HAPTIC_FEEDBACK_ENABLED = 1;
    public static final int MEASURED_SIZE_MASK = 0x00ffffff, MEASURED_STATE_MASK = 0xff000000, MEASURED_HEIGHT_STATE_SHIFT = 16, MEASURED_STATE_TOO_SMALL = 0x01000000;
    public static final int LAYOUT_DIRECTION_LTR = 0, LAYOUT_DIRECTION_RTL = 1, LAYOUT_DIRECTION_INHERIT = 2, LAYOUT_DIRECTION_LOCALE = 3;
    public static final int TEXT_ALIGNMENT_INHERIT = 0, TEXT_ALIGNMENT_GRAVITY = 1, TEXT_ALIGNMENT_TEXT_START = 2, TEXT_ALIGNMENT_TEXT_END = 3,
        TEXT_ALIGNMENT_CENTER = 4, TEXT_ALIGNMENT_VIEW_START = 5, TEXT_ALIGNMENT_VIEW_END = 6;
    public static final int TEXT_DIRECTION_INHERIT = 0, TEXT_DIRECTION_FIRST_STRONG = 1, TEXT_DIRECTION_ANY_RTL = 2, TEXT_DIRECTION_LTR = 3, TEXT_DIRECTION_RTL = 4, TEXT_DIRECTION_LOCALE = 5;
    public static final int OVER_SCROLL_ALWAYS = 0, OVER_SCROLL_IF_CONTENT_SCROLLS = 1, OVER_SCROLL_NEVER = 2;
    public static final int SCROLLBARS_INSIDE_OVERLAY = 0, SCROLLBARS_INSIDE_INSET = 0x01000000, SCROLLBARS_OUTSIDE_OVERLAY = 0x02000000, SCROLLBARS_OUTSIDE_INSET = 0x03000000,
        SCROLLBAR_POSITION_DEFAULT = 0, SCROLLBAR_POSITION_LEFT = 1, SCROLLBAR_POSITION_RIGHT = 2, SCROLL_AXIS_NONE = 0, SCROLL_AXIS_HORIZONTAL = 1, SCROLL_AXIS_VERTICAL = 2,
        SCROLL_INDICATOR_TOP = 1, SCROLL_INDICATOR_BOTTOM = 2, SCROLL_INDICATOR_LEFT = 4, SCROLL_INDICATOR_RIGHT = 8, SCROLL_INDICATOR_START = 16, SCROLL_INDICATOR_END = 32;
    public static final int FOCUS_BACKWARD = 1, FOCUS_FORWARD = 2, FOCUS_LEFT = 17, FOCUS_UP = 33, FOCUS_RIGHT = 66, FOCUS_DOWN = 130,
        FOCUSABLES_ALL = 0, FOCUSABLES_TOUCH_MODE = 1;
    public static final int IMPORTANT_FOR_ACCESSIBILITY_AUTO = 0, IMPORTANT_FOR_ACCESSIBILITY_YES = 1, IMPORTANT_FOR_ACCESSIBILITY_NO = 2,
        IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS = 4, ACCESSIBILITY_LIVE_REGION_NONE = 0, ACCESSIBILITY_LIVE_REGION_POLITE = 1, ACCESSIBILITY_LIVE_REGION_ASSERTIVE = 2;
    public static final int IMPORTANT_FOR_AUTOFILL_AUTO = 0, IMPORTANT_FOR_AUTOFILL_YES = 1, IMPORTANT_FOR_AUTOFILL_NO = 2, IMPORTANT_FOR_AUTOFILL_NO_EXCLUDE_DESCENDANTS = 8,
        AUTOFILL_TYPE_NONE = 0, AUTOFILL_TYPE_TEXT = 1;
    public static final int DRAG_FLAG_GLOBAL = 256, SCREEN_STATE_OFF = 0, SCREEN_STATE_ON = 1, NO_ID_ = -1;
    public static final int DRAWING_CACHE_QUALITY_AUTO = 0, DRAWING_CACHE_QUALITY_LOW = 0x00080000, DRAWING_CACHE_QUALITY_HIGH = 0x00100000;
    public static final int[] EMPTY_STATE_SET = {}, ENABLED_STATE_SET = { android.R.attr.state_enabled }, PRESSED_STATE_SET = { android.R.attr.state_pressed },
        SELECTED_STATE_SET = { android.R.attr.state_selected }, FOCUSED_STATE_SET = { android.R.attr.state_focused }, WINDOW_FOCUSED_STATE_SET = { android.R.attr.state_window_focused },
        ENABLED_FOCUSED_STATE_SET = { android.R.attr.state_enabled, android.R.attr.state_focused }, PRESSED_ENABLED_STATE_SET = { android.R.attr.state_pressed, android.R.attr.state_enabled };
    public static final android.util.Property<View, Float> ALPHA = new FloatProp("alpha") { public void setValue(View v, float f) { v.setAlpha(f); } public Float get(View v) { return v.getAlpha(); } };
    public static final android.util.Property<View, Float> TRANSLATION_X = new FloatProp("translationX") { public void setValue(View v, float f) { v.setTranslationX(f); } public Float get(View v) { return v.getTranslationX(); } };
    public static final android.util.Property<View, Float> TRANSLATION_Y = new FloatProp("translationY") { public void setValue(View v, float f) { v.setTranslationY(f); } public Float get(View v) { return v.getTranslationY(); } };
    public static final android.util.Property<View, Float> TRANSLATION_Z = new FloatProp("translationZ") { public void setValue(View v, float f) { v.setTranslationZ(f); } public Float get(View v) { return v.getTranslationZ(); } };
    public static final android.util.Property<View, Float> X = new FloatProp("x") { public void setValue(View v, float f) { v.setX(f); } public Float get(View v) { return v.getX(); } };
    public static final android.util.Property<View, Float> Y = new FloatProp("y") { public void setValue(View v, float f) { v.setY(f); } public Float get(View v) { return v.getY(); } };
    public static final android.util.Property<View, Float> Z = new FloatProp("z") { public void setValue(View v, float f) { v.setZ(f); } public Float get(View v) { return v.getZ(); } };
    public static final android.util.Property<View, Float> ROTATION = new FloatProp("rotation") { public void setValue(View v, float f) { v.setRotation(f); } public Float get(View v) { return v.getRotation(); } };
    public static final android.util.Property<View, Float> ROTATION_X = new FloatProp("rotationX") { public void setValue(View v, float f) { v.setRotationX(f); } public Float get(View v) { return v.getRotationX(); } };
    public static final android.util.Property<View, Float> ROTATION_Y = new FloatProp("rotationY") { public void setValue(View v, float f) { v.setRotationY(f); } public Float get(View v) { return v.getRotationY(); } };
    public static final android.util.Property<View, Float> SCALE_X = new FloatProp("scaleX") { public void setValue(View v, float f) { v.setScaleX(f); } public Float get(View v) { return v.getScaleX(); } };
    public static final android.util.Property<View, Float> SCALE_Y = new FloatProp("scaleY") { public void setValue(View v, float f) { v.setScaleY(f); } public Float get(View v) { return v.getScaleY(); } };
    private static abstract class FloatProp extends android.util.FloatProperty<View> { FloatProp(String n) { super(n); } }

    public interface OnTouchListener { boolean onTouch(View v, MotionEvent e); }
    public interface OnKeyListener { boolean onKey(View v, int code, KeyEvent e); }
    public interface OnClickListener { void onClick(View v); }
    public interface OnLongClickListener { boolean onLongClick(View v);
        // ---- generated by tools/compat/fillmembers.py (OnLongClickListener): the platform's members this class does not write (signatures only)
        default boolean onLongClickUseDefaultHapticFeedback(android.view.View p0) { return false; }
        // ---- end of generated members (OnLongClickListener)
    }
    public interface OnContextClickListener { boolean onContextClick(View v); }
    public interface OnGenericMotionListener { boolean onGenericMotion(View v, MotionEvent e); }
    public interface OnHoverListener { boolean onHover(View v, MotionEvent e); }
    public interface OnFocusChangeListener { void onFocusChange(View v, boolean focus); }
    public interface OnSystemUiVisibilityChangeListener { void onSystemUiVisibilityChange(int v); }
    public interface OnLayoutChangeListener { void onLayoutChange(View v, int l, int t, int r, int b, int ol, int ot, int or, int ob); }
    public interface OnApplyWindowInsetsListener { WindowInsets onApplyWindowInsets(View v, WindowInsets i); }
    public interface OnAttachStateChangeListener { void onViewAttachedToWindow(View v); void onViewDetachedFromWindow(View v); }
    public interface OnCreateContextMenuListener { void onCreateContextMenu(ContextMenu m, View v, ContextMenu.ContextMenuInfo info); }
    public interface OnDragListener { boolean onDrag(View v, DragEvent e); }
    public interface OnScrollChangeListener { void onScrollChange(View v, int x, int y, int ox, int oy); }
    public interface OnCapturedPointerListener { boolean onCapturedPointer(View v, MotionEvent e); }
    public interface OnUnhandledKeyEventListener { boolean onUnhandledKeyEvent(View v, KeyEvent e); }
    public static class BaseSavedState extends android.view.AbsSavedState {
        public BaseSavedState(Parcelable superState) { super(superState); }
        public BaseSavedState(android.os.Parcel source) { super(source); }
        public BaseSavedState(android.os.Parcel source, ClassLoader loader) { super(source, loader); }
        // ---- generated by tools/compat/fillmembers.py (BaseSavedState): the platform's members this class does not write (signatures only)
        public static android.os.Parcelable.Creator CREATOR;
        // ---- end of generated members (BaseSavedState)
    }
    public static class AccessibilityDelegate {
        public void sendAccessibilityEvent(View h, int t) {}
        public boolean performAccessibilityAction(View h, int a, Bundle args) { return false; }
        public void onInitializeAccessibilityNodeInfo(View h, AccessibilityNodeInfo i) {}
        public void onInitializeAccessibilityEvent(View h, android.view.accessibility.AccessibilityEvent e) {}
        public void onPopulateAccessibilityEvent(View h, android.view.accessibility.AccessibilityEvent e) {}
        public boolean dispatchPopulateAccessibilityEvent(View h, android.view.accessibility.AccessibilityEvent e) { return false; }
        public boolean onRequestSendAccessibilityEvent(ViewGroup h, View c, android.view.accessibility.AccessibilityEvent e) { return true; }
        public android.view.accessibility.AccessibilityNodeProvider getAccessibilityNodeProvider(View h) { return null; }
        public void sendAccessibilityEventUnchecked(View h, android.view.accessibility.AccessibilityEvent e) {}
        public void addExtraDataToAccessibilityNodeInfo(View h, AccessibilityNodeInfo i, String k, Bundle b) {}
        // ---- generated by tools/compat/fillmembers.py (AccessibilityDelegate): the platform's members this class does not write (signatures only)
        public android.view.accessibility.AccessibilityNodeInfo createAccessibilityNodeInfo(android.view.View p0) { return null; }
        // ---- end of generated members (AccessibilityDelegate)
    }
    public static class MeasureSpec {
        public static final int UNSPECIFIED = 0, EXACTLY = 0x40000000, AT_MOST = 0x80000000;
        private static final int MODE_MASK = 0xC0000000;
        public static int makeMeasureSpec(int size, int mode) { return (size & ~MODE_MASK) | (mode & MODE_MASK); }
        public static int makeSafeMeasureSpec(int size, int mode) { return mode == UNSPECIFIED ? 0 : makeMeasureSpec(size, mode); }
        public static int getMode(int spec) { return spec & MODE_MASK; }
        public static int getSize(int spec) { return spec & ~MODE_MASK; }
        public static String toString(int spec) { int m = getMode(spec); return "MeasureSpec: " + (m == EXACTLY ? "EXACTLY " : m == AT_MOST ? "AT_MOST " : "UNSPECIFIED ") + getSize(spec); }
    }

    // ---- private flags
    static final int PFLAG_FORCE_LAYOUT = 1, PFLAG_LAYOUT_REQUIRED = 2, PFLAG_DIRTY = 4, PFLAG_PRESSED = 8, PFLAG_SELECTED = 16, PFLAG_ACTIVATED = 32,
        PFLAG_FOCUSED = 64, PFLAG_HOVERED = 128, PFLAG_DRAWABLE_STATE_DIRTY = 256, PFLAG_WILL_NOT_DRAW = 512, PFLAG_HAS_PIVOT = 1024, PFLAG_PREPRESSED = 2048,
        PFLAG_CLIP_TO_OUTLINE = 4096, PFLAG_DUPLICATE_PARENT_STATE = 8192, PFLAG_SAVE_DISABLED = 16384;

    protected Context mContext;
    ViewGroup mParent;
    husk.ViewRoot mRoot;
    int mPrivateFlags = PFLAG_FORCE_LAYOUT | PFLAG_DRAWABLE_STATE_DIRTY;
    private int mID = NO_ID;
    private Object mTag;
    private SparseArray<Object> mKeyedTags;
    protected int mLeft, mTop, mRight, mBottom;
    protected int mScrollX, mScrollY;
    protected int mPaddingLeft, mPaddingTop, mPaddingRight, mPaddingBottom;
    private int mUserPaddingStart = Integer.MIN_VALUE, mUserPaddingEnd = Integer.MIN_VALUE;
    private int mMinWidth, mMinHeight;
    private int mMeasuredWidth, mMeasuredHeight, mOldWidthSpec = Integer.MIN_VALUE, mOldHeightSpec = Integer.MIN_VALUE;
    protected ViewGroup.LayoutParams mLayoutParams;
    private int mVisibility = VISIBLE;
    private boolean mEnabled = true, mClickable, mLongClickable, mContextClickable, mFocusable, mFocusableInTouchMode, mHapticEnabled = true, mSoundEnabled = true,
        mKeepScreenOn, mFitsSystemWindows, mHasPerformedLongPress, mInLayout, mIgnoreNextUp;
    private Drawable mBackground, mForeground;
    private ColorStateList mBackgroundTint;
    private PorterDuff.Mode mBackgroundTintMode;
    private int mForegroundGravity = Gravity.FILL;
    private float mAlpha = 1, mTranslationX, mTranslationY, mTranslationZ, mElevation, mScaleX = 1, mScaleY = 1, mRotation, mRotationX, mRotationY, mPivotX, mPivotY;
    private Matrix mMatrix, mInverse;
    private boolean mMatrixDirty = true;
    private ViewOutlineProvider mOutlineProvider = ViewOutlineProvider.BACKGROUND;
    private CharSequence mContentDescription, mTooltip;
    private int mSystemUiVisibility, mLayoutDirection = LAYOUT_DIRECTION_LTR, mTextAlignment = TEXT_ALIGNMENT_GRAVITY, mTextDirection, mOverScrollMode = OVER_SCROLL_IF_CONTENT_SCROLLS,
        mImportantForAccessibility, mScrollIndicators, mLabelFor = NO_ID, mNextFocusDown = NO_ID, mNextFocusUp = NO_ID, mNextFocusLeft = NO_ID, mNextFocusRight = NO_ID, mNextFocusForward = NO_ID;
    private boolean mVerticalScrollBar, mHorizontalScrollBar;
    int mWindowAttachCount;
    private Animation mAnimation;
    private final Transformation mAnimTx = new Transformation();
    private ViewPropertyAnimator mAnimator;
    private StateListAnimator mStateListAnimator;
    private int[] mDrawableState;
    private TouchDelegate mTouchDelegate;
    private AccessibilityDelegate mAccessibilityDelegate;
    private ViewTreeObserver mFloatingObserver;
    private WindowInsets mLastInsets;
    private Rect mClipBounds;
    private int mLayerType;
    private Paint mLayerPaint;
    private String mTransitionName;
    private Object mAutofillHints;

    // ---- listeners
    private OnTouchListener mOnTouch;
    private OnKeyListener mOnKey;
    OnClickListener mOnClick;
    private OnLongClickListener mOnLongClick;
    private OnContextClickListener mOnContextClick;
    private OnGenericMotionListener mOnGeneric;
    private OnHoverListener mOnHover;
    private OnFocusChangeListener mOnFocusChange;
    private OnApplyWindowInsetsListener mOnApplyInsets;
    private OnScrollChangeListener mOnScrollChange;
    private OnSystemUiVisibilityChangeListener mOnSystemUi;
    private OnCreateContextMenuListener mOnContextMenu;
    private OnDragListener mOnDrag;
    private ArrayList<OnLayoutChangeListener> mOnLayoutChange;
    private ArrayList<OnAttachStateChangeListener> mOnAttach;
    private ArrayList<OnUnhandledKeyEventListener> mOnUnhandledKey;

    // ---- construction
    public View(Context c) {
        mContext = c;
        if (c == null) throw new NullPointerException("Context must not be null");
    }
    public View(Context c, AttributeSet attrs) { this(c, attrs, 0); }
    public View(Context c, AttributeSet attrs, int defStyleAttr) { this(c, attrs, defStyleAttr, 0); }
    public View(Context c, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        this(c);
        if (attrs == null && defStyleAttr == 0 && defStyleRes == 0) return;
        TypedArray a = c.obtainStyledAttributes(attrs, husk.S.View, defStyleAttr, defStyleRes);
        try { readViewAttributes(a); } finally { a.recycle(); }
    }
    private void readViewAttributes(TypedArray a) {
        int pad = -1, pl = -1, pt = -1, pr = -1, pb = -1, ph = -1, pv = -1, ps = Integer.MIN_VALUE, pe = Integer.MIN_VALUE;
        float tx = 0, ty = 0, tz = 0, rot = 0, rx = 0, ry = 0, sx = 1, sy = 1;
        for (int i = 0, n = a.getIndexCount(); i < n; i++) {
            int at = a.getIndex(i);
            switch (at) {
            case husk.S.View_id: mID = a.getResourceId(at, NO_ID); break;
            case husk.S.View_tag: mTag = a.getText(at); break;
            case husk.S.View_background: mBackground = a.getDrawable(at); break;
            case husk.S.View_padding: pad = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_paddingLeft: pl = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_paddingTop: pt = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_paddingRight: pr = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_paddingBottom: pb = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_paddingStart: ps = a.getDimensionPixelSize(at, Integer.MIN_VALUE); break;
            case husk.S.View_paddingEnd: pe = a.getDimensionPixelSize(at, Integer.MIN_VALUE); break;
            case husk.S.View_paddingHorizontal: ph = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_paddingVertical: pv = a.getDimensionPixelSize(at, -1); break;
            case husk.S.View_visibility: { int v = a.getInt(at, 0); mVisibility = v == 1 ? INVISIBLE : v == 2 ? GONE : VISIBLE; break; }
            case husk.S.View_alpha: mAlpha = a.getFloat(at, 1); break;
            case husk.S.View_clickable: mClickable = a.getBoolean(at, false); break;
            case husk.S.View_longClickable: mLongClickable = a.getBoolean(at, false); break;
            case husk.S.View_contextClickable: mContextClickable = a.getBoolean(at, false); break;
            case husk.S.View_focusable: { android.util.TypedValue v = a.peekValue(at); mFocusable = v != null && v.data != 0; break; }
            case husk.S.View_focusableInTouchMode: mFocusableInTouchMode = a.getBoolean(at, false); if (mFocusableInTouchMode) mFocusable = true; break;
            case husk.S.View_enabled: mEnabled = a.getBoolean(at, true); break;
            case husk.S.View_minWidth: mMinWidth = a.getDimensionPixelSize(at, 0); break;
            case husk.S.View_minHeight: mMinHeight = a.getDimensionPixelSize(at, 0); break;
            case husk.S.View_translationX: tx = a.getDimension(at, 0); break;
            case husk.S.View_translationY: ty = a.getDimension(at, 0); break;
            case husk.S.View_translationZ: tz = a.getDimension(at, 0); break;
            case husk.S.View_elevation: mElevation = a.getDimension(at, 0); break;
            case husk.S.View_rotation: rot = a.getFloat(at, 0); break;
            case husk.S.View_rotationX: rx = a.getFloat(at, 0); break;
            case husk.S.View_rotationY: ry = a.getFloat(at, 0); break;
            case husk.S.View_scaleX: sx = a.getFloat(at, 1); break;
            case husk.S.View_scaleY: sy = a.getFloat(at, 1); break;
            case husk.S.View_transformPivotX: setPivotX(a.getDimension(at, 0)); break;
            case husk.S.View_transformPivotY: setPivotY(a.getDimension(at, 0)); break;
            case husk.S.View_contentDescription: mContentDescription = a.getText(at); break;
            case husk.S.View_tooltipText: mTooltip = a.getText(at); break;
            case husk.S.View_foreground: mForeground = a.getDrawable(at); break;
            case husk.S.View_foregroundGravity: mForegroundGravity = a.getInt(at, Gravity.FILL); break;
            case husk.S.View_backgroundTint: mBackgroundTint = a.getColorStateList(at); break;
            case husk.S.View_backgroundTintMode: mBackgroundTintMode = parseTintMode(a.getInt(at, -1)); break;
            case husk.S.View_fitsSystemWindows: mFitsSystemWindows = a.getBoolean(at, false); break;
            case husk.S.View_clipToOutline: if (a.getBoolean(at, false)) mPrivateFlags |= PFLAG_CLIP_TO_OUTLINE; break;
            case husk.S.View_keepScreenOn: mKeepScreenOn = a.getBoolean(at, false); break;
            case husk.S.View_soundEffectsEnabled: mSoundEnabled = a.getBoolean(at, true); break;
            case husk.S.View_hapticFeedbackEnabled: mHapticEnabled = a.getBoolean(at, true); break;
            case husk.S.View_saveEnabled: if (!a.getBoolean(at, true)) mPrivateFlags |= PFLAG_SAVE_DISABLED; break;
            case husk.S.View_duplicateParentState: if (a.getBoolean(at, false)) mPrivateFlags |= PFLAG_DUPLICATE_PARENT_STATE; break;
            case husk.S.View_textAlignment: mTextAlignment = a.getInt(at, TEXT_ALIGNMENT_GRAVITY); break;
            case husk.S.View_textDirection: mTextDirection = a.getInt(at, 0); break;
            case husk.S.View_overScrollMode: mOverScrollMode = a.getInt(at, OVER_SCROLL_IF_CONTENT_SCROLLS); break;
            case husk.S.View_importantForAccessibility: mImportantForAccessibility = a.getInt(at, 0); break;
            case husk.S.View_scrollX: mScrollX = a.getDimensionPixelOffset(at, 0); break;
            case husk.S.View_scrollY: mScrollY = a.getDimensionPixelOffset(at, 0); break;
            case husk.S.View_scrollbars: { int s = a.getInt(at, 0); mHorizontalScrollBar = (s & 0x100) != 0; mVerticalScrollBar = (s & 0x200) != 0; break; }
            case husk.S.View_nextFocusDown: mNextFocusDown = a.getResourceId(at, NO_ID); break;
            case husk.S.View_nextFocusUp: mNextFocusUp = a.getResourceId(at, NO_ID); break;
            case husk.S.View_nextFocusLeft: mNextFocusLeft = a.getResourceId(at, NO_ID); break;
            case husk.S.View_nextFocusRight: mNextFocusRight = a.getResourceId(at, NO_ID); break;
            case husk.S.View_onClick: {
                final String handler = a.getString(at);
                if (handler != null) setOnClickListener(new DeclaredOnClickListener(this, handler));
                break;
            }
            case husk.S.View_outlineProvider: { int o = a.getInt(at, 0); mOutlineProvider = o == 1 ? null : o == 2 ? ViewOutlineProvider.BOUNDS : o == 3 ? ViewOutlineProvider.PADDED_BOUNDS : ViewOutlineProvider.BACKGROUND; break; }
            }
        }
        if (pad >= 0) { mPaddingLeft = mPaddingTop = mPaddingRight = mPaddingBottom = pad; }
        else {
            if (ph >= 0) { mPaddingLeft = mPaddingRight = ph; }
            if (pv >= 0) { mPaddingTop = mPaddingBottom = pv; }
            if (pl >= 0) mPaddingLeft = pl;
            if (pt >= 0) mPaddingTop = pt;
            if (pr >= 0) mPaddingRight = pr;
            if (pb >= 0) mPaddingBottom = pb;
            if (ps != Integer.MIN_VALUE) { mPaddingLeft = ps; mUserPaddingStart = ps; }
            if (pe != Integer.MIN_VALUE) { mPaddingRight = pe; mUserPaddingEnd = pe; }
        }
        if (mBackground != null) {
            Rect p = new Rect();
            if (mBackground.getPadding(p) && pad < 0) {
                if (pl < 0 && ph < 0 && ps == Integer.MIN_VALUE) mPaddingLeft = p.left;
                if (pt < 0 && pv < 0) mPaddingTop = p.top;
                if (pr < 0 && ph < 0 && pe == Integer.MIN_VALUE) mPaddingRight = p.right;
                if (pb < 0 && pv < 0) mPaddingBottom = p.bottom;
            }
            mBackground.setCallback(this);
            applyBackgroundTint();
        }
        if (mForeground != null) mForeground.setCallback(this);
        mTranslationX = tx; mTranslationY = ty; mTranslationZ = tz; mRotation = rot; mRotationX = rx; mRotationY = ry; mScaleX = sx; mScaleY = sy;
        mMatrixDirty = true;
    }
    static PorterDuff.Mode parseTintMode(int v) {
        switch (v) { case 3: return PorterDuff.Mode.SRC_OVER; case 5: return PorterDuff.Mode.SRC_IN; case 9: return PorterDuff.Mode.SRC_ATOP;
        case 14: return PorterDuff.Mode.MULTIPLY; case 15: return PorterDuff.Mode.SCREEN; case 16: return PorterDuff.Mode.ADD; default: return null; }
    }
    private static final class DeclaredOnClickListener implements OnClickListener {
        private final View mHost; private final String mName; private java.lang.reflect.Method mMethod; private Context mCtx;
        DeclaredOnClickListener(View host, String name) { mHost = host; mName = name; }
        public void onClick(View v) {
            if (mMethod == null) {
                for (Context c = mHost.getContext(); c != null; c = c instanceof android.content.ContextWrapper ? ((android.content.ContextWrapper) c).getBaseContext() : null) {
                    try { mMethod = c.getClass().getMethod(mName, View.class); mCtx = c; break; } catch (NoSuchMethodException e) {}
                    if (!(c instanceof android.content.ContextWrapper) || ((android.content.ContextWrapper) c).getBaseContext() == c) break;
                }
                if (mMethod == null) throw new IllegalStateException("Could not find method " + mName + "(View) in a parent or ancestor Context for android:onClick attribute");
            }
            try { mMethod.invoke(mCtx, v); } catch (Exception e) { throw new IllegalStateException("Could not execute method for android:onClick", e); }
        }
    }

    // ---- identity and tags
    public Context getContext() { return mContext; }
    public Resources getResources() { return mContext.getResources(); }
    public int getId() { return mID; }
    public void setId(int id) { mID = id; }
    public static int generateViewId() { return husk.ViewRoot.nextViewId(); }
    public Object getTag() { return mTag; }
    public void setTag(Object t) { mTag = t; }
    public Object getTag(int key) { return mKeyedTags == null ? null : mKeyedTags.get(key); }
    public void setTag(int key, Object t) { if (mKeyedTags == null) mKeyedTags = new SparseArray<>(); mKeyedTags.put(key, t); }
    public final <T extends View> T findViewById(int id) { return id == NO_ID ? null : findViewTraversal(id); }
    public final <T extends View> T requireViewById(int id) { T v = findViewById(id); if (v == null) throw new IllegalArgumentException("ID does not reference a View inside this View"); return v; }
    @SuppressWarnings("unchecked") protected <T extends View> T findViewTraversal(int id) { return id == mID ? (T) this : null; }
    public final <T extends View> T findViewWithTag(Object tag) { return tag == null ? null : findViewWithTagTraversal(tag); }
    @SuppressWarnings("unchecked") protected <T extends View> T findViewWithTagTraversal(Object tag) { return tag.equals(mTag) ? (T) this : null; }
    public void findViewsWithText(ArrayList<View> out, CharSequence text, int flags) {
        if ((flags & 2) != 0 && mContentDescription != null && text != null && mContentDescription.toString().toLowerCase().contains(text.toString().toLowerCase())) out.add(this);
    }
    public String getTransitionName() { return mTransitionName; }
    public void setTransitionName(String n) { mTransitionName = n; }
    @Override public String toString() {
        StringBuilder b = new StringBuilder(getClass().getName()).append('{').append(Integer.toHexString(System.identityHashCode(this)));
        b.append(mVisibility == VISIBLE ? " V" : mVisibility == INVISIBLE ? " I" : " G").append(' ').append(mLeft).append(',').append(mTop).append('-').append(mRight).append(',').append(mBottom);
        if (mID != NO_ID) b.append(" #").append(Integer.toHexString(mID));
        return b.append('}').toString();
    }

    // ---- hierarchy
    public final ViewParent getParent() { return mParent != null ? mParent : mRoot; }
    public View getRootView() { View v = this; while (v.mParent != null) v = v.mParent; return v; }
    public boolean isAttachedToWindow() { return mRoot != null; }
    public boolean isLaidOut() { return mRoot != null && (mPrivateFlags & PFLAG_FORCE_LAYOUT) == 0; }
    public android.os.IBinder getWindowToken() { return mRoot == null ? null : mRoot.token(); }
    public android.os.IBinder getApplicationWindowToken() { return getWindowToken(); }
    public Display getDisplay() { return mRoot == null ? null : ((WindowManager) mContext.getSystemService(Context.WINDOW_SERVICE)).getDefaultDisplay(); }
    public WindowId getWindowId() { return null; }
    public ViewTreeObserver getViewTreeObserver() {
        if (mRoot != null) return mRoot.observer();
        if (mFloatingObserver == null) mFloatingObserver = new ViewTreeObserver();
        return mFloatingObserver;
    }
    public Handler getHandler() { return mRoot != null ? mRoot.handler() : null; }
    private Handler anyHandler() { Handler h = getHandler(); return h != null ? h : husk.ViewRoot.mainHandler(); }
    public boolean post(Runnable r) { return anyHandler().post(r); }
    public boolean postDelayed(Runnable r, long ms) { return anyHandler().postDelayed(r, ms); }
    public void postOnAnimation(Runnable r) { Choreographer.getMainThreadInstance().postCallback(Choreographer.CALLBACK_ANIMATION, r, null); }
    public void postOnAnimationDelayed(Runnable r, long ms) { Choreographer.getMainThreadInstance().postCallbackDelayed(Choreographer.CALLBACK_ANIMATION, r, null, ms); }
    public boolean removeCallbacks(Runnable r) {
        if (r != null) { anyHandler().removeCallbacks(r); Choreographer.getMainThreadInstance().removeCallbacks(Choreographer.CALLBACK_ANIMATION, r, null); }
        return true;
    }

    /** For husk.ViewRoot: the tree is in a window now / no longer. */
    public final void huskAttach(husk.ViewRoot root) { dispatchAttachedToWindow(root, VISIBLE); }
    public final void huskDetach() { dispatchDetachedFromWindow(); }
    void dispatchAttachedToWindow(husk.ViewRoot root, int visibility) {
        mRoot = root;
        if (mLayoutParams != null) mLayoutParams.resolveLayoutDirection(getLayoutDirection());
        mWindowAttachCount++;
        mPrivateFlags |= PFLAG_DRAWABLE_STATE_DIRTY;
        if (mFloatingObserver != null) { root.observer().merge(mFloatingObserver); mFloatingObserver = null; }
        onAttachedToWindow();
        if (mOnAttach != null) for (OnAttachStateChangeListener l : new ArrayList<>(mOnAttach)) l.onViewAttachedToWindow(this);
        onWindowVisibilityChanged(VISIBLE);
        if (mPrivateFlags != 0 && (mPrivateFlags & PFLAG_DRAWABLE_STATE_DIRTY) != 0) refreshDrawableState();
        if (mKeepScreenOn) root.keepScreenOn(true);
        if (mOverlay != null) mOverlay.huskAttached(root);
    }
    void dispatchDetachedFromWindow() {
        if (mOverlay != null) mOverlay.huskDetached();
        if (mAnimator != null) mAnimator.cancel();
        onWindowVisibilityChanged(GONE);
        onDetachedFromWindow();
        if (mOnAttach != null) for (OnAttachStateChangeListener l : new ArrayList<>(mOnAttach)) l.onViewDetachedFromWindow(this);
        if (mRoot != null && mRoot.focused() == this) mRoot.setFocused(null);
        mRoot = null;
    }
    protected void onAttachedToWindow() {}
    protected void onDetachedFromWindow() {}
    protected void onWindowVisibilityChanged(int v) {}
    public int getWindowVisibility() { return mRoot != null ? VISIBLE : GONE; }
    private boolean mRevealOnFocusHint = true;
    public final void setRevealOnFocusHint(boolean h) { mRevealOnFocusHint = h; }
    public final boolean getRevealOnFocusHint() { return mRevealOnFocusHint; }
    public void onWindowFocusChanged(boolean f) { refreshDrawableState(); }
    public void dispatchWindowFocusChanged(boolean f) { onWindowFocusChanged(f); }
    public boolean hasWindowFocus() { return mRoot != null && mRoot.hasWindowFocus(); }
    public void onVisibilityAggregated(boolean v) {}
    protected void onVisibilityChanged(View changed, int v) {}
    public void dispatchVisibilityChanged(View changed, int v) { onVisibilityChanged(changed, v); }
    public void dispatchWindowVisibilityChanged(int v) { onWindowVisibilityChanged(v); }
    public void addOnAttachStateChangeListener(OnAttachStateChangeListener l) { if (mOnAttach == null) mOnAttach = new ArrayList<>(); mOnAttach.add(l); }
    public void removeOnAttachStateChangeListener(OnAttachStateChangeListener l) { if (mOnAttach != null) mOnAttach.remove(l); }
    public int getWindowAttachCount() { return mWindowAttachCount; }
    public void onFinishInflate() {}
    protected void onFinishInflateHusk() { onFinishInflate(); }

    // ---- visibility and state
    public int getVisibility() { return mVisibility; }
    public void setVisibility(int v) {
        if (mVisibility == v) return;
        int old = mVisibility;
        mVisibility = v;
        if (v == GONE || old == GONE) requestLayout();
        invalidateParent();
        if (mParent != null) mParent.onChildVisibilityChanged(this, old, v);
        dispatchVisibilityChanged(this, v);
        if (v != VISIBLE && hasFocus()) clearFocus();
    }
    public boolean isShown() {
        View v = this;
        while (v != null) { if (v.mVisibility != VISIBLE) return false; if (v.mParent == null) return v.mRoot != null; v = v.mParent; }
        return false;
    }
    public boolean isEnabled() { return mEnabled; }
    public void setEnabled(boolean e) { if (mEnabled != e) { mEnabled = e; refreshDrawableState(); invalidate(); if (!e) cancelPendingInputEvents(); } }
    public boolean isClickable() { return mClickable; }
    public void setClickable(boolean c) { mClickable = c; }
    public boolean isLongClickable() { return mLongClickable; }
    public void setLongClickable(boolean c) { mLongClickable = c; }
    public boolean isContextClickable() { return mContextClickable; }
    public void setContextClickable(boolean c) { mContextClickable = c; }
    public boolean isPressed() { return (mPrivateFlags & PFLAG_PRESSED) != 0; }
    public void setPressed(boolean p) {
        boolean changed = p != isPressed();
        if (p) mPrivateFlags |= PFLAG_PRESSED; else mPrivateFlags &= ~PFLAG_PRESSED;
        if (changed) { refreshDrawableState(); dispatchSetPressed(p); }
    }
    protected void dispatchSetPressed(boolean p) {}
    public boolean isSelected() { return (mPrivateFlags & PFLAG_SELECTED) != 0; }
    public void setSelected(boolean s) {
        if (s == isSelected()) return;
        if (s) mPrivateFlags |= PFLAG_SELECTED; else mPrivateFlags &= ~PFLAG_SELECTED;
        invalidate(); refreshDrawableState(); dispatchSetSelected(s);
    }
    protected void dispatchSetSelected(boolean s) {}
    public boolean isActivated() { return (mPrivateFlags & PFLAG_ACTIVATED) != 0; }
    public void setActivated(boolean a) {
        if (a == isActivated()) return;
        if (a) mPrivateFlags |= PFLAG_ACTIVATED; else mPrivateFlags &= ~PFLAG_ACTIVATED;
        invalidate(); refreshDrawableState(); dispatchSetActivated(a);
    }
    protected void dispatchSetActivated(boolean a) {}
    public boolean isHovered() { return (mPrivateFlags & PFLAG_HOVERED) != 0; }
    public void setHovered(boolean h) { if (h != isHovered()) { if (h) mPrivateFlags |= PFLAG_HOVERED; else mPrivateFlags &= ~PFLAG_HOVERED; refreshDrawableState(); onHoverChanged(h); } }
    public void onHoverChanged(boolean h) {}
    public boolean isDuplicateParentStateEnabled() { return (mPrivateFlags & PFLAG_DUPLICATE_PARENT_STATE) != 0; }
    public void setDuplicateParentStateEnabled(boolean e) { if (e) mPrivateFlags |= PFLAG_DUPLICATE_PARENT_STATE; else mPrivateFlags &= ~PFLAG_DUPLICATE_PARENT_STATE; }
    public boolean isInEditMode() { return false; }
    public boolean isHardwareAccelerated() { return false; }
    public boolean isInTouchMode() { return true; }
    public boolean isSaveEnabled() { return (mPrivateFlags & PFLAG_SAVE_DISABLED) == 0; }
    public void setSaveEnabled(boolean e) { if (e) mPrivateFlags &= ~PFLAG_SAVE_DISABLED; else mPrivateFlags |= PFLAG_SAVE_DISABLED; }
    public void setSaveFromParentEnabled(boolean e) {}
    public boolean isSaveFromParentEnabled() { return true; }
    public boolean isOpaque() { return mBackground != null && mBackground.getOpacity() == PixelFormat.OPAQUE && mAlpha >= 1; }
    public boolean isDirty() { return (mPrivateFlags & PFLAG_DIRTY) != 0; }
    public boolean isLayoutRequested() { return (mPrivateFlags & PFLAG_FORCE_LAYOUT) != 0; }
    public boolean isInLayout() { return mRoot != null && mRoot.isInLayout(); }
    public boolean isTemporarilyDetached() { return false; }
    public boolean isAttachedToWindowHusk() { return mRoot != null; }
    public boolean isLayoutDirectionResolved() { return true; }
    public boolean isPaddingRelative() { return mUserPaddingStart != Integer.MIN_VALUE || mUserPaddingEnd != Integer.MIN_VALUE; }
    public boolean isVerticalScrollBarEnabled() { return mVerticalScrollBar; }
    public void setVerticalScrollBarEnabled(boolean e) { mVerticalScrollBar = e; }
    public boolean isHorizontalScrollBarEnabled() { return mHorizontalScrollBar; }
    public void setHorizontalScrollBarEnabled(boolean e) { mHorizontalScrollBar = e; }
    public void setScrollBarStyle(int s) {}
    public int getScrollBarStyle() { return SCROLLBARS_INSIDE_OVERLAY; }
    public void setScrollbarFadingEnabled(boolean e) {}
    public void setScrollBarSize(int s) {}
    public void setVerticalScrollbarPosition(int p) {}
    public void setScrollIndicators(int i) { mScrollIndicators = i; }
    public void setScrollIndicators(int i, int mask) { mScrollIndicators = (mScrollIndicators & ~mask) | (i & mask); }
    public int getScrollIndicators() { return mScrollIndicators; }
    public void setVerticalFadingEdgeEnabled(boolean e) {}
    public void setHorizontalFadingEdgeEnabled(boolean e) {}
    public void setFadingEdgeLength(int l) {}
    public boolean isVerticalFadingEdgeEnabled() { return false; }
    public boolean isHorizontalFadingEdgeEnabled() { return false; }
    public int getVerticalFadingEdgeLength() { return 0; }
    public int getHorizontalFadingEdgeLength() { return 0; }
    protected float getTopFadingEdgeStrength() { return 0; }
    protected float getBottomFadingEdgeStrength() { return 0; }
    public int getOverScrollMode() { return mOverScrollMode; }
    public void setOverScrollMode(int m) { mOverScrollMode = m; }
    public void setNestedScrollingEnabled(boolean e) {}
    public boolean isNestedScrollingEnabled() { return false; }
    public boolean startNestedScroll(int axes) { return false; }
    public void stopNestedScroll() {}
    public boolean hasNestedScrollingParent() { return false; }
    public boolean dispatchNestedScroll(int dxc, int dyc, int dxu, int dyu, int[] off) { return false; }
    public boolean dispatchNestedPreScroll(int dx, int dy, int[] consumed, int[] off) { return false; }
    public boolean dispatchNestedFling(float vx, float vy, boolean consumed) { return false; }
    public boolean dispatchNestedPreFling(float vx, float vy) { return false; }
    public int getLayerType() { return mLayerType; }
    public void setLayerType(int t, Paint p) { mLayerType = t; mLayerPaint = p; invalidate(); }
    public void setLayerPaint(Paint p) { mLayerPaint = p; invalidate(); }
    public void buildLayer() {}
    public void setDrawingCacheEnabled(boolean e) {}
    public boolean isDrawingCacheEnabled() { return false; }
    public void buildDrawingCache() {}
    public void buildDrawingCache(boolean a) {}
    public void destroyDrawingCache() {}
    public void setDrawingCacheQuality(int q) {}
    public void setDrawingCacheBackgroundColor(int c) {}
    public Bitmap getDrawingCache() { return getDrawingCache(false); }
    public Bitmap getDrawingCache(boolean a) {
        if (getWidth() <= 0 || getHeight() <= 0) return null;
        Bitmap b = Bitmap.createBitmap(getWidth(), getHeight(), Bitmap.Config.ARGB_8888);
        Canvas c = new Canvas(b); draw(c); c.huskRelease();
        return b;
    }
    public void setWillNotDraw(boolean w) { if (w) mPrivateFlags |= PFLAG_WILL_NOT_DRAW; else mPrivateFlags &= ~PFLAG_WILL_NOT_DRAW; }
    public boolean willNotDraw() { return (mPrivateFlags & PFLAG_WILL_NOT_DRAW) != 0; }
    public void setWillNotCacheDrawing(boolean w) {}
    public boolean willNotCacheDrawing() { return true; }
    public void setKeepScreenOn(boolean b) { mKeepScreenOn = b; if (mRoot != null) mRoot.keepScreenOn(b); }
    public boolean getKeepScreenOn() { return mKeepScreenOn; }
    public void setFitsSystemWindows(boolean f) { mFitsSystemWindows = f; }
    public boolean getFitsSystemWindows() { return mFitsSystemWindows; }
    public boolean fitsSystemWindows() { return mFitsSystemWindows; }
    public void setSoundEffectsEnabled(boolean e) { mSoundEnabled = e; }
    public boolean isSoundEffectsEnabled() { return mSoundEnabled; }
    public void playSoundEffect(int s) {}
    public void setHapticFeedbackEnabled(boolean e) { mHapticEnabled = e; }
    public boolean isHapticFeedbackEnabled() { return mHapticEnabled; }
    public boolean performHapticFeedback(int c) { return performHapticFeedback(c, 0); }
    public boolean performHapticFeedback(int c, int flags) { if (!mHapticEnabled && (flags & 1) == 0) return false; husk.Native.vibrate(c == HapticFeedbackConstants.LONG_PRESS ? 30 : 10); return true; }
    public void setContentDescription(CharSequence c) { mContentDescription = c; }
    public CharSequence getContentDescription() { return mContentDescription; }
    public void setTooltipText(CharSequence t) { mTooltip = t; }
    public CharSequence getTooltipText() { return mTooltip; }
    public void setStateDescription(CharSequence s) {}
    public void setAccessibilityDelegate(AccessibilityDelegate d) { mAccessibilityDelegate = d; }
    public AccessibilityDelegate getAccessibilityDelegate() { return mAccessibilityDelegate; }
    public void setImportantForAccessibility(int m) { mImportantForAccessibility = m; }
    public int getImportantForAccessibility() { return mImportantForAccessibility; }
    public boolean isImportantForAccessibility() { return mImportantForAccessibility != IMPORTANT_FOR_ACCESSIBILITY_NO; }
    public void setAccessibilityLiveRegion(int m) {}
    public int getAccessibilityLiveRegion() { return 0; }
    public void setAccessibilityHeading(boolean h) {}
    public void setScreenReaderFocusable(boolean f) {}
    public void setAccessibilityPaneTitle(CharSequence t) {}
    public void setAccessibilityTraversalBefore(int id) {}
    public void setAccessibilityTraversalAfter(int id) {}
    public void setLabelFor(int id) { mLabelFor = id; }
    public int getLabelFor() { return mLabelFor; }
    public void sendAccessibilityEvent(int t) {}
    public void sendAccessibilityEventUnchecked(android.view.accessibility.AccessibilityEvent e) {}
    public void announceForAccessibility(CharSequence t) {}
    public AccessibilityNodeInfo createAccessibilityNodeInfo() { return AccessibilityNodeInfo.obtain(this); }
    public void onInitializeAccessibilityNodeInfo(AccessibilityNodeInfo i) {}
    public void onInitializeAccessibilityEvent(android.view.accessibility.AccessibilityEvent e) {}
    public void onPopulateAccessibilityEvent(android.view.accessibility.AccessibilityEvent e) {}
    public boolean dispatchPopulateAccessibilityEvent(android.view.accessibility.AccessibilityEvent e) { return false; }
    public boolean performAccessibilityAction(int a, Bundle args) { return a == AccessibilityNodeInfo.ACTION_CLICK && performClick(); }
    public android.view.accessibility.AccessibilityNodeProvider getAccessibilityNodeProvider() { return null; }
    public CharSequence getAccessibilityClassName() { return getClass().getName(); }
    public boolean isAccessibilityFocused() { return false; }
    public void setImportantForAutofill(int m) {}
    public int getImportantForAutofill() { return 0; }
    public void setAutofillHints(String... h) { mAutofillHints = h; }
    public int getAutofillType() { return AUTOFILL_TYPE_NONE; }
    public void setImportantForContentCapture(int m) {}
    public void setLayoutDirection(int d) { if (mLayoutDirection != d) { mLayoutDirection = d; requestLayout(); } }
    public int getLayoutDirection() { return mLayoutDirection == LAYOUT_DIRECTION_RTL ? LAYOUT_DIRECTION_RTL : LAYOUT_DIRECTION_LTR; }
    public int getRawLayoutDirection() { return mLayoutDirection; }
    public void onRtlPropertiesChanged(int d) {}
    public boolean canResolveLayoutDirection() { return true; }
    public void setTextAlignment(int a) { mTextAlignment = a; }
    public int getTextAlignment() { return mTextAlignment; }
    public void setTextDirection(int d) { mTextDirection = d; }
    public int getTextDirection() { return mTextDirection; }
    public boolean canResolveTextDirection() { return true; }
    public boolean canResolveTextAlignment() { return true; }

    // ---- drawable state
    public final int[] getDrawableState() {
        if (mDrawableState != null && (mPrivateFlags & PFLAG_DRAWABLE_STATE_DIRTY) == 0) return mDrawableState;
        mDrawableState = onCreateDrawableState(0);
        mPrivateFlags &= ~PFLAG_DRAWABLE_STATE_DIRTY;
        return mDrawableState;
    }
    protected int[] onCreateDrawableState(int extraSpace) {
        if ((mPrivateFlags & PFLAG_DUPLICATE_PARENT_STATE) != 0 && mParent != null) return mParent.onCreateDrawableState(extraSpace);
        int[] s = new int[8 + extraSpace];
        int n = 0;
        if (mEnabled) s[n++] = android.R.attr.state_enabled;
        if (isFocused()) s[n++] = android.R.attr.state_focused;
        if (isPressed()) s[n++] = android.R.attr.state_pressed;
        if (isSelected()) s[n++] = android.R.attr.state_selected;
        if (isActivated()) s[n++] = android.R.attr.state_activated;
        if (hasWindowFocus() || mRoot == null) s[n++] = android.R.attr.state_window_focused;
        if (isHovered()) s[n++] = android.R.attr.state_hovered;
        return s;
    }
    protected static int[] mergeDrawableStates(int[] base, int[] add) {
        int i = base.length - 1;
        while (i >= 0 && base[i] == 0) i--;
        System.arraycopy(add, 0, base, i + 1, add.length);
        return base;
    }
    public void refreshDrawableState() {
        mPrivateFlags |= PFLAG_DRAWABLE_STATE_DIRTY;
        drawableStateChanged();
        if (mParent != null) mParent.childDrawableStateChanged(this);
    }
    protected void drawableStateChanged() {
        int[] s = getDrawableState();
        boolean changed = false;
        if (mBackground != null && mBackground.isStateful()) changed |= mBackground.setState(s);
        if (mForeground != null && mForeground.isStateful()) changed |= mForeground.setState(s);
        if (changed) invalidate();
    }
    public void drawableHotspotChanged(float x, float y) { if (mBackground != null) mBackground.setHotspot(x, y); if (mForeground != null) mForeground.setHotspot(x, y); }
    public void dispatchDrawableHotspotChanged(float x, float y) {}
    public void jumpDrawablesToCurrentState() { if (mBackground != null) mBackground.jumpToCurrentState(); if (mForeground != null) mForeground.jumpToCurrentState(); }
    protected boolean verifyDrawable(Drawable who) { return who == mBackground || who == mForeground || (mOverlay != null && mOverlay.huskHas(who)); }
    public void invalidateDrawable(Drawable d) { if (verifyDrawable(d)) invalidateHusk(); }
    public void scheduleDrawable(Drawable who, Runnable what, long when) { if (verifyDrawable(who) && what != null) anyHandler().postAtTime(what, when); }
    public void unscheduleDrawable(Drawable who, Runnable what) { if (verifyDrawable(who) && what != null) anyHandler().removeCallbacks(what); }
    public void unscheduleDrawable(Drawable who) {}
    public StateListAnimator getStateListAnimator() { return mStateListAnimator; }
    public void setStateListAnimator(StateListAnimator a) { mStateListAnimator = a; }

    // ---- background and foreground
    public Drawable getBackground() { return mBackground; }
    public void setBackground(Drawable d) { setBackgroundDrawable(d); }
    @Deprecated public void setBackgroundDrawable(Drawable d) {
        if (d == mBackground) return;
        if (mBackground != null) { mBackground.setCallback(null); }
        mBackground = d;
        if (d != null) {
            Rect p = new Rect();
            if (d.getPadding(p)) setPadding(p.left, p.top, p.right, p.bottom);
            d.setCallback(this);
            if (d.isStateful()) d.setState(getDrawableState());
            d.setVisible(mVisibility == VISIBLE, false);
            applyBackgroundTint();
        }
        requestLayout();
        invalidate();
    }
    public void setBackgroundColor(int c) {
        if (mBackground instanceof android.graphics.drawable.ColorDrawable) { ((android.graphics.drawable.ColorDrawable) mBackground.mutate()).setColor(c); invalidate(); }
        else setBackground(new android.graphics.drawable.ColorDrawable(c));
    }
    public void setBackgroundResource(int id) { setBackground(id == 0 ? null : mContext.getDrawable(id)); }
    public void setBackgroundTintList(ColorStateList t) { mBackgroundTint = t; applyBackgroundTint(); }
    public ColorStateList getBackgroundTintList() { return mBackgroundTint; }
    public void setBackgroundTintMode(PorterDuff.Mode m) { mBackgroundTintMode = m; applyBackgroundTint(); }
    public PorterDuff.Mode getBackgroundTintMode() { return mBackgroundTintMode; }
    public void setBackgroundTintBlendMode(BlendMode m) {}
    private void applyBackgroundTint() {
        if (mBackground != null && (mBackgroundTint != null || mBackgroundTintMode != null)) {
            mBackground = mBackground.mutate();
            if (mBackgroundTint != null) mBackground.setTintList(mBackgroundTint);
            if (mBackgroundTintMode != null) mBackground.setTintMode(mBackgroundTintMode);
            if (mBackground.isStateful()) mBackground.setState(getDrawableState());
            invalidate();
        }
    }
    public Drawable getForeground() { return mForeground; }
    public void setForeground(Drawable d) {
        if (mForeground != null) mForeground.setCallback(null);
        mForeground = d;
        if (d != null) { d.setCallback(this); if (d.isStateful()) d.setState(getDrawableState()); }
        invalidate();
    }
    public int getForegroundGravity() { return mForegroundGravity; }
    public void setForegroundGravity(int g) { mForegroundGravity = g; invalidate(); }
    public void setForegroundTintList(ColorStateList t) { if (mForeground != null) mForeground.setTintList(t); }

    // ---- padding
    public void setPadding(int l, int t, int r, int b) {
        mUserPaddingStart = mUserPaddingEnd = Integer.MIN_VALUE;
        if (mPaddingLeft != l || mPaddingTop != t || mPaddingRight != r || mPaddingBottom != b) {
            mPaddingLeft = l; mPaddingTop = t; mPaddingRight = r; mPaddingBottom = b;
            requestLayout(); invalidate();
        }
    }
    public void setPaddingRelative(int s, int t, int e, int b) { setPadding(s, t, e, b); mUserPaddingStart = s; mUserPaddingEnd = e; }
    public int getPaddingLeft() { return mPaddingLeft; }
    public int getPaddingTop() { return mPaddingTop; }
    public int getPaddingRight() { return mPaddingRight; }
    public int getPaddingBottom() { return mPaddingBottom; }
    public int getPaddingStart() { return mPaddingLeft; }
    public int getPaddingEnd() { return mPaddingRight; }
    public boolean hasOverlappingRendering() { return true; }
    public void forceHasOverlappingRendering(boolean b) {}

    // ---- geometry
    public final int getWidth() { return mRight - mLeft; }
    public final int getHeight() { return mBottom - mTop; }
    public final int getLeft() { return mLeft; }
    public final int getTop() { return mTop; }
    public final int getRight() { return mRight; }
    public final int getBottom() { return mBottom; }
    public final void setLeft(int l) { if (l != mLeft) { int ow = getWidth(); invalidateParent(); mLeft = l; sizeChange(getWidth(), getHeight(), ow, getHeight()); } }
    public final void setTop(int t) { if (t != mTop) { int oh = getHeight(); invalidateParent(); mTop = t; sizeChange(getWidth(), getHeight(), getWidth(), oh); } }
    public final void setRight(int r) { if (r != mRight) { int ow = getWidth(); invalidateParent(); mRight = r; sizeChange(getWidth(), getHeight(), ow, getHeight()); } }
    public final void setBottom(int b) { if (b != mBottom) { int oh = getHeight(); invalidateParent(); mBottom = b; sizeChange(getWidth(), getHeight(), getWidth(), oh); } }
    public void setLeftTopRightBottom(int l, int t, int r, int b) { setFrame(l, t, r, b); }
    private void sizeChange(int w, int h, int ow, int oh) { mMatrixDirty = true; onSizeChanged(w, h, ow, oh); if (mBackground != null) mBackground.setBounds(0, 0, w, h); invalidate(); }
    public float getX() { return mLeft + mTranslationX; }
    public float getY() { return mTop + mTranslationY; }
    public float getZ() { return mElevation + mTranslationZ; }
    public void setX(float x) { setTranslationX(x - mLeft); }
    public void setY(float y) { setTranslationY(y - mTop); }
    public void setZ(float z) { setTranslationZ(z - mElevation); }
    public void offsetLeftAndRight(int o) { if (o != 0) { mLeft += o; mRight += o; invalidateParent(); } }
    public void offsetTopAndBottom(int o) { if (o != 0) { mTop += o; mBottom += o; invalidateParent(); } }
    public void getHitRect(Rect out) {
        if (hasIdentityMatrix()) out.set(mLeft, mTop, mRight, mBottom);
        else { RectF r = new RectF(0, 0, getWidth(), getHeight()); getMatrix().mapRect(r); r.offset(mLeft, mTop); out.set((int) Math.floor(r.left), (int) Math.floor(r.top), (int) Math.ceil(r.right), (int) Math.ceil(r.bottom)); }
    }
    public void getDrawingRect(Rect out) { out.set(mScrollX, mScrollY, mScrollX + getWidth(), mScrollY + getHeight()); }
    public final boolean getLocalVisibleRect(Rect r) { Point o = new Point(); if (getGlobalVisibleRect(r, o)) { r.offset(-o.x, -o.y); return true; } return false; }
    public final boolean getGlobalVisibleRect(Rect r) { return getGlobalVisibleRect(r, null); }
    public boolean getGlobalVisibleRect(Rect r, Point globalOffset) {
        int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return false;
        int[] loc = new int[2];
        getLocationInWindow(loc);
        r.set(loc[0], loc[1], loc[0] + w, loc[1] + h);
        if (globalOffset != null) globalOffset.set(loc[0] - mScrollX, loc[1] - mScrollY);
        View p = mParent;
        while (p != null) {
            int[] pl = new int[2]; p.getLocationInWindow(pl);
            if (!r.intersect(pl[0], pl[1], pl[0] + p.getWidth(), pl[1] + p.getHeight())) return false;
            p = p.mParent;
        }
        return mRoot == null || r.intersect(0, 0, mRoot.width(), mRoot.height());
    }
    public void getLocationOnScreen(int[] out) { getLocationInWindow(out); if (mRoot != null) { out[0] += mRoot.windowX(); out[1] += mRoot.windowY(); } }
    public void getLocationInWindow(int[] out) {
        float[] p = { 0, 0 };
        View v = this;
        while (v != null) {
            if (!v.hasIdentityMatrix()) v.getMatrix().mapPoints(p);
            p[0] += v.mLeft; p[1] += v.mTop;
            View parent = v.mParent;
            if (parent != null) { p[0] -= parent.mScrollX; p[1] -= parent.mScrollY; }
            v = parent;
        }
        out[0] = Math.round(p[0]); out[1] = Math.round(p[1]);
    }
    public void getLocationInSurface(int[] out) { getLocationInWindow(out); }
    public void getWindowVisibleDisplayFrame(Rect r) { if (mRoot != null) mRoot.visibleDisplayFrame(r); else r.set(0, 0, husk.Native.screenWidth(), husk.Native.screenHeight()); }
    public void getWindowDisplayFrame(Rect r) { getWindowVisibleDisplayFrame(r); }
    public void setClipBounds(Rect r) { mClipBounds = r == null ? null : new Rect(r); invalidate(); }
    public Rect getClipBounds() { return mClipBounds == null ? null : new Rect(mClipBounds); }
    public boolean getClipBounds(Rect out) { if (mClipBounds == null) return false; out.set(mClipBounds); return true; }
    public void setClipToOutline(boolean c) { if (c) mPrivateFlags |= PFLAG_CLIP_TO_OUTLINE; else mPrivateFlags &= ~PFLAG_CLIP_TO_OUTLINE; invalidate(); }
    public final boolean getClipToOutline() { return (mPrivateFlags & PFLAG_CLIP_TO_OUTLINE) != 0; }
    public void setOutlineProvider(ViewOutlineProvider p) { mOutlineProvider = p; invalidateOutline(); }
    public ViewOutlineProvider getOutlineProvider() { return mOutlineProvider; }
    public void invalidateOutline() { invalidate(); }
    public void setOutlineAmbientShadowColor(int c) {}
    public void setOutlineSpotShadowColor(int c) {}
    public float getElevation() { return mElevation; }
    public void setElevation(float e) { if (e != mElevation) { mElevation = e; invalidateParent(); } }
    public void setSystemGestureExclusionRects(java.util.List<Rect> r) {}
    public void setTouchDelegate(TouchDelegate d) { mTouchDelegate = d; }
    public TouchDelegate getTouchDelegate() { return mTouchDelegate; }

    // ---- transforms
    public float getAlpha() { return mAlpha; }
    public void setAlpha(float a) { if (a != mAlpha) { mAlpha = a; invalidateParent(); } }
    public void setTransitionAlpha(float a) { setAlpha(a); }
    public float getTransitionAlpha() { return 1; }
    public float getTranslationX() { return mTranslationX; }
    public void setTranslationX(float t) { if (t != mTranslationX) { invalidateParent(); mTranslationX = t; mMatrixDirty = true; invalidateParent(); } }
    public float getTranslationY() { return mTranslationY; }
    public void setTranslationY(float t) { if (t != mTranslationY) { invalidateParent(); mTranslationY = t; mMatrixDirty = true; invalidateParent(); } }
    public float getTranslationZ() { return mTranslationZ; }
    public void setTranslationZ(float t) { mTranslationZ = t; invalidateParent(); }
    public float getScaleX() { return mScaleX; }
    public void setScaleX(float s) { if (s != mScaleX) { mScaleX = s; mMatrixDirty = true; invalidateParent(); } }
    public float getScaleY() { return mScaleY; }
    public void setScaleY(float s) { if (s != mScaleY) { mScaleY = s; mMatrixDirty = true; invalidateParent(); } }
    public float getRotation() { return mRotation; }
    public void setRotation(float r) { if (r != mRotation) { mRotation = r; mMatrixDirty = true; invalidateParent(); } }
    public float getRotationX() { return mRotationX; }
    public void setRotationX(float r) { if (r != mRotationX) { mRotationX = r; mMatrixDirty = true; invalidateParent(); } }
    public float getRotationY() { return mRotationY; }
    public void setRotationY(float r) { if (r != mRotationY) { mRotationY = r; mMatrixDirty = true; invalidateParent(); } }
    public float getPivotX() { return (mPrivateFlags & PFLAG_HAS_PIVOT) != 0 ? mPivotX : getWidth() / 2f; }
    public float getPivotY() { return (mPrivateFlags & PFLAG_HAS_PIVOT) != 0 ? mPivotY : getHeight() / 2f; }
    public void setPivotX(float p) { if ((mPrivateFlags & PFLAG_HAS_PIVOT) == 0) mPivotY = getHeight() / 2f; mPrivateFlags |= PFLAG_HAS_PIVOT; mPivotX = p; mMatrixDirty = true; invalidateParent(); }
    public void setPivotY(float p) { if ((mPrivateFlags & PFLAG_HAS_PIVOT) == 0) mPivotX = getWidth() / 2f; mPrivateFlags |= PFLAG_HAS_PIVOT; mPivotY = p; mMatrixDirty = true; invalidateParent(); }
    public boolean isPivotSet() { return (mPrivateFlags & PFLAG_HAS_PIVOT) != 0; }
    public void resetPivot() { mPrivateFlags &= ~PFLAG_HAS_PIVOT; mMatrixDirty = true; invalidateParent(); }
    public void setCameraDistance(float d) {}
    public float getCameraDistance() { return 1280; }
    public void setAnimationMatrix(Matrix m) {}
    public Matrix getAnimationMatrix() { return null; }
    public boolean hasIdentityMatrix() {
        return mTranslationX == 0 && mTranslationY == 0 && mScaleX == 1 && mScaleY == 1 && mRotation == 0 && mRotationX == 0 && mRotationY == 0;
    }
    public Matrix getMatrix() {
        if (mMatrix == null) { mMatrix = new Matrix(); mMatrixDirty = true; }
        if (mMatrixDirty) {
            Matrix m = mMatrix;
            m.reset();
            float px = getPivotX(), py = getPivotY();
            // rotations about X and Y show as a scale (no perspective here)
            float sx = mScaleX * (float) Math.cos(Math.toRadians(mRotationY)), sy = mScaleY * (float) Math.cos(Math.toRadians(mRotationX));
            m.setTranslate(mTranslationX, mTranslationY);
            m.preRotate(mRotation, px, py);
            m.preScale(sx, sy, px, py);
            mMatrixDirty = false;
            if (mInverse == null) mInverse = new Matrix();
            m.invert(mInverse);
        }
        return mMatrix;
    }
    final Matrix inverseMatrix() { getMatrix(); return mInverse; }
    public ViewPropertyAnimator animate() { if (mAnimator == null) mAnimator = new ViewPropertyAnimator(this); return mAnimator; }
    public void startAnimation(Animation a) { a.setStartTime(Animation.START_ON_FIRST_FRAME); setAnimation(a); invalidateParent(); }
    public void setAnimation(Animation a) { mAnimation = a; if (a != null) { a.reset(); } }
    public Animation getAnimation() { return mAnimation; }
    public void clearAnimation() { if (mAnimation != null) { mAnimation.cancel(); mAnimation = null; invalidateParent(); } }
    protected void onAnimationStart() {}
    protected void onAnimationEnd() {}
    public boolean hasTransientState() { return false; }
    public void setHasTransientState(boolean t) {}

    // ---- measure and layout
    public final int getMeasuredWidth() { return mMeasuredWidth & MEASURED_SIZE_MASK; }
    public final int getMeasuredWidthAndState() { return mMeasuredWidth; }
    public final int getMeasuredHeight() { return mMeasuredHeight & MEASURED_SIZE_MASK; }
    public final int getMeasuredHeightAndState() { return mMeasuredHeight; }
    public final int getMeasuredState() { return (mMeasuredWidth & MEASURED_STATE_MASK) | ((mMeasuredHeight >> MEASURED_HEIGHT_STATE_SHIFT) & (MEASURED_STATE_MASK >> MEASURED_HEIGHT_STATE_SHIFT)); }
    protected final void setMeasuredDimension(int w, int h) { mMeasuredWidth = w; mMeasuredHeight = h; }
    public int getMinimumWidth() { return mMinWidth; }
    public int getMinimumHeight() { return mMinHeight; }
    public void setMinimumWidth(int w) { mMinWidth = w; requestLayout(); }
    public void setMinimumHeight(int h) { mMinHeight = h; requestLayout(); }
    protected int getSuggestedMinimumWidth() { return mBackground == null ? mMinWidth : Math.max(mMinWidth, mBackground.getMinimumWidth()); }
    protected int getSuggestedMinimumHeight() { return mBackground == null ? mMinHeight : Math.max(mMinHeight, mBackground.getMinimumHeight()); }
    public static int getDefaultSize(int size, int spec) {
        int mode = MeasureSpec.getMode(spec), s = MeasureSpec.getSize(spec);
        return mode == MeasureSpec.UNSPECIFIED ? size : s;
    }
    public static int resolveSize(int size, int spec) { return resolveSizeAndState(size, spec, 0) & MEASURED_SIZE_MASK; }
    public static int resolveSizeAndState(int size, int spec, int childState) {
        int mode = MeasureSpec.getMode(spec), s = MeasureSpec.getSize(spec), r;
        if (mode == MeasureSpec.AT_MOST) r = s < size ? s | MEASURED_STATE_TOO_SMALL : size;
        else if (mode == MeasureSpec.EXACTLY) r = s;
        else r = size;
        return r | (childState & MEASURED_STATE_MASK);
    }
    public static int combineMeasuredStates(int a, int b) { return a | b; }
    protected void onMeasure(int ws, int hs) { setMeasuredDimension(getDefaultSize(getSuggestedMinimumWidth(), ws), getDefaultSize(getSuggestedMinimumHeight(), hs)); }
    public final void measure(int ws, int hs) {
        boolean force = (mPrivateFlags & PFLAG_FORCE_LAYOUT) != 0;
        boolean changed = ws != mOldWidthSpec || hs != mOldHeightSpec;
        if (force || changed) {
            // layout params resolve start/end against the direction, as Android does before measuring (ConstraintLayout's need it)
            if (mLayoutParams != null) mLayoutParams.resolveLayoutDirection(getLayoutDirection());
            onMeasure(ws, hs);
            mPrivateFlags |= PFLAG_LAYOUT_REQUIRED;
        }
        mOldWidthSpec = ws; mOldHeightSpec = hs;
    }
    public void layout(int l, int t, int r, int b) {
        int ol = mLeft, ot = mTop, or = mRight, ob = mBottom;
        boolean changed = setFrame(l, t, r, b);
        if (changed || (mPrivateFlags & PFLAG_LAYOUT_REQUIRED) != 0) {
            onLayout(changed, l, t, r, b);
            mPrivateFlags &= ~PFLAG_LAYOUT_REQUIRED;
            if (mOnLayoutChange != null) for (OnLayoutChangeListener li : new ArrayList<>(mOnLayoutChange)) li.onLayoutChange(this, l, t, r, b, ol, ot, or, ob);
        }
        mPrivateFlags &= ~PFLAG_FORCE_LAYOUT;
    }
    protected boolean setFrame(int l, int t, int r, int b) {
        if (mLeft == l && mRight == r && mTop == t && mBottom == b) return false;
        int ow = mRight - mLeft, oh = mBottom - mTop, nw = r - l, nh = b - t;
        invalidateParent();
        mLeft = l; mTop = t; mRight = r; mBottom = b;
        if (ow != nw || oh != nh) sizeChange(nw, nh, ow, oh);
        else mMatrixDirty = true;
        invalidateParent();
        return true;
    }
    protected void onLayout(boolean changed, int l, int t, int r, int b) {}
    protected void onSizeChanged(int w, int h, int ow, int oh) {}
    public void requestLayout() {
        mPrivateFlags |= PFLAG_FORCE_LAYOUT;
        mOldWidthSpec = Integer.MIN_VALUE;
        if (mParent != null && !mParent.isLayoutRequested()) mParent.requestLayout();
        else if (mParent == null && mRoot != null) mRoot.requestLayout();
    }
    public void forceLayout() { mPrivateFlags |= PFLAG_FORCE_LAYOUT; mOldWidthSpec = Integer.MIN_VALUE; }
    public void addOnLayoutChangeListener(OnLayoutChangeListener l) { if (mOnLayoutChange == null) mOnLayoutChange = new ArrayList<>(); mOnLayoutChange.add(l); }
    public void removeOnLayoutChangeListener(OnLayoutChangeListener l) { if (mOnLayoutChange != null) mOnLayoutChange.remove(l); }
    public ViewGroup.LayoutParams getLayoutParams() { return mLayoutParams; }
    public void setLayoutParams(ViewGroup.LayoutParams p) {
        if (p == null) throw new NullPointerException("Layout parameters cannot be null");
        mLayoutParams = p;
        if (mRoot != null) p.resolveLayoutDirection(getLayoutDirection());
        if (mParent != null) mParent.onSetLayoutParams(this, p);
        requestLayout();
    }
    public int getBaseline() { return -1; }
    public void onConfigurationChanged(android.content.res.Configuration c) {}
    public void dispatchConfigurationChanged(android.content.res.Configuration c) { onConfigurationChanged(c); }

    // ---- insets
    public WindowInsets onApplyWindowInsets(WindowInsets i) {
        if (mFitsSystemWindows) {
            setPadding(i.getSystemWindowInsetLeft(), i.getSystemWindowInsetTop(), i.getSystemWindowInsetRight(), i.getSystemWindowInsetBottom());
            return i.consumeSystemWindowInsets();
        }
        return i;
    }
    public WindowInsets dispatchApplyWindowInsets(WindowInsets i) {
        mLastInsets = i;
        return mOnApplyInsets != null ? mOnApplyInsets.onApplyWindowInsets(this, i) : onApplyWindowInsets(i);
    }
    public void setOnApplyWindowInsetsListener(OnApplyWindowInsetsListener l) { mOnApplyInsets = l; }
    public WindowInsets getRootWindowInsets() { return mRoot != null ? mRoot.insets() : null; }
    public WindowInsets computeSystemWindowInsets(WindowInsets in, Rect outLocal) { outLocal.set(in.getSystemWindowInsetLeft(), in.getSystemWindowInsetTop(), in.getSystemWindowInsetRight(), in.getSystemWindowInsetBottom()); return in.consumeSystemWindowInsets(); }
    public void requestApplyInsets() { if (mRoot != null) mRoot.requestApplyInsets(); }
    @Deprecated public void requestFitSystemWindows() { requestApplyInsets(); }
    protected boolean fitSystemWindows(Rect insets) { return false; }
    public void setWindowInsetsAnimationCallback(WindowInsetsAnimation.Callback cb) {}
    public WindowInsetsController getWindowInsetsController() {
        if (mRoot != null) return mRoot.insetsController();
        // not attached yet (an activity's onCreate): its parents', up to the window's decor, which has one from the start
        ViewParent p = getParent();
        return p instanceof View ? ((View) p).getWindowInsetsController() : null;
    }
    public void setSystemUiVisibility(int v) { mSystemUiVisibility = v; if (mRoot != null) mRoot.systemUiChanged(v); }
    public int getSystemUiVisibility() { return mSystemUiVisibility; }
    public int getWindowSystemUiVisibility() { return mRoot != null ? mRoot.systemUi() : mSystemUiVisibility; }
    public void setOnSystemUiVisibilityChangeListener(OnSystemUiVisibilityChangeListener l) { mOnSystemUi = l; }
    public void dispatchSystemUiVisibilityChanged(int v) { if (mOnSystemUi != null) mOnSystemUi.onSystemUiVisibilityChange(v); }
    public void onWindowSystemUiVisibilityChanged(int v) {}

    // ---- drawing
    /* As Android's invalidateInternal: the rectangle forms do not call the overridable invalidate(), which subclasses often
       override to invalidate their drawables (whose invalidateSelf comes back through invalidateDrawable) */
    private void invalidateHusk() {
        mPrivateFlags |= PFLAG_DIRTY;
        if (mRoot != null) mRoot.invalidate();
    }
    public void invalidate() { invalidateHusk(); }
    public void invalidate(Rect dirty) { invalidateHusk(); }
    public void invalidate(int l, int t, int r, int b) { invalidateHusk(); }
    void invalidateParent() { if (mRoot != null) mRoot.invalidate(); }
    public void postInvalidate() { if (mRoot != null) mRoot.invalidateFromAnyThread(); }
    public void postInvalidate(int l, int t, int r, int b) { postInvalidate(); }
    public void postInvalidateDelayed(long ms) { postDelayed(this::invalidate, ms); }
    public void postInvalidateOnAnimation() { postInvalidate(); }
    public void postInvalidateOnAnimation(int l, int t, int r, int b) { postInvalidate(); }
    public void invalidateDrawableHusk() { invalidate(); }
    protected void onDraw(Canvas c) {}
    protected void dispatchDraw(Canvas c) {}
    public void onDrawForeground(Canvas c) {
        if (mForeground != null) {
            Rect b = new Rect(0, 0, getWidth(), getHeight()), out = new Rect();
            if (mForegroundGravity == Gravity.FILL) out.set(b);
            else Gravity.apply(mForegroundGravity, mForeground.getIntrinsicWidth(), mForeground.getIntrinsicHeight(), b, out);
            mForeground.setBounds(out);
            c.save(); c.translate(mScrollX, mScrollY);
            mForeground.draw(c);
            c.restore();
        }
    }
    /** Background, content, children, foreground; the canvas is already in this view's coordinates. */
    public void draw(Canvas c) {
        mPrivateFlags &= ~PFLAG_DIRTY;
        drawBackground(c);
        if ((mPrivateFlags & PFLAG_WILL_NOT_DRAW) == 0 || !(this instanceof ViewGroup)) onDraw(c);
        dispatchDraw(c);
        onDrawForeground(c);
        if (mOverlay != null && !mOverlay.isEmpty()) mOverlay.huskDraw(c);
    }
    private void drawBackground(Canvas c) {
        Drawable bg = mBackground;
        if (bg == null) return;
        bg.setBounds(0, 0, getWidth(), getHeight());
        if (mScrollX == 0 && mScrollY == 0) bg.draw(c);
        else { c.translate(mScrollX, mScrollY); bg.draw(c); c.translate(-mScrollX, -mScrollY); }
    }
    /** As a parent draws a child: its position, transform, alpha and clip, then draw(). */
    void drawFromParent(Canvas c, ViewGroup parent) {
        boolean animating = false;
        if (mAnimation != null) {
            Animation a = mAnimation;
            if (!a.isInitialized()) { a.initialize(getWidth(), getHeight(), parent.getWidth(), parent.getHeight()); onAnimationStart(); }
            mAnimTx.clear();
            animating = a.getTransformation(android.view.animation.AnimationUtils.currentAnimationTimeMillis(), mAnimTx);
            if (animating) invalidateParent();
            else if (!a.getFillAfter()) { mAnimation = null; onAnimationEnd(); }
        }
        if (mVisibility != VISIBLE && !animating) return;
        int save = c.save();
        c.translate(mLeft, mTop);
        if (!hasIdentityMatrix()) c.concat(getMatrix());
        float alpha = mAlpha;
        if (mAnimation != null) { if (!mAnimTx.getMatrix().isIdentity()) c.concat(mAnimTx.getMatrix()); alpha *= mAnimTx.getAlpha(); }
        if (alpha <= 0.002f) { c.restoreToCount(save); return; }
        int w = getWidth(), h = getHeight();
        if (parent.getClipChildren()) c.clipRect(0, 0, w, h);
        if (mClipBounds != null) c.clipRect(mClipBounds);
        if (getClipToOutline() && mOutlineProvider != null) {
            Outline o = new Outline();
            mOutlineProvider.getOutline(this, o);
            if (o.mRect != null) {
                if (o.mRadius > 0) { Path p = new Path(); p.addRoundRect(new RectF(o.mRect), o.mRadius, o.mRadius, Path.Direction.CW); c.clipPath(p); }
                else c.clipRect(o.mRect);
            } else if (o.mPath != null) c.clipPath(o.mPath);
        }
        if (alpha < 0.998f || (mLayerType != LAYER_TYPE_NONE && mLayerPaint != null)) {
            int a = Math.round(alpha * 255);
            if (mLayerPaint != null) a = a * mLayerPaint.getAlpha() / 255;
            c.saveLayerAlpha(0, 0, w, h, a);
        }
        c.translate(-mScrollX, -mScrollY);
        draw(c);
        c.restoreToCount(save);
    }

    // ---- scrolling
    public final int getScrollX() { return mScrollX; }
    public final int getScrollY() { return mScrollY; }
    public void setScrollX(int x) { scrollTo(x, mScrollY); }
    public void setScrollY(int y) { scrollTo(mScrollX, y); }
    public void scrollTo(int x, int y) {
        if (mScrollX != x || mScrollY != y) {
            int ox = mScrollX, oy = mScrollY;
            mScrollX = x; mScrollY = y;
            onScrollChanged(x, y, ox, oy);
            invalidate();
        }
    }
    public void scrollBy(int x, int y) { scrollTo(mScrollX + x, mScrollY + y); }
    protected void onScrollChanged(int l, int t, int ol, int ot) {
        if (mOnScrollChange != null) mOnScrollChange.onScrollChange(this, l, t, ol, ot);
        if (mRoot != null) mRoot.observer().dispatchOnScrollChanged();
    }
    public void setOnScrollChangeListener(OnScrollChangeListener l) { mOnScrollChange = l; }
    public void computeScroll() {}
    public boolean canScrollHorizontally(int dir) { int off = computeHorizontalScrollOffset(), range = computeHorizontalScrollRange() - computeHorizontalScrollExtent(); if (range == 0) return false; return dir < 0 ? off > 0 : off < range - 1; }
    public boolean canScrollVertically(int dir) { int off = computeVerticalScrollOffset(), range = computeVerticalScrollRange() - computeVerticalScrollExtent(); if (range == 0) return false; return dir < 0 ? off > 0 : off < range - 1; }
    protected int computeHorizontalScrollRange() { return getWidth(); }
    protected int computeHorizontalScrollOffset() { return mScrollX; }
    protected int computeHorizontalScrollExtent() { return getWidth(); }
    protected int computeVerticalScrollRange() { return getHeight(); }
    protected int computeVerticalScrollOffset() { return mScrollY; }
    protected int computeVerticalScrollExtent() { return getHeight(); }
    protected boolean awakenScrollBars() { return false; }
    protected boolean awakenScrollBars(int d) { return false; }
    protected boolean overScrollBy(int dx, int dy, int sx, int sy, int rx, int ry, int mx, int my, boolean touch) {
        int nx = sx + dx, ny = sy + dy;
        boolean cx = false, cy = false;
        if (nx > rx + mx) { nx = rx + mx; cx = true; } else if (nx < -mx) { nx = -mx; cx = true; }
        if (ny > ry + my) { ny = ry + my; cy = true; } else if (ny < -my) { ny = -my; cy = true; }
        onOverScrolled(nx, ny, cx, cy);
        return cx || cy;
    }
    protected void onOverScrolled(int x, int y, boolean cx, boolean cy) {}
    public boolean isScrollContainer() { return false; }
    public void setScrollContainer(boolean c) {}
    public boolean requestRectangleOnScreen(Rect r) { return requestRectangleOnScreen(r, false); }
    public boolean requestRectangleOnScreen(Rect r, boolean immediate) {
        View child = this; ViewGroup parent = mParent; Rect pos = new Rect(r);
        boolean scrolled = false;
        while (parent != null) {
            scrolled |= parent.requestChildRectangleOnScreen(child, pos, immediate);
            pos.offset(child.mLeft - child.mScrollX, child.mTop - child.mScrollY);
            child = parent; parent = parent.mParent;
        }
        return scrolled;
    }

    // ---- focus
    public boolean isFocusable() { return mFocusable; }
    public int getFocusable() { return mFocusable ? FOCUSABLE : NOT_FOCUSABLE; }
    public void setFocusable(boolean f) { mFocusable = f; if (!f) mFocusableInTouchMode = false; }
    public void setFocusable(int f) { setFocusable(f != NOT_FOCUSABLE); }
    public boolean isFocusableInTouchMode() { return mFocusableInTouchMode; }
    public void setFocusableInTouchMode(boolean f) { mFocusableInTouchMode = f; if (f) mFocusable = true; }
    public void setFocusedByDefault(boolean f) {}
    public void setDefaultFocusHighlightEnabled(boolean b) {}
    public boolean isFocused() { return (mPrivateFlags & PFLAG_FOCUSED) != 0; }
    public boolean hasFocus() { return isFocused(); }
    public boolean gatherTransparentRegion(android.graphics.Region r) { return true; }
    public final void saveAttributeDataForStyleable(android.content.Context c, int[] styleable, android.util.AttributeSet attrs, android.content.res.TypedArray t, int defStyleAttr, int defStyleRes) {}
    public final int[] getAttributeResolutionStack(int attribute) { return new int[0]; }
    public java.util.Map<Integer, Integer> getAttributeSourceResourceMap() { return new java.util.HashMap<>(); }
    public final int getExplicitStyle() { return 0; }
    public boolean hasExplicitFocusable() { return hasFocusable(); }
    public boolean hasFocusable() { return mVisibility == VISIBLE && mEnabled && mFocusable; }
    public View findFocus() { return isFocused() ? this : null; }
    public final boolean requestFocus() { return requestFocus(FOCUS_DOWN); }
    public final boolean requestFocus(int dir) { return requestFocus(dir, null); }
    public boolean requestFocus(int dir, Rect prev) { return requestFocusNoSearch(dir, prev); }
    boolean requestFocusNoSearch(int dir, Rect prev) {
        if (!mFocusable || mVisibility != VISIBLE || !mEnabled) return false;
        if (!mFocusableInTouchMode && isInTouchMode()) return false;
        for (ViewGroup p = mParent; p != null; p = p.mParent) if (p.getDescendantFocusability() == ViewGroup.FOCUS_BLOCK_DESCENDANTS) return false;
        handleFocusGainInternal(dir, prev);
        return true;
    }
    public final boolean requestFocusFromTouch() { return requestFocus(); }
    void handleFocusGainInternal(int dir, Rect prev) {
        if (isFocused()) return;
        View old = mRoot != null ? mRoot.focused() : null;
        if (old != null && old != this) old.unFocus(this);
        mPrivateFlags |= PFLAG_FOCUSED;
        if (mParent != null) mParent.requestChildFocus(this, this);
        if (mRoot != null) mRoot.setFocused(this);
        onFocusChanged(true, dir, prev);
        refreshDrawableState();
    }
    void unFocus(View newFocus) {
        if (!isFocused()) return;
        mPrivateFlags &= ~PFLAG_FOCUSED;
        onFocusChanged(false, 0, null);
        refreshDrawableState();
    }
    public void clearFocus() {
        if (!isFocused()) return;
        unFocus(null);
        if (mParent != null) mParent.clearChildFocus(this);
        if (mRoot != null && mRoot.focused() == this) mRoot.setFocused(null);
    }
    protected void onFocusChanged(boolean gain, int dir, Rect prev) {
        if (mOnFocusChange != null) mOnFocusChange.onFocusChange(this, gain);
        if (mRoot != null) mRoot.observer().dispatchOnGlobalFocusChange(gain ? null : this, gain ? this : null);
        if (!gain && isPressed()) setPressed(false);
    }
    public void setOnFocusChangeListener(OnFocusChangeListener l) { mOnFocusChange = l; }
    public OnFocusChangeListener getOnFocusChangeListener() { return mOnFocusChange; }
    public View focusSearch(int dir) { return mParent != null ? mParent.focusSearch(this, dir) : null; }
    public void addFocusables(ArrayList<View> views, int dir) { addFocusables(views, dir, FOCUSABLES_TOUCH_MODE); }
    public void addFocusables(ArrayList<View> views, int dir, int mode) { if (mFocusable && mVisibility == VISIBLE && mEnabled && (mode != FOCUSABLES_TOUCH_MODE || mFocusableInTouchMode)) views.add(this); }
    public ArrayList<View> getFocusables(int dir) { ArrayList<View> r = new ArrayList<>(); addFocusables(r, dir); return r; }
    public void addTouchables(ArrayList<View> views) { if ((mClickable || mLongClickable) && mVisibility == VISIBLE && mEnabled) views.add(this); }
    public ArrayList<View> getTouchables() { ArrayList<View> r = new ArrayList<>(); addTouchables(r); return r; }
    public void setNextFocusDownId(int id) { mNextFocusDown = id; } public int getNextFocusDownId() { return mNextFocusDown; }
    public void setNextFocusUpId(int id) { mNextFocusUp = id; } public int getNextFocusUpId() { return mNextFocusUp; }
    public void setNextFocusLeftId(int id) { mNextFocusLeft = id; } public int getNextFocusLeftId() { return mNextFocusLeft; }
    public void setNextFocusRightId(int id) { mNextFocusRight = id; } public int getNextFocusRightId() { return mNextFocusRight; }
    public void setNextFocusForwardId(int id) { mNextFocusForward = id; } public int getNextFocusForwardId() { return mNextFocusForward; }
    public boolean restoreDefaultFocus() { return requestFocus(); }
    public void setKeyboardNavigationCluster(boolean c) {}
    public boolean onCheckIsTextEditor() { return false; }
    public android.view.inputmethod.InputConnection onCreateInputConnection(android.view.inputmethod.EditorInfo o) { return null; }
    public boolean checkInputConnectionProxy(View v) { return false; }
    public void cancelPendingInputEvents() { removeCallbacks(mPendingLongPress); mPrivateFlags &= ~PFLAG_PREPRESSED; }
    public void setPointerIcon(PointerIcon p) {}
    public PointerIcon getPointerIcon() { return null; }
    public PointerIcon onResolvePointerIcon(MotionEvent e, int i) { return null; }
    public void releasePointerCapture() {}
    public void requestPointerCapture() {}
    public boolean hasPointerCapture() { return false; }
    public void setAllowClickWhenDisabled(boolean b) {}

    // ---- input
    public void setOnTouchListener(OnTouchListener l) { mOnTouch = l; }
    public void setOnKeyListener(OnKeyListener l) { mOnKey = l; }
    public void setOnClickListener(OnClickListener l) { if (!mClickable) mClickable = true; mOnClick = l; }
    public boolean hasOnClickListeners() { return mOnClick != null; }
    public void setOnLongClickListener(OnLongClickListener l) { if (!mLongClickable) mLongClickable = true; mOnLongClick = l; }
    public boolean hasOnLongClickListeners() { return mOnLongClick != null; }
    public void setOnContextClickListener(OnContextClickListener l) { if (!mContextClickable) mContextClickable = true; mOnContextClick = l; }
    public void setOnGenericMotionListener(OnGenericMotionListener l) { mOnGeneric = l; }
    public void setOnHoverListener(OnHoverListener l) { mOnHover = l; }
    public void setOnCapturedPointerListener(OnCapturedPointerListener l) {}
    public void setOnCreateContextMenuListener(OnCreateContextMenuListener l) { if (!mLongClickable) mLongClickable = true; mOnContextMenu = l; }
    public void createContextMenu(ContextMenu m) {}
    public boolean showContextMenu() { return false; }
    public boolean showContextMenu(float x, float y) { return false; }
    public void setOnDragListener(OnDragListener l) { mOnDrag = l; }
    public boolean startDrag(android.content.ClipData d, DragShadowBuilder s, Object local, int flags) { return false; }
    public final boolean startDragAndDrop(android.content.ClipData d, DragShadowBuilder s, Object local, int flags) { return false; }
    public boolean onDragEvent(DragEvent e) { return false; }
    public boolean dispatchDragEvent(DragEvent e) { return mOnDrag != null && mOnDrag.onDrag(this, e) || onDragEvent(e); }
    public static class DragShadowBuilder {
        private final View mView;
        public DragShadowBuilder(View v) { mView = v; }
        public DragShadowBuilder() { mView = null; }
        public final View getView() { return mView; }
        public void onProvideShadowMetrics(Point size, Point touch) {}
        public void onDrawShadow(Canvas c) {}
    }
    public void addOnUnhandledKeyEventListener(OnUnhandledKeyEventListener l) { if (mOnUnhandledKey == null) mOnUnhandledKey = new ArrayList<>(); mOnUnhandledKey.add(l); }
    public void removeOnUnhandledKeyEventListener(OnUnhandledKeyEventListener l) { if (mOnUnhandledKey != null) mOnUnhandledKey.remove(l); }

    public boolean performClick() {
        if (mOnClick != null) { playSoundEffect(SoundEffectConstants.CLICK); mOnClick.onClick(this); return true; }
        return false;
    }
    public boolean callOnClick() { if (mOnClick != null) { mOnClick.onClick(this); return true; } return false; }
    public boolean performLongClick() {
        boolean handled = false;
        if (mOnLongClick != null) handled = mOnLongClick.onLongClick(this);
        if (!handled && mOnContextMenu != null) handled = showContextMenu();
        if (handled) performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
        return handled;
    }
    public boolean performLongClick(float x, float y) { return performLongClick(); }
    public boolean performContextClick() { return mOnContextClick != null && mOnContextClick.onContextClick(this); }
    public boolean performContextClick(float x, float y) { return performContextClick(); }

    public boolean dispatchTouchEvent(MotionEvent e) {
        if (!onFilterTouchEventForSecurity(e)) return false;
        if (mOnTouch != null && mEnabled && mOnTouch.onTouch(this, e)) return true;
        return onTouchEvent(e);
    }
    public boolean onFilterTouchEventForSecurity(MotionEvent e) { return true; }
    public boolean dispatchGenericMotionEvent(MotionEvent e) {
        if (mOnGeneric != null && mEnabled && mOnGeneric.onGenericMotion(this, e)) return true;
        return onGenericMotionEvent(e);
    }
    public boolean dispatchHoverEvent(MotionEvent e) { if (mOnHover != null && mOnHover.onHover(this, e)) return true; return onHoverEvent(e); }
    public boolean onHoverEvent(MotionEvent e) { return false; }
    public boolean onGenericMotionEvent(MotionEvent e) { return false; }
    public boolean dispatchCapturedPointerEvent(MotionEvent e) { return false; }
    public boolean dispatchTrackballEvent(MotionEvent e) { return onTrackballEvent(e); }
    public boolean onTrackballEvent(MotionEvent e) { return false; }

    private final Runnable mPendingLongPress = new Runnable() {
        public void run() {
            if (isPressed() && mParent != null) {
                if (performLongClick()) mHasPerformedLongPress = true;
            }
        }
    };
    private final Runnable mUnsetPressed = () -> setPressed(false);
    public boolean onTouchEvent(MotionEvent e) {
        float x = e.getX(), y = e.getY();
        int action = e.getActionMasked();
        boolean clickable = mClickable || mLongClickable || mContextClickable;
        if (!mEnabled) {
            if (action == MotionEvent.ACTION_UP && isPressed()) setPressed(false);
            return clickable;
        }
        if (mTouchDelegate != null && mTouchDelegate.onTouchEvent(e)) return true;
        if (!clickable) return false;
        int slop = ViewConfiguration.get(mContext).getScaledTouchSlop();
        switch (action) {
        case MotionEvent.ACTION_DOWN:
            mHasPerformedLongPress = false;
            setPressed(true);
            drawableHotspotChanged(x, y);
            if (mLongClickable) { removeCallbacks(mPendingLongPress); postDelayed(mPendingLongPress, ViewConfiguration.getLongPressTimeout()); }
            break;
        case MotionEvent.ACTION_MOVE:
            drawableHotspotChanged(x, y);
            if (x < -slop || y < -slop || x >= getWidth() + slop || y >= getHeight() + slop) {
                removeCallbacks(mPendingLongPress);
                if (isPressed()) setPressed(false);
            }
            break;
        case MotionEvent.ACTION_UP:
            if (isPressed()) {
                if (!mHasPerformedLongPress) {
                    removeCallbacks(mPendingLongPress);
                    if (!isFocused() && mFocusable && mFocusableInTouchMode) requestFocus();
                    post(this::performClickInternal);
                }
                postDelayed(mUnsetPressed, ViewConfiguration.getPressedStateDuration());
            }
            break;
        case MotionEvent.ACTION_CANCEL:
            setPressed(false);
            removeCallbacks(mPendingLongPress);
            mHasPerformedLongPress = false;
            break;
        }
        return true;
    }
    private void performClickInternal() { performClick(); }

    public boolean dispatchKeyEvent(KeyEvent e) {
        if (mOnKey != null && mEnabled && mOnKey.onKey(this, e.getKeyCode(), e)) return true;
        return e.dispatch(this, null, this);
    }
    public boolean dispatchKeyEventPreIme(KeyEvent e) { return onKeyPreIme(e.getKeyCode(), e); }
    public boolean dispatchKeyShortcutEvent(KeyEvent e) { return onKeyShortcut(e.getKeyCode(), e); }
    public boolean dispatchUnhandledMove(View focused, int dir) { return false; }
    public boolean onKeyDown(int code, KeyEvent e) {
        if (KeyEvent.isConfirmKey(code) && mEnabled && (mClickable || mLongClickable)) { setPressed(true); return true; }
        return false;
    }
    public boolean onKeyUp(int code, KeyEvent e) {
        if (KeyEvent.isConfirmKey(code) && mEnabled && isPressed()) { setPressed(false); return performClick(); }
        return false;
    }
    public boolean onKeyLongPress(int code, KeyEvent e) { return false; }
    public boolean onKeyMultiple(int code, int n, KeyEvent e) { return false; }
    public boolean onKeyPreIme(int code, KeyEvent e) { return false; }
    public boolean onKeyShortcut(int code, KeyEvent e) { return false; }
    public KeyEvent.DispatcherState getKeyDispatcherState() { return mRoot != null ? mRoot.keyDispatcherState() : null; }

    // ---- saved state
    public void saveHierarchyState(SparseArray<Parcelable> c) { dispatchSaveInstanceState(c); }
    protected void dispatchSaveInstanceState(SparseArray<Parcelable> c) {
        if (mID != NO_ID && isSaveEnabled()) { Parcelable s = onSaveInstanceState(); if (s != null) c.put(mID, s); }
    }
    public void restoreHierarchyState(SparseArray<Parcelable> c) { dispatchRestoreInstanceState(c); }
    protected void dispatchRestoreInstanceState(SparseArray<Parcelable> c) {
        if (mID != NO_ID) { Parcelable s = c.get(mID); if (s != null) onRestoreInstanceState(s); }
    }
    protected Parcelable onSaveInstanceState() { return AbsSavedState.EMPTY_STATE; }
    protected void onRestoreInstanceState(Parcelable s) {}
    protected void dispatchFreezeSelfOnly(SparseArray<Parcelable> c) { dispatchSaveInstanceState(c); }
    protected void dispatchThawSelfOnly(SparseArray<Parcelable> c) { dispatchRestoreInstanceState(c); }

    // ---- odds
    public void bringToFront() { if (mParent != null) mParent.bringChildToFront(this); }
    public void setTranslationXHusk(float x) { setTranslationX(x); }
    public void onStartTemporaryDetach() {}
    public void onFinishTemporaryDetach() {}
    public void dispatchStartTemporaryDetach() {}
    public void dispatchFinishTemporaryDetach() {}
    public boolean isShowingLayoutBounds() { return false; }
    private android.view.autofill.AutofillId mAutofillId;
    public void setAutofillId(android.view.autofill.AutofillId id) { mAutofillId = id; }
    private static int sNextAutofillId = 1073741823;
    public android.view.autofill.AutofillId getAutofillId() {
        if (mAutofillId == null) synchronized (View.class) { mAutofillId = new android.view.autofill.AutofillId(sNextAutofillId++); }
        return mAutofillId;
    }
    public void autofill(android.view.autofill.AutofillValue v) {}
    public void autofill(android.util.SparseArray<android.view.autofill.AutofillValue> values) {}
    public void setTooltip(CharSequence t) { mTooltip = t; }
    public void setFocusedInCluster() {}
    public void setNextClusterForwardId(int id) {}
    public void setVerticalScrollbarThumbDrawable(Drawable d) {}
    public void setHorizontalScrollbarThumbDrawable(Drawable d) {}
    public void setScrollCaptureHint(int h) {}
    public void setPreferKeepClear(boolean b) {}
    public void setHandwritingDelegatorCallback(Runnable r) {}
    public void setAutoHandwritingEnabled(boolean b) {}
    public void setKeyboardNavigationClusterHusk(boolean b) {}
    public boolean dispatchNestedPrePerformAccessibilityAction(int a, Bundle b) { return false; }
    public boolean onTouchEventHusk(MotionEvent e) { return onTouchEvent(e); }
    public void transformMatrixToGlobal(Matrix m) {
        if (mParent != null) { mParent.transformMatrixToGlobal(m); m.preTranslate(-mParent.mScrollX, -mParent.mScrollY); }
        m.preTranslate(mLeft, mTop);
        if (!hasIdentityMatrix()) m.preConcat(getMatrix());
    }
    public void transformMatrixToLocal(Matrix m) {
        if (mParent != null) { mParent.transformMatrixToLocal(m); m.postTranslate(mParent.mScrollX, mParent.mScrollY); }
        m.postTranslate(-mLeft, -mTop);
        if (!hasIdentityMatrix()) m.postConcat(inverseMatrix());
    }
    public Bitmap huskSnapshot() { return getDrawingCache(); }
    public boolean isAggregatedVisible() { return isShown(); }
    public void onCancelPendingInputEvents() {}
    public void setOnReceiveContentListener(String[] mime, OnReceiveContentListener l) {}
    public ContentInfo performReceiveContent(ContentInfo c) { return c; }
    public ContentInfo onReceiveContent(ContentInfo c) { return c; }
    public void requestUnbufferedDispatch(MotionEvent e) {}
    public void requestUnbufferedDispatch(int sources) {}
    public void setSystemUiVisibilityHusk(int v) { setSystemUiVisibility(v); }
    public void setForceDarkAllowed(boolean b) {}
    public void setVisibilityHusk(int v) { setVisibility(v); }
    public static View inflate(Context c, int resource, ViewGroup root) { return LayoutInflater.from(c).inflate(resource, root); }
    // ---- the overlay: drawables (and a ViewGroup's, views) drawn over the view
    ViewOverlay mOverlay;
    public ViewOverlay getOverlay() {
        if (mOverlay == null) { mOverlay = new ViewOverlay(getContext(), this); if (mRoot != null) mOverlay.huskAttached(mRoot); }
        return mOverlay;
    }
    // ---- generated by tools/compat/genstubs.py: the platform's nested classes this class does not write
    public static final class InspectionCompanion implements android.view.inspector.InspectionCompanion {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public InspectionCompanion() {}
        public void mapProperties(android.view.inspector.PropertyMapper p0) {}
        public void readProperties(android.view.View p0, android.view.inspector.PropertyReader p1) {}
        public void readProperties(java.lang.Object p0, android.view.inspector.PropertyReader p1) {}
    }
    public static final class NoPreloadHolder {
        private final java.util.HashMap<String, Object> huskProps = new java.util.HashMap<>();
        public NoPreloadHolder() {}
        public static int[][] parseFrameRateMapping(java.lang.String p0) { return null; }
    }
    // ---- end of generated nested classes
    // ---- generated by tools/compat/fillmembers.py: the platform's members this class does not write (signatures only)
    private final java.util.HashMap<String, Object> huskFill = new java.util.HashMap<>();
    public static final int ACCESSIBILITY_CURSOR_POSITION_UNDEFINED = -1;
    public static final int ACCESSIBILITY_DATA_SENSITIVE_AUTO = 0;
    public static final int ACCESSIBILITY_DATA_SENSITIVE_NO = 2;
    public static final int ACCESSIBILITY_DATA_SENSITIVE_YES = 1;
    public static final int AUTOFILL_FLAG_INCLUDE_NOT_IMPORTANT_VIEWS = 1;
    public static final java.lang.String AUTOFILL_HINT_CREDENTIAL_MANAGER = "credential";
    public static final java.lang.String AUTOFILL_HINT_CREDIT_CARD_EXPIRATION_DATE = "creditCardExpirationDate";
    public static final java.lang.String AUTOFILL_HINT_CREDIT_CARD_EXPIRATION_DAY = "creditCardExpirationDay";
    public static final java.lang.String AUTOFILL_HINT_CREDIT_CARD_EXPIRATION_MONTH = "creditCardExpirationMonth";
    public static final java.lang.String AUTOFILL_HINT_CREDIT_CARD_EXPIRATION_YEAR = "creditCardExpirationYear";
    public static final java.lang.String AUTOFILL_HINT_CREDIT_CARD_NUMBER = "creditCardNumber";
    public static final java.lang.String AUTOFILL_HINT_CREDIT_CARD_SECURITY_CODE = "creditCardSecurityCode";
    public static final java.lang.String AUTOFILL_HINT_EMAIL_ADDRESS = "emailAddress";
    public static final java.lang.String AUTOFILL_HINT_NAME = "name";
    public static final java.lang.String AUTOFILL_HINT_PASSWORD = "password";
    public static final java.lang.String AUTOFILL_HINT_PASSWORD_AUTO = "passwordAuto";
    public static final java.lang.String AUTOFILL_HINT_PHONE = "phone";
    public static final java.lang.String AUTOFILL_HINT_POSTAL_ADDRESS = "postalAddress";
    public static final java.lang.String AUTOFILL_HINT_POSTAL_CODE = "postalCode";
    public static final java.lang.String AUTOFILL_HINT_USERNAME = "username";
    public static final int AUTOFILL_TYPE_DATE = 4;
    public static final int AUTOFILL_TYPE_LIST = 3;
    public static final int AUTOFILL_TYPE_TOGGLE = 2;
    public static final int CONTENT_SENSITIVITY_AUTO = 0;
    public static final int CONTENT_SENSITIVITY_NOT_SENSITIVE = 2;
    public static final int CONTENT_SENSITIVITY_SENSITIVE = 1;
    public static boolean DEBUG_DRAW;
    public static final int DRAG_FLAG_ACCESSIBILITY_ACTION = 1024;
    public static final int DRAG_FLAG_GLOBAL_PERSISTABLE_URI_PERMISSION = 64;
    public static final int DRAG_FLAG_GLOBAL_PREFIX_URI_PERMISSION = 128;
    public static final int DRAG_FLAG_GLOBAL_SAME_APPLICATION = 4096;
    public static final int DRAG_FLAG_GLOBAL_URI_READ = 1;
    public static final int DRAG_FLAG_GLOBAL_URI_WRITE = 2;
    public static final int DRAG_FLAG_HIDE_CALLING_TASK_ON_DRAG_START = 16384;
    public static final int DRAG_FLAG_OPAQUE = 512;
    public static final int DRAG_FLAG_REQUEST_SURFACE_FOR_RETURN_ANIMATION = 2048;
    public static final int DRAG_FLAG_START_INTENT_SENDER_ON_UNHANDLED_DRAG = 8192;
    protected static int[] ENABLED_FOCUSED_SELECTED_STATE_SET;
    protected static int[] ENABLED_FOCUSED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] ENABLED_FOCUSED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] ENABLED_SELECTED_STATE_SET;
    protected static int[] ENABLED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] ENABLED_WINDOW_FOCUSED_STATE_SET;
    public static final int FIND_VIEWS_WITH_ACCESSIBILITY_NODE_PROVIDERS = 4;
    public static final int FIND_VIEWS_WITH_CONTENT_DESCRIPTION = 2;
    public static final int FIND_VIEWS_WITH_TEXT = 1;
    protected static int[] FOCUSED_SELECTED_STATE_SET;
    protected static int[] FOCUSED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] FOCUSED_WINDOW_FOCUSED_STATE_SET;
    public static final int FRAME_RATE_CATEGORY_REASON_BOOST = 134217728;
    public static final int FRAME_RATE_CATEGORY_REASON_CONFLICTED = 167772160;
    public static final int FRAME_RATE_CATEGORY_REASON_INTERMITTENT = 33554432;
    public static final int FRAME_RATE_CATEGORY_REASON_INVALID = 83886080;
    public static final int FRAME_RATE_CATEGORY_REASON_LARGE = 50331648;
    public static final int FRAME_RATE_CATEGORY_REASON_REQUESTED = 67108864;
    public static final int FRAME_RATE_CATEGORY_REASON_SMALL = 16777216;
    public static final int FRAME_RATE_CATEGORY_REASON_TOUCH = 150994944;
    public static final int FRAME_RATE_CATEGORY_REASON_UNKNOWN = 0;
    public static final int FRAME_RATE_CATEGORY_REASON_VELOCITY = 100663296;
    public static final int IMPORTANT_FOR_AUTOFILL_YES_EXCLUDE_DESCENDANTS = 4;
    public static final int IMPORTANT_FOR_CONTENT_CAPTURE_AUTO = 0;
    public static final int IMPORTANT_FOR_CONTENT_CAPTURE_NO = 2;
    public static final int IMPORTANT_FOR_CONTENT_CAPTURE_NO_EXCLUDE_DESCENDANTS = 8;
    public static final int IMPORTANT_FOR_CONTENT_CAPTURE_YES = 1;
    public static final int IMPORTANT_FOR_CONTENT_CAPTURE_YES_EXCLUDE_DESCENDANTS = 4;
    public static final int KEEP_SCREEN_ON = 67108864;
    public static final int LAST_APP_AUTOFILL_ID = 1073741823;
    public static final int LAYOUT_DIRECTION_UNDEFINED = -1;
    public static final int POINTER_CAPTURE_MODE_ABSOLUTE = 1;
    public static final int POINTER_CAPTURE_MODE_RELATIVE = 2;
    public static final int POINTER_CAPTURE_MODE_UNCAPTURED = 0;
    protected static int[] PRESSED_ENABLED_FOCUSED_SELECTED_STATE_SET;
    protected static int[] PRESSED_ENABLED_FOCUSED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_ENABLED_FOCUSED_STATE_SET;
    protected static int[] PRESSED_ENABLED_FOCUSED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_ENABLED_SELECTED_STATE_SET;
    protected static int[] PRESSED_ENABLED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_ENABLED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_FOCUSED_SELECTED_STATE_SET;
    protected static int[] PRESSED_FOCUSED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_FOCUSED_STATE_SET;
    protected static int[] PRESSED_FOCUSED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_SELECTED_STATE_SET;
    protected static int[] PRESSED_SELECTED_WINDOW_FOCUSED_STATE_SET;
    protected static int[] PRESSED_WINDOW_FOCUSED_STATE_SET;
    public static final int PUBLIC_STATUS_BAR_VISIBILITY_MASK = 16375;
    public static final int RECTANGLE_ON_SCREEN_REQUEST_SOURCE_INPUT_FOCUS = 3;
    public static final int RECTANGLE_ON_SCREEN_REQUEST_SOURCE_SCROLL_ONLY = 1;
    public static final int RECTANGLE_ON_SCREEN_REQUEST_SOURCE_TEXT_CURSOR = 2;
    public static final int RECTANGLE_ON_SCREEN_REQUEST_SOURCE_UNDEFINED = 0;
    public static final float REQUESTED_FRAME_RATE_CATEGORY_DEFAULT = Float.NaN;
    public static final float REQUESTED_FRAME_RATE_CATEGORY_HIGH = -4.0f;
    public static final float REQUESTED_FRAME_RATE_CATEGORY_LOW = -2.0f;
    public static final float REQUESTED_FRAME_RATE_CATEGORY_NORMAL = -3.0f;
    public static final float REQUESTED_FRAME_RATE_CATEGORY_NO_PREFERENCE = -1.0f;
    public static final int SCROLL_CAPTURE_HINT_AUTO = 0;
    public static final int SCROLL_CAPTURE_HINT_EXCLUDE = 1;
    public static final int SCROLL_CAPTURE_HINT_EXCLUDE_DESCENDANTS = 4;
    public static final int SCROLL_CAPTURE_HINT_INCLUDE = 2;
    protected static int[] SELECTED_WINDOW_FOCUSED_STATE_SET;
    public static final int SOUND_EFFECTS_ENABLED = 134217728;
    public static final int STATUS_BAR_DISABLE_BACK = 4194304;
    public static final int STATUS_BAR_DISABLE_CLOCK = 8388608;
    public static final int STATUS_BAR_DISABLE_EXPAND = 65536;
    public static final int STATUS_BAR_DISABLE_HOME = 2097152;
    public static final int STATUS_BAR_DISABLE_NOTIFICATION_ALERTS = 262144;
    public static final int STATUS_BAR_DISABLE_NOTIFICATION_ICONS = 131072;
    public static final int STATUS_BAR_DISABLE_NOTIFICATION_TICKER = 524288;
    public static final int STATUS_BAR_DISABLE_ONGOING_CALL_CHIP = 67108864;
    public static final int STATUS_BAR_DISABLE_RECENT = 16777216;
    public static final int STATUS_BAR_DISABLE_SEARCH = 33554432;
    public static final int STATUS_BAR_DISABLE_SYSTEM_INFO = 1048576;
    public static final int SYSTEM_UI_CLEARABLE_FLAGS = 7;
    public static final int TEXT_DIRECTION_FIRST_STRONG_LTR = 6;
    public static final int TEXT_DIRECTION_FIRST_STRONG_RTL = 7;
    protected static final java.lang.String VIEW_LOG_TAG = "View";
    protected static final int VIEW_STRUCTURE_FOR_ASSIST = 0;
    protected static final int VIEW_STRUCTURE_FOR_AUTOFILL = 1;
    protected static final int VIEW_STRUCTURE_FOR_CONTENT_CAPTURE = 2;
    protected static boolean sBrokenWindowBackground;
    public static boolean sDebugViewAttributes;
    public static java.lang.String sDebugViewAttributesApplicationPackage;
    protected static boolean sPreserveMarginParamsInLayoutParamConversion;
    protected static boolean sToolkitSetFrameRateReadOnlyFlagValue;
    public java.lang.String[] mAttributes;
    public boolean mCachingFailed;
    protected android.view.animation.Animation mCurrentAnimation;
    protected int mUserPaddingBottom;
    protected int mUserPaddingLeft;
    protected int mUserPaddingRight;
    protected static java.lang.String debugIndent(int p0) { return null; }
    public static boolean isDefaultFocusHighlightEnabled() { return false; }
    public static boolean isLayoutModeOptical(java.lang.Object p0) { return false; }
    public static void setTraceLayoutSteps(boolean p0) {}
    public static void setTracedRequestLayoutClassClass(java.lang.String p0) {}
    public void addChildrenForAccessibility(java.util.ArrayList p0) {}
    public void addExtraDataToAccessibilityNodeInfo(android.view.accessibility.AccessibilityNodeInfo p0, java.lang.String p1, android.os.Bundle p2) {}
    public void addFrameMetricsListener(android.view.Window p0, android.view.Window.OnFrameMetricsAvailableListener p1, android.os.Handler p2) {}
    public void addKeyboardNavigationClusters(java.util.Collection p0, int p1) {}
    public void applyDrawableToTransparentRegion(android.graphics.drawable.Drawable p0, android.graphics.Region p1) {}
    protected boolean awakenScrollBars(int p0, boolean p1) { return false; }
    public boolean canHaveDisplayList() { return false; }
    public boolean canNotifyAutofillEnterExitEvent() { return false; }
    protected boolean canReceivePointerEvents() { return false; }
    public void cancelDragAndDrop() {}
    public void captureTransitioningViews(java.util.List p0) {}
    public void clearAccessibilityFocus() {}
    public void clearFocusInternal(android.view.View p0, boolean p1, boolean p2) {}
    public void clearPendingCredentialRequest() {}
    public void clearTranslationState() {}
    public void clearViewTranslationCallback() {}
    public void clearViewTranslationResponse() {}
    protected boolean computeFitSystemWindows(android.graphics.Rect p0, android.graphics.Rect p1) { return false; }
    protected void computeOpaqueFlags() {}
    public android.view.accessibility.AccessibilityNodeInfo createAccessibilityNodeInfoInternal() { return null; }
    public android.view.ScrollCaptureCallback createScrollCaptureCallbackInternal(android.graphics.Rect p0, android.graphics.Point p1) { return null; }
    public android.graphics.Bitmap createSnapshot(android.view.ViewDebug.CanvasProvider p0, boolean p1) { return null; }
    protected void damageInParent() {}
    public void debug() {}
    protected void debug(int p0) {}
    public boolean dispatchActivityResult(java.lang.String p0, int p1, int p2, android.content.Intent p3) { return false; }
    public void dispatchCreateViewTranslationRequest(java.util.Map p0, int[] p1, android.view.translation.TranslationCapability p2, java.util.List p3) {}
    public void dispatchDisplayHint(int p0) {}
    protected boolean dispatchGenericFocusedEvent(android.view.MotionEvent p0) { return false; }
    protected boolean dispatchGenericPointerEvent(android.view.MotionEvent p0) { return false; }
    protected void dispatchGetDisplayList() {}
    public void dispatchInitialProvideContentCaptureStructure() {}
    public void dispatchPointerCaptureChanged(boolean p0) {}
    public boolean dispatchPointerEvent(android.view.MotionEvent p0) { return false; }
    public void dispatchProvideAutofillStructure(android.view.ViewStructure p0, int p1) {}
    public void dispatchProvideStructure(android.view.ViewStructure p0) {}
    public void dispatchScrollCaptureSearch(android.graphics.Rect p0, android.graphics.Point p1, java.util.function.Consumer p2) {}
    public void dispatchWindowInsetsAnimationEnd(android.view.WindowInsetsAnimation p0) {}
    public void dispatchWindowInsetsAnimationPrepare(android.view.WindowInsetsAnimation p0) {}
    public android.view.WindowInsets dispatchWindowInsetsAnimationProgress(android.view.WindowInsets p0, java.util.List p1) { return null; }
    public android.view.WindowInsetsAnimation.Bounds dispatchWindowInsetsAnimationStart(android.view.WindowInsetsAnimation p0, android.view.WindowInsetsAnimation.Bounds p1) { return null; }
    public void dispatchWindowSystemUiVisiblityChanged(int p0) {}
    protected boolean drawsWithRenderNode(android.graphics.Canvas p0) { return false; }
    public void encode(android.view.ViewHierarchyEncoder p0) {}
    protected void encodeProperties(android.view.ViewHierarchyEncoder p0) {}
    public void fakeFocusAfterAttachingToWindow() {}
    public void findAutofillableViewsByTraversal(java.util.List p0) {}
    public void findNamedViews(java.util.Map p0) {}
    public android.window.OnBackInvokedDispatcher findOnBackInvokedDispatcher() { return null; }
    public android.view.View findViewByAccessibilityIdTraversal(int p0) { return this; }
    public android.view.View findViewByAutofillIdTraversal(int p0) { return this; }
    public android.view.View findViewByPredicate(java.util.function.Predicate p0) { return this; }
    public android.view.View findViewByPredicateInsideOut(android.view.View p0, java.util.function.Predicate p1) { return this; }
    protected android.view.View findViewByPredicateTraversal(java.util.function.Predicate p0, android.view.View p1) { return this; }
    public void finishMovingTask() {}
    public void generateDisplayHash(java.lang.String p0, android.graphics.Rect p1, java.util.concurrent.Executor p2, android.view.displayhash.DisplayHashResultCallback p3) {}
    public java.lang.CharSequence getAccessibilityPaneTitle() { return null; }
    public int getAccessibilitySelectionEnd() { return 0; }
    public int getAccessibilitySelectionStart() { return 0; }
    public int getAccessibilityTraversalAfter() { return 0; }
    public int getAccessibilityTraversalBefore() { return 0; }
    public int getAccessibilityViewId() { return 0; }
    public int getAccessibilityWindowId() { return 0; }
    public java.lang.String getAllowedHandwritingDelegatePackageName() { return null; }
    public java.lang.String getAllowedHandwritingDelegatorPackageName() { return null; }
    public java.lang.String[] getAutofillHints() { return null; }
    public android.view.autofill.AutofillValue getAutofillValue() { return null; }
    public int getAutofillViewId() { return 0; }
    public android.graphics.BlendMode getBackgroundTintBlendMode() { return null; }
    protected int getBottomPaddingOffset() { return 0; }
    public void getBoundsInWindow(android.graphics.Rect p0, boolean p1) {}
    public void getBoundsOnScreen(android.graphics.Rect p0) {}
    public void getBoundsOnScreen(android.graphics.Rect p0, boolean p1) {}
    public void getBoundsOnScreen(android.graphics.RectF p0, boolean p1) {}
    public android.view.contentcapture.ContentCaptureSession getContentCaptureSession() { return (android.view.contentcapture.ContentCaptureSession) huskFill.get("ContentCaptureSession"); }
    public int getContentSensitivity() { return (huskFill.get("ContentSensitivity") instanceof Integer ? (Integer) huskFill.get("ContentSensitivity") : 0); }
    public boolean getDefaultFocusHighlightEnabled() { return false; }
    public int getDrawingCacheBackgroundColor() { return 0; }
    public int getDrawingCacheQuality() { return 0; }
    protected int getFadeHeight(boolean p0) { return 0; }
    protected int getFadeTop(boolean p0) { return 0; }
    public int getFadingEdge() { return 0; }
    public int getFadingEdgeLength() { return 0; }
    public boolean getFilterTouchesWhenObscured() { return (huskFill.get("FilterTouchesWhenObscured") instanceof Boolean ? (Boolean) huskFill.get("FilterTouchesWhenObscured") : false); }
    public void getFocusedRect(android.graphics.Rect p0) {}
    public android.graphics.BlendMode getForegroundTintBlendMode() { return (android.graphics.BlendMode) huskFill.get("ForegroundTintBlendMode"); }
    public android.content.res.ColorStateList getForegroundTintList() { return null; }
    public android.graphics.PorterDuff.Mode getForegroundTintMode() { return (android.graphics.PorterDuff.Mode) huskFill.get("ForegroundTintMode"); }
    public float getFrameContentVelocity() { return (huskFill.get("FrameContentVelocity") instanceof Float ? (Float) huskFill.get("FrameContentVelocity") : 0f); }
    public android.graphics.Rect getHandwritingArea() { return (android.graphics.Rect) huskFill.get("HandwritingArea"); }
    public float getHandwritingBoundsOffsetBottom() { return 0f; }
    public float getHandwritingBoundsOffsetLeft() { return 0f; }
    public float getHandwritingBoundsOffsetRight() { return 0f; }
    public float getHandwritingBoundsOffsetTop() { return 0f; }
    public int getHandwritingDelegateFlags() { return (huskFill.get("HandwritingDelegateFlags") instanceof Integer ? (Integer) huskFill.get("HandwritingDelegateFlags") : 0); }
    public java.lang.Runnable getHandwritingDelegatorCallback() { return null; }
    public boolean getHasOverlappingRendering() { return false; }
    protected float getHorizontalScrollFactor() { return 0f; }
    protected int getHorizontalScrollbarHeight() { return 0; }
    public android.graphics.drawable.Drawable getHorizontalScrollbarThumbDrawable() { return null; }
    public android.graphics.drawable.Drawable getHorizontalScrollbarTrackDrawable() { return (android.graphics.drawable.Drawable) huskFill.get("HorizontalScrollbarTrackDrawable"); }
    public void getHotspotBounds(android.graphics.Rect p0) {}
    public int getImportantForContentCapture() { return 0; }
    public android.graphics.Matrix getInverseMatrix() { return null; }
    public java.lang.CharSequence getIterableTextForAccessibility() { return null; }
    protected float getLeftFadingEdgeStrength() { return 0f; }
    protected int getLeftPaddingOffset() { return 0; }
    public int[] getLocationOnScreen() { return null; }
    protected int getLongPressTimeoutMillis() { return 0; }
    public int getNextClusterForwardId() { return 0; }
    protected boolean getNotifiedContentCaptureAppeared() { return false; }
    public android.view.View.OnLongClickListener getOnLongClickListener() { return null; }
    public android.graphics.Insets getOpticalInsets() { return (android.graphics.Insets) huskFill.get("OpticalInsets"); }
    public int getOutlineAmbientShadowColor() { return 0; }
    public int getOutlineSpotShadowColor() { return 0; }
    public android.os.OutcomeReceiver getPendingCredentialCallback() { return null; }
    public android.credentials.GetCredentialRequest getPendingCredentialRequest() { return (android.credentials.GetCredentialRequest) huskFill.get("PendingCredentialRequest"); }
    public java.util.List getPreferKeepClearRects() { return (huskFill.get("PreferKeepClearRects") != null ? (java.util.List) huskFill.get("PreferKeepClearRects") : new java.util.ArrayList()); }
    public int getRawTextAlignment() { return 0; }
    public int getRawTextDirection() { return 0; }
    public java.lang.String[] getReceiveContentMimeTypes() { return null; }
    public float getRequestedFrameRate() { return (huskFill.get("RequestedFrameRate") instanceof Float ? (Float) huskFill.get("RequestedFrameRate") : 0f); }
    protected float getRightFadingEdgeStrength() { return 0f; }
    protected int getRightPaddingOffset() { return 0; }
    public android.view.AttachedSurfaceControl getRootSurfaceControl() { return null; }
    public int getScrollBarDefaultDelayBeforeFade() { return (huskFill.get("ScrollBarDefaultDelayBeforeFade") instanceof Integer ? (Integer) huskFill.get("ScrollBarDefaultDelayBeforeFade") : 0); }
    public int getScrollBarFadeDuration() { return (huskFill.get("ScrollBarFadeDuration") instanceof Integer ? (Integer) huskFill.get("ScrollBarFadeDuration") : 0); }
    public int getScrollBarSize() { return 0; }
    public int getScrollCaptureHint() { return 0; }
    public int getSourceLayoutResId() { return 0; }
    public java.lang.CharSequence getStateDescription() { return null; }
    public java.lang.CharSequence getSupplementalDescription() { return (java.lang.CharSequence) huskFill.get("SupplementalDescription"); }
    public java.util.List getSystemGestureExclusionRects() { return new java.util.ArrayList(); }
    public java.lang.CharSequence getTooltip() { return null; }
    public android.view.View getTooltipView() { return this; }
    protected int getTopPaddingOffset() { return 0; }
    public long getUniqueDrawingId() { return 0L; }
    public java.util.List getUnrestrictedPreferKeepClearRects() { return (huskFill.get("UnrestrictedPreferKeepClearRects") != null ? (java.util.List) huskFill.get("UnrestrictedPreferKeepClearRects") : new java.util.ArrayList()); }
    protected float getVerticalScrollFactor() { return 0f; }
    public int getVerticalScrollbarPosition() { return 0; }
    public android.graphics.drawable.Drawable getVerticalScrollbarThumbDrawable() { return null; }
    public android.graphics.drawable.Drawable getVerticalScrollbarTrackDrawable() { return (android.graphics.drawable.Drawable) huskFill.get("VerticalScrollbarTrackDrawable"); }
    public android.view.translation.ViewTranslationCallback getViewTranslationCallback() { return (android.view.translation.ViewTranslationCallback) huskFill.get("ViewTranslationCallback"); }
    public android.view.translation.ViewTranslationResponse getViewTranslationResponse() { return null; }
    protected android.view.IWindow getWindow() { return null; }
    protected boolean handleScrollBarDragging(android.view.MotionEvent p0) { return false; }
    protected boolean hasContentOnApplyWindowInsetsListener() { return false; }
    protected boolean hasHoveredChild() { return false; }
    public boolean hasImeFocus() { return false; }
    protected boolean hasOpaqueScrollbars() { return false; }
    public boolean hasShadow() { return false; }
    public boolean hasTranslationTransientState() { return false; }
    public boolean hasWindowInsetsAnimationCallback() { return false; }
    public boolean hideAutofillHighlight() { return false; }
    public boolean includeForAccessibility() { return false; }
    public boolean includeForAccessibility(boolean p0) { return false; }
    protected void initializeFadingEdge(android.content.res.TypedArray p0) {}
    protected void initializeFadingEdgeInternal(android.content.res.TypedArray p0) {}
    protected void initializeScrollbars(android.content.res.TypedArray p0) {}
    protected void initializeScrollbarsInternal(android.content.res.TypedArray p0) {}
    protected void internalSetPadding(int p0, int p1, int p2, int p3) {}
    public void invalidate(boolean p0) {}
    protected void invalidateParentCaches() {}
    protected void invalidateParentIfNeeded() {}
    protected void invalidateParentIfNeededAndWasQuickRejected() {}
    public boolean isAccessibilityDataSensitive() { return (huskFill.get("AccessibilityDataSensitive") instanceof Boolean ? (Boolean) huskFill.get("AccessibilityDataSensitive") : false); }
    public boolean isAccessibilityHeading() { return false; }
    public boolean isAccessibilitySelectionExtendable() { return false; }
    public boolean isActionableForAccessibility() { return false; }
    public boolean isAssistBlocked() { return (huskFill.get("AssistBlocked") instanceof Boolean ? (Boolean) huskFill.get("AssistBlocked") : false); }
    public boolean isAutoHandwritingEnabled() { return false; }
    public boolean isAutofilled() { return (huskFill.get("Autofilled") instanceof Boolean ? (Boolean) huskFill.get("Autofilled") : false); }
    public boolean isContentSensitive() { return false; }
    public boolean isCredential() { return false; }
    public boolean isDefaultFocusHighlightNeeded(android.graphics.drawable.Drawable p0, android.graphics.drawable.Drawable p1) { return false; }
    public boolean isFocusedByDefault() { return false; }
    public boolean isForceDarkAllowed() { return false; }
    public boolean isForegroundInsidePadding() { return false; }
    public boolean isFrameworkOptionalFitsSystemWindows() { return false; }
    public boolean isHandwritingDelegate() { return false; }
    public boolean isImportantForAutofill() { return false; }
    public boolean isImportantForContentCapture() { return false; }
    public boolean isInScrollingContainer() { return false; }
    public boolean isKeyboardNavigationCluster() { return false; }
    public boolean isLayoutDirectionInherited() { return false; }
    public boolean isLayoutRtl() { return false; }
    protected boolean isPaddingOffsetRequired() { return false; }
    public boolean isPreferKeepClear() { return false; }
    public boolean isRootNamespace() { return false; }
    public boolean isScreenReaderFocusable() { return false; }
    public boolean isScrollbarFadingEnabled() { return false; }
    public boolean isStylusHandwritingAvailable() { return false; }
    public boolean isTextAlignmentInherited() { return false; }
    public boolean isTextDirectionInherited() { return false; }
    protected boolean isVerticalScrollBarHidden() { return false; }
    public boolean isVisibleToUser() { return false; }
    protected boolean isVisibleToUser(android.graphics.Rect p0) { return false; }
    public boolean isVisibleToUserForAutofill(int p0) { return false; }
    public android.view.View keyboardNavigationClusterSearch(android.view.View p0, int p1) { return this; }
    public void makeFrameworkOptionalFitsSystemWindows() {}
    public void makeOptionalFitsSystemWindows() {}
    public void mapRectFromViewToScreenCoords(android.graphics.RectF p0, boolean p1) {}
    public void mapRectFromViewToWindowCoords(android.graphics.RectF p0, boolean p1) {}
    public void notifyEnterOrExitForAutoFillIfNeeded(boolean p0) {}
    public void notifySubtreeAccessibilityStateChangedIfNeeded() {}
    public void notifyViewAccessibilityStateChangedIfNeeded(int p0) {}
    public void onActivityResult(int p0, int p1, android.content.Intent p2) {}
    public boolean onCapturedPointerEvent(android.view.MotionEvent p0) { return false; }
    public void onCloseSystemDialogs(java.lang.String p0) {}
    protected void onCreateContextMenu(android.view.ContextMenu p0) {}
    public void onCreateViewTranslationRequest(int[] p0, java.util.function.Consumer p1) {}
    public void onCreateVirtualViewTranslationRequests(long[] p0, int[] p1, java.util.function.Consumer p2) {}
    protected void onDisplayHint(int p0) {}
    protected void onDrawHorizontalScrollBar(android.graphics.Canvas p0, android.graphics.drawable.Drawable p1, int p2, int p3, int p4, int p5) {}
    protected void onDrawScrollBars(android.graphics.Canvas p0) {}
    protected void onDrawVerticalScrollBar(android.graphics.Canvas p0, android.graphics.drawable.Drawable p1, int p2, int p3, int p4, int p5) {}
    protected void onFocusLost() {}
    public void onGetCredentialException(java.lang.String p0, java.lang.String p1) {}
    public void onGetCredentialResponse(android.credentials.GetCredentialResponse p0) {}
    public void onInputConnectionClosedInternal() {}
    public void onInputConnectionOpenedInternal(android.view.inputmethod.InputConnection p0, android.view.inputmethod.EditorInfo p1, android.os.Handler p2) {}
    public void onMovedToDisplay(int p0, android.content.res.Configuration p1) {}
    public void onPointerCaptureChange(boolean p0) {}
    public void onPopulateAccessibilityEventInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public void onProvideAutofillStructure(android.view.ViewStructure p0, int p1) {}
    public void onProvideAutofillVirtualStructure(android.view.ViewStructure p0, int p1) {}
    public void onProvideContentCaptureStructure(android.view.ViewStructure p0, int p1) {}
    public void onProvideStructure(android.view.ViewStructure p0) {}
    protected void onProvideStructure(android.view.ViewStructure p0, int p1, int p2) {}
    public void onProvideVirtualStructure(android.view.ViewStructure p0) {}
    public void onResolveDrawables(int p0) {}
    public void onScreenStateChanged(int p0) {}
    public void onScrollCaptureSearch(android.graphics.Rect p0, android.graphics.Point p1, java.util.function.Consumer p2) {}
    protected boolean onSetAlpha(int p0) { return false; }
    public void onSystemBarAppearanceChanged(int p0) {}
    public void onViewTranslationResponse(android.view.translation.ViewTranslationResponse p0) {}
    public void onVirtualViewTranslationResponses(android.util.LongSparseArray p0) {}
    public void outputDirtyFlags(java.lang.String p0, boolean p1, int p2) {}
    protected boolean performButtonActionOnTouchDown(android.view.MotionEvent p0) { return false; }
    public void performHapticFeedbackForInputDevice(int p0, int p1, int p2, int p3) {}
    protected boolean pointInHoveredChild(android.view.MotionEvent p0) { return false; }
    public boolean pointInView(float p0, float p1, float p2) { return false; }
    public void postInvalidateDelayed(long p0, int p1, int p2, int p3, int p4) {}
    public void prepareForExtendedAccessibilitySelection() {}
    public boolean probablyHasInput() { return false; }
    protected void recomputePadding() {}
    public void removeFrameMetricsListener(android.view.Window.OnFrameMetricsAvailableListener p0) {}
    public boolean requestAccessibilityFocus() { return false; }
    public void requestKeyboardShortcuts(java.util.List p0, int p1) {}
    public void requestPointerCapture(int p0) {}
    public boolean requestRectangleOnScreen(android.graphics.Rect p0, boolean p1, int p2) { return false; }
    public void resetPaddingToInitialValues() {}
    protected void resetResolvedDrawables() {}
    public void resetResolvedLayoutDirection() {}
    public void resetResolvedPadding() {}
    public void resetResolvedTextAlignment() {}
    public void resetResolvedTextDirection() {}
    public void resetRtlProperties() {}
    public void resetSubtreeAutofillIds() {}
    protected void resolveDrawables() {}
    public boolean resolveLayoutDirection() { return false; }
    public void resolveLayoutParams() {}
    public void resolvePadding() {}
    public boolean resolveRtlPropertiesIfNeeded() { return false; }
    public boolean resolveTextAlignment() { return false; }
    public boolean resolveTextDirection() { return false; }
    public boolean restoreFocusInCluster(int p0) { return false; }
    public boolean restoreFocusNotInCluster() { return false; }
    public void sendAccessibilityEventUncheckedInternal(android.view.accessibility.AccessibilityEvent p0) {}
    public void setAccessibilityDataSensitive(int p0) { huskFill.put("AccessibilityDataSensitive", Integer.valueOf(p0)); }
    public void setAccessibilitySelection(int p0, int p1) {}
    public void setAllowedHandwritingDelegatePackage(java.lang.String p0) {}
    public void setAllowedHandwritingDelegatorPackage(java.lang.String p0) {}
    public void setAssistBlocked(boolean p0) { huskFill.put("AssistBlocked", Boolean.valueOf(p0)); }
    public void setAutofilled(boolean p0, boolean p1) {}
    public void setBackdropRenderEffect(android.graphics.RenderEffect p0) {}
    public void setContentCaptureSession(android.view.contentcapture.ContentCaptureSession p0) { huskFill.put("ContentCaptureSession", p0); }
    public void setContentSensitivity(int p0) { huskFill.put("ContentSensitivity", Integer.valueOf(p0)); }
    protected void setDetached(boolean p0) {}
    public void setDisabledSystemUiVisibility(int p0) {}
    public void setFilterTouchesWhenObscured(boolean p0) { huskFill.put("FilterTouchesWhenObscured", Boolean.valueOf(p0)); }
    public void setForegroundTintBlendMode(android.graphics.BlendMode p0) { huskFill.put("ForegroundTintBlendMode", p0); }
    public void setForegroundTintMode(android.graphics.PorterDuff.Mode p0) { huskFill.put("ForegroundTintMode", p0); }
    public void setFrameContentVelocity(float p0) { huskFill.put("FrameContentVelocity", Float.valueOf(p0)); }
    public void setHandwritingArea(android.graphics.Rect p0) { huskFill.put("HandwritingArea", p0); }
    public void setHandwritingBoundsOffsets(float p0, float p1, float p2, float p3) {}
    public void setHandwritingDelegateFlags(int p0) { huskFill.put("HandwritingDelegateFlags", Integer.valueOf(p0)); }
    public void setHasTranslationTransientState(boolean p0) {}
    public void setHorizontalScrollbarTrackDrawable(android.graphics.drawable.Drawable p0) { huskFill.put("HorizontalScrollbarTrackDrawable", p0); }
    public void setIsCredential(boolean p0) {}
    public void setIsHandwritingDelegate(boolean p0) {}
    public void setIsRootNamespace(boolean p0) {}
    public void setNotifyAutofillManagerOnClick(boolean p0) {}
    public void setOpticalInsets(android.graphics.Insets p0) { huskFill.put("OpticalInsets", p0); }
    public void setPendingCredentialRequest(android.credentials.GetCredentialRequest p0, android.os.OutcomeReceiver p1) {}
    public void setPreferKeepClearRects(java.util.List p0) { huskFill.put("PreferKeepClearRects", p0); }
    public void setRenderEffect(android.graphics.RenderEffect p0) {}
    public void setRequestedFrameRate(float p0) { huskFill.put("RequestedFrameRate", Float.valueOf(p0)); }
    public void setRevealClip(boolean p0, float p1, float p2, float p3) {}
    public void setScrollBarDefaultDelayBeforeFade(int p0) { huskFill.put("ScrollBarDefaultDelayBeforeFade", Integer.valueOf(p0)); }
    public void setScrollBarFadeDuration(int p0) { huskFill.put("ScrollBarFadeDuration", Integer.valueOf(p0)); }
    public void setScrollCaptureCallback(android.view.ScrollCaptureCallback p0) {}
    public void setShowingLayoutBounds(boolean p0) {}
    public void setSupplementalDescription(java.lang.CharSequence p0) { huskFill.put("SupplementalDescription", p0); }
    public void setTagInternal(int p0, java.lang.Object p1) {}
    public void setTransitionVisibility(int p0) {}
    public void setUnrestrictedPreferKeepClearRects(java.util.List p0) { huskFill.put("UnrestrictedPreferKeepClearRects", p0); }
    public void setUsageHint(int p0) {}
    public void setVerticalScrollbarTrackDrawable(android.graphics.drawable.Drawable p0) { huskFill.put("VerticalScrollbarTrackDrawable", p0); }
    public void setViewTranslationCallback(android.view.translation.ViewTranslationCallback p0) { huskFill.put("ViewTranslationCallback", p0); }
    public boolean shouldTrackHandwritingArea() { return false; }
    public android.view.ActionMode startActionMode(android.view.ActionMode.Callback p0) { return null; }
    public android.view.ActionMode startActionMode(android.view.ActionMode.Callback p0, int p1) { return null; }
    public void startActivityForResult(android.content.Intent p0, int p1) {}
    public boolean startMovingTask(float p0, float p1) { return false; }
    public boolean toGlobalMotionEvent(android.view.MotionEvent p0) { return false; }
    public boolean toLocalMotionEvent(android.view.MotionEvent p0) { return false; }
    public void transformFromViewToWindowSpace(int[] p0) {}
    public void transformMatrixRootToLocal(android.graphics.Matrix p0) {}
    public android.graphics.RenderNode updateDisplayListIfDirty() { return null; }
    public void updateDragShadow(android.view.View.DragShadowBuilder p0) {}
    // ---- end of generated members
}
