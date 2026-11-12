package android.media;

import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.hardware.HardwareBuffer;
import android.os.Handler;
import android.os.Looper;
import android.view.Surface;
import java.nio.ByteBuffer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.concurrent.Executor;

/**
 * A bounded queue of real RGBA Surface frames. The existing SurfaceTexture producer
 * accepts Canvas, ANativeWindow and EGL frames; acquired Images own independent
 * snapshots, so the producer cannot overwrite pixels still held by a consumer.
 */
public class ImageReader implements AutoCloseable {
    private final int mWidth, mHeight, mFormat, mMaxImages, mDataSpace;
    private final long mUsage;
    private final SurfaceTexture mTexture;
    private final Surface mSurface;
    private final ArrayDeque<Frame> mPending = new ArrayDeque<>();
    private final ArrayList<ReaderImage> mAcquired = new ArrayList<>();
    private boolean mClosed;
    private OnImageAvailableListener mListener;
    private Executor mExecutor;
    private int mListenerGeneration;

    private static final class Frame {
        final Bitmap bitmap;
        final long timestamp;
        Frame(Bitmap b, long t) { bitmap = b; timestamp = t; }
    }
    private ImageReader(int width, int height, int format, int maxImages, long usage, int dataSpace) {
        if (!HardwareBuffer.isSupported(width, height, format, 1, usage) || maxImages < 1)
            throw new IllegalArgumentException("ImageReader needs positive dimensions/maxImages and RGBA/RGBX pixels");
        mWidth = width; mHeight = height; mFormat = format; mMaxImages = maxImages; mUsage = usage; mDataSpace = dataSpace;
        mTexture = new SurfaceTexture(false);
        mTexture.setDefaultBufferSize(width, height);
        mTexture.huskSetFrameConsumer(this::posted);
        mSurface = new Surface(mTexture);
    }
    public static ImageReader newInstance(int width, int height, int format, int maxImages) {
        return new ImageReader(width, height, format, maxImages, HardwareBuffer.USAGE_CPU_READ_OFTEN, 0);
    }
    public static ImageReader newInstance(int width, int height, int format, int maxImages, long usage) {
        return new ImageReader(width, height, format, maxImages, usage, 0);
    }
    public static ImageReader newInstance(int width, int height, int format, int maxImages,
                                         android.hardware.camera2.MultiResolutionImageReader parent) {
        return newInstance(width, height, format, maxImages);
    }
    private void checkOpen() { if (mClosed) throw new IllegalStateException("ImageReader is closed"); }
    private void posted(Bitmap bitmap, long timestamp) {
        final OnImageAvailableListener listener;
        final Executor executor;
        final int generation;
        synchronized (this) {
            if (mClosed) { bitmap.recycle(); return; }
            mPending.addLast(new Frame(bitmap, timestamp));
            // A slow consumer drops unacquired old frames, while acquired Images retain their pixels.
            while (mPending.size() > mMaxImages) mPending.removeFirst().bitmap.recycle();
            listener = mListener; executor = mExecutor; generation = mListenerGeneration;
        }
        if (listener != null && executor != null) executor.execute(() -> {
            synchronized (ImageReader.this) {
                if (mClosed || generation != mListenerGeneration || listener != mListener) return;
            }
            listener.onImageAvailable(ImageReader.this);
        });
    }
    public synchronized Image acquireNextImage() { return acquire(false, false); }
    public synchronized Image acquireLatestImage() { return acquire(true, false); }
    public synchronized Image acquireNextImageNoThrowISE() { return acquire(false, true); }
    private Image acquire(boolean latest, boolean noThrow) {
        checkOpen();
        if (mAcquired.size() >= mMaxImages) {
            if (noThrow) return null;
            throw new IllegalStateException("maxImages already acquired; close an Image first");
        }
        if (mPending.isEmpty()) return null;
        // Android needs two available acquisition slots to discard older images.
        if (latest && mMaxImages - mAcquired.size() >= 2)
            while (mPending.size() > 1) mPending.removeFirst().bitmap.recycle();
        ReaderImage image = new ReaderImage(mPending.removeFirst());
        mAcquired.add(image);
        return image;
    }
    public void close() {
        synchronized (this) {
            if (mClosed) return;
            mClosed = true; mListener = null; mExecutor = null; mListenerGeneration++;
            while (!mPending.isEmpty()) mPending.removeFirst().bitmap.recycle();
            for (ReaderImage image : new ArrayList<>(mAcquired)) image.close();
        }
        // Never take the texture's lock while holding the reader lock: a producer may be posting.
        mTexture.huskSetFrameConsumer(null);
        mSurface.release();
        mTexture.release();
    }
    public int getWidth() { return mWidth; }
    public int getHeight() { return mHeight; }
    public int getImageFormat() { return mFormat; }
    public int getHardwareBufferFormat() { return mFormat; }
    public int getMaxImages() { return mMaxImages; }
    public int getDataSpace() { return mDataSpace; }
    public long getUsage() { return mUsage; }
    public synchronized Surface getSurface() { checkOpen(); return mSurface; }
    public synchronized void discardFreeBuffers() {
        checkOpen();
        // Snapshots have no reusable free-buffer pool. Queued or acquired frames
        // remain available: Android's discardFreeBuffers does not discard them.
    }
    public void detachImage(Image image) { throw new UnsupportedOperationException("ImageReader detachImage"); }
    public void setOnImageAvailableListener(OnImageAvailableListener listener, Handler handler) {
        Handler target = handler;
        if (listener != null && target == null) {
            Looper looper = Looper.myLooper();
            if (looper == null) throw new IllegalArgumentException("No Handler or current Looper");
            target = new Handler(looper);
        }
        final Handler destination = target;
        setOnImageAvailableListenerWithExecutor(listener, destination == null ? null : command -> destination.post(command));
    }
    public synchronized void setOnImageAvailableListenerWithExecutor(OnImageAvailableListener listener, Executor executor) {
        checkOpen();
        if (listener != null && executor == null) throw new IllegalArgumentException("executor");
        mListener = listener; mExecutor = executor; mListenerGeneration++;
    }
    public interface OnImageAvailableListener { void onImageAvailable(ImageReader reader); }

    private final class ReaderImage extends Image {
        private HardwareBuffer mBuffer;
        private long mTimestamp;
        private Rect mCrop = new Rect(0, 0, mWidth, mHeight);
        private ImagePlane mPlane;
        ReaderImage(Frame frame) {
            mBuffer = HardwareBuffer.huskTakeBitmap(frame.bitmap, mUsage);
            mTimestamp = frame.timestamp;
            mIsImageValid = true;
        }
        @Override protected void throwISEIfImageIsInvalid() { if (!mIsImageValid) throw new IllegalStateException("Image is closed"); }
        @Override public int getWidth() { throwISEIfImageIsInvalid(); return mWidth; }
        @Override public int getHeight() { throwISEIfImageIsInvalid(); return mHeight; }
        @Override public int getFormat() { throwISEIfImageIsInvalid(); return mFormat; }
        @Override public int getScalingMode() { throwISEIfImageIsInvalid(); return 0; }
        @Override public int getTransform() { throwISEIfImageIsInvalid(); return 0; }
        @Override public long getTimestamp() { throwISEIfImageIsInvalid(); return mTimestamp; }
        @Override public void setTimestamp(long timestamp) { throwISEIfImageIsInvalid(); mTimestamp = timestamp; }
        @Override public int getDataSpace() { throwISEIfImageIsInvalid(); return mDataSpace; }
        @Override public Rect getCropRect() { throwISEIfImageIsInvalid(); return new Rect(mCrop); }
        @Override public void setCropRect(Rect crop) {
            throwISEIfImageIsInvalid();
            mCrop = crop == null ? new Rect(0, 0, mWidth, mHeight) : new Rect(crop);
            if (!mCrop.intersect(0, 0, mWidth, mHeight)) mCrop.setEmpty();
        }
        @Override public HardwareBuffer getHardwareBuffer() {
            synchronized (ImageReader.this) { throwISEIfImageIsInvalid(); return mBuffer.huskRetain(); }
        }
        @Override public int getPlaneCount() { throwISEIfImageIsInvalid(); return 1; }
        @Override public Plane[] getPlanes() {
            synchronized (ImageReader.this) {
                throwISEIfImageIsInvalid();
                if (mPlane == null) {
                    ByteBuffer bytes = ByteBuffer.allocateDirect(mWidth * mHeight * 4);
                    mBuffer.huskCopyPixelsToBuffer(bytes);
                    bytes.rewind();
                    mPlane = new ImagePlane(this, bytes, mWidth * 4);
                }
                return new Plane[] { mPlane };
            }
        }
        @Override public void close() {
            synchronized (ImageReader.this) {
                if (!mIsImageValid) return;
                mIsImageValid = false;
                mBuffer.close(); mBuffer = null; mPlane = null;
                mAcquired.remove(this);
            }
        }
    }
    public static class ImagePlane extends Image.Plane {
        private final Image mImage;
        private final ByteBuffer mBytes;
        private final int mRowStride;
        private ImagePlane(Image image, ByteBuffer bytes, int rowStride) { mImage = image; mBytes = bytes; mRowStride = rowStride; }
        public ByteBuffer getBuffer() { mImage.throwISEIfImageIsInvalid(); return mBytes.duplicate(); }
        public int getPixelStride() { mImage.throwISEIfImageIsInvalid(); return 4; }
        public int getRowStride() { mImage.throwISEIfImageIsInvalid(); return mRowStride; }
    }
    public static ImagePlane[] initializeImagePlanes(int count, android.graphics.GraphicBuffer buffer, int fence,
            int format, long timestamp, int transform, int scaling, Rect crop) {
        throw new UnsupportedOperationException("GraphicBuffer import");
    }
    public static void unlockGraphicBuffer(android.graphics.GraphicBuffer buffer) { throw new UnsupportedOperationException("GraphicBuffer import"); }
    public static final class Builder {
        private final int mWidth, mHeight;
        private int mFormat = HardwareBuffer.RGBA_8888, mMaxImages = 1, mDataSpace;
        private long mUsage = HardwareBuffer.USAGE_CPU_READ_OFTEN;
        public Builder(int width, int height) { mWidth = width; mHeight = height; }
        public Builder setImageFormat(int format) { mFormat = format; return this; }
        public Builder setDefaultHardwareBufferFormat(int format) { mFormat = format; return this; }
        public Builder setDefaultDataSpace(int dataSpace) { mDataSpace = dataSpace; return this; }
        public Builder setMaxImages(int maxImages) { mMaxImages = maxImages; return this; }
        public Builder setUsage(long usage) { mUsage = usage; return this; }
        public ImageReader build() { return new ImageReader(mWidth, mHeight, mFormat, mMaxImages, mUsage, mDataSpace); }
    }
    @Override protected void finalize() throws Throwable { try { close(); } finally { super.finalize(); } }
}
