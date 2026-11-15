// SPDX-License-Identifier: GPL-2.0-or-later
package android.opengl;

import android.app.Activity;
import android.view.SurfaceHolder;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/** Exercises real surface callbacks and frame admission with only the host submission/wake boundary substituted. */
public final class GLSurfaceLifecycleTest {
    private static void check(boolean value, String message) {
        if (!value) throw new AssertionError(message);
    }

    private static final class Renderer implements GLSurfaceView.Renderer {
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {}
        public void onSurfaceChanged(GL10 gl, int width, int height) {}
        public void onDrawFrame(GL10 gl) {}
    }

    private static final class Surface extends GLSurfaceView {
        boolean attached;
        int starts, submittedVersion, submittedWidth, submittedHeight;
        GLSurfaceView.Renderer submittedRenderer;
        Surface() { super(new Activity()); }
        @Override public boolean isAttachedToWindow() { return attached; }
        // SurfaceView's actual callbacks are delivered explicitly below, as by the window system.
        @Override public boolean post(Runnable r) { return true; }
        @Override protected void onSizeChanged(int w, int h, int oldW, int oldH) {}
        @Override public void requestRender() {
            try { super.requestRender(); }
            catch (UnsatisfiedLinkError missingHostWake) { /* JVM has no host wake service. */ }
        }
        @Override protected void huskStartGL(GLSurfaceView.Renderer renderer, int version) {
            starts++;
            submittedRenderer = renderer;
            submittedVersion = version;
            int[] size = huskSurfaceSize();
            submittedWidth = size[0]; submittedHeight = size[1];
        }
        void attach() { attached = true; super.onAttachedToWindow(); }
        void queue(Runnable r) {
            try { queueEvent(r); }
            catch (UnsatisfiedLinkError missingHostWake) { /* queue insertion precedes the native wake. */ }
        }
    }

    public static void main(String[] args) {
        Renderer renderer = new Renderer();
        Surface earlyRenderer = new Surface();
        earlyRenderer.setEGLContextClientVersion(3);
        earlyRenderer.setRenderer(renderer);
        check(earlyRenderer.starts == 0, "setting a renderer before attachment does not start native GL");
        earlyRenderer.attach();
        check(earlyRenderer.starts == 0, "attachment without a sized surface does not start native GL");
        check(!earlyRenderer.huskHasSurface() && !earlyRenderer.huskBeginFrame(),
              "no rendering is admitted before a surface exists");
        earlyRenderer.layout(0, 0, 320, 180);
        SurfaceHolder holder = earlyRenderer.getHolder();
        earlyRenderer.surfaceCreated(holder);
        earlyRenderer.surfaceChanged(holder, android.graphics.PixelFormat.OPAQUE, 320, 180);
        check(earlyRenderer.starts == 1 && earlyRenderer.submittedRenderer == renderer,
              "surface readiness submits the selected renderer exactly once");
        check(earlyRenderer.submittedWidth == 320 && earlyRenderer.submittedHeight == 180 && earlyRenderer.submittedVersion == 3,
              "initial host submission receives nonzero surface dimensions and requested ES version");
        check(earlyRenderer.huskBeginFrame(), "continuous drawing is admitted with a live surface");
        check(earlyRenderer.huskTakeResize() == (((long)320 << 32) | 180), "initial resize reaches the renderer");
        check(earlyRenderer.huskTakeResize() == 0, "a resize is delivered once");

        earlyRenderer.surfaceDestroyed(holder);
        check(!earlyRenderer.huskHasSurface(), "destroy callback clears live-surface admission");
        final int[] queued = {0};
        earlyRenderer.queue(() -> queued[0]++);
        check(!earlyRenderer.huskBeginFrame(), "destroyed surfaces do not admit continuous rendering");
        check(queued[0] == 1, "queued GL work still runs before the draw gate");
        check(!earlyRenderer.huskBeginFrame() && queued[0] == 1, "destroyed frames remain blocked and queued work runs once");

        earlyRenderer.surfaceCreated(holder);
        earlyRenderer.surfaceChanged(holder, android.graphics.PixelFormat.OPAQUE, 640, 360);
        check(earlyRenderer.starts == 1, "recreation reuses the retained native renderer instead of starting another thread");
        check(earlyRenderer.huskHasSurface() && earlyRenderer.huskBeginFrame(), "surface recreation resumes drawing");
        check(earlyRenderer.huskTakeResize() == (((long)640 << 32) | 360), "recreation delivers the replacement size");

        earlyRenderer.setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        check(earlyRenderer.huskBeginFrame() && !earlyRenderer.huskBeginFrame(), "dirty-mode admission is consumed once");
        earlyRenderer.onPause();
        earlyRenderer.queue(() -> queued[0]++);
        check(!earlyRenderer.huskBeginFrame() && queued[0] == 2, "pause still services queued events without drawing");
        earlyRenderer.onResume();
        check(earlyRenderer.huskBeginFrame(), "resume requests a frame on an existing surface");

        Surface lateRenderer = new Surface();
        lateRenderer.attach();
        lateRenderer.layout(0, 0, 256, 144);
        lateRenderer.surfaceCreated(lateRenderer.getHolder());
        lateRenderer.surfaceChanged(lateRenderer.getHolder(), android.graphics.PixelFormat.OPAQUE, 256, 144);
        check(lateRenderer.starts == 0, "a ready surface waits for its renderer");
        lateRenderer.setRenderer(renderer);
        check(lateRenderer.starts == 1 && lateRenderer.submittedWidth == 256 && lateRenderer.submittedHeight == 144,
              "setting a renderer after surface creation starts GL with the existing dimensions");
        lateRenderer.surfaceChanged(lateRenderer.getHolder(), android.graphics.PixelFormat.OPAQUE, 512, 288);
        check(lateRenderer.starts == 1, "subsequent size callbacks do not duplicate initial GL startup");

        System.out.println("PASS: GLSurfaceView waits for sized surface callbacks, starts once in either renderer order, blocks destroyed/paused drawing while servicing events, and resumes retained-context drawing with replacement dimensions");
    }
}
