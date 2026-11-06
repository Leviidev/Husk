package android.hardware;

import android.graphics.Bitmap;
import android.os.Parcel;
import android.os.Parcelable;

/**
 * RGBA buffers backed by Husk's software pixels. These support ImageReader/Bitmap interop;
 * protected content, external GPU handles, parcel transfer and other formats are unsupported.
 */
public final class HardwareBuffer implements Parcelable, AutoCloseable {
    public static final int BGRA_1010102 = 67;
    public static final int BGRX_1010102 = 68;
    public static final int BLOB = 33;
    public static final int DS_24UI8 = 50;
    public static final int DS_FP32UI8 = 52;
    public static final int D_16 = 48;
    public static final int D_24 = 49;
    public static final int D_FP32 = 51;
    public static final int RGBA_10101010 = 59;
    public static final int RGBA_1010102 = 43;
    public static final int RGBA_12121212 = 65;
    public static final int RGBA_14141414 = 66;
    public static final int RGBA_8888 = 1;
    public static final int RGBA_FP16 = 22;
    public static final int RGBX_8888 = 2;
    public static final int RGB_565 = 4;
    public static final int RGB_888 = 3;
    public static final int RG_1212 = 63;
    public static final int RG_1414 = 64;
    public static final int RG_1616 = 58;
    public static final int R_12 = 61;
    public static final int R_14 = 62;
    public static final int R_16 = 57;
    public static final int R_8 = 56;
    public static final int S_UI8 = 53;
    public static final long USAGE_COMPOSER_OVERLAY = 2048L;
    public static final long USAGE_CPU_READ_OFTEN = 3L;
    public static final long USAGE_CPU_READ_RARELY = 2L;
    public static final long USAGE_CPU_WRITE_OFTEN = 48L;
    public static final long USAGE_CPU_WRITE_RARELY = 32L;
    public static final long USAGE_FRONT_BUFFER = 4294967296L;
    public static final long USAGE_GPU_COLOR_OUTPUT = 512L;
    public static final long USAGE_GPU_CUBE_MAP = 33554432L;
    public static final long USAGE_GPU_DATA_BUFFER = 16777216L;
    public static final long USAGE_GPU_MIPMAP_COMPLETE = 67108864L;
    public static final long USAGE_GPU_SAMPLED_IMAGE = 256L;
    public static final long USAGE_PROTECTED_CONTENT = 16384L;
    public static final long USAGE_SENSOR_DIRECT_DATA = 8388608L;
    public static final long USAGE_VIDEO_ENCODE = 65536L;
    public static final int YCBCR_420_888 = 35;
    public static final int YCBCR_P010 = 54;
    public static final int YCBCR_P210 = 60;
    private static final java.util.concurrent.atomic.AtomicLong sNextId = new java.util.concurrent.atomic.AtomicLong(1);
    private static final class Pixels {
        Bitmap bitmap;
        int refs = 1;
        final long id = sNextId.getAndIncrement();
        Pixels(Bitmap b) { bitmap = b; }
    }
    private final Pixels mPixels;
    private final int mWidth, mHeight, mFormat;
    private final long mUsage;
    private boolean mClosed;
    private HardwareBuffer(Pixels pixels, int width, int height, int format, long usage) {
        mPixels = pixels; mWidth = width; mHeight = height; mFormat = format; mUsage = usage;
    }
    private void checkOpen() { if (mClosed) throw new IllegalStateException("HardwareBuffer is closed"); }
    public static boolean isSupported(int width, int height, int format, int layers, long usage) {
        return width > 0 && height > 0 && (long) width * height <= Integer.MAX_VALUE / 4
            && layers == 1 && (format == RGBA_8888 || format == RGBX_8888)
            && (usage & (USAGE_PROTECTED_CONTENT | USAGE_GPU_CUBE_MAP | USAGE_GPU_MIPMAP_COMPLETE
                         | USAGE_SENSOR_DIRECT_DATA | USAGE_GPU_DATA_BUFFER)) == 0;
    }
    public static HardwareBuffer create(int width, int height, int format, int layers, long usage) {
        if (!isSupported(width, height, format, layers, usage)) throw new IllegalArgumentException("Unsupported software HardwareBuffer");
        Bitmap pixels = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        if (format == RGBX_8888) pixels.eraseColor(0xff000000);
        return new HardwareBuffer(new Pixels(pixels), width, height, format, usage);
    }
    /** Internal: takes ownership of an immutable RGBA snapshot from ImageReader. */
    public static HardwareBuffer huskTakeBitmap(Bitmap bitmap, long usage) {
        if (bitmap == null || bitmap.isRecycled()) throw new IllegalArgumentException("No buffer pixels");
        return new HardwareBuffer(new Pixels(bitmap), bitmap.getWidth(), bitmap.getHeight(), RGBA_8888, usage);
    }
    /** A separately closable reference, as Image.getHardwareBuffer requires. */
    public HardwareBuffer huskRetain() {
        synchronized (mPixels) { checkOpen(); mPixels.refs++; return new HardwareBuffer(mPixels, mWidth, mHeight, mFormat, mUsage); }
    }
    /** Internal: Bitmap.wrapHardwareBuffer gets an independent pixel snapshot. */
    public Bitmap huskCopyBitmap() { synchronized (mPixels) { checkOpen(); return mPixels.bitmap.copy(Bitmap.Config.ARGB_8888, false); } }
    public void huskCopyPixelsToBuffer(java.nio.Buffer buffer) { synchronized (mPixels) { checkOpen(); mPixels.bitmap.copyPixelsToBuffer(buffer); } }
    public void close() {
        synchronized (mPixels) {
            if (mClosed) return;
            mClosed = true;
            if (--mPixels.refs == 0) { mPixels.bitmap.recycle(); mPixels.bitmap = null; }
        }
    }
    public boolean isClosed() { synchronized (mPixels) { return mClosed; } }
    public int getWidth() { synchronized (mPixels) { checkOpen(); return mWidth; } }
    public int getHeight() { synchronized (mPixels) { checkOpen(); return mHeight; } }
    public int getFormat() { synchronized (mPixels) { checkOpen(); return mFormat; } }
    public int getLayers() { synchronized (mPixels) { checkOpen(); return 1; } }
    public long getUsage() { synchronized (mPixels) { checkOpen(); return mUsage; } }
    public long getId() { synchronized (mPixels) { checkOpen(); return mPixels.id; } }
    public static HardwareBuffer createFromGraphicBuffer(android.graphics.GraphicBuffer buffer) { throw new UnsupportedOperationException("GraphicBuffer import"); }
    public int describeContents() { return 0; }
    public void writeToParcel(Parcel parcel, int flags) { throw new UnsupportedOperationException("HardwareBuffer parcel transfer"); }
    public static final Parcelable.Creator<HardwareBuffer> CREATOR = new Parcelable.Creator<HardwareBuffer>() {
        public HardwareBuffer createFromParcel(Parcel parcel) { throw new UnsupportedOperationException("HardwareBuffer parcel transfer"); }
        public HardwareBuffer[] newArray(int size) { return new HardwareBuffer[size]; }
    };
    protected void finalize() throws Throwable { try { close(); } finally { super.finalize(); } }
}
