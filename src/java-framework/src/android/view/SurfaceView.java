package android.view;

import java.util.ArrayList;

public class SurfaceView extends View {
    private final ArrayList<SurfaceHolder.Callback> callbacks = new ArrayList<>();
    private final Surface surface = new Surface();
    private boolean created;
    private final SurfaceHolder holder = new SurfaceHolder() {
        public void addCallback(Callback c) { callbacks.add(c); }
        public void removeCallback(Callback c) { callbacks.remove(c); }
        public Surface getSurface() { return surface; }
        public void setFormat(int f) {} public void setType(int t) {} public void setFixedSize(int w, int h) {} public void setSizeFromLayout() {}
        public void setKeepScreenOn(boolean b) {}
        public android.graphics.Rect getSurfaceFrame() { return new android.graphics.Rect(0, 0, getWidth(), getHeight()); }
        public boolean isCreating() { return false; }
    };
    public SurfaceView(android.content.Context c) { super(c); }
    public SurfaceView(android.content.Context c, android.util.AttributeSet a) { super(c); }
    public SurfaceHolder getHolder() { return holder; }
    public void setZOrderOnTop(boolean b) {} public void setZOrderMediaOverlay(boolean b) {}
    @Override protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (!created) { created = true; for (SurfaceHolder.Callback c : new ArrayList<>(callbacks)) c.surfaceCreated(holder); }
    }
    @Override protected void onSizeChanged(int w, int h, int ow, int oh) {
        super.onSizeChanged(w, h, ow, oh);
        for (SurfaceHolder.Callback c : new ArrayList<>(callbacks)) c.surfaceChanged(holder, 1, w, h);
    }
}
