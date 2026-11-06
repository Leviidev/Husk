import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.view.View;
import android.widget.FrameLayout;

/** Native graphics test: a hardware-layer hint must not swallow a native surface's clear hole. */
public final class NativeSurfaceLayerTest {
    static final class ClearView extends View {
        private final boolean nativeContent;
        ClearView(boolean nativeContent) { super(husk.ContextImpl.app()); this.nativeContent = nativeContent; setLayerType(LAYER_TYPE_HARDWARE, null); }
        @Override protected boolean huskHasNativeContent() { return nativeContent; }
        @Override protected void onDraw(Canvas canvas) { canvas.drawColor(Color.TRANSPARENT, PorterDuff.Mode.CLEAR); }
    }
    private static int draw(boolean nativeContent) {
        FrameLayout parent = new FrameLayout(husk.ContextImpl.app());
        parent.setBackgroundColor(Color.RED);
        parent.addView(new ClearView(nativeContent), new FrameLayout.LayoutParams(1, 1));
        parent.measure(View.MeasureSpec.makeMeasureSpec(2, View.MeasureSpec.EXACTLY),
                       View.MeasureSpec.makeMeasureSpec(2, View.MeasureSpec.EXACTLY));
        parent.layout(0, 0, 2, 2);
        Bitmap bitmap = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        parent.draw(canvas);
        int hole = bitmap.getPixel(0, 0), outside = bitmap.getPixel(1, 1);
        canvas.huskRelease(); bitmap.recycle();
        if (outside != Color.RED) throw new AssertionError("clear escaped the child's clip");
        return hole;
    }
    public static String run() {
        if (draw(false) != Color.RED) throw new AssertionError("ordinary hardware-layer clear must stay isolated");
        if (draw(true) != Color.TRANSPARENT) throw new AssertionError("native content must clear through the window layer");
        return "PASS: native surface clear survives hardware-layer hints; ordinary layer masks remain isolated";
    }
}
