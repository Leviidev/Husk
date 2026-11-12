import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.ColorSpace;
import android.graphics.Paint;
import android.graphics.SurfaceTexture;
import android.hardware.HardwareBuffer;
import android.media.Image;
import android.media.ImageReader;
import android.view.Surface;
import java.nio.ByteBuffer;

/** Run on Husk's real Dalvik/Gfx implementation, not on a host-JDK graphics stub. */
public final class ImageReaderBufferTest {
    private static void check(boolean condition, String label) {
        if (!condition) throw new AssertionError(label);
    }
    private static void post(Surface surface, int color) {
        Canvas canvas = surface.lockCanvas(null);
        canvas.drawColor(color);
        surface.unlockCanvasAndPost(canvas);
    }
    private static Bitmap bitmap(Image image) {
        HardwareBuffer buffer = image.getHardwareBuffer();
        try { return Bitmap.wrapHardwareBuffer(buffer, ColorSpace.get(ColorSpace.Named.SRGB)); }
        finally { buffer.close(); }
    }
    public static String run() {
        check(!HardwareBuffer.isSupported(0, 2, 1, 1, 768), "zero width unsupported");
        check(!HardwareBuffer.isSupported(2, 2, HardwareBuffer.YCBCR_420_888, 1, 768), "YUV is not advertised");
        check(!HardwareBuffer.isSupported(2, 2, 1, 1, HardwareBuffer.USAGE_PROTECTED_CONTENT), "protected content unsupported");
        ImageReader reader = ImageReader.newInstance(2, 2, HardwareBuffer.RGBA_8888, 2, 768);
        check(reader.getWidth() == 2 && reader.getHeight() == 2 && reader.getMaxImages() == 2, "reader dimensions");
        check(reader.acquireLatestImage() == null, "no fabricated frame before producer posts");
        final int[] notifications = { 0 };
        reader.setOnImageAvailableListenerWithExecutor(value -> notifications[0]++, Runnable::run);
        Surface surface = reader.getSurface();
        post(surface, 0xffff0000);
        post(surface, 0xff0000ff);
        post(surface, 0xff00ff00);
        reader.discardFreeBuffers();
        check(notifications[0] == 3, "one listener delivery per real frame");
        Image first = reader.acquireNextImage();
        Bitmap blue = bitmap(first);
        check(blue.getPixel(0, 0) == 0xff0000ff, "bounded queue discards only unacquired oldest frame");
        Image second = reader.acquireLatestImage();
        Bitmap green = bitmap(second);
        check(green.getPixel(1, 1) == 0xff00ff00, "second frame contains producer pixels");
        check(second.getTimestamp() >= first.getTimestamp() && first.getTimestamp() > 0, "real frame timestamps");
        boolean exhausted = false;
        try { reader.acquireNextImage(); } catch (IllegalStateException expected) { exhausted = true; }
        check(exhausted && reader.acquireNextImageNoThrowISE() == null, "acquired-image limit enforced");
        Image.Plane plane = first.getPlanes()[0];
        ByteBuffer rgba = plane.getBuffer();
        check(plane.getPixelStride() == 4 && plane.getRowStride() == 8 && rgba.remaining() == 16, "RGBA plane layout");
        check((rgba.get(0) & 255) == 0 && (rgba.get(2) & 255) == 255 && (rgba.get(3) & 255) == 255, "plane stores actual blue RGBA pixels");
        HardwareBuffer retained = first.getHardwareBuffer();
        first.close(); first.close();
        check(!retained.isClosed(), "hardware buffer owns a separately closable image reference");
        boolean invalidPlane = false;
        try { plane.getBuffer(); } catch (IllegalStateException expected) { invalidPlane = true; }
        check(invalidPlane, "closed image invalidates planes");
        post(surface, 0xffffffff);
        check(blue.getPixel(0, 0) == 0xff0000ff && green.getPixel(0, 0) == 0xff00ff00,
              "producer cannot overwrite acquired or wrapped snapshots");
        second.close();
        post(surface, 0xff112233);
        Image latest = reader.acquireLatestImage();
        Bitmap newest = bitmap(latest);
        check(newest.getPixel(0, 0) == 0xff112233, "acquireLatest discards older pending frame");
        reader.close(); reader.close();
        boolean invalidImage = false;
        try { latest.getWidth(); } catch (IllegalStateException expected) { invalidImage = true; }
        check(invalidImage && !surface.isValid(), "reader close invalidates outstanding images and producer surface");
        Bitmap surviving = Bitmap.wrapHardwareBuffer(retained, null);
        retained.close();
        check(surviving.getPixel(0, 0) == 0xff0000ff && newest.getPixel(0, 0) == 0xff112233,
              "wrapped pixels and retained buffer survive Image/Reader close");
        blue.recycle(); green.recycle(); newest.recycle(); surviving.recycle();
        orientation();
        releaseLifecycle();
        return "PASS: real ImageReader pixels, queue order, maxImages, listeners, planes and close ownership";
    }
    public static String orientation() {
        ImageReader rows = ImageReader.newInstance(2, 2, HardwareBuffer.RGBA_8888, 2);
        for (int frame = 0; frame < 6; frame++) {
            Canvas canvas = rows.getSurface().lockCanvas(null);
            Paint paint = new Paint();
            paint.setColor(0xffff0000); canvas.drawRect(0, 0, 2, 1, paint);
            paint.setColor(0xff0000ff); canvas.drawRect(0, 1, 2, 2, paint);
            rows.getSurface().unlockCanvasAndPost(canvas);
            Image image = rows.acquireLatestImage();
            Bitmap pixels = bitmap(image);
            check(pixels.getPixel(0, 0) == 0xffff0000 && pixels.getPixel(0, 1) == 0xff0000ff,
                  "reused double buffer preserves top/bottom orientation at frame " + frame);
            pixels.recycle(); image.close();
        }
        rows.close();
        ImageReader nativeRows = ImageReader.newInstance(2, 2, HardwareBuffer.RGBA_8888, 2);
        Bitmap source = Bitmap.createBitmap(new int[] { 0xff0000ff, 0xff0000ff, 0xffff0000, 0xffff0000 },
                                            2, 2, Bitmap.Config.ARGB_8888);
        long address = husk.Gfx.bmAddress(source.huskNative());
        for (int frame = 0; frame < 6; frame++) {
            boolean flip = (frame & 1) == 0;
            nativeRows.getSurface().huskTexture.huskPostRgba(address, 2, 2, flip);
            Image image = nativeRows.acquireLatestImage();
            Bitmap pixels = bitmap(image);
            check(pixels.getPixel(0, 0) == (flip ? 0xffff0000 : 0xff0000ff)
                  && pixels.getPixel(0, 1) == (flip ? 0xff0000ff : 0xffff0000),
                  "native RGBA producer preserves explicit row order at frame " + frame);
            pixels.recycle(); image.close();
        }
        nativeRows.close(); source.recycle();
        return "PASS: six successive asymmetric ImageReader frames preserve orientation";
    }
    private static Object field(Object target, String name) {
        try {
            java.lang.reflect.Field field = SurfaceTexture.class.getDeclaredField(name);
            field.setAccessible(true);
            return field.get(target);
        } catch (Exception error) { throw new RuntimeException(error); }
    }
    public static String releaseLifecycle() {
        SurfaceTexture canvasTexture = new SurfaceTexture(false);
        canvasTexture.setDefaultBufferSize(2, 2);
        final int[] consumed = { 0 };
        canvasTexture.huskSetFrameConsumer((pixels, timestamp) -> { consumed[0]++; pixels.recycle(); });
        Canvas held = canvasTexture.huskLock(null);
        Bitmap back = (Bitmap) field(canvasTexture, "mBack");
        held.drawColor(0xffff0000);
        canvasTexture.release(); canvasTexture.release();
        check(canvasTexture.isReleased() && !back.isRecycled(), "release defers pixels held by Canvas producer");
        held.drawColor(0xff0000ff);
        canvasTexture.huskPost(held);
        check(back.isRecycled() && field(canvasTexture, "mBackCanvas") == null && field(canvasTexture, "mBack") == null,
              "producer completion releases its native Canvas and pixels");
        check(consumed[0] == 0 && canvasTexture.huskLock(null) == null, "released producer cannot publish or lock another frame");

        ImageReader reader = ImageReader.newInstance(2, 2, HardwareBuffer.RGBA_8888, 2);
        SurfaceTexture texture = reader.getSurface().huskTexture;
        post(reader.getSurface(), 0xff123456);
        Image image = reader.acquireLatestImage();
        HardwareBuffer retained = image.getHardwareBuffer();
        Bitmap source = Bitmap.createBitmap(2, 2, Bitmap.Config.ARGB_8888);
        source.eraseColor(0xffabcdef);
        long address = husk.Gfx.bmAddress(source.huskNative());
        Object gate = new Object();
        boolean[] state = { false, false };
        int[] notifications = { 0 };
        Throwable[] failure = { null };
        reader.setOnImageAvailableListenerWithExecutor(value -> {
            synchronized (gate) {
                notifications[0]++; state[0] = true; gate.notifyAll();
                long deadline = System.currentTimeMillis() + 5000;
                while (!state[1] && System.currentTimeMillis() < deadline)
                    try { gate.wait(50); } catch (InterruptedException error) { throw new RuntimeException(error); }
                check(state[1], "test callback release timed out");
            }
        }, Runnable::run);
        Thread producer = new Thread(() -> {
            try { texture.huskPostRgba(address, 2, 2, false); }
            catch (Throwable error) { failure[0] = error; }
        }, "image-reader-release-race");
        producer.setDaemon(true);
        Bitmap stage = null, front = null;
        try {
            producer.start();
            synchronized (gate) {
                long deadline = System.currentTimeMillis() + 5000;
                while (!state[0] && System.currentTimeMillis() < deadline) gate.wait(50);
                check(state[0], "native producer did not reach callback");
            }
            stage = (Bitmap) field(texture, "mStage"); front = texture.huskFront();
            reader.close();
            check(stage != null && front != null && !stage.isRecycled() && !front.isRecycled(),
                  "reader close does not recycle an in-flight native producer's pixels");
            check(texture.huskFront() == null && !texture.huskValid(), "released texture exposes no borrowed front pixels");
        } catch (InterruptedException error) { throw new RuntimeException(error); }
        finally {
            synchronized (gate) { state[1] = true; gate.notifyAll(); }
            try { producer.join(5000); } catch (InterruptedException error) { throw new RuntimeException(error); }
            reader.close();
        }
        check(!producer.isAlive() && failure[0] == null, "native producer finishes without release race or deadlock");
        check(stage.isRecycled() && front.isRecycled() && field(texture, "mStage") == null
              && field(texture, "mFront") == null && field(texture, "mBackCanvas") == null,
              "last native producer reclaims staging, front/back and Canvas after release");
        texture.huskPostRgba(address, 2, 2, false);
        check(notifications[0] == 1 && field(texture, "mStage") == null, "release unsubscribes callbacks and stops native allocation");
        Bitmap surviving = Bitmap.wrapHardwareBuffer(retained, null);
        retained.close();
        check(surviving.getPixel(0, 0) == 0xff123456, "independent retained image pixels survive producer release");
        surviving.recycle(); source.recycle();
        return "PASS: Canvas/native producer release races, prompt pixel cleanup and retained image ownership";
    }
}
